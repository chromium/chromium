// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>

#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/dev/device_ref_dev.h"
#include "ppapi/cpp/dev/video_capture_client_dev.h"
#include "ppapi/cpp/dev/video_capture_dev.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/lib/gl/include/GLES2/gl2.h"
#include "ppapi/utility/completion_callback_factory.h"

// When compiling natively on Windows, PostMessage can be #define-d to
// something else.
#ifdef PostMessage
#undef PostMessage
#endif

// Assert |context_| isn't holding any GL Errors.  Done as a macro instead of a
// function to preserve line number information in the failure message.
#define AssertNoGLError() \
  PP_DCHECK(!gles2_if_->GetError(context_->pp_resource()));

namespace {

const char* const kDelimiter = "#__#";

// This object is the global object representing this plugin library as long
// as it is loaded.
class VCDemoModule : public pp::Module {
 public:
  VCDemoModule() : pp::Module() {}
  virtual ~VCDemoModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance);
};

class VCDemoInstance : public pp::Instance,
                       public pp::Graphics3DClient,
                       public pp::VideoCaptureClient_Dev {
 public:
  VCDemoInstance(PP_Instance instance, pp::Module* module);
  virtual ~VCDemoInstance();

  // pp::Instance implementation (see PPP_Instance).
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip_ignored);
  virtual void HandleMessage(const pp::Var& message_data);

  // pp::Graphics3DClient implementation.
  virtual void Graphics3DContextLost() {
    InitGL();
    CreateYUVTextures();
    Render();
  }

  virtual void OnDeviceInfo(PP_Resource resource,
                            const PP_VideoCaptureDeviceInfo_Dev& info,
                            const std::vector<pp::Buffer_Dev>& buffers) {
    capture_info_ = info;
    buffers_ = buffers;
    CreateYUVTextures();
  }

  virtual void OnStatus(PP_Resource resource, uint32_t status) {
  }

  virtual void OnError(PP_Resource resource, uint32_t error) {
  }

  virtual void OnBufferReady(PP_Resource resource, uint32_t buffer) {
    const char* data = static_cast<const char*>(buffers_[buffer].data());
    int32_t width = capture_info_.width;
    int32_t height = capture_info_.height;
    gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0);
    gles2_if_->TexSubImage2D(
        context_->pp_resource(), GL_TEXTURE_2D, 0, 0, 0, width, height,
        GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    data += width * height;
    width /= 2;
    height /= 2;

    gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE1);
    gles2_if_->TexSubImage2D(
        context_->pp_resource(), GL_TEXTURE_2D, 0, 0, 0, width, height,
        GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    data += width * height;
    gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE2);
    gles2_if_->TexSubImage2D(
        context_->pp_resource(), GL_TEXTURE_2D, 0, 0, 0, width, height,
        GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    video_capture_.ReuseBuffer(buffer);
    if (is_painting_)
      needs_paint_ = true;
    else
      Render();
  }

 private:
  void Render();

  // GL-related functions.
  void InitGL();
  GLuint CreateTexture(int32_t width, int32_t height, int unit);
  void CreateGLObjects();
  void CreateShader(GLuint program, GLenum type, const char* source, int size);
  void PaintFinished(int32_t result);
  void CreateYUVTextures();

  void Open(const pp::DeviceRef_Dev& device);
  void Stop();
  void Start();
  void EnumerateDevicesFinished(int32_t result,
                                std::vector<pp::DeviceRef_Dev>& devices);
  void OpenFinished(int32_t result);

  static void MonitorDeviceChangeCallback(void* user_data,
                                          uint32_t device_count,
                                          const PP_Resource devices[]);

  pp::Size position_size_;
  bool is_painting_;
  bool needs_paint_;
  GLuint texture_y_;
  GLuint texture_u_;
  GLuint texture_v_;
  pp::VideoCapture_Dev video_capture_;
  PP_VideoCaptureDeviceInfo_Dev capture_info_;
  std::vector<pp::Buffer_Dev> buffers_;
  pp::CompletionCallbackFactory<VCDemoInstance> callback_factory_;

  // Unowned pointers.
  const struct PPB_OpenGLES2* gles2_if_;

  // Owned data.
  pp::Graphics3D* context_;

  std::vector<pp::DeviceRef_Dev> enumerate_devices_;
  std::vector<pp::DeviceRef_Dev> monitor_devices_;
};

VCDemoInstance::VCDemoInstance(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance),
      pp::Graphics3DClient(this),
      pp::VideoCaptureClient_Dev(this),
      is_painting_(false),
      needs_paint_(false),
      texture_y_(0),
      texture_u_(0),
      texture_v_(0),
      video_capture_(this),
      callback_factory_(this),
      context_(NULL) {
  gles2_if_ = static_cast<const struct PPB_OpenGLES2*>(
      module->GetBrowserInterface(PPB_OPENGLES2_INTERFACE));
  PP_DCHECK(gles2_if_);

  capture_info_.width = 320;
  capture_info_.height = 240;
  capture_info_.frames_per_second = 30;
}

VCDemoInstance::~VCDemoInstance() {
  video_capture_.MonitorDeviceChange(NULL, NULL);
  delete context_;
}

void VCDemoInstance::DidChangeView(
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

void VCDemoInstance::HandleMessage(const pp::Var& message_data) {
  if (message_data.is_string()) {
    std::string event = message_data.AsString();
    if (event == "PageInitialized") {
      int32_t result = video_capture_.MonitorDeviceChange(
          &VCDemoInstance::MonitorDeviceChangeCallback, this);
      if (result != PP_OK)
        PostMessage(pp::Var("MonitorDeviceChangeFailed"));

      pp::CompletionCallbackWithOutput<std::vector<pp::DeviceRef_Dev> >
          callback = callback_factory_.NewCallbackWithOutput(
              &VCDemoInstance::EnumerateDevicesFinished);
      result = video_capture_.EnumerateDevices(callback);
      if (result != PP_OK_COMPLETIONPENDING)
        PostMessage(pp::Var("EnumerationFailed"));
    } else if (event == "UseDefault") {
      Open(pp::DeviceRef_Dev());
    } else if (event == "Stop") {
      Stop();
    } else if (event == "Start") {
      Start();
    } else if (event.find("Monitor:") == 0) {
      std::string index_str = event.substr(strlen("Monitor:"));
      int index = atoi(index_str.c_str());
      if (index >= 0 && index < static_cast<int>(monitor_devices_.size()))
        Open(monitor_devices_[index]);
      else
        PP_NOTREACHED();
    } else if (event.find("Enumerate:") == 0) {
      std::string index_str = event.substr(strlen("Enumerate:"));
      int index = atoi(index_str.c_str());
      if (index >= 0 && index < static_cast<int>(enumerate_devices_.size()))
        Open(enumerate_devices_[index]);
      else
        PP_NOTREACHED();
    }
  }
}

void VCDemoInstance::InitGL() {
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

  // Set viewport window size and clear color bit.
  gles2_if_->ClearColor(context_->pp_resource(), 1, 0, 0, 1);
  gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);
  gles2_if_->Viewport(context_->pp_resource(), 0, 0,
                      position_size_.width(), position_size_.height());

  BindGraphics(*context_);
  AssertNoGLError();

  CreateGLObjects();
}

void VCDemoInstance::Render() {
  PP_DCHECK(!is_painting_);
  is_painting_ = true;
  needs_paint_ = false;
  if (texture_y_) {
    gles2_if_->DrawArrays(context_->pp_resource(), GL_TRIANGLE_STRIP, 0, 4);
  } else {
    gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);
  }
  pp::CompletionCallback cb = callback_factory_.NewCallback(
      &VCDemoInstance::PaintFinished);
  context_->SwapBuffers(cb);
}

void VCDemoInstance::PaintFinished(int32_t result) {
  is_painting_ = false;
  if (needs_paint_)
    Render();
}

GLuint VCDemoInstance::CreateTexture(int32_t width, int32_t height, int unit) {
  GLuint texture_id;
  gles2_if_->GenTextures(context_->pp_resource(), 1, &texture_id);
  AssertNoGLError();
  // Assign parameters.
  gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0 + unit);
  gles2_if_->BindTexture(context_->pp_resource(), GL_TEXTURE_2D, texture_id);
  gles2_if_->TexParameteri(
      context_->pp_resource(), GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
      GL_NEAREST);
  gles2_if_->TexParameteri(
      context_->pp_resource(), GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
      GL_NEAREST);
  gles2_if_->TexParameterf(
      context_->pp_resource(), GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
      GL_CLAMP_TO_EDGE);
  gles2_if_->TexParameterf(
      context_->pp_resource(), GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
      GL_CLAMP_TO_EDGE);

  // Allocate texture.
  gles2_if_->TexImage2D(
      context_->pp_resource(), GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
      GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
  AssertNoGLError();
  return texture_id;
}

void VCDemoInstance::CreateGLObjects() {
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

  static const char kFragmentShader[] =
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

  static const float kColorMatrix[9] = {
    1.1643828125f, 1.1643828125f, 1.1643828125f,
    0.0f, -0.39176171875f, 2.017234375f,
    1.59602734375f, -0.81296875f, 0.0f
  };

  PP_Resource context = context_->pp_resource();

  // Create shader program.
  GLuint program = gles2_if_->CreateProgram(context);
  CreateShader(program, GL_VERTEX_SHADER, kVertexShader, sizeof(kVertexShader));
  CreateShader(
      program, GL_FRAGMENT_SHADER, kFragmentShader, sizeof(kFragmentShader));
  gles2_if_->LinkProgram(context, program);
  gles2_if_->UseProgram(context, program);
  gles2_if_->DeleteProgram(context, program);
  gles2_if_->Uniform1i(
      context, gles2_if_->GetUniformLocation(context, program, "y_texture"), 0);
  gles2_if_->Uniform1i(
      context, gles2_if_->GetUniformLocation(context, program, "u_texture"), 1);
  gles2_if_->Uniform1i(
      context, gles2_if_->GetUniformLocation(context, program, "v_texture"), 2);
  gles2_if_->UniformMatrix3fv(
      context,
      gles2_if_->GetUniformLocation(context, program, "color_matrix"),
      1, GL_FALSE, kColorMatrix);
  AssertNoGLError();

  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  static const float kVertices[] = {
    -1, 1, -1, -1, 1, 1, 1, -1,  // Position coordinates.
    0, 0, 0, 1, 1, 0, 1, 1,  // Texture coordinates.
  };

  GLuint buffer;
  gles2_if_->GenBuffers(context, 1, &buffer);
  gles2_if_->BindBuffer(context, GL_ARRAY_BUFFER, buffer);
  gles2_if_->BufferData(context, GL_ARRAY_BUFFER,
                        sizeof(kVertices), kVertices, GL_STATIC_DRAW);
  AssertNoGLError();
  GLint pos_location = gles2_if_->GetAttribLocation(
      context, program, "a_position");
  GLint tc_location = gles2_if_->GetAttribLocation(
      context, program, "a_texCoord");
  AssertNoGLError();
  gles2_if_->EnableVertexAttribArray(context, pos_location);
  gles2_if_->VertexAttribPointer(context, pos_location, 2,
                                 GL_FLOAT, GL_FALSE, 0, 0);
  gles2_if_->EnableVertexAttribArray(context, tc_location);
  gles2_if_->VertexAttribPointer(
      context, tc_location, 2, GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<void*>(8 *
                              sizeof(GLfloat)));  // Skip position coordinates.
  AssertNoGLError();
}

void VCDemoInstance::CreateShader(
    GLuint program, GLenum type, const char* source, int size) {
  PP_Resource context = context_->pp_resource();
  GLuint shader = gles2_if_->CreateShader(context, type);
  gles2_if_->ShaderSource(context, shader, 1, &source, &size);
  gles2_if_->CompileShader(context, shader);
  gles2_if_->AttachShader(context, program, shader);
  gles2_if_->DeleteShader(context, shader);
}

void VCDemoInstance::CreateYUVTextures() {
  int32_t width = capture_info_.width;
  int32_t height = capture_info_.height;
  texture_y_ = CreateTexture(width, height, 0);

  width /= 2;
  height /= 2;
  texture_u_ = CreateTexture(width, height, 1);
  texture_v_ = CreateTexture(width, height, 2);
}

void VCDemoInstance::Open(const pp::DeviceRef_Dev& device) {
  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &VCDemoInstance::OpenFinished);
  int32_t result = video_capture_.Open(device, capture_info_, 4, callback);
  if (result != PP_OK_COMPLETIONPENDING)
    PostMessage(pp::Var("OpenFailed"));
}

void VCDemoInstance::Stop() {
  if (video_capture_.StopCapture() != PP_OK)
    PostMessage(pp::Var("StopFailed"));
}

void VCDemoInstance::Start() {
  if (video_capture_.StartCapture() != PP_OK)
    PostMessage(pp::Var("StartFailed"));
}

void VCDemoInstance::EnumerateDevicesFinished(
    int32_t result,
  std::vector<pp::DeviceRef_Dev>& devices) {
  if (result == PP_OK) {
    enumerate_devices_.swap(devices);
    std::string device_names = "Enumerate:";
    for (size_t index = 0; index < enumerate_devices_.size(); ++index) {
      pp::Var name = enumerate_devices_[index].GetName();
      PP_DCHECK(name.is_string());

      if (index != 0)
        device_names += kDelimiter;
      device_names += name.AsString();
    }
    PostMessage(pp::Var(device_names));
  } else {
    PostMessage(pp::Var("EnumerationFailed"));
  }
}

void VCDemoInstance::OpenFinished(int32_t result) {
  if (result == PP_OK)
    Start();
  else
    PostMessage(pp::Var("OpenFailed"));
}

// static
void VCDemoInstance::MonitorDeviceChangeCallback(void* user_data,
                                                 uint32_t device_count,
                                                 const PP_Resource devices[]) {
  VCDemoInstance* thiz = static_cast<VCDemoInstance*>(user_data);

  std::string device_names = "Monitor:";
  thiz->monitor_devices_.clear();
  thiz->monitor_devices_.reserve(device_count);
  for (size_t index = 0; index < device_count; ++index) {
    thiz->monitor_devices_.push_back(pp::DeviceRef_Dev(devices[index]));
    pp::Var name = thiz->monitor_devices_.back().GetName();
    PP_DCHECK(name.is_string());

    if (index != 0)
      device_names += kDelimiter;
    device_names += name.AsString();
  }
  thiz->PostMessage(pp::Var(device_names));
}

pp::Instance* VCDemoModule::CreateInstance(PP_Instance instance) {
  return new VCDemoInstance(instance, this);
}

}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new VCDemoModule();
}
}  // namespace pp
