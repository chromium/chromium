// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common/fps.h"
#include "matrix.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

extern const uint8_t kRLETextureData[];
extern const size_t kRLETextureDataLength;

namespace {

const float kMinFovY = 45.0f;
const float kZNear = 1.0f;
const float kZFar = 10.0f;
const float kCameraZ = -4.0f;
const float kXAngleDelta = 2.0f;
const float kYAngleDelta = 0.5f;

const size_t kTextureDataLength = 128 * 128 * 3;  // 128x128, 3 Bytes/pixel.

// The decompressed data is written here.
uint8_t g_texture_data[kTextureDataLength];

void DecompressTexture() {
  // The image is first encoded with a very simple RLE scheme:
  //   <value0> <count0> <value1> <count1> ...
  // Because a <count> of 0 is useless, we use it to represent 256.
  //
  // It is then Base64 encoded to make it use only printable characters (it
  // stores more easily in a source file that way).
  //
  // To decompress, we have to reverse the process.
  static const uint8_t kBase64Decode[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62,  0,  0,  0, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,
    0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,  0,  0,  0,  0,
    0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  };
  const uint8_t* input = &kRLETextureData[0];
  const uint8_t* const input_end = &kRLETextureData[kRLETextureDataLength];
  uint8_t* output = &g_texture_data[0];
#ifndef NDEBUG
  const uint8_t* const output_end = &g_texture_data[kTextureDataLength];
#endif

  uint8_t decoded[4];
  int decoded_count = 0;

  while (input < input_end || decoded_count > 0) {
    if (decoded_count < 2) {
      assert(input + 4 <= input_end);
      // Grab four base-64 encoded (6-bit) bytes.
      uint32_t data = 0;
      data |= (kBase64Decode[*input++] << 18);
      data |= (kBase64Decode[*input++] << 12);
      data |= (kBase64Decode[*input++] <<  6);
      data |= (kBase64Decode[*input++]      );
      // And decode it to 3 (8-bit) bytes.
      decoded[decoded_count++] = (data >> 16) & 0xff;
      decoded[decoded_count++] = (data >>  8) & 0xff;
      decoded[decoded_count++] = (data      ) & 0xff;

      // = is the base64 end marker. Remove decoded bytes if we see any.
      if (input[-1] == '=') decoded_count--;
      if (input[-2] == '=') decoded_count--;
    }

    int value = decoded[0];
    int count = decoded[1];
    decoded_count -= 2;
    // Move the other decoded bytes (if any) down.
    decoded[0] = decoded[2];
    decoded[1] = decoded[3];

    // Expand the RLE data.
    if (count == 0)
      count = 256;
    assert(output <= output_end);
    memset(output, value, count);
    output += count;
  }
  assert(output == output_end);
}

GLuint CompileShader(GLenum type, const char* data) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &data, NULL);
  glCompileShader(shader);

  GLint compile_status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (compile_status != GL_TRUE) {
    // Shader failed to compile, let's see what the error is.
    char buffer[1024];
    GLsizei length;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, &buffer[0]);
    fprintf(stderr, "Shader failed to compile: %s\n", buffer);
    return 0;
  }

  return shader;
}

GLuint LinkProgram(GLuint frag_shader, GLuint vert_shader) {
  GLuint program = glCreateProgram();
  glAttachShader(program, frag_shader);
  glAttachShader(program, vert_shader);
  glLinkProgram(program);

  GLint link_status;
  glGetProgramiv(program, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    // Program failed to link, let's see what the error is.
    char buffer[1024];
    GLsizei length;
    glGetProgramInfoLog(program, sizeof(buffer), &length, &buffer[0]);
    fprintf(stderr, "Program failed to link: %s\n", buffer);
    return 0;
  }

  return program;
}

const char kFragShaderSource[] =
    "precision mediump float;\n"
    "varying vec3 v_color;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(u_texture, v_texcoord);\n"
    "  gl_FragColor += vec4(v_color, 1);\n"
    "}\n";

const char kVertexShaderSource[] =
    "uniform mat4 u_mvp;\n"
    "attribute vec2 a_texcoord;\n"
    "attribute vec3 a_color;\n"
    "attribute vec4 a_position;\n"
    "varying vec3 v_color;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "  gl_Position = u_mvp * a_position;\n"
    "  v_color = a_color;\n"
    "  v_texcoord = a_texcoord;\n"
    "}\n";

struct Vertex {
  float loc[3];
  float color[3];
  float tex[2];
};

const Vertex kCubeVerts[24] = {
  // +Z (red arrow, black tip)
  {{-1.0, -1.0, +1.0}, {0.0, 0.0, 0.0}, {1.0, 0.0}},
  {{+1.0, -1.0, +1.0}, {0.0, 0.0, 0.0}, {0.0, 0.0}},
  {{+1.0, +1.0, +1.0}, {0.5, 0.0, 0.0}, {0.0, 1.0}},
  {{-1.0, +1.0, +1.0}, {0.5, 0.0, 0.0}, {1.0, 1.0}},

  // +X (green arrow, black tip)
  {{+1.0, -1.0, -1.0}, {0.0, 0.0, 0.0}, {1.0, 0.0}},
  {{+1.0, +1.0, -1.0}, {0.0, 0.0, 0.0}, {0.0, 0.0}},
  {{+1.0, +1.0, +1.0}, {0.0, 0.5, 0.0}, {0.0, 1.0}},
  {{+1.0, -1.0, +1.0}, {0.0, 0.5, 0.0}, {1.0, 1.0}},

  // +Y (blue arrow, black tip)
  {{-1.0, +1.0, -1.0}, {0.0, 0.0, 0.0}, {1.0, 0.0}},
  {{-1.0, +1.0, +1.0}, {0.0, 0.0, 0.0}, {0.0, 0.0}},
  {{+1.0, +1.0, +1.0}, {0.0, 0.0, 0.5}, {0.0, 1.0}},
  {{+1.0, +1.0, -1.0}, {0.0, 0.0, 0.5}, {1.0, 1.0}},

  // -Z (red arrow, red tip)
  {{+1.0, +1.0, -1.0}, {0.0, 0.0, 0.0}, {1.0, 1.0}},
  {{-1.0, +1.0, -1.0}, {0.0, 0.0, 0.0}, {0.0, 1.0}},
  {{-1.0, -1.0, -1.0}, {1.0, 0.0, 0.0}, {0.0, 0.0}},
  {{+1.0, -1.0, -1.0}, {1.0, 0.0, 0.0}, {1.0, 0.0}},

  // -X (green arrow, green tip)
  {{-1.0, +1.0, +1.0}, {0.0, 0.0, 0.0}, {1.0, 1.0}},
  {{-1.0, -1.0, +1.0}, {0.0, 0.0, 0.0}, {0.0, 1.0}},
  {{-1.0, -1.0, -1.0}, {0.0, 1.0, 0.0}, {0.0, 0.0}},
  {{-1.0, +1.0, -1.0}, {0.0, 1.0, 0.0}, {1.0, 0.0}},

  // -Y (blue arrow, blue tip)
  {{+1.0, -1.0, +1.0}, {0.0, 0.0, 0.0}, {1.0, 1.0}},
  {{+1.0, -1.0, -1.0}, {0.0, 0.0, 0.0}, {0.0, 1.0}},
  {{-1.0, -1.0, -1.0}, {0.0, 0.0, 1.0}, {0.0, 0.0}},
  {{-1.0, -1.0, +1.0}, {0.0, 0.0, 1.0}, {1.0, 0.0}},
};

const GLubyte kCubeIndexes[36] = {
   2,  1,  0,  3,  2,  0,
   6,  5,  4,  7,  6,  4,
  10,  9,  8, 11, 10,  8,
  14, 13, 12, 15, 14, 12,
  18, 17, 16, 19, 18, 16,
  22, 21, 20, 23, 22, 20,
};

}  // namespace


class CubeInstance : public pp::Instance {
 public:
  explicit CubeInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        width_(0),
        height_(0),
        frag_shader_(0),
        vertex_shader_(0),
        program_(0),
        texture_loc_(0),
        position_loc_(0),
        color_loc_(0),
        mvp_loc_(0),
        x_angle_(0),
        y_angle_(0),
        animating_(true) {
    FpsInit(&fps_state_);
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  virtual void DidChangeView(const pp::View& view) {
    int32_t new_width = view.GetRect().width();
    int32_t new_height = view.GetRect().height();

    if (context_.is_null()) {
      if (!InitGL(new_width, new_height)) {
        // failed.
        return;
      }

      InitShaders();
      InitBuffers();
      InitTexture();
      MainLoop(0);
    } else {
      // Resize the buffers to the new size of the module.
      int32_t result = context_.ResizeBuffers(new_width, new_height);
      if (result < 0) {
        fprintf(stderr,
                "Unable to resize buffers to %d x %d!\n",
                new_width,
                new_height);
        return;
      }
    }

    width_ = new_width;
    height_ = new_height;
    glViewport(0, 0, width_, height_);
  }

  virtual void HandleMessage(const pp::Var& message) {
    // A bool message sets whether the cube is animating or not.
    if (message.is_bool()) {
      animating_ = message.AsBool();
      return;
    }

    // An array message sets the current x and y rotation.
    if (!message.is_array()) {
      fprintf(stderr, "Expected array message.\n");
      return;
    }

    pp::VarArray array(message);
    if (array.GetLength() != 2) {
      fprintf(stderr, "Expected array of length 2.\n");
      return;
    }

    pp::Var x_angle_var = array.Get(0);
    if (x_angle_var.is_int()) {
      x_angle_ = x_angle_var.AsInt();
    } else if (x_angle_var.is_double()) {
      x_angle_ = x_angle_var.AsDouble();
    } else {
      fprintf(stderr, "Expected value to be an int or double.\n");
    }

    pp::Var y_angle_var = array.Get(1);
    if (y_angle_var.is_int()) {
      y_angle_ = y_angle_var.AsInt();
    } else if (y_angle_var.is_double()) {
      y_angle_ = y_angle_var.AsDouble();
    } else {
      fprintf(stderr, "Expected value to be an int or double.\n");
    }
  }

 private:
  bool InitGL(int32_t new_width, int32_t new_height) {
    if (!glInitializePPAPI(pp::Module::Get()->get_browser_interface())) {
      fprintf(stderr, "Unable to initialize GL PPAPI!\n");
      return false;
    }

    const int32_t attrib_list[] = {
      PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
      PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
      PP_GRAPHICS3DATTRIB_WIDTH, new_width,
      PP_GRAPHICS3DATTRIB_HEIGHT, new_height,
      PP_GRAPHICS3DATTRIB_NONE
    };

    context_ = pp::Graphics3D(this, attrib_list);
    if (!BindGraphics(context_)) {
      fprintf(stderr, "Unable to bind 3d context!\n");
      context_ = pp::Graphics3D();
      glSetCurrentContextPPAPI(0);
      return false;
    }

    glSetCurrentContextPPAPI(context_.pp_resource());
    return true;
  }

  void InitShaders() {
    frag_shader_ = CompileShader(GL_FRAGMENT_SHADER, kFragShaderSource);
    if (!frag_shader_)
      return;

    vertex_shader_ = CompileShader(GL_VERTEX_SHADER, kVertexShaderSource);
    if (!vertex_shader_)
      return;

    program_ = LinkProgram(frag_shader_, vertex_shader_);
    if (!program_)
      return;

    texture_loc_ = glGetUniformLocation(program_, "u_texture");
    position_loc_ = glGetAttribLocation(program_, "a_position");
    texcoord_loc_ = glGetAttribLocation(program_, "a_texcoord");
    color_loc_ = glGetAttribLocation(program_, "a_color");
    mvp_loc_ = glGetUniformLocation(program_, "u_mvp");
  }

  void InitBuffers() {
    glGenBuffers(1, &vertex_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVerts), &kCubeVerts[0],
                 GL_STATIC_DRAW);

    glGenBuffers(1, &index_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kCubeIndexes),
                 &kCubeIndexes[0], GL_STATIC_DRAW);
  }

  void InitTexture() {
    DecompressTexture();
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 128,
                 128,
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 &g_texture_data[0]);
  }

  void Animate() {
    if (animating_) {
      x_angle_ = fmod(360.0f + x_angle_ + kXAngleDelta, 360.0f);
      y_angle_ = fmod(360.0f + y_angle_ + kYAngleDelta, 360.0f);

      // Send new values to JavaScript.
      pp::VarArray array;
      array.SetLength(2);
      array.Set(0, x_angle_);
      array.Set(1, y_angle_);
      PostMessage(array);
    }
  }

  void Render() {
    glClearColor(0.5, 0.5, 0.5, 1);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    //set what program to use
    glUseProgram(program_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glUniform1i(texture_loc_, 0);

    //create our perspective matrix
    float mvp[16];
    float trs[16];
    float rot[16];

    identity_matrix(mvp);
    const float aspect_ratio = static_cast<float>(width_) / height_;
    float fovY = kMinFovY / aspect_ratio;
    if (fovY < kMinFovY)
      fovY = kMinFovY;

    glhPerspectivef2(&mvp[0], fovY, aspect_ratio, kZNear, kZFar);

    translate_matrix(0, 0, kCameraZ, trs);
    rotate_matrix(x_angle_, y_angle_, 0.0f, rot);
    multiply_matrix(trs, rot, trs);
    multiply_matrix(mvp, trs, mvp);
    glUniformMatrix4fv(mvp_loc_, 1, GL_FALSE, mvp);

    //define the attributes of the vertex
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glVertexAttribPointer(position_loc_,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, loc)));
    glEnableVertexAttribArray(position_loc_);
    glVertexAttribPointer(color_loc_,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(color_loc_);
    glVertexAttribPointer(texcoord_loc_,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, tex)));
    glEnableVertexAttribArray(texcoord_loc_);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, 0);
  }

  void MainLoop(int32_t) {
    Animate();
    Render();
    context_.SwapBuffers(
        callback_factory_.NewCallback(&CubeInstance::MainLoop));

    double fps;
    if (FpsStep(&fps_state_, &fps))
      PostMessage(fps);
  }

  pp::CompletionCallbackFactory<CubeInstance> callback_factory_;
  pp::Graphics3D context_;
  int32_t width_;
  int32_t height_;
  GLuint frag_shader_;
  GLuint vertex_shader_;
  GLuint program_;
  GLuint vertex_buffer_;
  GLuint index_buffer_;
  GLuint texture_;

  GLuint texture_loc_;
  GLuint position_loc_;
  GLuint texcoord_loc_;
  GLuint color_loc_;
  GLuint mvp_loc_;

  float x_angle_;
  float y_angle_;
  bool animating_;
  FpsState fps_state_;
};

class CubeModule : public pp::Module {
 public:
  CubeModule() : pp::Module() {}
  virtual ~CubeModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new CubeInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new CubeModule(); }
}  // namespace pp
