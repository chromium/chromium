// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/media_stream_video_track.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/cpp/video_frame.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/utility/completion_callback_factory.h"

// When compiling natively on Windows, PostMessage can be #define-d to
// something else.
#ifdef PostMessage
#undef PostMessage
#endif

// Assert |context_| isn't holding any GL Errors.  Done as a macro instead of a
// function to preserve line number information in the failure message.
#define AssertNoGLError() \
  PP_DCHECK(!glGetError());

namespace {

// This object is the global object representing this plugin library as long
// as it is loaded.
class MediaStreamVideoModule : public pp::Module {
 public:
  MediaStreamVideoModule() : pp::Module() {}
  virtual ~MediaStreamVideoModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance);
};

class MediaStreamVideoDemoInstance : public pp::Instance,
                        public pp::Graphics3DClient {
 public:
  MediaStreamVideoDemoInstance(PP_Instance instance, pp::Module* module);
  virtual ~MediaStreamVideoDemoInstance();

  // pp::Instance implementation (see PPP_Instance).
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip_ignored);
  virtual void HandleMessage(const pp::Var& message_data);

  // pp::Graphics3DClient implementation.
  virtual void Graphics3DContextLost() {
    InitGL();
    CreateTextures();
    Render();
  }

 private:
  void DrawYUV();
  void DrawRGB();
  void Render();

  // GL-related functions.
  void InitGL();
  GLuint CreateTexture(int32_t width, int32_t height, int unit, bool rgba);
  void CreateGLObjects();
  void CreateShader(GLuint program, GLenum type, const char* source);
  void PaintFinished(int32_t result);
  void CreateTextures();
  void ConfigureTrack();


  // MediaStreamVideoTrack callbacks.
  void OnConfigure(int32_t result);
  void OnGetFrame(int32_t result, pp::VideoFrame frame);

  pp::Size position_size_;
  bool is_painting_;
  bool needs_paint_;
  bool is_bgra_;
  GLuint program_yuv_;
  GLuint program_rgb_;
  GLuint buffer_;
  GLuint texture_y_;
  GLuint texture_u_;
  GLuint texture_v_;
  GLuint texture_rgb_;
  pp::MediaStreamVideoTrack video_track_;
  pp::CompletionCallbackFactory<MediaStreamVideoDemoInstance> callback_factory_;
  std::vector<int32_t> attrib_list_;

  // MediaStreamVideoTrack attributes:
  bool need_config_;
  PP_VideoFrame_Format attrib_format_;
  int32_t attrib_width_;
  int32_t attrib_height_;

  // Owned data.
  pp::Graphics3D* context_;

  pp::Size frame_size_;
};

MediaStreamVideoDemoInstance::MediaStreamVideoDemoInstance(
    PP_Instance instance, pp::Module* module)
    : pp::Instance(instance),
      pp::Graphics3DClient(this),
      is_painting_(false),
      needs_paint_(false),
      is_bgra_(false),
      texture_y_(0),
      texture_u_(0),
      texture_v_(0),
      texture_rgb_(0),
      callback_factory_(this),
      need_config_(false),
      attrib_format_(PP_VIDEOFRAME_FORMAT_I420),
      attrib_width_(0),
      attrib_height_(0),
      context_(NULL) {
  if (!glInitializePPAPI(pp::Module::Get()->get_browser_interface())) {
    LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Unable to initialize GL PPAPI!"));
    assert(false);
  }
}

MediaStreamVideoDemoInstance::~MediaStreamVideoDemoInstance() {
  delete context_;
}

void MediaStreamVideoDemoInstance::DidChangeView(
    const pp::Rect& position, const pp::Rect& clip_ignored) {
  if (position.width() == 0 || position.height() == 0)
    return;
  if (position.size() == position_size_)
    return;

  position_size_ = position.size();

  // Initialize graphics.
  InitGL();
  Render();
}

void MediaStreamVideoDemoInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_dictionary()) {
    LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Invalid message!"));
    return;
  }

  pp::VarDictionary var_dictionary_message(var_message);
  std::string command = var_dictionary_message.Get("command").AsString();

  if (command == "init") {
    pp::Var var_track = var_dictionary_message.Get("track");
    if (!var_track.is_resource())
      return;
    pp::Resource resource_track = var_track.AsResource();
    video_track_ = pp::MediaStreamVideoTrack(resource_track);
    ConfigureTrack();
  } else if (command == "format") {
    std::string str_format = var_dictionary_message.Get("format").AsString();
    if (str_format == "YV12") {
      attrib_format_ = PP_VIDEOFRAME_FORMAT_YV12;
    } else if (str_format == "I420") {
      attrib_format_ = PP_VIDEOFRAME_FORMAT_I420;
    } else if (str_format == "BGRA") {
      attrib_format_ = PP_VIDEOFRAME_FORMAT_BGRA;
    } else {
      attrib_format_ = PP_VIDEOFRAME_FORMAT_UNKNOWN;
    }
    need_config_ = true;
  } else if (command == "size") {
    attrib_width_ = var_dictionary_message.Get("width").AsInt();
    attrib_height_ = var_dictionary_message.Get("height").AsInt();
    need_config_ = true;
  } else {
    LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Invalid command!"));
  }
}

void MediaStreamVideoDemoInstance::InitGL() {
  PP_DCHECK(position_size_.width() && position_size_.height());
  is_painting_ = false;

  delete context_;
  int32_t attributes[] = {
    PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 0,
    PP_GRAPHICS3DATTRIB_BLUE_SIZE, 8,
    PP_GRAPHICS3DATTRIB_GREEN_SIZE, 8,
    PP_GRAPHICS3DATTRIB_RED_SIZE, 8,
    PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 0,
    PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 0,
    PP_GRAPHICS3DATTRIB_SAMPLES, 0,
    PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
    PP_GRAPHICS3DATTRIB_WIDTH, position_size_.width(),
    PP_GRAPHICS3DATTRIB_HEIGHT, position_size_.height(),
    PP_GRAPHICS3DATTRIB_NONE,
  };
  context_ = new pp::Graphics3D(this, attributes);
  PP_DCHECK(!context_->is_null());

  glSetCurrentContextPPAPI(context_->pp_resource());

  // Set viewport window size and clear color bit.
  glClearColor(1, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(0, 0, position_size_.width(), position_size_.height());

  BindGraphics(*context_);
  AssertNoGLError();

  CreateGLObjects();
}

void MediaStreamVideoDemoInstance::DrawYUV() {
  static const float kColorMatrix[9] = {
    1.1643828125f, 1.1643828125f, 1.1643828125f,
    0.0f, -0.39176171875f, 2.017234375f,
    1.59602734375f, -0.81296875f, 0.0f
  };

  glUseProgram(program_yuv_);
  glUniform1i(glGetUniformLocation(program_yuv_, "y_texture"), 0);
  glUniform1i(glGetUniformLocation(program_yuv_, "u_texture"), 1);
  glUniform1i(glGetUniformLocation(program_yuv_, "v_texture"), 2);
  glUniformMatrix3fv(glGetUniformLocation(program_yuv_, "color_matrix"),
      1, GL_FALSE, kColorMatrix);
  AssertNoGLError();

  GLint pos_location = glGetAttribLocation(program_yuv_, "a_position");
  GLint tc_location = glGetAttribLocation(program_yuv_, "a_texCoord");
  AssertNoGLError();
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(
      tc_location, 2, GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<void*>(16 *
                              sizeof(GLfloat)));  // Skip position coordinates.
  AssertNoGLError();

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  AssertNoGLError();
}

void MediaStreamVideoDemoInstance::DrawRGB() {
  glUseProgram(program_rgb_);
  glUniform1i(glGetUniformLocation(program_rgb_, "rgb_texture"), 3);
  AssertNoGLError();

  GLint pos_location = glGetAttribLocation(program_rgb_, "a_position");
  GLint tc_location = glGetAttribLocation(program_rgb_, "a_texCoord");
  AssertNoGLError();
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(
      tc_location, 2, GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<void*>(16 *
                              sizeof(GLfloat)));  // Skip position coordinates.
  AssertNoGLError();

  glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
}

void MediaStreamVideoDemoInstance::Render() {
  PP_DCHECK(!is_painting_);
  is_painting_ = true;
  needs_paint_ = false;

  if (texture_y_) {
    DrawRGB();
    DrawYUV();
  } else {
    glClear(GL_COLOR_BUFFER_BIT);
  }
  pp::CompletionCallback cb = callback_factory_.NewCallback(
      &MediaStreamVideoDemoInstance::PaintFinished);
  context_->SwapBuffers(cb);
}

void MediaStreamVideoDemoInstance::PaintFinished(int32_t result) {
  is_painting_ = false;
  if (needs_paint_)
    Render();
}

GLuint MediaStreamVideoDemoInstance::CreateTexture(
    int32_t width, int32_t height, int unit, bool rgba) {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  AssertNoGLError();

  // Assign parameters.
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Allocate texture.
  glTexImage2D(GL_TEXTURE_2D, 0,
               rgba ? GL_BGRA_EXT : GL_LUMINANCE,
               width, height, 0,
               rgba ? GL_BGRA_EXT : GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
  AssertNoGLError();
  return texture_id;
}

void MediaStreamVideoDemoInstance::CreateGLObjects() {
  // Code and constants for shader.
  static const char kVertexShader[] =
      "varying vec2 v_texCoord;            \n"
      "attribute vec4 a_position;          \n"
      "attribute vec2 a_texCoord;          \n"
      "void main()                         \n"
      "{                                   \n"
      "    v_texCoord = a_texCoord;        \n"
      "    gl_Position = a_position;       \n"
      "}";

  static const char kFragmentShaderYUV[] =
      "precision mediump float;                                   \n"
      "varying vec2 v_texCoord;                                   \n"
      "uniform sampler2D y_texture;                               \n"
      "uniform sampler2D u_texture;                               \n"
      "uniform sampler2D v_texture;                               \n"
      "uniform mat3 color_matrix;                                 \n"
      "void main()                                                \n"
      "{                                                          \n"
      "  vec3 yuv;                                                \n"
      "  yuv.x = texture2D(y_texture, v_texCoord).r;              \n"
      "  yuv.y = texture2D(u_texture, v_texCoord).r;              \n"
      "  yuv.z = texture2D(v_texture, v_texCoord).r;              \n"
      "  vec3 rgb = color_matrix * (yuv - vec3(0.0625, 0.5, 0.5));\n"
      "  gl_FragColor = vec4(rgb, 1.0);                           \n"
      "}";

  static const char kFragmentShaderRGB[] =
      "precision mediump float;                                   \n"
      "varying vec2 v_texCoord;                                   \n"
      "uniform sampler2D rgb_texture;                             \n"
      "void main()                                                \n"
      "{                                                          \n"
      "  gl_FragColor = texture2D(rgb_texture, v_texCoord);       \n"
      "}";

  // Create shader programs.
  program_yuv_ = glCreateProgram();
  CreateShader(program_yuv_, GL_VERTEX_SHADER, kVertexShader);
  CreateShader(program_yuv_, GL_FRAGMENT_SHADER, kFragmentShaderYUV);
  glLinkProgram(program_yuv_);
  AssertNoGLError();

  program_rgb_ = glCreateProgram();
  CreateShader(program_rgb_, GL_VERTEX_SHADER, kVertexShader);
  CreateShader(program_rgb_, GL_FRAGMENT_SHADER, kFragmentShaderRGB);
  glLinkProgram(program_rgb_);
  AssertNoGLError();

  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  static const float kVertices[] = {
    -1, 1, -1, -1, 0, 1, 0, -1,  // Position coordinates.
    0, 1, 0, -1, 1, 1, 1, -1,  // Position coordinates.
    0, 0, 0, 1, 1, 0, 1, 1,  // Texture coordinates.
    0, 0, 0, 1, 1, 0, 1, 1,  // Texture coordinates.
  };

  glGenBuffers(1, &buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);
  AssertNoGLError();
}

void MediaStreamVideoDemoInstance::CreateShader(
    GLuint program, GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  GLint length = static_cast<GLint>(strlen(source) + 1);
  glShaderSource(shader, 1, &source, &length);
  glCompileShader(shader);
  glAttachShader(program, shader);
  glDeleteShader(shader);
}

void MediaStreamVideoDemoInstance::CreateTextures() {
  int32_t width = frame_size_.width();
  int32_t height = frame_size_.height();
  if (width == 0 || height == 0)
    return;
  if (texture_y_)
    glDeleteTextures(1, &texture_y_);
  if (texture_u_)
    glDeleteTextures(1, &texture_u_);
  if (texture_v_)
    glDeleteTextures(1, &texture_v_);
  if (texture_rgb_)
    glDeleteTextures(1, &texture_rgb_);
  texture_y_ = CreateTexture(width, height, 0, false);

  texture_u_ = CreateTexture(width / 2, height / 2, 1, false);
  texture_v_ = CreateTexture(width / 2, height / 2, 2, false);
  texture_rgb_ = CreateTexture(width, height, 3, true);
}

void MediaStreamVideoDemoInstance::ConfigureTrack() {
  const int32_t attrib_list[] = {
      PP_MEDIASTREAMVIDEOTRACK_ATTRIB_FORMAT, attrib_format_,
      PP_MEDIASTREAMVIDEOTRACK_ATTRIB_WIDTH, attrib_width_,
      PP_MEDIASTREAMVIDEOTRACK_ATTRIB_HEIGHT, attrib_height_,
      PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE
    };
  video_track_.Configure(attrib_list, callback_factory_.NewCallback(
        &MediaStreamVideoDemoInstance::OnConfigure));
}

void MediaStreamVideoDemoInstance::OnConfigure(int32_t result) {
  video_track_.GetFrame(callback_factory_.NewCallbackWithOutput(
      &MediaStreamVideoDemoInstance::OnGetFrame));
}

void MediaStreamVideoDemoInstance::OnGetFrame(
    int32_t result, pp::VideoFrame frame) {
  if (result != PP_OK)
    return;
  const char* data = static_cast<const char*>(frame.GetDataBuffer());
  pp::Size size;
  frame.GetSize(&size);

  if (size != frame_size_) {
    frame_size_ = size;
    CreateTextures();
  }

  is_bgra_ = (frame.GetFormat() == PP_VIDEOFRAME_FORMAT_BGRA);

  int32_t width = frame_size_.width();
  int32_t height = frame_size_.height();
  if (!is_bgra_) {
    glActiveTexture(GL_TEXTURE0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    data += width * height;
    width /= 2;
    height /= 2;

    glActiveTexture(GL_TEXTURE1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    data += width * height;
    glActiveTexture(GL_TEXTURE2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
  } else {
    glActiveTexture(GL_TEXTURE3);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
  }

  if (is_painting_)
    needs_paint_ = true;
  else
    Render();

  video_track_.RecycleFrame(frame);
  if (need_config_) {
    ConfigureTrack();
    need_config_ = false;
  } else {
    video_track_.GetFrame(callback_factory_.NewCallbackWithOutput(
        &MediaStreamVideoDemoInstance::OnGetFrame));
  }
}

pp::Instance* MediaStreamVideoModule::CreateInstance(PP_Instance instance) {
  return new MediaStreamVideoDemoInstance(instance, this);
}

}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MediaStreamVideoModule();
}
}  // namespace pp
