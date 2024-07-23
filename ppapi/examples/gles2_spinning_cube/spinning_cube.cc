// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This example program is based on Simple_VertexShader.c from:

//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

#include "ppapi/examples/gles2_spinning_cube/spinning_cube.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "ppapi/lib/gl/include/GLES2/gl2.h"

namespace {

const float kPi = 3.14159265359f;

int GenerateCube(GLuint *vbo_vertices,
                 GLuint *vbo_indices) {
  const int num_indices = 36;

  const GLfloat cube_vertices[] = {
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    0.5f, -0.5f,  0.5f,
    0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,
    0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    0.5f,  0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, 0.5f,
    -0.5f,  0.5f, 0.5f,
    0.5f,  0.5f, 0.5f,
    0.5f, -0.5f, 0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, -0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,
    0.5f,  0.5f, -0.5f,
  };

  const GLushort cube_indices[] = {
    0, 2, 1,
    0, 3, 2,
    4, 5, 6,
    4, 6, 7,
    8, 9, 10,
    8, 10, 11,
    12, 15, 14,
    12, 14, 13,
    16, 17, 18,
    16, 18, 19,
    20, 23, 22,
    20, 22, 21
  };

  if (vbo_vertices) {
    glGenBuffers(1, vbo_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo_vertices);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(cube_vertices),
                 cube_vertices,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  if (vbo_indices) {
    glGenBuffers(1, vbo_indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *vbo_indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(cube_indices),
                 cube_indices,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  return num_indices;
}

GLuint LoadShader(GLenum type,
                  const char* shader_source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &shader_source, NULL);
  glCompileShader(shader);

  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

  if (!compiled) {
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint LoadProgram(const char* vertext_shader_source,
                   const char* fragment_shader_source) {
  GLuint vertex_shader = LoadShader(GL_VERTEX_SHADER,
                                    vertext_shader_source);
  if (!vertex_shader)
    return 0;

  GLuint fragment_shader = LoadShader(GL_FRAGMENT_SHADER,
                                      fragment_shader_source);
  if (!fragment_shader) {
    glDeleteShader(vertex_shader);
    return 0;
  }

  GLuint program_object = glCreateProgram();
  glAttachShader(program_object, vertex_shader);
  glAttachShader(program_object, fragment_shader);

  glLinkProgram(program_object);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  GLint linked = 0;
  glGetProgramiv(program_object, GL_LINK_STATUS, &linked);

  if (!linked) {
    glDeleteProgram(program_object);
    return 0;
  }

  return program_object;
}

class ESMatrix {
 public:
  GLfloat m[4][4];

  ESMatrix() {
    LoadZero();
  }

  void LoadZero() {
    memset(this, 0x0, sizeof(ESMatrix));
  }

  void LoadIdentity() {
    LoadZero();
    m[0][0] = 1.0f;
    m[1][1] = 1.0f;
    m[2][2] = 1.0f;
    m[3][3] = 1.0f;
  }

  void Multiply(ESMatrix* a, ESMatrix* b) {
    ESMatrix result;
    for (int i = 0; i < 4; ++i) {
      result.m[i][0] = (a->m[i][0] * b->m[0][0]) +
                       (a->m[i][1] * b->m[1][0]) +
                       (a->m[i][2] * b->m[2][0]) +
                       (a->m[i][3] * b->m[3][0]);

      result.m[i][1] = (a->m[i][0] * b->m[0][1]) +
                       (a->m[i][1] * b->m[1][1]) +
                       (a->m[i][2] * b->m[2][1]) +
                       (a->m[i][3] * b->m[3][1]);

      result.m[i][2] = (a->m[i][0] * b->m[0][2]) +
                       (a->m[i][1] * b->m[1][2]) +
                       (a->m[i][2] * b->m[2][2]) +
                       (a->m[i][3] * b->m[3][2]);

      result.m[i][3] = (a->m[i][0] * b->m[0][3]) +
                       (a->m[i][1] * b->m[1][3]) +
                       (a->m[i][2] * b->m[2][3]) +
                       (a->m[i][3] * b->m[3][3]);
    }
    *this = result;
  }

  void Frustum(float left,
               float right,
               float bottom,
               float top,
               float near_z,
               float far_z) {
    float delta_x = right - left;
    float delta_y = top - bottom;
    float delta_z = far_z - near_z;

    if ((near_z <= 0.0f) ||
        (far_z <= 0.0f) ||
        (delta_z <= 0.0f) ||
        (delta_y <= 0.0f) ||
        (delta_y <= 0.0f))
      return;

    ESMatrix frust;
    frust.m[0][0] = 2.0f * near_z / delta_x;
    frust.m[0][1] = frust.m[0][2] = frust.m[0][3] = 0.0f;

    frust.m[1][1] = 2.0f * near_z / delta_y;
    frust.m[1][0] = frust.m[1][2] = frust.m[1][3] = 0.0f;

    frust.m[2][0] = (right + left) / delta_x;
    frust.m[2][1] = (top + bottom) / delta_y;
    frust.m[2][2] = -(near_z + far_z) / delta_z;
    frust.m[2][3] = -1.0f;

    frust.m[3][2] = -2.0f * near_z * far_z / delta_z;
    frust.m[3][0] = frust.m[3][1] = frust.m[3][3] = 0.0f;

    Multiply(&frust, this);
  }

  void Perspective(float fov_y, float aspect, float near_z, float far_z) {
    GLfloat frustum_h = tanf(fov_y / 360.0f * kPi) * near_z;
    GLfloat frustum_w = frustum_h * aspect;
    Frustum(-frustum_w, frustum_w, -frustum_h, frustum_h, near_z, far_z);
  }

  void Translate(GLfloat tx, GLfloat ty, GLfloat tz) {
    m[3][0] += m[0][0] * tx + m[1][0] * ty + m[2][0] * tz;
    m[3][1] += m[0][1] * tx + m[1][1] * ty + m[2][1] * tz;
    m[3][2] += m[0][2] * tx + m[1][2] * ty + m[2][2] * tz;
    m[3][3] += m[0][3] * tx + m[1][3] * ty + m[2][3] * tz;
  }

  void Rotate(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    GLfloat mag = sqrtf(x * x + y * y + z * z);

    GLfloat sin_angle = sinf(angle * kPi / 180.0f);
    GLfloat cos_angle = cosf(angle * kPi / 180.0f);
    if (mag > 0.0f) {
      GLfloat xx, yy, zz, xy, yz, zx, xs, ys, zs;
      GLfloat one_minus_cos;
      ESMatrix rotation;

      x /= mag;
      y /= mag;
      z /= mag;

      xx = x * x;
      yy = y * y;
      zz = z * z;
      xy = x * y;
      yz = y * z;
      zx = z * x;
      xs = x * sin_angle;
      ys = y * sin_angle;
      zs = z * sin_angle;
      one_minus_cos = 1.0f - cos_angle;

      rotation.m[0][0] = (one_minus_cos * xx) + cos_angle;
      rotation.m[0][1] = (one_minus_cos * xy) - zs;
      rotation.m[0][2] = (one_minus_cos * zx) + ys;
      rotation.m[0][3] = 0.0F;

      rotation.m[1][0] = (one_minus_cos * xy) + zs;
      rotation.m[1][1] = (one_minus_cos * yy) + cos_angle;
      rotation.m[1][2] = (one_minus_cos * yz) - xs;
      rotation.m[1][3] = 0.0F;

      rotation.m[2][0] = (one_minus_cos * zx) - ys;
      rotation.m[2][1] = (one_minus_cos * yz) + xs;
      rotation.m[2][2] = (one_minus_cos * zz) + cos_angle;
      rotation.m[2][3] = 0.0F;

      rotation.m[3][0] = 0.0F;
      rotation.m[3][1] = 0.0F;
      rotation.m[3][2] = 0.0F;
      rotation.m[3][3] = 1.0F;

      Multiply(&rotation, this);
    }
  }
};

float RotationForTimeDelta(float delta_time) {
  return delta_time * 40.0f;
}

float RotationForDragDistance(float drag_distance) {
  return drag_distance / 5; // Arbitrary damping.
}

}  // namespace

class SpinningCube::GLState {
 public:
  GLState();

  void OnGLContextLost();

  GLfloat angle_;  // Survives losing the GL context.

  GLuint program_object_;
  GLint position_location_;
  GLint mvp_location_;
  GLuint vbo_vertices_;
  GLuint vbo_indices_;
  int num_indices_;
  ESMatrix mvp_matrix_;
};

SpinningCube::GLState::GLState()
    : angle_(0) {
  OnGLContextLost();
}

void SpinningCube::GLState::OnGLContextLost() {
  program_object_ = 0;
  position_location_ = 0;
  mvp_location_ = 0;
  vbo_vertices_ = 0;
  vbo_indices_ = 0;
  num_indices_ = 0;
}

SpinningCube::SpinningCube()
    : initialized_(false),
      width_(0),
      height_(0),
      state_(new GLState()),
      fling_multiplier_(1.0f),
      direction_(1) {
  state_->angle_ = 45.0f;
}

SpinningCube::~SpinningCube() {
  if (!initialized_)
    return;
  if (state_->vbo_vertices_)
    glDeleteBuffers(1, &state_->vbo_vertices_);
  if (state_->vbo_indices_)
    glDeleteBuffers(1, &state_->vbo_indices_);
  if (state_->program_object_)
    glDeleteProgram(state_->program_object_);

  delete state_;
}

void SpinningCube::Init(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;

  if (!initialized_) {
    initialized_ = true;
    const char vertext_shader_source[] =
        "uniform mat4 u_mvpMatrix;                   \n"
        "attribute vec4 a_position;                  \n"
        "void main()                                 \n"
        "{                                           \n"
        "   gl_Position = u_mvpMatrix * a_position;  \n"
        "}                                           \n";

    const char fragment_shader_source[] =
        "precision mediump float;                            \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  gl_FragColor = vec4( 0.0, 0.0, 1.0, 1.0 );        \n"
        "}                                                   \n";

    state_->program_object_ = LoadProgram(
        vertext_shader_source, fragment_shader_source);
    state_->position_location_ = glGetAttribLocation(
        state_->program_object_, "a_position");
    state_->mvp_location_ = glGetUniformLocation(
        state_->program_object_, "u_mvpMatrix");
    state_->num_indices_ = GenerateCube(
        &state_->vbo_vertices_, &state_->vbo_indices_);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  }
}

void SpinningCube::OnGLContextLost() {
  // TODO(yzshen): Is it correct that in this case we don't need to do cleanup
  // for program and buffers?
  initialized_ = false;
  height_ = 0;
  width_ = 0;
  state_->OnGLContextLost();
}

void SpinningCube::SetFlingMultiplier(float drag_distance,
                                      float drag_time) {
  fling_multiplier_ = RotationForDragDistance(drag_distance) /
      RotationForTimeDelta(drag_time);

}

void SpinningCube::UpdateForTimeDelta(float delta_time) {
  state_->angle_ += RotationForTimeDelta(delta_time) * fling_multiplier_;
  if (state_->angle_ >= 360.0f)
    state_->angle_ -= 360.0f;

  // Arbitrary 50-step linear reduction in spin speed.
  if (fling_multiplier_ > 1.0f) {
    fling_multiplier_ =
        std::max(1.0f, fling_multiplier_ - (fling_multiplier_ - 1.0f) / 50);
  }

  Update();
}

void SpinningCube::UpdateForDragDistance(float distance) {
  state_->angle_ += RotationForDragDistance(distance);
  if (state_->angle_ >= 360.0f )
    state_->angle_ -= 360.0f;

  Update();
}

void SpinningCube::Draw() {
  glViewport(0, 0, width_, height_);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(state_->program_object_);
  glBindBuffer(GL_ARRAY_BUFFER, state_->vbo_vertices_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state_->vbo_indices_);
  glVertexAttribPointer(state_->position_location_,
                           3,
                           GL_FLOAT,
                           GL_FALSE, 3 * sizeof(GLfloat),
                           0);
  glEnableVertexAttribArray(state_->position_location_);
  glUniformMatrix4fv(state_->mvp_location_,
                        1,
                        GL_FALSE,
                        (GLfloat*) &state_->mvp_matrix_.m[0][0]);
  glDrawElements(GL_TRIANGLES,
                    state_->num_indices_,
                    GL_UNSIGNED_SHORT,
                    0);
}

void SpinningCube::Update() {
  float aspect = static_cast<GLfloat>(width_) / static_cast<GLfloat>(height_);

  ESMatrix perspective;
  perspective.LoadIdentity();
  perspective.Perspective(60.0f, aspect, 1.0f, 20.0f );

  ESMatrix modelview;
  modelview.LoadIdentity();
  modelview.Translate(0.0, 0.0, -2.0);
  modelview.Rotate(state_->angle_ * direction_, 1.0, 0.0, 1.0);

  state_->mvp_matrix_.Multiply(&modelview, &perspective);
}
