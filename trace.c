#include <stdio.h>
#include <stdlib.h>
#define __USE_XOPEN /* For M_PI, apparently */
#include <math.h>
#include "SDL.h"
#define GL_GLEXT_PROTOTYPES /* Needed to actually get function prototypes */
#include "SDL_opengl.h"

#define STRING(x) #x
#define XSTRING(x) STRING(x)
#define SPHERE_COUNT 5

/* |vt−c|² = r² = t²−2(c·v)t+c·c */
/* t = (2(c·v)−√[4(c·v)²−4(c·c−r²)])/2 = c·v−√[(c·v)²+r²−c·c] */
const GLchar *shaderSource =
    "#version 120\n"
XSTRING(
    const vec2 size = vec2(640, 480);
    const float fov = 75; /* horizontal */
    const vec2 slope = tan(radians(fov/2)) * vec2(1, size.y/size.x);
    const vec3 lightDirection = normalize(vec3(2, 4, 1));

    uniform vec4[SPHERE_COUNT] spheres; /* xyz = center, w = radius */

    void castRay(
        const in vec3 point, const in vec3 direction, out vec4 color,
        out vec3 next_point, out vec3 next_direction
    ) {
        float distance = 0;
        int index = 0;
        for (int i = 0; i < SPHERE_COUNT; i++) {
            vec3 center = spheres[i].xyz - point;
            float center_dot_dir = dot(center, direction);
            float discriminant =
                center_dot_dir*center_dot_dir + spheres[i].w*spheres[i].w
                - dot(center, center);
            if (discriminant >= 0) {
                float this_dist = center_dot_dir - sqrt(discriminant);
                if (this_dist > 0 && (distance == 0 || this_dist < distance)) {
                    distance = this_dist;
                    index = i;
                }
            }
        }
        if (distance > 0) {
            next_point = point + distance*direction;
            vec3 normal = (next_point-spheres[index].xyz) / spheres[index].w;
            color = vec4(
                vec3(.8, .5, .9)
                * max(0, dot(normal, lightDirection)),
                .85
            );
            next_direction =
                reflect(direction, normalize(next_point-spheres[index].xyz));
        }
        else {
            color = vec4(0);
            next_point = point;
            next_direction = direction;
        }
    }

    void accumulate_color(const in vec4 new_color, inout vec4 color) {
        color.rgb = mix(
            color.rgb, new_color.rgb, (1-color.a) / (color.a+new_color.a)
        );
        color.a += (1-color.a) * new_color.a;
    }

    void main() {
        vec3 direction =
            normalize(vec3((2*gl_FragCoord.xy/size-1) * slope, -1));
        gl_FragColor = vec4(0);
        vec3 point = vec3(0);
        vec3 next_point;
        vec3 next_direction;
        for (int i = 0; i < 5; i++) {
            vec4 color;
            castRay(point, direction, color, next_point, next_direction);
            if (color.a == 0) break;
            accumulate_color(color, gl_FragColor);
            point = next_point;
            direction = next_direction;
        }
        vec4 color;
        if (direction.y >= 0) {
            color = vec4(vec3(.5, .9, 1) * (1-direction.y), 1);
        }
        else {
            vec2 floorCoords =
                direction.xz*(point.y+1)/(-direction.y)+point.xz + vec2(0,5);
            float r = length(floorCoords);
            float r_f = fract(r);
            float angle = atan(floorCoords.y, floorCoords.x) + M_PI;
            float angle_f = fract(ceil(2*M_PI*ceil(r)) * angle / (2*M_PI));
            color = mix(
                vec4(.2, .05, 0, 1), vec4(.8, .2, .15, 1),
                .5 + (pow(16*r_f*(1-r_f)*angle_f*(1-angle_f), .3) - .5) * 10 / (10+r)
            );
            for (int i = 0; i < SPHERE_COUNT; i++) {
                vec3 delta = spheres[i].xyz - vec3(floorCoords-vec2(0,5), -1).xzy;
                if (
                    length(delta - dot(delta, lightDirection)*lightDirection)
                    < spheres[i].w
                ) {
                    color.rgb *= .2;
                    break;
                }
            }
        }
        accumulate_color(color, gl_FragColor);
    }
);

void render(SDL_Window *window) {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLE_STRIP);
        glVertex2d(-1.0, -1.0);
        glVertex2d(1.0, -1.0);
        glVertex2d(-1.0, 1.0);
        glVertex2d(1.0, 1.0);
    glEnd();
    SDL_GL_SwapWindow(window);
}

int main() {
    SDL_Window *window;
    SDL_GLContext context;
    GLuint program;
    GLuint shader;
    GLint compileStatus;
    GLint linkStatus;
    GLint sphereLocation;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    atexit(SDL_Quit);
    window = SDL_CreateWindow(
        "GLSL Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480, SDL_WINDOW_OPENGL
    );
    if (window == NULL) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return 1;
    }
    context = SDL_GL_CreateContext(window);
    if (context == NULL) {
        fprintf(
            stderr, "OpenGL context creation failed: %s\n", SDL_GetError()
        );
        return 1;
    }
    glMatrixMode(GL_PROJECTION);
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader, 1, &shaderSource, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus != GL_TRUE) {
        GLint infoLogLength;
        GLchar *infoLog;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
        infoLog = malloc(infoLogLength+1);
        glGetShaderInfoLog(shader, infoLogLength+1, NULL, infoLog);
        fprintf(stderr, "Shader compilation failed: %s\n", infoLog);
        free(infoLog);
        return 0;
    }
    program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        GLint infoLogLength;
        GLchar *infoLog;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
        infoLog = malloc(infoLogLength+1);
        glGetProgramInfoLog(program, infoLogLength+1, NULL, infoLog);
        fprintf(stderr, "Shader linking failed: %s\n", infoLog);
        free(infoLog);
        return 0;
    }
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glUseProgram(program);
    sphereLocation = glGetUniformLocation(program, "spheres");
    for (;;) {
        SDL_Event event;
        float time;
        GLenum error;
        if (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) break;
            if (event.type == SDL_KEYDOWN) break;
        }
        time = (SDL_GetTicks() % 5000) / 5000.f;
        {
            GLfloat spheres[SPHERE_COUNT*4];
            int i;
            for (i = 0; i < SPHERE_COUNT; i++) {
                spheres[4*i] = 3.f*sinf(2*M_PI*(time+(float)i/SPHERE_COUNT));
                spheres[4*i+1] =
                    .5f+0.5f*sinf(2*M_PI*(time+(float)i/SPHERE_COUNT-.3f));
                spheres[4*i+2] =
                    -4.f+2.f*sinf(4*M_PI*(time+(float)i/SPHERE_COUNT));
                spheres[4*i+3] = .2f + .8f*expf(-(float)i/SPHERE_COUNT);
            }
            glUniform4fv(sphereLocation, SPHERE_COUNT, spheres);
        }
        render(window);
    }
    SDL_GL_DeleteContext(context);
    return 0;
}
