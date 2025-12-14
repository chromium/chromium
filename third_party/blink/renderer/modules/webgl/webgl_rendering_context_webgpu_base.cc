// Copyright 2025 The Chromium Authors Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_webgpu_base.h"

#include <dawn/dawn_proc.h>
#include <dawn/wire/WireClient.h>

#include "base/notimplemented.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/webgl/webgl_active_info.h"
#include "third_party/blink/renderer/modules/webgl/webgl_buffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_object.h"
#include "third_party/blink/renderer/modules/webgl/webgl_program.h"
#include "third_party/blink/renderer/modules/webgl/webgl_shader.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/modules/webgl/webgl_uniform_location.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/extension_set.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/angle_platform_impl.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"

#if BUILDFLAG(USE_STATIC_ANGLE)
extern "C" {
// The ANGLE internal eglGetProcAddress
EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY
EGL_GetProcAddress(const char* procname);
}
namespace {
GLGetProcAddressProc GetStaticANGLEGetProcAddressFunction() {
  return EGL_GetProcAddress;
}
}  // namespace
#else
namespace {
GLGetProcAddressProc GetStaticANGLEGetProcAddressFunction() {
  LOG(ERROR) << "WebGLOnWebGPU requires use_static_angle.";
  return nullptr;
}
}  // namespace
#endif  // BUILDFLAG(USE_STATIC_ANGLE)

namespace blink {
namespace {

const DawnProcTable* GetDawnProcs() {
#if BUILDFLAG(USE_DAWN)
  return &dawn::wire::client::GetProcs();
#else
  LOG(ERROR) << "WebGLOnWebGPU requires use_dawn";
  return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

void GL_APIENTRY
WebGLRenderingContextWebGPUBaseDebugMessageCallback(GLenum source,
                                                    GLenum type,
                                                    GLuint id,
                                                    GLenum severity,
                                                    GLsizei length,
                                                    const GLchar* message,
                                                    const GLvoid* user_data) {
  DCHECK(user_data != nullptr);
  WebGLRenderingContextWebGPUBase* rendering_context =
      static_cast<WebGLRenderingContextWebGPUBase*>(
          const_cast<void*>(user_data));
  rendering_context->OnDebugMessage(source, type, id, severity, length,
                                    message);
}

void InitializeGLDebugLogging(const gl::DriverGL& gl,
                              bool log_non_errors,
                              GLDEBUGPROC callback,
                              const void* user_param) {
  gl.fn.glEnableFn(GL_DEBUG_OUTPUT);
  gl.fn.glEnableFn(GL_DEBUG_OUTPUT_SYNCHRONOUS);

  gl.fn.glDebugMessageControlFn(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR,
                                GL_DONT_CARE, 0, nullptr, GL_TRUE);

  if (log_non_errors) {
    // Enable logging of medium and high severity messages
    gl.fn.glDebugMessageControlFn(GL_DONT_CARE, GL_DONT_CARE,
                                  GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE);
    gl.fn.glDebugMessageControlFn(GL_DONT_CARE, GL_DONT_CARE,
                                  GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr,
                                  GL_TRUE);
    gl.fn.glDebugMessageControlFn(GL_DONT_CARE, GL_DONT_CARE,
                                  GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_FALSE);
    gl.fn.glDebugMessageControlFn(GL_DONT_CARE, GL_DONT_CARE,
                                  GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                                  GL_FALSE);
  }

  gl.fn.glDebugMessageCallbackFn(callback, user_param);
}

void InitializeEGLDebugLogging(const gl::DriverEGL& egl,
                               EGLDEBUGPROCKHR callback) {
  if (!egl.client_ext.b_EGL_KHR_debug) {
    return;
  }

  constexpr EGLAttrib controls[] = {
      EGL_DEBUG_MSG_CRITICAL_KHR,
      EGL_TRUE,
      EGL_DEBUG_MSG_ERROR_KHR,
      EGL_TRUE,
      EGL_DEBUG_MSG_WARN_KHR,
      EGL_TRUE,
      EGL_DEBUG_MSG_INFO_KHR,
      EGL_TRUE,
      EGL_NONE,
      EGL_NONE,
  };

  egl.fn.eglDebugMessageControlKHRFn(callback, controls);
}

class ScopedBindTexture {
 public:
  ScopedBindTexture(const gl::DriverGL& gl, GLenum target, GLuint texture)
      : gl_(gl), target_(target) {
    GLenum target_getter = 0;
    switch (target_) {
      case GL_TEXTURE_2D:
        target_getter = GL_TEXTURE_BINDING_2D;
        break;
      default:
        NOTIMPLEMENTED() << " Target not supported.";
    }
    gl_->fn.glGetIntegervFn(target_getter, &prev_texture_);
    gl_->fn.glBindTextureFn(target_, texture);
  }

  ~ScopedBindTexture() { gl_->fn.glBindTextureFn(target_, prev_texture_); }

  ScopedBindTexture(const ScopedBindTexture&) = delete;
  ScopedBindTexture& operator=(const ScopedBindTexture&) = delete;

 private:
  raw_ref<const gl::DriverGL> gl_;
  GLenum target_;
  GLint prev_texture_ = 0;
};

class ScopedBindFramebuffer {
 public:
  ScopedBindFramebuffer(const gl::DriverGL& gl,
                        bool supports_separate_targets,
                        GLenum target,
                        GLuint fbo)
      : gl_(gl) {
    if (supports_separate_targets) {
      gl_->fn.glGetIntegervFn(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo_);
      gl_->fn.glGetIntegervFn(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo_);
    } else {
      DCHECK(target == GL_FRAMEBUFFER);
      gl_->fn.glGetIntegervFn(GL_FRAMEBUFFER_BINDING, &prev_draw_fbo_);
      prev_read_fbo_ = prev_draw_fbo_;
    }
    gl_->fn.glBindFramebufferEXTFn(target, fbo);
  }

  ~ScopedBindFramebuffer() {
    if (prev_draw_fbo_ == prev_read_fbo_) {
      gl_->fn.glBindFramebufferEXTFn(GL_FRAMEBUFFER, prev_draw_fbo_);
    } else {
      gl_->fn.glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER, prev_draw_fbo_);
      gl_->fn.glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, prev_read_fbo_);
    }
  }

  ScopedBindFramebuffer(const ScopedBindFramebuffer&) = delete;
  ScopedBindFramebuffer& operator=(const ScopedBindFramebuffer&) = delete;

 private:
  raw_ref<const gl::DriverGL> gl_;
  GLint prev_draw_fbo_ = 0;
  GLint prev_read_fbo_ = 0;
};

class ScopedBindRenderbuffer {
 public:
  ScopedBindRenderbuffer(const gl::DriverGL& gl, GLuint renderbuffer)
      : gl_(gl) {
    gl_->fn.glGetIntegervFn(GL_RENDERBUFFER_BINDING, &prev_renderbuffer_);
    gl_->fn.glBindRenderbufferEXTFn(GL_RENDERBUFFER, renderbuffer);
  }

  ~ScopedBindRenderbuffer() {
    gl_->fn.glBindRenderbufferEXTFn(GL_RENDERBUFFER, prev_renderbuffer_);
  }

  ScopedBindRenderbuffer(const ScopedBindRenderbuffer&) = delete;
  ScopedBindRenderbuffer& operator=(const ScopedBindRenderbuffer&) = delete;

 private:
  raw_ref<const gl::DriverGL> gl_;
  GLint prev_renderbuffer_ = 0;
};

const char* GetErrorString(GLenum error) {
  switch (error) {
    case GL_INVALID_ENUM:
      return "INVALID_ENUM";
    case GL_INVALID_VALUE:
      return "INVALID_VALUE";
    case GL_INVALID_OPERATION:
      return "INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "INVALID_FRAMEBUFFER_OPERATION";
    case GC3D_CONTEXT_LOST_WEBGL:
      return "CONTEXT_LOST_WEBGL";
    default:
      return "UNKNOWN";
  }
}

// A partial implementation of GLES2Interface with only the calls used by WebGL
// objects implemented. The call directly proxy to the gl::DriverGL. It isn't
// possible to autogenerate an interface that 100% proxies the call to a
// gl::DriverGL because 1) GLES2Interface has special function that don't exist
// in real GL, and 2) a number of gl::DriverGL procs use a different extension
// suffix, seemingly at random.
class PartialGLES2ForObjects : public gpu::gles2::GLES2InterfaceStub {
 public:
  explicit PartialGLES2ForObjects(gl::DriverGL* gl) : gl_(gl) {}

  void BeginQueryEXT(GLenum target, GLuint id) override {
    gl_->fn.glBeginQueryFn(target, id);
  }
  GLuint CreateProgram() override { return gl_->fn.glCreateProgramFn(); }
  GLuint CreateShader(GLenum type) override {
    return gl_->fn.glCreateShaderFn(type);
  }
  void DeleteBuffers(GLsizei n, const GLuint* buffers) override {
    gl_->fn.glDeleteBuffersARBFn(n, buffers);
  }
  void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) override {
    gl_->fn.glDeleteFramebuffersEXTFn(n, framebuffers);
  }
  void DeleteProgram(GLuint program) override {
    gl_->fn.glDeleteProgramFn(program);
  }
  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override {
    gl_->fn.glDeleteQueriesFn(n, queries);
  }
  void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) override {
    gl_->fn.glDeleteRenderbuffersEXTFn(n, renderbuffers);
  }
  void DeleteSamplers(GLsizei n, const GLuint* samplers) override {
    gl_->fn.glDeleteSamplersFn(n, samplers);
  }
  void DeleteSync(GLsync sync) override { gl_->fn.glDeleteSyncFn(sync); }
  void DeleteShader(GLuint shader) override {
    gl_->fn.glDeleteShaderFn(shader);
  }
  void DeleteTextures(GLsizei n, const GLuint* textures) override {
    gl_->fn.glDeleteTexturesFn(n, textures);
  }
  void DeleteTransformFeedbacks(GLsizei n, const GLuint* ids) override {
    gl_->fn.glDeleteTransformFeedbacksFn(n, ids);
  }
  void DeleteVertexArraysOES(GLsizei n, const GLuint* arrays) override {
    gl_->fn.glDeleteVertexArraysOESFn(n, arrays);
  }
  void DrawBuffersEXT(GLsizei count, const GLenum* bufs) override {
    gl_->fn.glDrawBuffersARBFn(count, bufs);
  }
  void EndQueryEXT(GLenum target) override { gl_->fn.glEndQueryFn(target); }
  void FramebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               GLuint renderbuffer) override {
    gl_->fn.glFramebufferRenderbufferEXTFn(target, attachment,
                                           renderbuffertarget, renderbuffer);
  }
  void FramebufferTexture2D(GLenum target,
                            GLenum attachment,
                            GLenum textarget,
                            GLuint texture,
                            GLint level) override {
    gl_->fn.glFramebufferTexture2DEXTFn(target, attachment, textarget, texture,
                                        level);
  }
  void FramebufferTextureLayer(GLenum target,
                               GLenum attachment,
                               GLuint texture,
                               GLint level,
                               GLint layer) override {
    gl_->fn.glFramebufferTextureLayerFn(target, attachment, texture, level,
                                        layer);
  }
  void GenBuffers(GLsizei n, GLuint* buffers) override {
    gl_->fn.glGenBuffersARBFn(n, buffers);
  }
  void GenFramebuffers(GLsizei n, GLuint* framebuffers) override {
    gl_->fn.glGenFramebuffersEXTFn(n, framebuffers);
  }
  void GenQueriesEXT(GLsizei n, GLuint* queries) override {
    gl_->fn.glGenQueriesFn(n, queries);
  }
  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override {
    gl_->fn.glGenRenderbuffersEXTFn(n, renderbuffers);
  }
  void GenSamplers(GLsizei n, GLuint* samplers) override {
    gl_->fn.glGenSamplersFn(n, samplers);
  }
  void GenTextures(GLsizei n, GLuint* textures) override {
    gl_->fn.glGenTexturesFn(n, textures);
  }
  void GenTransformFeedbacks(GLsizei n, GLuint* ids) override {
    gl_->fn.glGenTransformFeedbacksFn(n, ids);
  }
  void GenVertexArraysOES(GLsizei n, GLuint* arrays) override {
    gl_->fn.glGenVertexArraysOESFn(n, arrays);
  }
  void GetIntegerv(GLenum pname, GLint* params) override {
    gl_->fn.glGetIntegervFn(pname, params);
  }
  void GetProgramiv(GLuint program, GLenum pname, GLint* params) override {
    gl_->fn.glGetProgramivFn(program, pname, params);
  }
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override {
    gl_->fn.glGetQueryObjectuivFn(id, pname, params);
  }
  void GetQueryObjectui64vEXT(GLuint id,
                              GLenum pname,
                              GLuint64* params) override {
    gl_->fn.glGetQueryObjectui64vFn(id, pname, params);
  }

 private:
  raw_ptr<gl::DriverGL> gl_;
};

}  // anonymous namespace

#define RETURN_IF_GL_ERROR(code, ...)        \
  {                                          \
    CheckAndClearErrorCallbackState();       \
    code;                                    \
    if (CheckAndClearErrorCallbackState()) { \
      return __VA_ARGS__;                    \
    }                                        \
  }                                          \
  while (false)

WebGLRenderingContextWebGPUBase::WebGLRenderingContextWebGPUBase(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& requested_attributes,
    CanvasRenderingAPI api)
    : WebGLContextObjectSupport(
          host->GetTopExecutionContext()->GetTaskRunner(TaskType::kWebGL),
          /* is_webgl2 */ api == CanvasRenderingAPI::kWebgl2),
      CanvasRenderingContext(host, requested_attributes, api) {}

WebGLRenderingContextWebGPUBase::~WebGLRenderingContextWebGPUBase() {
  Destroy();
}

HTMLCanvasElement* WebGLRenderingContextWebGPUBase::canvas() const {
  if (Host()->IsOffscreenCanvas()) {
    return nullptr;
  }
  return static_cast<HTMLCanvasElement*>(Host());
}

ScriptPromise<IDLUndefined> WebGLRenderingContextWebGPUBase::initAsync(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  // Synchronously connect to the GPU process to use WebGPU.
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider =
      Platform::Current()->CreateWebGPUGraphicsContext3DProvider(
          execution_context->Url());

  if (context_provider == nullptr) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kOperationError,
        "Failed to create a WebGPU context provider");
    return promise;
  }

  // The context provider requires being bound on a single thread because it was
  // initially designed only for GL.
  context_provider->BindToCurrentSequence();
  // Send the execution token to the GPU process, it won't return devices until
  // it receives it.
  context_provider->WebGPUInterface()->SetWebGPUExecutionContextToken(
      To<LocalDOMWindow>(execution_context)->document()->Token());

  // Create the wgpu::Instance (as part of the DawnControlClientHolder).
  dawn_control_client_ = DawnControlClientHolder::Create(
      std::move(context_provider),
      execution_context->GetTaskRunner(TaskType::kWebGPU));

  // Request the adapter, making it resolve the result promise when it is done.
  auto* callback =
      MakeWGPUOnceCallback(resolver->WrapCallbackInScriptScope(blink::BindOnce(
          &WebGLRenderingContextWebGPUBase::InitRequestAdapterCallback,
          WrapPersistent(this), WrapPersistent(script_state))));

  dawn_control_client_->GetWGPUInstance().RequestAdapter(
      nullptr, wgpu::CallbackMode::AllowSpontaneous,
      callback->UnboundCallback(), callback->AsUserdata());
  dawn_control_client_->EnsureFlush(ToEventLoop(script_state));

  return promise;
}

void WebGLRenderingContextWebGPUBase::InitRequestAdapterCallback(
    ScriptState* script_state,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    wgpu::RequestAdapterStatus status,
    wgpu::Adapter adapter,
    wgpu::StringView error_message) {
  if (status != wgpu::RequestAdapterStatus::Success) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kOperationError,
        String::FromUTF8WithLatin1Fallback(error_message));
    return;
  }

  adapter_ = std::move(adapter);

  // Request the device.
  auto* callback = MakeWGPUOnceCallback(blink::BindOnce(
      &WebGLRenderingContextWebGPUBase::InitRequestDeviceCallback,
      WrapPersistent(this), WrapPersistent(script_state),
      WrapPersistent(resolver)));

  adapter_.RequestDevice(nullptr, wgpu::CallbackMode::AllowSpontaneous,
                         callback->UnboundCallback(), callback->AsUserdata());
  dawn_control_client_->EnsureFlush(ToEventLoop(script_state));
}

void WebGLRenderingContextWebGPUBase::InitRequestDeviceCallback(
    ScriptState* script_state,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    wgpu::RequestDeviceStatus status,
    wgpu::Device device,
    wgpu::StringView error_message) {
  if (status != wgpu::RequestDeviceStatus::Success) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kOperationError,
        String::FromUTF8WithLatin1Fallback(error_message));
    return;
  }

  device_ = std::move(device);

  InitializeContext();

  // We are required to present to the compositor on context creation.
  EnsureDefaultFramebuffer();

  resolver->Resolve();
}

// ****************************************************************************
// Start of WebGLRenderingContextBase's IDL methods
// ****************************************************************************

V8UnionHTMLCanvasElementOrOffscreenCanvas*
WebGLRenderingContextWebGPUBase::getHTMLOrOffscreenCanvas() const {
  if (canvas()) {
    return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
        static_cast<HTMLCanvasElement*>(Host()));
  }
  return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
      static_cast<OffscreenCanvas*>(Host()));
}

int WebGLRenderingContextWebGPUBase::drawingBufferWidth() const {
  return isContextLost() ? 0 : default_framebuffer_size_.width();
}

int WebGLRenderingContextWebGPUBase::drawingBufferHeight() const {
  return isContextLost() ? 0 : default_framebuffer_size_.height();
}

GLenum WebGLRenderingContextWebGPUBase::drawingBufferFormat() const {
  return GL_RGBA8;
}

V8PredefinedColorSpace WebGLRenderingContextWebGPUBase::drawingBufferColorSpace(
    ScriptState*) const {
  NOTIMPLEMENTED();
  return V8PredefinedColorSpace(V8PredefinedColorSpace::Enum::kSRGB);
}

void WebGLRenderingContextWebGPUBase::setDrawingBufferColorSpace(
    ScriptState*,
    const V8PredefinedColorSpace& color_space,
    ExceptionState&) {
  NOTIMPLEMENTED();
}

V8PredefinedColorSpace WebGLRenderingContextWebGPUBase::unpackColorSpace(
    ScriptState*) const {
  NOTIMPLEMENTED();
  return V8PredefinedColorSpace(V8PredefinedColorSpace::Enum::kSRGB);
}

void WebGLRenderingContextWebGPUBase::setUnpackColorSpace(
    ScriptState*,
    const V8PredefinedColorSpace& color_space,
    ExceptionState&) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::activeTexture(GLenum texture) {
  RETURN_IF_GL_ERROR(driver_gl_.fn.glActiveTextureFn(texture));

  active_texture_unit_ = texture - GL_TEXTURE0;
  DCHECK(active_texture_unit_ < kMaxTextureUnits);
}

void WebGLRenderingContextWebGPUBase::attachShader(WebGLProgram* program,
                                                   WebGLShader* shader) {
  if (!ValidateProgramOrShader("attachShader", program) ||
      !ValidateProgramOrShader("attachShader", shader)) {
    return;
  }

  RETURN_IF_GL_ERROR(
      driver_gl_.fn.glAttachShaderFn(program->Object(), shader->Object()));

  bool attachSuccess = program->AttachShader(shader);
  // While the WRCWebGPU might not need the OnAttach/OnDetach calls,
  // WebGLProgram calls OnDetach so we keep it balanced with an OnAttach here.
  DCHECK(attachSuccess);
  shader->OnAttached();
}

void WebGLRenderingContextWebGPUBase::bindAttribLocation(WebGLProgram*,
                                                         GLuint index,
                                                         const String& name) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::bindBuffer(GLenum target,
                                                 WebGLBuffer* buffer) {
  if (!ValidateNullableObject("bindBuffer", buffer)) {
    return;
  }

  RETURN_IF_GL_ERROR(
      driver_gl_.fn.glBindBufferFn(target, ObjectOrZero(buffer)));

  switch (target) {
    case GL_ARRAY_BUFFER:
      array_buffer_binding_ = buffer;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      element_array_buffer_binding_ = buffer;
      break;
    case GL_COPY_READ_BUFFER:
    case GL_COPY_WRITE_BUFFER:
    case GL_PIXEL_PACK_BUFFER:
    case GL_PIXEL_UNPACK_BUFFER:
    case GL_TRANSFORM_FEEDBACK_BUFFER:
    case GL_UNIFORM_BUFFER:
      // TODO(413078308): Implement WebGL2 buffer bindings.
      NOTIMPLEMENTED();
      break;
  }
}

void WebGLRenderingContextWebGPUBase::bindFramebuffer(
    GLenum target,
    WebGLFramebuffer* framebuffer) {
  if (!ValidateNullableObject("bindFramebuffer", framebuffer)) {
    return;
  }

  GLuint id = ObjectOrZero(framebuffer);
  if (id == 0) {
    EnsureDefaultFramebuffer();
    id = default_framebuffer_;
  }

  RETURN_IF_GL_ERROR(driver_gl_.fn.glBindFramebufferEXTFn(target, id));

  switch (target) {
    case GL_FRAMEBUFFER:
      draw_framebuffer_binding_ = framebuffer;
      read_framebuffer_binding_ = framebuffer;
      break;
    case GL_DRAW_FRAMEBUFFER:
      DCHECK(supports_separate_framebuffer_targets_);
      draw_framebuffer_binding_ = framebuffer;
      break;
    case GL_READ_FRAMEBUFFER:
      DCHECK(supports_separate_framebuffer_targets_);
      read_framebuffer_binding_ = framebuffer;
      break;
    default:
      NOTREACHED();
  }
}

void WebGLRenderingContextWebGPUBase::bindRenderbuffer(GLenum target,
                                                       WebGLRenderbuffer*) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::bindTexture(GLenum target,
                                                  WebGLTexture* texture) {
  if (!ValidateNullableObject("bindTexture", texture)) {
    return;
  }

  RETURN_IF_GL_ERROR(
      driver_gl_.fn.glBindTextureFn(target, ObjectOrZero(texture)));

  texture->SetTarget(target);
  bound_textures_[static_cast<size_t>(GLenumToTextureTarget(target))]
                 [active_texture_unit_] = texture;
}

void WebGLRenderingContextWebGPUBase::blendColor(GLfloat red,
                                                 GLfloat green,
                                                 GLfloat blue,
                                                 GLfloat alpha) {
  driver_gl_.fn.glBlendColorFn(red, green, blue, alpha);
}

void WebGLRenderingContextWebGPUBase::blendEquation(GLenum mode) {
  driver_gl_.fn.glBlendEquationFn(mode);
}

void WebGLRenderingContextWebGPUBase::blendEquationSeparate(GLenum mode_rgb,
                                                            GLenum mode_alpha) {
  driver_gl_.fn.glBlendEquationSeparateFn(mode_rgb, mode_alpha);
}

void WebGLRenderingContextWebGPUBase::blendFunc(GLenum sfactor,
                                                GLenum dfactor) {
  driver_gl_.fn.glBlendFuncFn(sfactor, dfactor);
}

void WebGLRenderingContextWebGPUBase::blendFuncSeparate(GLenum src_rgb,
                                                        GLenum dst_rgb,
                                                        GLenum src_alpha,
                                                        GLenum dst_alpha) {
  driver_gl_.fn.glBlendFuncSeparateFn(src_rgb, dst_rgb, src_alpha, dst_alpha);
}

void WebGLRenderingContextWebGPUBase::bufferData(GLenum target,
                                                 int64_t size,
                                                 GLenum usage) {
  if (!ValidateFitsNonNegInt32("bufferData", "size", size)) {
    return;
  }

  driver_gl_.fn.glBufferDataFn(target, static_cast<GLsizeiptr>(size), nullptr,
                               usage);
}

void WebGLRenderingContextWebGPUBase::bufferData(GLenum target,
                                                 DOMArrayBufferBase* data,
                                                 GLenum usage) {
  if (!data) {
    InsertGLError(GL_INVALID_VALUE, "bufferData", "no data");
    return;
  }
  if (!ValidateFitsNonNegInt32("bufferData", "size", data->ByteLength())) {
    return;
  }

  driver_gl_.fn.glBufferDataFn(target,
                               static_cast<GLsizeiptr>(data->ByteLength()),
                               data->DataMaybeShared(), usage);
}

void WebGLRenderingContextWebGPUBase::bufferData(
    GLenum target,
    MaybeShared<DOMArrayBufferView> data,
    GLenum usage) {
  if (!ValidateFitsNonNegInt32("bufferData", "size", data->byteLength())) {
    return;
  }
  driver_gl_.fn.glBufferDataFn(target,
                               static_cast<GLsizeiptr>(data->byteLength()),
                               data->BaseAddressMaybeShared(), usage);
}

void WebGLRenderingContextWebGPUBase::bufferSubData(
    GLenum target,
    int64_t offset,
    base::span<const uint8_t> data) {
  if (!ValidateFitsNonNegInt32("bufferSubData", "data", data.size()) ||
      !ValidateFitsNonNegInt32("bufferSubData", "offset", offset)) {
    return;
  }
  driver_gl_.fn.glBufferSubDataFn(target, GLsizeiptr(offset),
                                  GLsizeiptr(data.size()), data.data());
}

GLenum WebGLRenderingContextWebGPUBase::checkFramebufferStatus(GLenum target) {
  return driver_gl_.fn.glCheckFramebufferStatusEXTFn(target);
}

void WebGLRenderingContextWebGPUBase::clear(GLbitfield mask) {
  EnsureDefaultFramebuffer();
  driver_gl_.fn.glClearFn(mask);
}

void WebGLRenderingContextWebGPUBase::clearColor(GLfloat red,
                                                 GLfloat green,
                                                 GLfloat blue,
                                                 GLfloat alpha) {
  // TODO(413078308): Handle NaNs.
  driver_gl_.fn.glClearColorFn(red, green, blue, alpha);
}

void WebGLRenderingContextWebGPUBase::clearDepth(GLfloat depth) {
  driver_gl_.fn.glClearDepthFn(depth);
}

void WebGLRenderingContextWebGPUBase::clearStencil(GLint stencil) {
  driver_gl_.fn.glClearStencilFn(stencil);
}

void WebGLRenderingContextWebGPUBase::colorMask(GLboolean red,
                                                GLboolean green,
                                                GLboolean blue,
                                                GLboolean alpha) {
  driver_gl_.fn.glColorMaskFn(red, green, blue, alpha);
}

void WebGLRenderingContextWebGPUBase::compileShader(WebGLShader* shader) {
  if (!ValidateProgramOrShader("compileShader", shader)) {
    return;
  }

  driver_gl_.fn.glCompileShaderFn(shader->Object());
}

void WebGLRenderingContextWebGPUBase::compressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    MaybeShared<DOMArrayBufferView> data) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::compressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    MaybeShared<DOMArrayBufferView> data) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::copyTexImage2D(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLint border) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::copyTexSubImage2D(GLenum target,
                                                        GLint level,
                                                        GLint xoffset,
                                                        GLint yoffset,
                                                        GLint x,
                                                        GLint y,
                                                        GLsizei width,
                                                        GLsizei height) {
  NOTIMPLEMENTED();
}

WebGLBuffer* WebGLRenderingContextWebGPUBase::createBuffer() {
  return MakeGarbageCollected<WebGLBuffer>(this);
}

WebGLFramebuffer* WebGLRenderingContextWebGPUBase::createFramebuffer() {
  return MakeGarbageCollected<WebGLFramebuffer>(this);
}

WebGLProgram* WebGLRenderingContextWebGPUBase::createProgram() {
  return MakeGarbageCollected<WebGLProgram>(this);
}

WebGLRenderbuffer* WebGLRenderingContextWebGPUBase::createRenderbuffer() {
  NOTIMPLEMENTED();
  return nullptr;
}

WebGLShader* WebGLRenderingContextWebGPUBase::createShader(GLenum type) {
  WebGLShader* shader;
  RETURN_IF_GL_ERROR(shader = MakeGarbageCollected<WebGLShader>(this, type),
                     nullptr);
  return shader;
}

WebGLTexture* WebGLRenderingContextWebGPUBase::createTexture() {
  return MakeGarbageCollected<WebGLTexture>(this);
}

void WebGLRenderingContextWebGPUBase::cullFace(GLenum mode) {
  driver_gl_.fn.glCullFaceFn(mode);
}

void WebGLRenderingContextWebGPUBase::deleteBuffer(WebGLBuffer*) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::deleteFramebuffer(
    WebGLFramebuffer* framebuffer) {
  if (!DeleteObject(framebuffer)) {
    return;
  }

  if (framebuffer == draw_framebuffer_binding_) {
    if (supports_separate_framebuffer_targets_ ||
        draw_framebuffer_binding_ != read_framebuffer_binding_) {
      bindFramebuffer(GL_DRAW_FRAMEBUFFER, nullptr);
    } else {
      bindFramebuffer(GL_FRAMEBUFFER, nullptr);
    }
  }
  if (framebuffer == read_framebuffer_binding_) {
    DCHECK(supports_separate_framebuffer_targets_);
    bindFramebuffer(GL_READ_FRAMEBUFFER, nullptr);
  }
}

void WebGLRenderingContextWebGPUBase::deleteProgram(WebGLProgram* program) {
  DeleteObject(program);
}

void WebGLRenderingContextWebGPUBase::deleteRenderbuffer(WebGLRenderbuffer*) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::deleteShader(WebGLShader* shader) {
  DeleteObject(shader);
}

void WebGLRenderingContextWebGPUBase::deleteTexture(WebGLTexture* texture) {
  if (!DeleteObject(texture)) {
    return;
  }

  TextureTarget texture_target = GLenumToTextureTarget(texture->GetTarget());
  if (texture_target != TextureTarget::kUnkown) {
    size_t texture_type_idx = static_cast<size_t>(texture_target);
    CHECK_LT(texture_type_idx, bound_textures_.size());
    auto& bound_textures_for_type = bound_textures_[texture_type_idx];
    for (auto& bound_texture : bound_textures_for_type) {
      if (bound_texture == texture) {
        bound_texture = nullptr;
      }
    }
  }

  if (draw_framebuffer_binding_) {
    draw_framebuffer_binding_->RemoveAttachmentFromBoundFramebuffer(
        GL_FRAMEBUFFER, texture);
  }
  if (read_framebuffer_binding_ &&
      read_framebuffer_binding_ != draw_framebuffer_binding_) {
    read_framebuffer_binding_->RemoveAttachmentFromBoundFramebuffer(
        GL_READ_FRAMEBUFFER, texture);
  }
}

void WebGLRenderingContextWebGPUBase::depthFunc(GLenum func) {
  driver_gl_.fn.glDepthFuncFn(func);
}

void WebGLRenderingContextWebGPUBase::depthMask(GLboolean mask) {
  driver_gl_.fn.glDepthMaskFn(mask);
}

void WebGLRenderingContextWebGPUBase::depthRange(GLfloat z_near,
                                                 GLfloat z_far) {
  driver_gl_.fn.glDepthRangeFn(z_near, z_far);
}

void WebGLRenderingContextWebGPUBase::detachShader(WebGLProgram*,
                                                   WebGLShader*) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::disable(GLenum cap) {
  driver_gl_.fn.glDisableFn(cap);
}

void WebGLRenderingContextWebGPUBase::disableVertexAttribArray(GLuint index) {
  driver_gl_.fn.glDisableVertexAttribArrayFn(index);
}

void WebGLRenderingContextWebGPUBase::drawArrays(GLenum mode,
                                                 GLint first,
                                                 GLsizei count) {
  EnsureDefaultFramebuffer();
  driver_gl_.fn.glDrawArraysFn(mode, first, count);
}

void WebGLRenderingContextWebGPUBase::drawElements(GLenum mode,
                                                   GLsizei count,
                                                   GLenum type,
                                                   int64_t offset) {
  if (!ValidateFitsNonNegInt32("drawElements", "offset", offset)) {
    return;
  }

  EnsureDefaultFramebuffer();
  driver_gl_.fn.glDrawElementsFn(
      mode, count, type,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void WebGLRenderingContextWebGPUBase::enable(GLenum cap) {
  driver_gl_.fn.glEnableFn(cap);
}

void WebGLRenderingContextWebGPUBase::enableVertexAttribArray(GLuint index) {
  driver_gl_.fn.glEnableVertexAttribArrayFn(index);
}

void WebGLRenderingContextWebGPUBase::finish() {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::flush() {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::framebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    WebGLRenderbuffer*) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::framebufferTexture2D(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    WebGLTexture* texture,
    GLint level) {
  if (!ValidateNullableObject("framebufferTexture2D", texture)) {
    return;
  }

  WebGLFramebuffer* framebuffer = GetBoundFramebuffer(target);

  // Disallow modifying the default framebuffer. The WebGL framebuffer object
  // will be null but default_framebuffer_ will actually be bound and we do not
  // want to modify it.
  if (!framebuffer || !framebuffer->Object()) {
    InsertGLError(GL_INVALID_OPERATION, "framebufferTexture2D",
                  "no framebuffer bound");
    return;
  }

  // TODO(413078308): If the internal glFramebufferTexture2D call fails, state
  // tracking should not be updated.
  framebuffer->SetAttachmentForBoundFramebuffer(target, attachment, textarget,
                                                texture, level, 0, 0);
}

void WebGLRenderingContextWebGPUBase::frontFace(GLenum mode) {
  driver_gl_.fn.glFrontFaceFn(mode);
}

void WebGLRenderingContextWebGPUBase::generateMipmap(GLenum target) {
  NOTIMPLEMENTED();
}

WebGLActiveInfo* WebGLRenderingContextWebGPUBase::getActiveAttrib(
    WebGLProgram* program,
    GLuint index) {
  if (!ValidateProgramOrShader("getActiveAttrib", program)) {
    return nullptr;
  }

  GLint max_name_length = -1;
  RETURN_IF_GL_ERROR(
      driver_gl_.fn.glGetProgramivFn(
          program->Object(), GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_name_length),
      nullptr);

  if (max_name_length <= 0) {
    return nullptr;
  }

  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  base::span<LChar> name_buffer;
  scoped_refptr<StringImpl> name_impl =
      StringImpl::CreateUninitialized(max_name_length, name_buffer);
  RETURN_IF_GL_ERROR(
      driver_gl_.fn.glGetActiveAttribFn(
          program->Object(), index, max_name_length, &length, &size, &type,
          reinterpret_cast<GLchar*>(name_buffer.data())),
      nullptr);
  DCHECK_GE(size, 0);
  return MakeGarbageCollected<WebGLActiveInfo>(name_impl->Substring(0, length),
                                               type, size);
}

WebGLActiveInfo* WebGLRenderingContextWebGPUBase::getActiveUniform(
    WebGLProgram* program,
    GLuint index) {
  if (!ValidateProgramOrShader("getActiveUniform", program)) {
    return nullptr;
  }

  GLint max_name_length = -1;
  RETURN_IF_GL_ERROR(
      driver_gl_.fn.glGetProgramivFn(
          program->Object(), GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length),
      nullptr);

  if (max_name_length <= 0) {
    return nullptr;
  }

  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  base::span<LChar> name_buffer;
  scoped_refptr<StringImpl> name_impl =
      StringImpl::CreateUninitialized(max_name_length, name_buffer);
  RETURN_IF_GL_ERROR(
      driver_gl_.fn.glGetActiveUniformFn(
          program->Object(), index, max_name_length, &length, &size, &type,
          reinterpret_cast<GLchar*>(name_buffer.data())),
      nullptr);
  DCHECK_GE(size, 0);
  return MakeGarbageCollected<WebGLActiveInfo>(name_impl->Substring(0, length),
                                               type, size);
}

std::optional<HeapVector<Member<WebGLShader>>>
WebGLRenderingContextWebGPUBase::getAttachedShaders(WebGLProgram*) {
  NOTIMPLEMENTED();
  return {};
}

GLint WebGLRenderingContextWebGPUBase::getAttribLocation(WebGLProgram* program,
                                                         const String& name) {
  if (!ValidateProgramOrShader("getAttribLocation", program)) {
    return -1;
  }

  return driver_gl_.fn.glGetAttribLocationFn(program->Object(),
                                             name.Utf8().c_str());
}

ScriptValue WebGLRenderingContextWebGPUBase::getBufferParameter(ScriptState*,
                                                                GLenum target,
                                                                GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

WebGLContextAttributes* WebGLRenderingContextWebGPUBase::getContextAttributes()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

GLenum WebGLRenderingContextWebGPUBase::getError() {
  FlushErrors();

  GLenum error = GL_NO_ERROR;
  if (!errors_.empty()) {
    error = errors_.front();
    errors_.EraseAt(0);
  }
  return error;
}

ScriptObject WebGLRenderingContextWebGPUBase::getExtension(ScriptState*,
                                                           const String& name) {
  NOTIMPLEMENTED();
  return {};
}

ScriptValue WebGLRenderingContextWebGPUBase::getFramebufferAttachmentParameter(
    ScriptState*,
    GLenum target,
    GLenum attachment,
    GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

ScriptValue WebGLRenderingContextWebGPUBase::getParameter(
    ScriptState* script_state,
    GLenum pname) {
  if (IsLost()) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  GLint int_value = 0;
  switch (pname) {
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
    case GL_MAX_TEXTURE_SIZE: {
      driver_gl_.fn.glGetIntegervFn(pname, &int_value);
      return WebGLAny(script_state, int_value);
    }

    default:
      // TODO(413078308): Support other pnames.
      NOTIMPLEMENTED();
  }

  return {};
}

ScriptValue WebGLRenderingContextWebGPUBase::getProgramParameter(
    ScriptState* script_state,
    WebGLProgram* program,
    GLenum pname) {
  if (!ValidateProgramOrShader("getProgramParameter", program)) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  GLint value = 0;
  switch (pname) {
    case GL_DELETE_STATUS:
      return WebGLAny(script_state, program->MarkedForDeletion());

    case GL_VALIDATE_STATUS:
    case GL_LINK_STATUS:
      driver_gl_.fn.glGetProgramivFn(program->Object(), pname, &value);
      return WebGLAny(script_state, static_cast<bool>(value));

    case GL_COMPLETION_STATUS_KHR:
      // Completion status queries always return true on a lost context. This is
      // intended to prevent applications from entering an infinite polling
      // loop.
      if (IsLost()) {
        return WebGLAny(script_state, true);
      }
      driver_gl_.fn.glGetProgramivFn(program->Object(),
                                     GL_COMPLETION_STATUS_KHR, &value);
      return WebGLAny(script_state, bool(value));

    case GL_ACTIVE_UNIFORM_BLOCKS:
    case GL_TRANSFORM_FEEDBACK_VARYINGS:
    case GL_ATTACHED_SHADERS:
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_UNIFORMS:
    case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
      driver_gl_.fn.glGetProgramivFn(program->Object(), pname, &value);
      return WebGLAny(script_state, value);

    default:
      InsertGLError(GL_INVALID_ENUM, "getProgramParameter",
                    "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

String WebGLRenderingContextWebGPUBase::getProgramInfoLog(
    WebGLProgram* program) {
  if (!ValidateObject("getProgramInfoLog", program)) {
    return String();
  }

  GLint length = 0;
  RETURN_IF_GL_ERROR(driver_gl_.fn.glGetProgramivFn(
                         program->Object(), GL_INFO_LOG_LENGTH, &length),
                     String());

  if (length == 0) {
    return String();
  }

  GLsizei write_length = 0;
  StringBuffer<LChar> log_buffer(length);
  RETURN_IF_GL_ERROR(driver_gl_.fn.glGetProgramInfoLogFn(
                         program->Object(), length, &write_length,
                         reinterpret_cast<GLchar*>(log_buffer.Span().data())),
                     String());

  // The returnedLength excludes the null terminator. If this check wasn't
  // true, then we'd need to tell the returned String the real length.
  DCHECK_EQ(write_length + 1, length);
  return String::Adopt(log_buffer);
}

ScriptValue WebGLRenderingContextWebGPUBase::getRenderbufferParameter(
    ScriptState*,
    GLenum target,
    GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

ScriptValue WebGLRenderingContextWebGPUBase::getShaderParameter(
    ScriptState* script_state,
    WebGLShader* shader,
    GLenum pname) {
  if (!ValidateProgramOrShader("getShaderParameter", shader)) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  GLint value = 0;
  switch (pname) {
    case GL_DELETE_STATUS:
      return WebGLAny(script_state, shader->MarkedForDeletion());

    case GL_COMPILE_STATUS:
      driver_gl_.fn.glGetShaderivFn(shader->Object(), GL_COMPILE_STATUS,
                                    &value);
      return WebGLAny(script_state, bool(value));

    case GL_COMPLETION_STATUS_KHR:
      // Completion status queries always return true on a lost context. This is
      // intended to prevent applications from entering an infinite polling
      // loop.
      if (IsLost()) {
        return WebGLAny(script_state, true);
      }
      driver_gl_.fn.glGetShaderivFn(shader->Object(), GL_COMPLETION_STATUS_KHR,
                                    &value);
      return WebGLAny(script_state, bool(value));

    case GL_SHADER_TYPE:
      driver_gl_.fn.glGetShaderivFn(shader->Object(), GL_SHADER_TYPE, &value);
      return WebGLAny(script_state, static_cast<unsigned>(value));

    default:
      InsertGLError(GL_INVALID_ENUM, "getShaderParameter",
                    "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

String WebGLRenderingContextWebGPUBase::getShaderInfoLog(WebGLShader* shader) {
  if (!ValidateObject("getShaderInfoLog", shader)) {
    return String();
  }

  GLint length = 0;
  RETURN_IF_GL_ERROR(driver_gl_.fn.glGetShaderivFn(shader->Object(),
                                                   GL_INFO_LOG_LENGTH, &length),
                     String());

  if (length == 0) {
    return String();
  }

  GLsizei write_length = 0;
  StringBuffer<LChar> log_buffer(length);
  RETURN_IF_GL_ERROR(driver_gl_.fn.glGetShaderInfoLogFn(
                         shader->Object(), length, &write_length,
                         reinterpret_cast<GLchar*>(log_buffer.Span().data())),
                     String());

  // The returnedLength excludes the null terminator. If this check wasn't
  // true, then we'd need to tell the returned String the real length.
  DCHECK_EQ(write_length + 1, length);
  return String::Adopt(log_buffer);
}

WebGLShaderPrecisionFormat*
WebGLRenderingContextWebGPUBase::getShaderPrecisionFormat(
    GLenum shader_type,
    GLenum precision_type) {
  NOTIMPLEMENTED();
  return nullptr;
}

String WebGLRenderingContextWebGPUBase::getShaderSource(WebGLShader*) {
  NOTIMPLEMENTED();
  return {};
}

std::optional<Vector<String>>
WebGLRenderingContextWebGPUBase::getSupportedExtensions() {
  NOTIMPLEMENTED();
  return {};
}

ScriptValue WebGLRenderingContextWebGPUBase::getTexParameter(ScriptState*,
                                                             GLenum target,
                                                             GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

ScriptValue WebGLRenderingContextWebGPUBase::getUniform(
    ScriptState*,
    WebGLProgram*,
    const WebGLUniformLocation*) {
  NOTIMPLEMENTED();
  return {};
}

WebGLUniformLocation* WebGLRenderingContextWebGPUBase::getUniformLocation(
    WebGLProgram* program,
    const String& name) {
  if (!ValidateProgramOrShader("getUniformLocation", program)) {
    return nullptr;
  }

  GLint location = driver_gl_.fn.glGetUniformLocationFn(program->Object(),
                                                        name.Utf8().c_str());
  if (location == -1) {
    return nullptr;
  }
  return MakeGarbageCollected<WebGLUniformLocation>(program, location);
}

ScriptValue WebGLRenderingContextWebGPUBase::getVertexAttrib(ScriptState*,
                                                             GLuint index,
                                                             GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

int64_t WebGLRenderingContextWebGPUBase::getVertexAttribOffset(GLuint index,
                                                               GLenum pname) {
  NOTIMPLEMENTED();
  return 0;
}

void WebGLRenderingContextWebGPUBase::hint(GLenum target, GLenum mode) {
  NOTIMPLEMENTED();
}

bool WebGLRenderingContextWebGPUBase::isBuffer(WebGLBuffer*) {
  NOTIMPLEMENTED();
  return false;
}

bool WebGLRenderingContextWebGPUBase::isEnabled(GLenum cap) {
  NOTIMPLEMENTED();
  return false;
}

bool WebGLRenderingContextWebGPUBase::isFramebuffer(WebGLFramebuffer*) {
  NOTIMPLEMENTED();
  return false;
}

bool WebGLRenderingContextWebGPUBase::isProgram(WebGLProgram*) {
  NOTIMPLEMENTED();
  return false;
}

bool WebGLRenderingContextWebGPUBase::isRenderbuffer(WebGLRenderbuffer*) {
  NOTIMPLEMENTED();
  return false;
}

bool WebGLRenderingContextWebGPUBase::isShader(WebGLShader*) {
  NOTIMPLEMENTED();
  return false;
}

bool WebGLRenderingContextWebGPUBase::isTexture(WebGLTexture*) {
  NOTIMPLEMENTED();
  return false;
}

void WebGLRenderingContextWebGPUBase::lineWidth(GLfloat width) {
  driver_gl_.fn.glLineWidthFn(width);
}

void WebGLRenderingContextWebGPUBase::linkProgram(WebGLProgram* program) {
  if (!ValidateProgramOrShader("linkProgram", program)) {
    return;
  }

  RETURN_IF_GL_ERROR(driver_gl_.fn.glLinkProgramFn(program->Object()));

  // TODO(413078308): Handle KHR_parallel_shader_compile.
  // Increase the link count so TFO can know which version it uses.
  program->IncreaseLinkCount();
}

void WebGLRenderingContextWebGPUBase::pixelStorei(GLenum pname, GLint param) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::polygonOffset(GLfloat factor,
                                                    GLfloat units) {
  driver_gl_.fn.glPolygonOffsetFn(factor, units);
}

void WebGLRenderingContextWebGPUBase::readPixels(
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::renderbufferStorage(GLenum target,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::sampleCoverage(GLfloat value,
                                                     GLboolean invert) {
  driver_gl_.fn.glSampleCoverageFn(value, invert);
}

void WebGLRenderingContextWebGPUBase::scissor(GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height) {
  driver_gl_.fn.glScissorFn(x, y, width, height);
}

void WebGLRenderingContextWebGPUBase::shaderSource(WebGLShader* shader,
                                                   const String& source) {
  if (!ValidateProgramOrShader("shaderSource", shader)) {
    return;
  }

  // Handle non-ASCII characters (by replacing them with '?').
  std::vector<char> ascii_source;
  ascii_source.reserve(source.length());
  for (auto code_point : source) {
    ascii_source.push_back(IsASCII(code_point) ? code_point : '?');
  }

  GLint c_ascii_size = ascii_source.size();
  const char* c_ascii_source = ascii_source.data();
  RETURN_IF_GL_ERROR(driver_gl_.fn.glShaderSourceFn(
      shader->Object(), 1, &c_ascii_source, &c_ascii_size));

  shader->SetSource(source);
}

void WebGLRenderingContextWebGPUBase::stencilFunc(GLenum func,
                                                  GLint ref,
                                                  GLuint mask) {
  driver_gl_.fn.glStencilFuncFn(func, ref, mask);
}

void WebGLRenderingContextWebGPUBase::stencilFuncSeparate(GLenum face,
                                                          GLenum func,
                                                          GLint ref,
                                                          GLuint mask) {
  driver_gl_.fn.glStencilFuncSeparateFn(face, func, ref, mask);
}

void WebGLRenderingContextWebGPUBase::stencilMask(GLuint mask) {
  driver_gl_.fn.glStencilMaskFn(mask);
}

void WebGLRenderingContextWebGPUBase::stencilMaskSeparate(GLenum face,
                                                          GLuint mask) {
  driver_gl_.fn.glStencilMaskSeparateFn(face, mask);
}

void WebGLRenderingContextWebGPUBase::stencilOp(GLenum fail,
                                                GLenum zfail,
                                                GLenum zpass) {
  driver_gl_.fn.glStencilOpFn(fail, zfail, zpass);
}

void WebGLRenderingContextWebGPUBase::stencilOpSeparate(GLenum face,
                                                        GLenum fail,
                                                        GLenum zfail,
                                                        GLenum zpass) {
  driver_gl_.fn.glStencilOpSeparateFn(face, fail, zfail, zpass);
}

void WebGLRenderingContextWebGPUBase::texParameterf(GLenum target,
                                                    GLenum pname,
                                                    GLfloat param) {
  driver_gl_.fn.glTexParameterfFn(target, pname, param);
}

void WebGLRenderingContextWebGPUBase::texParameteri(GLenum target,
                                                    GLenum pname,
                                                    GLint param) {
  driver_gl_.fn.glTexParameteriFn(target, pname, param);
}

void WebGLRenderingContextWebGPUBase::texImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  const void* pixel_data = nullptr;
  GLsizei pixel_data_size = 0;
  if (pixels) {
    pixel_data = pixels->BaseAddress();

    if (!ValidateFitsNonNegInt32("texImage2D", "pixels",
                                 pixels->byteLength())) {
      return;
    }
    pixel_data_size = static_cast<GLsizei>(pixels->byteLength());
  }

  driver_gl_.fn.glTexImage2DRobustANGLEFn(target, level, internalformat, width,
                                          height, border, format, type,
                                          pixel_data_size, pixel_data);
}

void WebGLRenderingContextWebGPUBase::texImage2D(GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLenum format,
                                                 GLenum type,
                                                 ImageData*) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(ScriptState*,
                                                 GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLenum format,
                                                 GLenum type,
                                                 HTMLImageElement*,
                                                 ExceptionState&) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(ScriptState*,
                                                 GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLenum format,
                                                 GLenum type,
                                                 CanvasRenderingContextHost*,
                                                 ExceptionState&) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(ScriptState*,
                                                 GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLenum format,
                                                 GLenum type,
                                                 HTMLVideoElement*,
                                                 ExceptionState&) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(ScriptState*,
                                                 GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLenum format,
                                                 GLenum type,
                                                 VideoFrame*,
                                                 ExceptionState&) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLenum format,
                                                 GLenum type,
                                                 ImageBitmap*,
                                                 ExceptionState&) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  const void* pixel_data = nullptr;
  GLsizei pixel_data_size = 0;
  if (pixels) {
    pixel_data = pixels->BaseAddress();

    if (!ValidateFitsNonNegInt32("texSubImage2D", "pixels",
                                 pixels->byteLength())) {
      return;
    }
    pixel_data_size = static_cast<GLsizei>(pixels->byteLength());
  }

  driver_gl_.fn.glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset,
                                             width, height, format, type,
                                             pixel_data_size, pixel_data);
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLenum format,
                                                    GLenum type,
                                                    ImageData*) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(ScriptState*,
                                                    GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLenum format,
                                                    GLenum type,
                                                    HTMLImageElement*,
                                                    ExceptionState&) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(ScriptState*,
                                                    GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLenum format,
                                                    GLenum type,
                                                    CanvasRenderingContextHost*,
                                                    ExceptionState&) {
  NOTIMPLEMENTED();
}
void WebGLRenderingContextWebGPUBase::texSubImage2D(ScriptState*,
                                                    GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLenum format,
                                                    GLenum type,
                                                    HTMLVideoElement*,
                                                    ExceptionState&) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(ScriptState*,
                                                    GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLenum format,
                                                    GLenum type,
                                                    VideoFrame*,
                                                    ExceptionState&) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLenum format,
                                                    GLenum type,
                                                    ImageBitmap*,
                                                    ExceptionState&) {
  NOTIMPLEMENTED();
}

// TODO(413078308): Add validation that the uniform location still being current
// and matching the current program. For the "v" versions of the uniforms, also
// check that the byte size fits in a i32 and the array size is a multiple of
// 1/2/3/4. Finally null locations must result in noops.
void WebGLRenderingContextWebGPUBase::uniform1f(
    const WebGLUniformLocation* location,
    GLfloat x) {
  if (!ValidateUniformLocation("uniform1f", location)) {
    return;
  }
  driver_gl_.fn.glUniform1fFn(location->Location(), x);
}

void WebGLRenderingContextWebGPUBase::uniform1fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniform1fv", location, 1, v.size())) {
    return;
  }
  driver_gl_.fn.glUniform1fvFn(location->Location(), v.size(), v.data());
}

void WebGLRenderingContextWebGPUBase::uniform1i(
    const WebGLUniformLocation* location,
    GLint x) {
  if (!ValidateUniformLocation("uniform1i", location)) {
    return;
  }
  driver_gl_.fn.glUniform1iFn(location->Location(), x);
}

void WebGLRenderingContextWebGPUBase::uniform1iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v) {
  if (!ValidateUniformV("uniform1iv", location, 1, v.size())) {
    return;
  }
  driver_gl_.fn.glUniform1ivFn(location->Location(), v.size(), v.data());
}

void WebGLRenderingContextWebGPUBase::uniform2f(
    const WebGLUniformLocation* location,
    GLfloat x,
    GLfloat y) {
  if (!ValidateUniformLocation("uniform2f", location)) {
    return;
  }
  driver_gl_.fn.glUniform2fFn(location->Location(), x, y);
}

void WebGLRenderingContextWebGPUBase::uniform2fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniform2fv", location, 2, v.size())) {
    return;
  }
  driver_gl_.fn.glUniform2fvFn(location->Location(), v.size() / 2, v.data());
}

void WebGLRenderingContextWebGPUBase::uniform2i(
    const WebGLUniformLocation* location,
    GLint x,
    GLint y) {
  if (!ValidateUniformLocation("uniform2i", location)) {
    return;
  }
  driver_gl_.fn.glUniform2iFn(location->Location(), x, y);
}

void WebGLRenderingContextWebGPUBase::uniform2iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v) {
  if (!ValidateUniformV("uniform2iv", location, 2, v.size())) {
    return;
  }
  driver_gl_.fn.glUniform2ivFn(location->Location(), v.size() / 2, v.data());
}

void WebGLRenderingContextWebGPUBase::uniform3f(
    const WebGLUniformLocation* location,
    GLfloat x,
    GLfloat y,
    GLfloat z) {
  if (!ValidateUniformLocation("uniform3f", location)) {
    return;
  }
  driver_gl_.fn.glUniform3fFn(location->Location(), x, y, z);
}

void WebGLRenderingContextWebGPUBase::uniform3fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniform3fv", location, 3, v.size())) {
    return;
  }
  driver_gl_.fn.glUniform3fvFn(location->Location(), v.size() / 3, v.data());
}

void WebGLRenderingContextWebGPUBase::uniform3i(
    const WebGLUniformLocation* location,
    GLint x,
    GLint y,
    GLint z) {
  if (!ValidateUniformLocation("uniform3i", location)) {
    return;
  }
  driver_gl_.fn.glUniform3iFn(location->Location(), x, y, z);
}

void WebGLRenderingContextWebGPUBase::uniform3iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v) {
  if (!ValidateUniformV("uniform3iv", location, 3, v.size())) {
    return;
  }
  driver_gl_.fn.glUniform3ivFn(location->Location(), v.size() / 3, v.data());
}

void WebGLRenderingContextWebGPUBase::uniform4f(
    const WebGLUniformLocation* location,
    GLfloat x,
    GLfloat y,
    GLfloat z,
    GLfloat w) {
  if (!ValidateUniformLocation("uniform4f", location)) {
    return;
  }
  driver_gl_.fn.glUniform4fFn(location->Location(), x, y, z, w);
}

void WebGLRenderingContextWebGPUBase::uniform4fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniform4fv", location, 4, v.size())) {
    return;
  }
  driver_gl_.fn.glUniform4fvFn(location->Location(), v.size() / 4, v.data());
}

void WebGLRenderingContextWebGPUBase::uniform4i(
    const WebGLUniformLocation* location,
    GLint x,
    GLint y,
    GLint z,
    GLint w) {
  if (!ValidateUniformLocation("uniform4i", location)) {
    return;
  }
  driver_gl_.fn.glUniform4iFn(location->Location(), x, y, z, w);
}

void WebGLRenderingContextWebGPUBase::uniform4iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v) {
  if (!ValidateUniformV("uniform4iv", location, 4, v.size())) {
    return;
  }
  driver_gl_.fn.glUniform4ivFn(location->Location(), v.size() / 4, v.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniformMatrix2fv", location, 4, v.size())) {
    return;
  }
  driver_gl_.fn.glUniformMatrix2fvFn(location->Location(), v.size() / 4,
                                     transpose, v.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniformMatrix3fv", location, 9, v.size())) {
    return;
  }
  driver_gl_.fn.glUniformMatrix3fvFn(location->Location(), v.size() / 9,
                                     transpose, v.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniformMatrix4fv", location, 16, v.size())) {
    return;
  }
  driver_gl_.fn.glUniformMatrix4fvFn(location->Location(), v.size() / 16,
                                     transpose, v.data());
}

void WebGLRenderingContextWebGPUBase::useProgram(WebGLProgram* program) {
  if (!ValidateNullableObject("useProgram", program)) {
    return;
  }

  RETURN_IF_GL_ERROR(driver_gl_.fn.glUseProgramFn(ObjectOrZero(program)));

  program_binding_ = program;
}

void WebGLRenderingContextWebGPUBase::validateProgram(WebGLProgram* program) {
  if (!ValidateProgramOrShader("useProgram", program)) {
    return;
  }

  driver_gl_.fn.glValidateProgramFn(program->Object());
}

void WebGLRenderingContextWebGPUBase::vertexAttrib1f(GLuint index, GLfloat x) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttrib1fv(
    GLuint index,
    base::span<const GLfloat> values) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttrib2f(GLuint index,
                                                     GLfloat x,
                                                     GLfloat y) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttrib2fv(
    GLuint index,
    base::span<const GLfloat> values) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttrib3f(GLuint index,
                                                     GLfloat x,
                                                     GLfloat y,
                                                     GLfloat z) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttrib3fv(
    GLuint index,
    base::span<const GLfloat> values) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttrib4f(GLuint index,
                                                     GLfloat x,
                                                     GLfloat y,
                                                     GLfloat z,
                                                     GLfloat w) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttrib4fv(
    GLuint index,
    base::span<const GLfloat> values) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttribPointer(GLuint index,
                                                          GLint size,
                                                          GLenum type,
                                                          GLboolean normalized,
                                                          GLsizei stride,
                                                          int64_t offset) {
  if (!ValidateFitsNonNegInt32("vertexAttribPointer", "offset", offset)) {
    return;
  }

  driver_gl_.fn.glVertexAttribPointerFn(
      index, size, type, normalized, stride,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));

  // TODO(413078308): Update the tracked buffer in the current VAO if there was
  // no GL error.
}

void WebGLRenderingContextWebGPUBase::viewport(GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height) {
  driver_gl_.fn.glViewportFn(x, y, width, height);
}

void WebGLRenderingContextWebGPUBase::drawingBufferStorage(GLenum sizedformat,
                                                           GLsizei width,
                                                           GLsizei height) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::commit() {
  NOTIMPLEMENTED();
}

ScriptPromise<IDLUndefined> WebGLRenderingContextWebGPUBase::makeXRCompatible(
    ScriptState*,
    ExceptionState&) {
  NOTIMPLEMENTED();
  return {};
}

// ****************************************************************************
// End of WebGLRenderingContextBase's IDL methods
// ****************************************************************************

// **************************************************************************
// Start of WebGL2RenderingContextBase's IDL methods
// **************************************************************************

void WebGLRenderingContextWebGPUBase::bufferData(
    GLenum target,
    MaybeShared<DOMArrayBufferView> srcData,
    GLenum usage,
    int64_t srcOffset,
    GLuint length) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::bufferSubData(
    GLenum target,
    int64_t offset,
    MaybeShared<DOMArrayBufferView> srcData,
    int64_t srcOffset,
    GLuint length) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::copyBufferSubData(GLenum readTarget,
                                                        GLenum writeTarget,
                                                        int64_t readOffset,
                                                        int64_t writeOffset,
                                                        int64_t size) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::getBufferSubData(
    GLenum target,
    int64_t srcByteOffset,
    MaybeShared<DOMArrayBufferView> dstData,
    int64_t dstOffset,
    GLuint length) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::blitFramebuffer(GLint src_x0,
                                                      GLint src_y0,
                                                      GLint src_x1,
                                                      GLint src_y1,
                                                      GLint dst_x0,
                                                      GLint dst_y0,
                                                      GLint dst_x1,
                                                      GLint dst_y1,
                                                      GLbitfield mask,
                                                      GLenum filter) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::framebufferTextureLayer(
    GLenum target,
    GLenum attachment,
    WebGLTexture* texture,
    GLint level,
    GLint layer) {
  NOTIMPLEMENTED();
}

ScriptValue WebGLRenderingContextWebGPUBase::getInternalformatParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum internalformat,
    GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

void WebGLRenderingContextWebGPUBase::invalidateFramebuffer(
    GLenum target,
    const Vector<GLenum>& attachments) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::invalidateSubFramebuffer(
    GLenum target,
    const Vector<GLenum>& attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::readBuffer(GLenum mode) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::renderbufferStorageMultisample(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLint border,
                                                 GLenum format,
                                                 GLenum type,
                                                 int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLint border,
                                                 GLenum format,
                                                 GLenum type,
                                                 ImageData* pixels) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    HTMLImageElement* image,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    CanvasRenderingContextHost* canvas,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    VideoFrame* frame,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    ImageBitmap* bitmap,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> data,
    int64_t src_offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texElementImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    Element* element,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texElementImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    Element* element,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texElementImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLfloat sx,
    GLfloat sy,
    GLfloat swidth,
    GLfloat sheight,
    GLenum format,
    GLenum type,
    Element* element,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texElementImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLfloat sx,
    GLfloat sy,
    GLfloat swidth,
    GLfloat sheight,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    Element* element,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texElement2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    Element* element,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texElement2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    Element* element,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texElement2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLfloat sx,
    GLfloat sy,
    GLfloat swidth,
    GLfloat sheight,
    GLenum format,
    GLenum type,
    Element* element,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texElement2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLfloat sx,
    GLfloat sy,
    GLfloat swidth,
    GLfloat sheight,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    Element* element,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLenum format,
                                                    GLenum type,
                                                    int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLenum format,
                                                    GLenum type,
                                                    ImageData* pixels) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    HTMLImageElement* image,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    CanvasRenderingContextHost* canvas,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    VideoFrame* frame,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    ImageBitmap* bitmap,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels,
    int64_t src_offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texStorage2D(GLenum target,
                                                   GLsizei levels,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texStorage3D(GLenum target,
                                                   GLsizei levels,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLsizei depth) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage3D(GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLint border,
                                                 GLenum format,
                                                 GLenum type,
                                                 int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage3D(GLenum target,
                                                 GLint level,
                                                 GLint internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLint border,
                                                 GLenum format,
                                                 GLenum type,
                                                 ImageData* pixels) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage3D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    HTMLImageElement* image,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage3D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    CanvasRenderingContextHost* canvas,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage3D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage3D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    VideoFrame* frame,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage3D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    ImageBitmap* bitmap,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage3D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texImage3D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels,
    GLuint src_offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage3D(GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLint zoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLsizei depth,
                                                    GLenum format,
                                                    GLenum type,
                                                    int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage3D(GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLint zoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLsizei depth,
                                                    GLenum format,
                                                    GLenum type,
                                                    ImageData* pixels) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage3D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    HTMLImageElement* image,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage3D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    CanvasRenderingContextHost* context_host,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage3D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage3D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    VideoFrame* frame,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    ImageBitmap* bitmap,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::texSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels,
    GLuint src_offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::copyTexSubImage3D(GLenum target,
                                                        GLint level,
                                                        GLint xoffset,
                                                        GLint yoffset,
                                                        GLint zoffset,
                                                        GLint x,
                                                        GLint y,
                                                        GLsizei width,
                                                        GLsizei height) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::compressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    MaybeShared<DOMArrayBufferView> data,
    GLuint src_offset,
    GLuint src_length_override) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::compressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    MaybeShared<DOMArrayBufferView> data,
    GLuint src_offset,
    GLuint src_length_override) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::compressedTexImage3D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    MaybeShared<DOMArrayBufferView> data,
    GLuint src_offset,
    GLuint src_length_override) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::compressedTexSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    MaybeShared<DOMArrayBufferView> data,
    GLuint src_offset,
    GLuint src_length_override) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::compressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLsizei image_size,
    int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::compressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLsizei image_size,
    int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::compressedTexImage3D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLsizei image_size,
    int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::compressedTexSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLsizei image_size,
    int64_t offset) {
  NOTIMPLEMENTED();
}

GLint WebGLRenderingContextWebGPUBase::getFragDataLocation(
    WebGLProgram* program,
    const String& name) {
  NOTIMPLEMENTED();
  return 0;
}

void WebGLRenderingContextWebGPUBase::uniform1ui(
    const WebGLUniformLocation* location,
    GLuint v0) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::uniform2ui(
    const WebGLUniformLocation* location,
    GLuint v0,
    GLuint v1) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::uniform3ui(
    const WebGLUniformLocation* location,
    GLuint v0,
    GLuint v1,
    GLuint v2) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::uniform4ui(
    const WebGLUniformLocation* location,
    GLuint v0,
    GLuint v1,
    GLuint v2,
    GLuint v3) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::uniform1fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniform1fv", location, 1, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform1fvFn(location->Location(), data.size(), data.data());
}

void WebGLRenderingContextWebGPUBase::uniform2fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniform2fv", location, 2, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform2fvFn(location->Location(), data.size() / 2,
                               data.data());
}

void WebGLRenderingContextWebGPUBase::uniform3fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniform3fv", location, 3, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform3fvFn(location->Location(), data.size() / 3,
                               data.data());
}

void WebGLRenderingContextWebGPUBase::uniform4fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniform4fv", location, 4, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform4fvFn(location->Location(), data.size() / 4,
                               data.data());
}

void WebGLRenderingContextWebGPUBase::uniform1iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLint> data;
  if (!ValidateUniformV("uniform1iv", location, 1, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform1ivFn(location->Location(), data.size(), data.data());
}

void WebGLRenderingContextWebGPUBase::uniform2iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLint> data;
  if (!ValidateUniformV("uniform2iv", location, 2, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform2ivFn(location->Location(), data.size() / 2,
                               data.data());
}

void WebGLRenderingContextWebGPUBase::uniform3iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLint> data;
  if (!ValidateUniformV("uniform3iv", location, 3, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform3ivFn(location->Location(), data.size() / 3,
                               data.data());
}

void WebGLRenderingContextWebGPUBase::uniform4iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLint> data;
  if (!ValidateUniformV("uniform4iv", location, 4, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform4ivFn(location->Location(), data.size() / 4,
                               data.data());
}

void WebGLRenderingContextWebGPUBase::uniform1uiv(
    const WebGLUniformLocation* location,
    base::span<const GLuint> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLuint> data;
  if (!ValidateUniformV("uniform1uiv", location, 1, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform1uivFn(location->Location(), data.size(), data.data());
}

void WebGLRenderingContextWebGPUBase::uniform2uiv(
    const WebGLUniformLocation* location,
    base::span<const GLuint> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLuint> data;
  if (!ValidateUniformV("uniform2uiv", location, 2, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform2uivFn(location->Location(), data.size() / 2,
                                data.data());
}

void WebGLRenderingContextWebGPUBase::uniform3uiv(
    const WebGLUniformLocation* location,
    base::span<const GLuint> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLuint> data;
  if (!ValidateUniformV("uniform3uiv", location, 3, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform3uivFn(location->Location(), data.size() / 3,
                                data.data());
}

void WebGLRenderingContextWebGPUBase::uniform4uiv(
    const WebGLUniformLocation* location,
    base::span<const GLuint> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLuint> data;
  if (!ValidateUniformV("uniform4uiv", location, 4, v, src_offset, src_length,
                        &data)) {
    return;
  }
  driver_gl_.fn.glUniform4uivFn(location->Location(), data.size() / 4,
                                data.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniformMatrix2fv", location, 4, v, src_offset,
                        src_length, &data)) {
    return;
  }
  driver_gl_.fn.glUniformMatrix2fvFn(location->Location(), data.size() / 4,
                                     transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniformMatrix3fv", location, 9, v, src_offset,
                        src_length, &data)) {
    return;
  }
  driver_gl_.fn.glUniformMatrix3fvFn(location->Location(), data.size() / 9,
                                     transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniformMatrix4fv", location, 16, v, src_offset,
                        src_length, &data)) {
    return;
  }
  driver_gl_.fn.glUniformMatrix4fvFn(location->Location(), data.size() / 16,
                                     transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix2x3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniformMatrix2x3fv", location, 6, v, src_offset,
                        src_length, &data)) {
    return;
  }
  driver_gl_.fn.glUniformMatrix2x3fvFn(location->Location(), data.size() / 6,
                                       transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix3x2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniformMatrix3x2fv", location, 6, v, src_offset,
                        src_length, &data)) {
    return;
  }
  driver_gl_.fn.glUniformMatrix3x2fvFn(location->Location(), data.size() / 6,
                                       transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix2x4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniformMatrix2x4fv", location, 8, v, src_offset,
                        src_length, &data)) {
    return;
  }
  driver_gl_.fn.glUniformMatrix2x4fvFn(location->Location(), data.size() / 8,
                                       transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix4x2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniformMatrix4x2fv", location, 8, v, src_offset,
                        src_length, &data)) {
    return;
  }
  driver_gl_.fn.glUniformMatrix4x2fvFn(location->Location(), data.size() / 8,
                                       transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix3x4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniformMatrix3x4fv", location, 12, v, src_offset,
                        src_length, &data)) {
    return;
  }
  driver_gl_.fn.glUniformMatrix3x4fvFn(location->Location(), data.size() / 12,
                                       transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix4x3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v,
    GLuint src_offset,
    GLuint src_length) {
  base::span<const GLfloat> data;
  if (!ValidateUniformV("uniformMatrix4x3fv", location, 12, v, src_offset,
                        src_length, &data)) {
    return;
  }
  driver_gl_.fn.glUniformMatrix4x3fvFn(location->Location(), data.size() / 12,
                                       transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::vertexAttribI4i(GLuint index,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z,
                                                      GLint w) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttribI4iv(
    GLuint index,
    base::span<const GLint> v) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttribI4ui(GLuint index,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z,
                                                       GLuint w) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttribI4uiv(
    GLuint index,
    base::span<const GLuint> v) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttribIPointer(GLuint index,
                                                           GLint size,
                                                           GLenum type,
                                                           GLsizei stride,
                                                           int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::vertexAttribDivisor(GLuint index,
                                                          GLuint divisor) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::drawArraysInstanced(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei instance_count) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::drawElementsInstanced(
    GLenum mode,
    GLsizei count,
    GLenum type,
    int64_t offset,
    GLsizei instance_count) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::drawRangeElements(GLenum mode,
                                                        GLuint start,
                                                        GLuint end,
                                                        GLsizei count,
                                                        GLenum type,
                                                        int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::drawBuffers(
    const Vector<GLenum>& buffers) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::clearBufferiv(
    GLenum buffer,
    GLint drawbuffer,
    base::span<const GLint> value,
    GLuint src_offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::clearBufferuiv(
    GLenum buffer,
    GLint drawbuffer,
    base::span<const GLuint> value,
    GLuint src_offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::clearBufferfv(
    GLenum buffer,
    GLint drawbuffer,
    base::span<const GLfloat> value,
    GLuint src_offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::clearBufferfi(GLenum buffer,
                                                    GLint drawbuffer,
                                                    GLfloat depth,
                                                    GLint stencil) {
  NOTIMPLEMENTED();
}

WebGLQuery* WebGLRenderingContextWebGPUBase::createQuery() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebGLRenderingContextWebGPUBase::deleteQuery(WebGLQuery* query) {
  NOTIMPLEMENTED();
}

bool WebGLRenderingContextWebGPUBase::isQuery(WebGLQuery* query) {
  NOTIMPLEMENTED();
  return false;
}

void WebGLRenderingContextWebGPUBase::beginQuery(GLenum target,
                                                 WebGLQuery* query) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::endQuery(GLenum target) {
  NOTIMPLEMENTED();
}

ScriptValue WebGLRenderingContextWebGPUBase::getQuery(ScriptState* script_state,
                                                      GLenum target,
                                                      GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

ScriptValue WebGLRenderingContextWebGPUBase::getQueryParameter(
    ScriptState* script_state,
    WebGLQuery* query,
    GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

WebGLSampler* WebGLRenderingContextWebGPUBase::createSampler() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebGLRenderingContextWebGPUBase::deleteSampler(WebGLSampler* sampler) {
  NOTIMPLEMENTED();
}

bool WebGLRenderingContextWebGPUBase::isSampler(WebGLSampler* sampler) {
  NOTIMPLEMENTED();
  return false;
}

void WebGLRenderingContextWebGPUBase::bindSampler(GLuint unit,
                                                  WebGLSampler* sampler) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::samplerParameteri(WebGLSampler* sampler,
                                                        GLenum pname,
                                                        GLint param) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::samplerParameterf(WebGLSampler* sampler,
                                                        GLenum pname,
                                                        GLfloat param) {
  NOTIMPLEMENTED();
}

ScriptValue WebGLRenderingContextWebGPUBase::getSamplerParameter(
    ScriptState* script_state,
    WebGLSampler* sampler,
    GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

WebGLSync* WebGLRenderingContextWebGPUBase::fenceSync(GLenum condition,
                                                      GLbitfield flags) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool WebGLRenderingContextWebGPUBase::isSync(WebGLSync* sync) {
  NOTIMPLEMENTED();
  return false;
}

void WebGLRenderingContextWebGPUBase::deleteSync(WebGLSync* sync) {
  NOTIMPLEMENTED();
}

GLenum WebGLRenderingContextWebGPUBase::clientWaitSync(WebGLSync* sync,
                                                       GLbitfield flags,
                                                       GLuint64 timeout) {
  NOTIMPLEMENTED();
  return 0;
}

void WebGLRenderingContextWebGPUBase::waitSync(WebGLSync* sync,
                                               GLbitfield flags,
                                               GLint64 timeout) {
  NOTIMPLEMENTED();
}

ScriptValue WebGLRenderingContextWebGPUBase::getSyncParameter(
    ScriptState* script_state,
    WebGLSync* sync,
    GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

WebGLTransformFeedback*
WebGLRenderingContextWebGPUBase::createTransformFeedback() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebGLRenderingContextWebGPUBase::deleteTransformFeedback(
    WebGLTransformFeedback* feedback) {
  NOTIMPLEMENTED();
}

bool WebGLRenderingContextWebGPUBase::isTransformFeedback(
    WebGLTransformFeedback* feedback) {
  NOTIMPLEMENTED();
  return false;
}

void WebGLRenderingContextWebGPUBase::bindTransformFeedback(
    GLenum target,
    WebGLTransformFeedback* feedback) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::beginTransformFeedback(
    GLenum primitive_mode) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::endTransformFeedback() {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::transformFeedbackVaryings(
    WebGLProgram* program,
    const Vector<String>& varyings,
    GLenum buffer_mode) {
  NOTIMPLEMENTED();
}

WebGLActiveInfo* WebGLRenderingContextWebGPUBase::getTransformFeedbackVarying(
    WebGLProgram* program,
    GLuint index) {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebGLRenderingContextWebGPUBase::pauseTransformFeedback() {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::resumeTransformFeedback() {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::bindBufferBase(GLenum target,
                                                     GLuint index,
                                                     WebGLBuffer* buffer) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::bindBufferRange(GLenum target,
                                                      GLuint index,
                                                      WebGLBuffer* buffer,
                                                      int64_t offset,
                                                      int64_t size) {
  NOTIMPLEMENTED();
}

ScriptValue WebGLRenderingContextWebGPUBase::getIndexedParameter(
    ScriptState* script_state,
    GLenum target,
    GLuint index) {
  NOTIMPLEMENTED();
  return {};
}

std::optional<Vector<GLuint>>
WebGLRenderingContextWebGPUBase::getUniformIndices(
    WebGLProgram* program,
    const Vector<String>& uniform_names) {
  NOTIMPLEMENTED();
  return {};
}

ScriptValue WebGLRenderingContextWebGPUBase::getActiveUniforms(
    ScriptState* script_state,
    WebGLProgram* program,
    const Vector<GLuint>& uniform_indices,
    GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

GLuint WebGLRenderingContextWebGPUBase::getUniformBlockIndex(
    WebGLProgram* program,
    const String& uniform_block_name) {
  NOTIMPLEMENTED();
  return 0;
}

ScriptValue WebGLRenderingContextWebGPUBase::getActiveUniformBlockParameter(
    ScriptState* script_state,
    WebGLProgram* program,
    GLuint uniform_block_index,
    GLenum pname) {
  NOTIMPLEMENTED();
  return {};
}

String WebGLRenderingContextWebGPUBase::getActiveUniformBlockName(
    WebGLProgram* program,
    GLuint uniform_block_index) {
  NOTIMPLEMENTED();
  return {};
}

void WebGLRenderingContextWebGPUBase::uniformBlockBinding(
    WebGLProgram* program,
    GLuint uniform_block_index,
    GLuint uniform_block_binding) {
  NOTIMPLEMENTED();
}

WebGLVertexArrayObject* WebGLRenderingContextWebGPUBase::createVertexArray() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebGLRenderingContextWebGPUBase::deleteVertexArray(
    WebGLVertexArrayObject* vertex_array) {
  NOTIMPLEMENTED();
}

bool WebGLRenderingContextWebGPUBase::isVertexArray(
    WebGLVertexArrayObject* vertex_array) {
  NOTIMPLEMENTED();
  return false;
}

void WebGLRenderingContextWebGPUBase::bindVertexArray(
    WebGLVertexArrayObject* vertex_array) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::readPixels(GLint x,
                                                 GLint y,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLenum type,
                                                 int64_t offset) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::readPixels(
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels,
    int64_t offset) {
  NOTIMPLEMENTED();
}

// **************************************************************************
// End of WebGL2RenderingContextBase's IDL methods
// **************************************************************************

// ****************************************************************************
// Start of CanvasRenderingContext implementation
// ****************************************************************************
SkAlphaType WebGLRenderingContextWebGPUBase::GetAlphaType() const {
  // WebGL spec section 2.2 The Drawing Buffer
  //
  //   If defined, the alpha channel is used by the HTML compositor to combine
  //   the color buffer with the rest of the page.
  return CreationAttributes().alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
}

viz::SharedImageFormat WebGLRenderingContextWebGPUBase::GetSharedImageFormat()
    const {
  // TODO(413078308): Add support for RGBA16Float drawing buffer.
  if (swap_buffers_) {
    return swap_buffers_->Format();
  }
  return GetN32FormatForCanvas();
}

gfx::ColorSpace WebGLRenderingContextWebGPUBase::GetColorSpace() const {
  // TODO(413078308): Add support for non-SRGB color spaces.
  return gfx::ColorSpace::CreateSRGB();
}

int WebGLRenderingContextWebGPUBase::AllocatedBufferCountPerPixel() const {
  // Front and back buffers.
  // TODO(413078308): Add support configuring MSAA and depth-stencil.
  // Note: If/once this class creates a CanvasResourceProvider it should track
  // the memory of the provider here as well.
  return 2;
}

bool WebGLRenderingContextWebGPUBase::isContextLost() const {
  return IsLost();
}

scoped_refptr<StaticBitmapImage> WebGLRenderingContextWebGPUBase::GetImage() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebGLRenderingContextWebGPUBase::SetHdrMetadata(
    const gfx::HDRMetadata& hdr_metadata) {
  NOTIMPLEMENTED();
}

bool WebGLRenderingContextWebGPUBase::IsComposited() const {
  return true;
}

bool WebGLRenderingContextWebGPUBase::IsPaintable() const {
  return true;
}

void WebGLRenderingContextWebGPUBase::PageVisibilityChanged() {
  NOTIMPLEMENTED();
}

scoped_refptr<StaticBitmapImage>
WebGLRenderingContextWebGPUBase::PaintRenderingResultsToSnapshot(
    SourceDrawingBuffer source_buffer) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool WebGLRenderingContextWebGPUBase::CopyRenderingResultsToVideoFrame(
    WebGraphicsContext3DVideoFramePool*,
    SourceDrawingBuffer,
    const gfx::ColorSpace&,
    VideoFrameCopyCompletedCallback) {
  NOTIMPLEMENTED();
  return false;
}

cc::Layer* WebGLRenderingContextWebGPUBase::CcLayer() const {
  if (swap_buffers_) {
    return swap_buffers_->CcLayer();
  }
  return nullptr;
}

void WebGLRenderingContextWebGPUBase::Reshape(int width, int height) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::Stop() {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::FinalizeFrame(FlushReason) {
  NOTIMPLEMENTED();
}

bool WebGLRenderingContextWebGPUBase::PushFrame() {
  NOTIMPLEMENTED();
  return false;
}

// ****************************************************************************
// End of CanvasRenderingContext implementation
// ****************************************************************************

void WebGLRenderingContextWebGPUBase::OnTextureTransferred() {
  driver_gl_.fn.glFlushFn();

  {
    ScopedBindFramebuffer bind_default_fbo(
        driver_gl_, supports_separate_framebuffer_targets_, GL_FRAMEBUFFER,
        default_framebuffer_);
    driver_gl_.fn.glFramebufferTexture2DEXTFn(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
  }
  driver_gl_.fn.glDeleteTexturesFn(1, &default_framebuffer_color_texture_);
  default_framebuffer_color_texture_ = 0;
  driver_egl_.fn.eglDestroyImageKHRFn(display_,
                                      default_framebuffer_color_image_);
  current_swap_buffer_ = nullptr;
}

void WebGLRenderingContextWebGPUBase::InitializeLayer(cc::Layer* layer) {
  if (Host()) {
    Host()->InitializeLayerWithCSSProperties(layer);
  }
}

void WebGLRenderingContextWebGPUBase::SetNeedsCompositingUpdate() {
  if (Host()) {
    Host()->SetNeedsCompositingUpdate();
  }
}

bool WebGLRenderingContextWebGPUBase::IsGPUDeviceDestroyed() {
  return IsLost();
}

void WebGLRenderingContextWebGPUBase::Trace(Visitor* visitor) const {
  visitor->Trace(draw_framebuffer_binding_);
  visitor->Trace(read_framebuffer_binding_);
  visitor->Trace(array_buffer_binding_);
  visitor->Trace(element_array_buffer_binding_);
  visitor->Trace(program_binding_);
  for (size_t texture_type_idx = 0; texture_type_idx < bound_textures_.size();
       texture_type_idx++) {
    for (size_t texture_unit_idx = 0;
         texture_unit_idx < bound_textures_[texture_type_idx].size();
         texture_unit_idx++) {
      visitor->Trace(bound_textures_[texture_type_idx][texture_unit_idx]);
    }
  }
  WebGLContextObjectSupport::Trace(visitor);
  CanvasRenderingContext::Trace(visitor);
}

void WebGLRenderingContextWebGPUBase::OnDebugMessage(GLenum source,
                                                     GLenum type,
                                                     GLuint id,
                                                     GLenum severity,
                                                     GLsizei length,
                                                     const GLchar* message) {
  if (type == GL_DEBUG_TYPE_ERROR && source == GL_DEBUG_SOURCE_API) {
    had_error_callback_ = true;
    String formatted_message =
        String::Format("WebGL: %s: %s", GetErrorString(id), message);
    PrintGLErrorToConsole(formatted_message);
  } else {
    String formatted_message = String::Format(
        "WebGL: (%s, %s, %s, %d): %s", gl::GetDebugSourceString(source),
        gl::GetDebugTypeString(type), gl::GetDebugSeverityString(severity), id,
        message);
    PrintWarningToConsole(formatted_message);
  }
}

void WebGLRenderingContextWebGPUBase::EnsureDefaultFramebuffer() {
  if (current_swap_buffer_) {
    return;
  }

  gfx::Size framebuffer_size = Host()->Size();

  wgpu::TextureDescriptor texDesc;
  texDesc.size.width = std::max(1, framebuffer_size.width());
  texDesc.size.height = std::max(1, framebuffer_size.height());
  texDesc.usage = swap_buffers_->TextureUsage();
  texDesc.format = swap_buffers_->TextureFormat();
  texDesc.dimension = wgpu::TextureDimension::e2D;

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      swap_buffers_->GetNewTexture(texDesc, GetAlphaType());
  mailbox_texture->SetNeedsPresent(true);

  current_swap_buffer_ = mailbox_texture->GetTexture();

  // Create an EGL image of the swap buffer texture.
  const EGLint image_attribs[] = {
      EGL_NONE,
  };
  default_framebuffer_color_image_ = driver_egl_.fn.eglCreateImageKHRFn(
      display_, EGL_NO_CONTEXT, EGL_WEBGPU_TEXTURE_ANGLE,
      current_swap_buffer_.Get(), image_attribs);
  CHECK(default_framebuffer_color_image_);

  // Import the EGL image to a GL texture and bind it to the default framebuffer
  // as color attachment 0.
  driver_gl_.fn.glGenTexturesFn(1, &default_framebuffer_color_texture_);
  {
    ScopedBindTexture bind_color_texture(driver_gl_, GL_TEXTURE_2D,
                                         default_framebuffer_color_texture_);
    driver_gl_.fn.glEGLImageTargetTexture2DOESFn(
        GL_TEXTURE_2D, default_framebuffer_color_image_);

    {
      ScopedBindFramebuffer bind_default_fbo(
          driver_gl_, supports_separate_framebuffer_targets_, GL_FRAMEBUFFER,
          default_framebuffer_);
      driver_gl_.fn.glFramebufferTexture2DEXTFn(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
          default_framebuffer_color_texture_, 0);
    }
  }

  // TODO(413078308): Don't create a depth stencil when the user did not request
  // it. Also choose a format based on the request.
  if (!default_framebuffer_depth_stencil_renderbuffer_ ||
      framebuffer_size != default_framebuffer_size_) {
    driver_gl_.fn.glDeleteRenderbuffersEXTFn(
        1, &default_framebuffer_depth_stencil_renderbuffer_);
    driver_gl_.fn.glGenRenderbuffersEXTFn(
        1, &default_framebuffer_depth_stencil_renderbuffer_);

    {
      ScopedBindRenderbuffer bind_renderbuffer(
          driver_gl_, default_framebuffer_depth_stencil_renderbuffer_);
      driver_gl_.fn.glRenderbufferStorageEXTFn(
          GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
          std::max(1, framebuffer_size.width()),
          std::max(1, framebuffer_size.height()));
    }

    {
      ScopedBindFramebuffer bind_default_fbo(
          driver_gl_, supports_separate_framebuffer_targets_, GL_FRAMEBUFFER,
          default_framebuffer_);
      driver_gl_.fn.glFramebufferRenderbufferEXTFn(
          GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
          default_framebuffer_depth_stencil_renderbuffer_);
    }
  }

  default_framebuffer_size_ = framebuffer_size;
}

// Do the full initialization of EGL Display and EGL context from the WebGPU
// device.
void WebGLRenderingContextWebGPUBase::InitializeContext() {
  // Use the static ANGLE bindings. Loading the GL driver dynamically is not
  // supported in the renderer process.
  GLGetProcAddressProc get_proc_address =
      GetStaticANGLEGetProcAddressFunction();
  CHECK(get_proc_address != nullptr);
  driver_egl_.InitializeStaticBindings(get_proc_address);

  InitializeEGLDebugLogging(driver_egl_, ui::LogEGLDebugMessage);

  // Initialize the EGL display using the device and the dawn wire client proc
  // table.
  // Force-enable the avoidWaitAny feature because synchronous waiting is not
  // possible yet in dawn wire client.
  constexpr const char* display_enabled_features[] = {
      "avoidWaitAny",
      nullptr,
  };
  const EGLAttrib display_attribs[] = {
      EGL_PLATFORM_ANGLE_TYPE_ANGLE,
      EGL_PLATFORM_ANGLE_TYPE_WEBGPU_ANGLE,
      EGL_PLATFORM_ANGLE_WEBGPU_DEVICE_ANGLE,
      reinterpret_cast<EGLAttrib>(device_.Get()),
      EGL_PLATFORM_ANGLE_DAWN_PROC_TABLE_ANGLE,
      reinterpret_cast<EGLAttrib>(GetDawnProcs()),
      EGL_FEATURE_OVERRIDES_ENABLED_ANGLE,
      reinterpret_cast<EGLAttrib>(display_enabled_features),
      EGL_NONE,
  };
  display_ = driver_egl_.fn.eglGetPlatformDisplayFn(EGL_PLATFORM_ANGLE_ANGLE,
                                                    nullptr, display_attribs);
  CHECK(display_ != EGL_NO_DISPLAY);

  EGLint egl_version_major, egl_version_minor;
  driver_egl_.fn.eglInitializeFn(display_, &egl_version_major,
                                 &egl_version_minor);

  // Setup the ANGLE platform for internal logging and trace events
  angle::InitializePlatform(display_, get_proc_address);

  // Create a GL Context.
  // TODO(413078308): Request version 2 vs 3 depending on WebGL version.
  // TODO(413078308): Request a WebGL compatibility context when requesting
  // extensions is supported both for basic functionality and WebGL extensions.
  const EGLint context_attribs[] = {
      EGL_CONTEXT_MAJOR_VERSION,
      IsWebGL() ? 3 : 2,
      EGL_CONTEXT_MINOR_VERSION,
      0,
      EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE,
      EGL_FALSE,
      EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE,
      EGL_FALSE,
      EGL_NONE,
  };
  context_ = driver_egl_.fn.eglCreateContextFn(display_, EGL_NO_CONFIG_KHR,
                                               nullptr, context_attribs);
  CHECK(context_ != EGL_NO_CONTEXT);

  driver_egl_.fn.eglMakeCurrentFn(display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                                  context_);

  // With the context created, we can load the GL entry points
  driver_gl_.InitializeStaticBindings(get_proc_address);

  const char* extensions_string =
      reinterpret_cast<const char*>(driver_gl_.fn.glGetStringFn(GL_EXTENSIONS));
  gfx::ExtensionSet extensions = gfx::MakeExtensionSet(extensions_string);

  gl::GLVersionInfo version(
      reinterpret_cast<const char*>(driver_gl_.fn.glGetStringFn(GL_VERSION)),
      reinterpret_cast<const char*>(driver_gl_.fn.glGetStringFn(GL_RENDERER)),
      extensions);

  // Load the extension entry points based on the version and extension string.
  // TODO(413078308): Store the enabled extensions, update them after requesting
  // new extensions.
  driver_gl_.InitializeDynamicBindings(get_proc_address, &version, extensions);
  LOG(ERROR) << "GL context extensions: " << extensions_string;

  // Just log all GL debug messages.
  // TODO(413078308): Forward debug messages to the JS console and use the
  // callback for determining when GL calls generate errors without having to
  // check glGetError
  InitializeGLDebugLogging(driver_gl_, true,
                           WebGLRenderingContextWebGPUBaseDebugMessageCallback,
                           this);

  // Initialize the GLES2 interface used for WebGL objects that just proxies to
  // the gl::DriverGL.
  gles2_for_objects_ = std::make_unique<PartialGLES2ForObjects>(&driver_gl_);
  WebGLContextObjectSupport::OnContextRestored(gles2_for_objects_.get());

  // The separate read and draw framebuffer bindings require ES3 or framebuffer
  // blit extensions
  supports_separate_framebuffer_targets_ =
      driver_gl_.ext.b_GL_ANGLE_framebuffer_blit || version.IsAtLeastGLES(3, 0);

  // Create the underlying WebGPUSwapBufferProvider. Usages are based on what
  // ANGLE needs to be able to use this for texturing and rendering.
  constexpr wgpu::TextureUsage kDefaultFBOUsages =
      wgpu::TextureUsage::TextureBinding |
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc |
      wgpu::TextureUsage::CopyDst;
#if BUILDFLAG(IS_ANDROID)
  constexpr wgpu::TextureFormat kDefaultFBON32Format =
      wgpu::TextureFormat::RGBA8Unorm;
#else
  constexpr wgpu::TextureFormat kDefaultFBON32Format =
      wgpu::TextureFormat::BGRA8Unorm;
#endif

  // TODO(413078308): Add support for non-SRGB color spaces and HDR metadata.
  // TODO(413078308): Add support for RGBA16Float drawing buffer.
  swap_buffers_ = base::AdoptRef(new WebGPUSwapBufferProvider(
      this, dawn_control_client_, device_, kDefaultFBOUsages,
      wgpu::TextureUsage::None, kDefaultFBON32Format,
      PredefinedColorSpace::kSRGB, gfx::HDRMetadata{},
      kBottomLeft_GrSurfaceOrigin));

  // Create the default framebuffer and leave it as the bound framebuffer. It
  // has no texture attached yet, that is done in EnsureDefaultFramebuffer
  driver_gl_.fn.glGenFramebuffersEXTFn(1, &default_framebuffer_);
  driver_gl_.fn.glBindFramebufferEXTFn(GL_FRAMEBUFFER, default_framebuffer_);

  // Initialize the default fixed-function state.
  driver_gl_.fn.glViewportFn(0, 0, Host()->Size().width(),
                             Host()->Size().height());
  driver_gl_.fn.glScissorFn(0, 0, Host()->Size().width(),
                            Host()->Size().height());
}

void WebGLRenderingContextWebGPUBase::Destroy() {
  if (context_) {
    DCHECK(display_ != EGL_NO_DISPLAY);
    driver_egl_.fn.eglMakeCurrentFn(EGL_NO_DISPLAY, EGL_NO_CONTEXT,
                                    EGL_NO_SURFACE, EGL_NO_SURFACE);
    driver_egl_.fn.eglDestroyContextFn(display_, context_);
    context_ = EGL_NO_CONTEXT;
  }
  driver_gl_.ClearBindings();
  gles2_for_objects_ = nullptr;

  if (display_) {
    angle::ResetPlatform(display_, driver_egl_.fn.eglGetProcAddressFn);
    driver_egl_.fn.eglTerminateFn(display_);
    display_ = EGL_NO_DISPLAY;
  }
  driver_egl_.ClearBindings();
}

bool WebGLRenderingContextWebGPUBase::ValidateFitsNonNegInt32(
    const char* function_name,
    const char* param_name,
    int64_t value) {
  if (value < 0) {
    String error_msg = StrCat({param_name, " < 0"});
    InsertGLError(GL_INVALID_VALUE, function_name, error_msg.Ascii().c_str());
    return false;
  }
  if (value > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
    String error_msg = StrCat({param_name, " more than 32-bit"});
    InsertGLError(GL_INVALID_OPERATION, function_name,
                  error_msg.Ascii().c_str());
    return false;
  }
  return true;
}

bool WebGLRenderingContextWebGPUBase::ValidateUniformLocation(
    const char* function_name,
    const WebGLUniformLocation* location) {
  if (location == nullptr) {
    return false;
  }
  if (location->Program() != program_binding_) {
    InsertGLError(GL_INVALID_OPERATION, function_name,
                  "location is not from the associated program");
    return false;
  }
  return true;
}

bool WebGLRenderingContextWebGPUBase::ValidateUniformV(
    const char* function_name,
    const WebGLUniformLocation* location,
    size_t required_size_alignment,
    size_t size) {
  if (!ValidateUniformLocation(function_name, location) ||
      !ValidateFitsNonNegInt32(function_name, "data", size)) {
    return false;
  }
  if (size % required_size_alignment != 0) {
    InsertGLError(GL_INVALID_VALUE, function_name, "invalid size");
    return false;
  }
  return true;
}

template <typename T>
bool WebGLRenderingContextWebGPUBase::ValidateUniformV(
    const char* function_name,
    const WebGLUniformLocation* location,
    size_t required_size_alignment,
    base::span<const T> data,
    GLuint src_offset,
    GLuint src_size,
    base::span<const T>* out_data) {
  if (!ValidateUniformLocation(function_name, location) ||
      !ValidateFitsNonNegInt32(function_name, "data", data.size())) {
    return false;
  }

  // Check that src_offset is valid.
  if (src_offset >= data.size()) {
    InsertGLError(GL_INVALID_VALUE, function_name, "invalid srcOffset");
    return false;
  }
  GLuint remaining_data_size = static_cast<GLuint>(data.size()) - src_offset;

  // Compute defaults and validate that the src_size is valid.
  if (src_size == 0) {
    src_size = remaining_data_size;
  } else {
    if (src_size > remaining_data_size) {
      InsertGLError(GL_INVALID_VALUE, function_name, "invalid size");
      return false;
    }
  }
  if (src_size % required_size_alignment != 0) {
    InsertGLError(GL_INVALID_VALUE, function_name, "invalid size");
    return false;
  }

  *out_data = data.subspan(src_offset, src_size);
  return true;
}

bool WebGLRenderingContextWebGPUBase::ValidateNullableObject(
    const char* function_name,
    const WebGLObject* object) {
  if (!object) {
    // This differs in behavior to ValidateObject; null objects are allowed
    // in these entry points.
    return true;
  }
  return ValidateObject(function_name, object);
}

bool WebGLRenderingContextWebGPUBase::ValidateObject(
    const char* function_name,
    const WebGLObject* object) {
  DCHECK(object);
  if (object->MarkedForDeletion()) {
    InsertGLError(GL_INVALID_OPERATION, function_name,
                  "attempt to use a deleted object");
    return false;
  }
  if (!object->Validate(this)) {
    InsertGLError(GL_INVALID_OPERATION, function_name,
                  "object does not belong to this context");
    return false;
  }
  return true;
}

bool WebGLRenderingContextWebGPUBase::ValidateProgramOrShader(
    const char* function_name,
    const WebGLObject* object) {
  DCHECK(object);
  if (object->MarkedForDeletion()) {
    InsertGLError(GL_INVALID_VALUE, function_name,
                  "attempt to use a deleted object");
    return false;
  }
  if (!object->Validate(this)) {
    InsertGLError(GL_INVALID_OPERATION, function_name,
                  "object does not belong to this context");
    return false;
  }
  return true;
}

bool WebGLRenderingContextWebGPUBase::DeleteObject(WebGLObject* object) {
  if (!object) {
    return false;
  }
  if (!object->Validate(this)) {
    InsertGLError(GL_INVALID_OPERATION, "delete",
                  "object does not belong to this context");
    return false;
  }
  if (object->MarkedForDeletion()) {
    // This is specified to be a no-op, including skipping all unbinding from
    // the context's attachment points that would otherwise happen.
    return false;
  }
  if (object->HasObject()) {
    // We need to pass in context here because we want
    // things in this context unbound.
    object->DeleteObject(ContextGL());
  }
  return true;
}

bool WebGLRenderingContextWebGPUBase::CheckAndClearErrorCallbackState() {
  bool had_error_ = had_error_callback_;
  had_error_callback_ = false;
  if (had_error_) {
    // Make sure lose-context-on-OOM logic is triggered as early as possible.
    FlushErrors();
  }
  return had_error_;
}

void WebGLRenderingContextWebGPUBase::FlushErrors() {
  GLenum error = driver_gl_.fn.glGetErrorFn();
  while (error != GL_NO_ERROR) {
    if (!errors_.Contains(error)) {
      errors_.push_back(error);
    }

    // Check for context loss on out-of-memory errors
    if (error == GL_OUT_OF_MEMORY || error == GL_CONTEXT_LOST) {
      // TODO(6596816): Handle context loss and OOM
      break;
    }

    error = driver_gl_.fn.glGetErrorFn();
  }
}

void WebGLRenderingContextWebGPUBase::InsertGLError(GLenum error,
                                                    const char* function_name,
                                                    const char* description) {
  if (!errors_.Contains(error)) {
    errors_.push_back(error);
  }

  String error_type = GetErrorString(error);
  String message =
      StrCat({"WebGL: ", error_type, ": ", function_name, ": ", description});

  PrintGLErrorToConsole(message);
  probe::DidFireWebGLError(canvas(), error_type);
}

void WebGLRenderingContextWebGPUBase::PrintGLErrorToConsole(
    const String& message) {
  if (num_gl_errors_to_console_allowed_ == 0) {
    return;
  }

  --num_gl_errors_to_console_allowed_;
  PrintWarningToConsole(message);

  if (num_gl_errors_to_console_allowed_ == 0) {
    PrintWarningToConsole(
        "WebGL: too many errors, no more errors will be reported to the "
        "console for this context.");
  }

  return;
}
void WebGLRenderingContextWebGPUBase::PrintWarningToConsole(
    const String& message) {
  blink::ExecutionContext* context = Host()->GetTopExecutionContext();
  if (context && !context->IsContextDestroyed()) {
    context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning, message));
  }
}

WebGLFramebuffer* WebGLRenderingContextWebGPUBase::GetBoundFramebuffer(
    GLenum target) const {
  switch (target) {
    case GL_FRAMEBUFFER:
    case GL_DRAW_FRAMEBUFFER:
      return draw_framebuffer_binding_;
    case GL_READ_FRAMEBUFFER:
      return read_framebuffer_binding_;
    default:
      return nullptr;
  }
}

WebGLRenderingContextWebGPUBase::TextureTarget
WebGLRenderingContextWebGPUBase::GLenumToTextureTarget(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return TextureTarget::k2D;
    case GL_TEXTURE_CUBE_MAP:
      return TextureTarget::kCubeMap;
    case GL_TEXTURE_2D_ARRAY:
      return TextureTarget::k2DArray;
    case GL_TEXTURE_3D:
      return TextureTarget::k3D;
    case GL_TEXTURE_2D_MULTISAMPLE:
      return TextureTarget::k2DMultisample;
    default:
      return TextureTarget::kUnkown;
  }
}
}  // namespace blink
