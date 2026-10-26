// Copyright 2025 The Chromium Authors Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_webgpu_base.h"

#include <dawn/dawn_proc.h>
#include <dawn/wire/WireClient.h>

#include <algorithm>
#include <array>
#include <type_traits>

#include "base/notimplemented.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_tone_mapping.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_elementimage.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webgl_copy_element_image_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/webgl/webgl_active_info.h"
#include "third_party/blink/renderer/modules/webgl/webgl_buffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_attribute_helpers.h"
#include "third_party/blink/renderer/modules/webgl/webgl_debug_renderer_info.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_object.h"
#include "third_party/blink/renderer/modules/webgl/webgl_program.h"
#include "third_party/blink/renderer/modules/webgl/webgl_query.h"
#include "third_party/blink/renderer/modules/webgl/webgl_renderbuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_sampler.h"
#include "third_party/blink/renderer/modules/webgl/webgl_shader.h"
#include "third_party/blink/renderer/modules/webgl/webgl_shader_precision_format.h"
#include "third_party/blink/renderer/modules/webgl/webgl_sync.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/modules/webgl/webgl_transform_feedback.h"
#include "third_party/blink/renderer/modules/webgl/webgl_uniform_location.h"
#include "third_party/blink/renderer/modules/webgl/webgl_vertex_array_object.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"
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

// Helper template for validating WebGL objects for is* queries.
template <typename T>
bool ValidateIsObject(WebGLRenderingContextWebGPUBase* context, T* object) {
  if (!object || !object->Validate(context)) {
    return false;
  }
  // Programs and Shaders are exempt from deletion checks because GLES spec
  // dictates they remain valid as long as they are attached, which matches
  // WebGL spec. All other object types are subject to GL ID reuse, so we
  // must check MarkedForDeletion() to avoid returning true for a deleted
  // JS object whose ID has been recycled by ANGLE.
  constexpr bool check_deletion =
      !std::is_same_v<T, WebGLProgram> && !std::is_same_v<T, WebGLShader>;
  if constexpr (check_deletion) {
    if (object->MarkedForDeletion()) {
      return false;
    }
  }
  return true;
}

// Helper template for implementing standard WebGLRenderingContext.is* queries:
template <typename T, typename GLIsFn>
bool IsObject(WebGLRenderingContextWebGPUBase* context,
              T* object,
              GLIsFn gl_is_fn) {
  return ValidateIsObject(context, object) &&
         gl_is_fn(object->Object()) == GL_TRUE;
}

const DawnProcTable* GetDawnProcs() {
#if BUILDFLAG(USE_DAWN)
  return &dawn::wire::client::GetProcs();
#else
  LOG(ERROR) << "WebGLOnWebGPU requires use_dawn";
  return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

#define EGLFN(driver, func) (driver.driver_egl.fn.func##Fn)

const WebGLWebGPUDriver& GetDriverHelper(const WebGLWebGPUDriver& driver) {
  return driver;
}
const WebGLWebGPUDriver& GetDriverHelper(
    const raw_ref<const WebGLWebGPUDriver>& driver) {
  return driver.get();
}
const WebGLWebGPUDriver& GetDriverHelper(
    const raw_ptr<const WebGLWebGPUDriver>& driver) {
  return *driver.get();
}

// Calls the GL_ANGLE_explicit_context variant of FUNC, with the display and
// context of DRIVER.
#define GLFN(DRIVER, FUNC)                         \
  ([&d = GetDriverHelper(DRIVER)](auto... args) {  \
    auto fn = d.driver_gl.fn.FUNC##ContextANGLEFn; \
    DCHECK(fn) << #FUNC " is null.";               \
    return fn(d.display, d.context, args...);      \
  })

// The maximum supported size of an ArrayBuffer is the maximum size that can be
// allocated in JavaScript. This maximum is defined by the maximum size
// PartitionAlloc can allocate. We limit the maximum size of ArrayBuffers we
// support to avoid integer overflows in the WebGL implementation. WebGL stores
// the data size as uint32_t, so if sizes just below uint32_t::max() were passed
// in, integer overflows could happen. The limit defined here is (2GB-2MB),
// which should be enough buffer to avoid integer overflow. This limit should
// restrict the usability of WebGL2 only insignificantly, as JavaScript cannot
// allocate bigger ArrayBuffers anyways. Only with WebAssembly it is possible to
// allocate bigger ArrayBuffers.
static constexpr size_t kMaximumSupportedArrayBufferSize =
    ::partition_alloc::MaxAllocationSize();

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

void InitializeGLDebugLogging(const WebGLWebGPUDriver& driver,
                              bool log_non_errors,
                              GLDEBUGPROC callback,
                              const void* user_param) {
  GLFN(driver, glEnable)(GL_DEBUG_OUTPUT);
  GLFN(driver, glEnable)(GL_DEBUG_OUTPUT_SYNCHRONOUS);

  GLFN(driver, glDebugMessageControlKHR)(GL_DEBUG_SOURCE_API,
                                         GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0,
                                         nullptr, GL_TRUE);

  if (log_non_errors) {
    // Enable logging of medium and high severity messages
    GLFN(driver, glDebugMessageControlKHR)(GL_DONT_CARE, GL_DONT_CARE,
                                           GL_DEBUG_SEVERITY_HIGH, 0, nullptr,
                                           GL_TRUE);
    GLFN(driver, glDebugMessageControlKHR)(GL_DONT_CARE, GL_DONT_CARE,
                                           GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr,
                                           GL_TRUE);
    GLFN(driver, glDebugMessageControlKHR)(GL_DONT_CARE, GL_DONT_CARE,
                                           GL_DEBUG_SEVERITY_LOW, 0, nullptr,
                                           GL_FALSE);
    GLFN(driver, glDebugMessageControlKHR)(GL_DONT_CARE, GL_DONT_CARE,
                                           GL_DEBUG_SEVERITY_NOTIFICATION, 0,
                                           nullptr, GL_FALSE);
  }

  GLFN(driver, glDebugMessageCallbackKHR)(callback, user_param);
}

void InitializeEGLDebugLogging(const WebGLWebGPUDriver& driver,
                               EGLDEBUGPROCKHR callback) {
  if (!driver.driver_egl.client_ext.b_EGL_KHR_debug) {
    return;
  }

  constexpr auto controls = std::to_array<EGLAttrib>({
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
  });

  EGLFN(driver, eglDebugMessageControlKHR)(callback, controls.data());
}

class ScopedBindTexture {
 public:
  ScopedBindTexture(const WebGLWebGPUDriver& driver,
                    GLenum target,
                    GLuint texture)
      : driver_(driver), target_(target) {
    GLenum target_getter = 0;
    switch (target_) {
      case GL_TEXTURE_2D:
        target_getter = GL_TEXTURE_BINDING_2D;
        break;
      default:
        NOTIMPLEMENTED() << " Target not supported.";
    }
    GLFN(driver_, glGetIntegerv)(target_getter, &prev_texture_);
    GLFN(driver_, glBindTexture)(target_, texture);
  }

  ~ScopedBindTexture() { GLFN(driver_, glBindTexture)(target_, prev_texture_); }

  ScopedBindTexture(const ScopedBindTexture&) = delete;
  ScopedBindTexture& operator=(const ScopedBindTexture&) = delete;

 private:
  raw_ref<const WebGLWebGPUDriver> driver_;
  GLenum target_;
  GLint prev_texture_ = 0;
};

class ScopedBindFramebuffer {
 public:
  ScopedBindFramebuffer(const WebGLWebGPUDriver& driver,
                        bool supports_separate_targets,
                        GLenum target,
                        GLuint fbo)
      : driver_(driver) {
    if (supports_separate_targets) {
      GLFN(driver_, glGetIntegerv)(GL_READ_FRAMEBUFFER_BINDING,
                                   &prev_read_fbo_);
      GLFN(driver_, glGetIntegerv)(GL_DRAW_FRAMEBUFFER_BINDING,
                                   &prev_draw_fbo_);
    } else {
      DCHECK(target == GL_FRAMEBUFFER);
      GLFN(driver_, glGetIntegerv)(GL_FRAMEBUFFER_BINDING, &prev_draw_fbo_);
      prev_read_fbo_ = prev_draw_fbo_;
    }
    GLFN(driver_, glBindFramebufferEXT)(target, fbo);
  }

  ~ScopedBindFramebuffer() {
    if (prev_draw_fbo_ == prev_read_fbo_) {
      GLFN(driver_, glBindFramebufferEXT)(GL_FRAMEBUFFER, prev_draw_fbo_);
    } else {
      GLFN(driver_, glBindFramebufferEXT)(GL_DRAW_FRAMEBUFFER, prev_draw_fbo_);
      GLFN(driver_, glBindFramebufferEXT)(GL_READ_FRAMEBUFFER, prev_read_fbo_);
    }
  }

  ScopedBindFramebuffer(const ScopedBindFramebuffer&) = delete;
  ScopedBindFramebuffer& operator=(const ScopedBindFramebuffer&) = delete;

 private:
  raw_ref<const WebGLWebGPUDriver> driver_;
  GLint prev_draw_fbo_ = 0;
  GLint prev_read_fbo_ = 0;
};

class ScopedBindRenderbuffer {
 public:
  ScopedBindRenderbuffer(const WebGLWebGPUDriver& driver, GLuint renderbuffer)
      : driver_(driver) {
    GLFN(driver_, glGetIntegerv)(GL_RENDERBUFFER_BINDING, &prev_renderbuffer_);
    GLFN(driver_, glBindRenderbufferEXT)(GL_RENDERBUFFER, renderbuffer);
  }

  ~ScopedBindRenderbuffer() {
    GLFN(driver_, glBindRenderbufferEXT)(GL_RENDERBUFFER, prev_renderbuffer_);
  }

  ScopedBindRenderbuffer(const ScopedBindRenderbuffer&) = delete;
  ScopedBindRenderbuffer& operator=(const ScopedBindRenderbuffer&) = delete;

 private:
  raw_ref<const WebGLWebGPUDriver> driver_;
  GLint prev_renderbuffer_ = 0;
};

std::string_view GetErrorString(GLenum error) {
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
  explicit PartialGLES2ForObjects(const WebGLWebGPUDriver* driver)
      : driver_(driver) {}

  void BeginQueryEXT(GLenum target, GLuint id) override {
    GLFN(driver_, glBeginQuery)(target, id);
  }
  GLuint CreateProgram() override { return GLFN(driver_, glCreateProgram)(); }
  GLuint CreateShader(GLenum type) override {
    return GLFN(driver_, glCreateShader)(type);
  }
  void DeleteBuffers(GLsizei n, const GLuint* buffers) override {
    GLFN(driver_, glDeleteBuffersARB)(n, buffers);
  }
  void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) override {
    GLFN(driver_, glDeleteFramebuffersEXT)(n, framebuffers);
  }
  void DeleteProgram(GLuint program) override {
    GLFN(driver_, glDeleteProgram)(program);
  }
  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override {
    GLFN(driver_, glDeleteQueries)(n, queries);
  }
  void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) override {
    GLFN(driver_, glDeleteRenderbuffersEXT)(n, renderbuffers);
  }
  void DeleteSamplers(GLsizei n, const GLuint* samplers) override {
    GLFN(driver_, glDeleteSamplers)(n, samplers);
  }
  void DeleteSync(GLsync sync) override { GLFN(driver_, glDeleteSync)(sync); }
  void DeleteShader(GLuint shader) override {
    GLFN(driver_, glDeleteShader)(shader);
  }
  void DeleteTextures(GLsizei n, const GLuint* textures) override {
    GLFN(driver_, glDeleteTextures)(n, textures);
  }
  void DeleteTransformFeedbacks(GLsizei n, const GLuint* ids) override {
    GLFN(driver_, glDeleteTransformFeedbacks)(n, ids);
  }
  void DeleteVertexArraysOES(GLsizei n, const GLuint* arrays) override {
    GLFN(driver_, glDeleteVertexArraysOES)(n, arrays);
  }
  void DrawBuffersEXT(GLsizei count, const GLenum* bufs) override {
    GLFN(driver_, glDrawBuffersARB)(count, bufs);
  }
  void EndQueryEXT(GLenum target) override {
    GLFN(driver_, glEndQuery)(target);
  }
  void FramebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               GLuint renderbuffer) override {
    GLFN(driver_, glFramebufferRenderbufferEXT)(
        target, attachment, renderbuffertarget, renderbuffer);
  }
  void FramebufferTexture2D(GLenum target,
                            GLenum attachment,
                            GLenum textarget,
                            GLuint texture,
                            GLint level) override {
    GLFN(driver_, glFramebufferTexture2DEXT)(target, attachment, textarget,
                                             texture, level);
  }
  void FramebufferTextureLayer(GLenum target,
                               GLenum attachment,
                               GLuint texture,
                               GLint level,
                               GLint layer) override {
    GLFN(driver_, glFramebufferTextureLayer)(target, attachment, texture, level,
                                             layer);
  }
  void GenBuffers(GLsizei n, GLuint* buffers) override {
    GLFN(driver_, glGenBuffersARB)(n, buffers);
  }
  void GenFramebuffers(GLsizei n, GLuint* framebuffers) override {
    GLFN(driver_, glGenFramebuffersEXT)(n, framebuffers);
  }
  void GenQueriesEXT(GLsizei n, GLuint* queries) override {
    GLFN(driver_, glGenQueries)(n, queries);
  }
  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override {
    GLFN(driver_, glGenRenderbuffersEXT)(n, renderbuffers);
  }
  void GenSamplers(GLsizei n, GLuint* samplers) override {
    GLFN(driver_, glGenSamplers)(n, samplers);
  }
  void GenTextures(GLsizei n, GLuint* textures) override {
    GLFN(driver_, glGenTextures)(n, textures);
  }
  void GenTransformFeedbacks(GLsizei n, GLuint* ids) override {
    GLFN(driver_, glGenTransformFeedbacks)(n, ids);
  }
  void GenVertexArraysOES(GLsizei n, GLuint* arrays) override {
    GLFN(driver_, glGenVertexArraysOES)(n, arrays);
  }
  void GetIntegerv(GLenum pname, GLint* params) override {
    GLFN(driver_, glGetIntegerv)(pname, params);
  }
  void GetProgramiv(GLuint program, GLenum pname, GLint* params) override {
    GLFN(driver_, glGetProgramiv)(program, pname, params);
  }
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override {
    GLFN(driver_, glGetQueryObjectuiv)(id, pname, params);
  }
  void GetQueryObjectui64vEXT(GLuint id,
                              GLenum pname,
                              GLuint64* params) override {
    GLFN(driver_, glGetQueryObjectui64v)(id, pname, params);
  }

 private:
  raw_ptr<const WebGLWebGPUDriver> driver_;
};

}  // anonymous namespace

// We proxy all the WebGPU commands related to wgpu::Instance that ANGLE does
// through this class. This is necessary because we wait to intercept WaitAny
// calls with timeout > 0 and Flush the DawnControlClient in that case (to
// support blocking WaitAnys).
class ProxyDawnInstanceForANGLE {
 public:
  explicit ProxyDawnInstanceForANGLE(
      wgpu::Instance instance,
      scoped_refptr<DawnControlClientHolder> dawn_control_client)
      : instance_(std::move(instance)),
        dawn_control_client_(std::move(dawn_control_client)) {
    // Prepare the proc table with overridden procs that use the proxy instance
    // instead. This is the proc table that will be given to ANGLE.
    procs_ = *GetDawnProcs();

    // Proxy WaitAny with logic to Flush the WebGPU commands if needed. This is
    // necessary for ANGLE to be able to block on WGPUFuture completion. If we
    // didn't do a flush, the GPU process could never receive the commands that
    // will eventually signal the future, and ANGLE would block forever.
    procs_.instanceWaitAny =
        [](WGPUInstance angle_instance, size_t future_count,
           WGPUFutureWaitInfo* futures, uint64_t timeout_ns) -> WGPUWaitStatus {
      if (timeout_ns > 0) {
        FromWGPU(angle_instance)->dawn_control_client_->Flush();
      }

      WGPUInstance actual_instance = FromWGPU(angle_instance)->instance_.Get();
      return GetDawnProcs()->instanceWaitAny(actual_instance, future_count,
                                             futures, timeout_ns);
    };

    // ANGLE should use the instance we give it and not recreate one.
    procs_.createInstance = [](const WGPUInstanceDescriptor*) -> WGPUInstance {
      NOTREACHED();
    };

    // Ignore refcounts since we ensure the instance outlives ANGLE.
    procs_.instanceAddRef = [](WGPUInstance) {};
    procs_.instanceRelease = [](WGPUInstance) {};

    // The rest are passthrough
    procs_.instanceCreateSurface =
        [](WGPUInstance angle_instance,
           const WGPUSurfaceDescriptor* descriptor) -> WGPUSurface {
      WGPUInstance actual_instance = FromWGPU(angle_instance)->instance_.Get();
      return GetDawnProcs()->instanceCreateSurface(actual_instance, descriptor);
    };
    procs_.instanceGetWGSLLanguageFeatures =
        [](WGPUInstance angle_instance,
           WGPUSupportedWGSLLanguageFeatures* features) {
          WGPUInstance actual_instance =
              FromWGPU(angle_instance)->instance_.Get();
          return GetDawnProcs()->instanceGetWGSLLanguageFeatures(
              actual_instance, features);
        };
    procs_.instanceHasWGSLLanguageFeature =
        [](WGPUInstance angle_instance,
           WGPUWGSLLanguageFeatureName feature) -> WGPUBool {
      WGPUInstance actual_instance = FromWGPU(angle_instance)->instance_.Get();
      return GetDawnProcs()->instanceHasWGSLLanguageFeature(actual_instance,
                                                            feature);
    };
    procs_.instanceProcessEvents = [](WGPUInstance angle_instance) {
      WGPUInstance actual_instance = FromWGPU(angle_instance)->instance_.Get();
      return GetDawnProcs()->instanceProcessEvents(actual_instance);
    };
    procs_.instanceRequestAdapter =
        [](WGPUInstance angle_instance,
           const WGPURequestAdapterOptions* options,
           WGPURequestAdapterCallbackInfo callback) -> WGPUFuture {
      WGPUInstance actual_instance = FromWGPU(angle_instance)->instance_.Get();
      return GetDawnProcs()->instanceRequestAdapter(actual_instance, options,
                                                    callback);
    };
  }

  ProxyDawnInstanceForANGLE(const ProxyDawnInstanceForANGLE&) = delete;
  ProxyDawnInstanceForANGLE& operator=(const ProxyDawnInstanceForANGLE&) =
      delete;

  EGLAttrib GetProcTableForANGLE() const {
    return reinterpret_cast<EGLAttrib>(&procs_);
  }

  EGLAttrib GetInstanceForANGLE() const {
    return reinterpret_cast<EGLAttrib>(this);
  }

 private:
  WGPUInstance ToWGPU() { return reinterpret_cast<WGPUInstance>(this); }
  static ProxyDawnInstanceForANGLE* FromWGPU(WGPUInstance instance) {
    return reinterpret_cast<ProxyDawnInstanceForANGLE*>(instance);
  }

  wgpu::Instance instance_;
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  DawnProcTable procs_;
};

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

bool WebGLRenderingContextWebGPUBase::Initialize(
    ExecutionContext* execution_context,
    String* error_msg) {
  // Synchronously connect to the GPU process to use WebGPU.
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider =
      Platform::Current()->CreateWebGPUGraphicsContext3DProvider(
          execution_context->Url(), Platform::WebGPUReplyThread::kIOThread);

  if (context_provider == nullptr) {
    *error_msg = "Failed to create a WebGPU context provider";
    return false;
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
  instance_ = dawn_control_client_->GetWGPUInstance();
  proxy_instance_ = std::make_unique<ProxyDawnInstanceForANGLE>(
      instance_, dawn_control_client_);

  InitializeContext();

  // We are required to present to the compositor on context creation.
  EnsureDefaultFramebuffer();

  return true;
}

// ****************************************************************************
// Start of WebGLRenderingContextBase's IDL methods
// ****************************************************************************

V8UnionHTMLCanvasElementOrOffscreenCanvas::Ret
WebGLRenderingContextWebGPUBase::getHTMLOrOffscreenCanvas(
    ScriptState* script_state) const {
  if (canvas()) {
    return V8UnionHTMLCanvasElementOrOffscreenCanvas::Ret(
        script_state, static_cast<HTMLCanvasElement*>(Host()));
  }
  return V8UnionHTMLCanvasElementOrOffscreenCanvas::Ret(
      script_state, static_cast<OffscreenCanvas*>(Host()));
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

CanvasToneMapping* WebGLRenderingContextWebGPUBase::drawingBufferToneMapping(
    const CanvasToneMapping* tone_mapping) {
  NOTIMPLEMENTED();
  return CanvasToneMapping::Create();
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
  RETURN_IF_GL_ERROR(GLFN(driver_, glActiveTexture)(texture));

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
      GLFN(driver_, glAttachShader)(program->Object(), shader->Object()));

  bool attachSuccess = program->AttachShader(shader);
  // While the WRCWebGPU might not need the OnAttach/OnDetach calls,
  // WebGLProgram calls OnDetach so we keep it balanced with an OnAttach here.
  DCHECK(attachSuccess);
  shader->OnAttached();
}

void WebGLRenderingContextWebGPUBase::bindAttribLocation(WebGLProgram* program,
                                                         GLuint index,
                                                         const String& name) {
  if (!ValidateProgramOrShader("bindAttribLocation", program)) {
    return;
  }

  GLFN(driver_, glBindAttribLocation)(program->Object(), index,
                                      name.Utf8().c_str());
}

void WebGLRenderingContextWebGPUBase::bindBuffer(GLenum target,
                                                 WebGLBuffer* buffer) {
  if (!ValidateNullableObject("bindBuffer", buffer)) {
    return;
  }

  RETURN_IF_GL_ERROR(GLFN(driver_, glBindBuffer)(target, ObjectOrZero(buffer)));

  switch (target) {
    case GL_ARRAY_BUFFER:
      array_buffer_binding_ = buffer;
      break;
    case GL_COPY_READ_BUFFER:
      copy_read_buffer_binding_ = buffer;
      break;
    case GL_COPY_WRITE_BUFFER:
      copy_write_buffer_binding_ = buffer;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      element_array_buffer_binding_ = buffer;
      break;
    case GL_PIXEL_PACK_BUFFER:
      pixel_pack_buffer_binding_ = buffer;
      break;
    case GL_PIXEL_UNPACK_BUFFER:
      pixel_unpack_buffer_binding_ = buffer;
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      transform_feedback_buffer_binding_ = buffer;
      break;
    case GL_UNIFORM_BUFFER:
      uniform_buffer_binding_ = buffer;
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

  RETURN_IF_GL_ERROR(GLFN(driver_, glBindFramebufferEXT)(target, id));

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
      GLFN(driver_, glBindTexture)(target, ObjectOrZero(texture)));

  texture->SetTarget(target);
  bound_textures_[static_cast<size_t>(GLenumToTextureTarget(target))]
                 [active_texture_unit_] = texture;
}

void WebGLRenderingContextWebGPUBase::blendColor(GLfloat red,
                                                 GLfloat green,
                                                 GLfloat blue,
                                                 GLfloat alpha) {
  GLFN(driver_, glBlendColor)(red, green, blue, alpha);
}

void WebGLRenderingContextWebGPUBase::blendEquation(GLenum mode) {
  GLFN(driver_, glBlendEquation)(mode);
}

void WebGLRenderingContextWebGPUBase::blendEquationSeparate(GLenum mode_rgb,
                                                            GLenum mode_alpha) {
  GLFN(driver_, glBlendEquationSeparate)(mode_rgb, mode_alpha);
}

void WebGLRenderingContextWebGPUBase::blendFunc(GLenum sfactor,
                                                GLenum dfactor) {
  GLFN(driver_, glBlendFunc)(sfactor, dfactor);
}

void WebGLRenderingContextWebGPUBase::blendFuncSeparate(GLenum src_rgb,
                                                        GLenum dst_rgb,
                                                        GLenum src_alpha,
                                                        GLenum dst_alpha) {
  GLFN(driver_, glBlendFuncSeparate)(src_rgb, dst_rgb, src_alpha, dst_alpha);
}

void WebGLRenderingContextWebGPUBase::bufferData(GLenum target,
                                                 int64_t size,
                                                 GLenum usage) {
  if (!ValidateFitsNonNegInt32("bufferData", "size", size)) {
    return;
  }

  GLFN(driver_, glBufferData)(target, static_cast<GLsizeiptr>(size), nullptr,
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

  GLFN(driver_, glBufferData)(target,
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
  GLFN(driver_, glBufferData)(target,
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
  GLFN(driver_, glBufferSubData)(target, static_cast<GLintptr>(offset),
                                 static_cast<GLsizeiptr>(data.size()),
                                 data.data());
}

GLenum WebGLRenderingContextWebGPUBase::checkFramebufferStatus(GLenum target) {
  return GLFN(driver_, glCheckFramebufferStatusEXT)(target);
}

void WebGLRenderingContextWebGPUBase::clear(GLbitfield mask) {
  EnsureDefaultFramebuffer();
  GLFN(driver_, glClear)(mask);
}

void WebGLRenderingContextWebGPUBase::clearColor(GLfloat red,
                                                 GLfloat green,
                                                 GLfloat blue,
                                                 GLfloat alpha) {
  // TODO(413078308): Handle NaNs.
  GLFN(driver_, glClearColor)(red, green, blue, alpha);
}

void WebGLRenderingContextWebGPUBase::clearDepth(GLfloat depth) {
  GLFN(driver_, glClearDepthf)(depth);
}

void WebGLRenderingContextWebGPUBase::clearStencil(GLint stencil) {
  GLFN(driver_, glClearStencil)(stencil);
}

void WebGLRenderingContextWebGPUBase::colorMask(GLboolean red,
                                                GLboolean green,
                                                GLboolean blue,
                                                GLboolean alpha) {
  GLFN(driver_, glColorMask)(red, green, blue, alpha);
}

void WebGLRenderingContextWebGPUBase::compileShader(WebGLShader* shader) {
  if (!ValidateProgramOrShader("compileShader", shader)) {
    return;
  }

  GLFN(driver_, glCompileShader)(shader->Object());
}

void WebGLRenderingContextWebGPUBase::compressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    MaybeShared<DOMArrayBufferView> data) {
  // WebGL 2.0 uses a separate overload taking an offset for uploads from a
  // PIXEL_UNPACK_BUFFER. GL would interpret the data pointer as an offset.
  if (pixel_unpack_buffer_binding_) {
    InsertGLError(GL_INVALID_OPERATION, "compressedTexImage2D",
                  "a buffer is bound to PIXEL_UNPACK_BUFFER");
    return;
  }
  if (!ValidateFitsNonNegInt32("compressedTexImage2D", "data",
                               data->byteLength())) {
    return;
  }

  GLFN(driver_, glCompressedTexImage2D)(
      target, level, internalformat, width, height, border,
      static_cast<GLsizei>(data->byteLength()), data->BaseAddressMaybeShared());
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
  // WebGL 2.0 uses a separate overload taking an offset for uploads from a
  // PIXEL_UNPACK_BUFFER. GL would interpret the data pointer as an offset.
  if (pixel_unpack_buffer_binding_) {
    InsertGLError(GL_INVALID_OPERATION, "compressedTexSubImage2D",
                  "a buffer is bound to PIXEL_UNPACK_BUFFER");
    return;
  }
  if (!ValidateFitsNonNegInt32("compressedTexSubImage2D", "data",
                               data->byteLength())) {
    return;
  }

  GLFN(driver_, glCompressedTexSubImage2D)(
      target, level, xoffset, yoffset, width, height, format,
      static_cast<GLsizei>(data->byteLength()), data->BaseAddressMaybeShared());
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
  GLFN(driver_, glCullFace)(mode);
}

void WebGLRenderingContextWebGPUBase::deleteBuffer(WebGLBuffer* buffer) {
  if (!DeleteObject(buffer)) {
    return;
  }

  // glDeleteBuffers reset the GL bindings to 0. Do the same for Blink's.
  for (Member<WebGLBuffer>* binding : {
           &array_buffer_binding_,
           &copy_read_buffer_binding_,
           &copy_write_buffer_binding_,
           &element_array_buffer_binding_,
           &pixel_pack_buffer_binding_,
           &pixel_unpack_buffer_binding_,
           &transform_feedback_buffer_binding_,
           &uniform_buffer_binding_,
       }) {
    if (*binding == buffer) {
      *binding = nullptr;
    }
  }
  // TODO(413078308): With VAOs and transform feedback objects, call their
  // UnbindBuffer(). It releases their attachment to the buffer, otherwise the
  // GL buffer is never deleted. For VAOs, this replaces
  // element_array_buffer_binding_. Also clear the indexed bindings once
  // implemented.
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
  GLFN(driver_, glDepthFunc)(func);
}

void WebGLRenderingContextWebGPUBase::depthMask(GLboolean mask) {
  GLFN(driver_, glDepthMask)(mask);
}

void WebGLRenderingContextWebGPUBase::depthRange(GLfloat z_near,
                                                 GLfloat z_far) {
  GLFN(driver_, glDepthRangef)(z_near, z_far);
}

void WebGLRenderingContextWebGPUBase::detachShader(WebGLProgram* program,
                                                   WebGLShader* shader) {
  if (!ValidateProgramOrShader("detachShader", program) ||
      !ValidateProgramOrShader("detachShader", shader)) {
    return;
  }

  // ANGLE generates INVALID_OPERATION if the shader isn't attached.
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glDetachShader)(program->Object(), shader->Object()));

  // Update the JS wrappers. This may delete a shader marked for deletion.
  // ANGLE accepted the detach, so Blink must agree the shader was attached.
  CHECK(program->DetachShader(shader));
  shader->OnDetached(ContextGL());
}

void WebGLRenderingContextWebGPUBase::disable(GLenum cap) {
  GLFN(driver_, glDisable)(cap);
}

void WebGLRenderingContextWebGPUBase::disableVertexAttribArray(GLuint index) {
  GLFN(driver_, glDisableVertexAttribArray)(index);
}

void WebGLRenderingContextWebGPUBase::drawArrays(GLenum mode,
                                                 GLint first,
                                                 GLsizei count) {
  EnsureDefaultFramebuffer();
  GLFN(driver_, glDrawArrays)(mode, first, count);
}

void WebGLRenderingContextWebGPUBase::drawElements(GLenum mode,
                                                   GLsizei count,
                                                   GLenum type,
                                                   int64_t offset) {
  if (!ValidateFitsNonNegInt32("drawElements", "offset", offset)) {
    return;
  }

  EnsureDefaultFramebuffer();
  GLFN(driver_, glDrawElements)(
      mode, count, type,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void WebGLRenderingContextWebGPUBase::enable(GLenum cap) {
  GLFN(driver_, glEnable)(cap);
}

void WebGLRenderingContextWebGPUBase::enableVertexAttribArray(GLuint index) {
  GLFN(driver_, glEnableVertexAttribArray)(index);
}

void WebGLRenderingContextWebGPUBase::finish() {
  // Intentionally a flush, not a finish, to prevent blocking the Blink main
  // thread and triggering "Page Unresponsive" dialogs.
  flush();
}

void WebGLRenderingContextWebGPUBase::flush() {
  GLFN(driver_, glFlush)();
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
  GLFN(driver_, glFrontFace)(mode);
}

void WebGLRenderingContextWebGPUBase::generateMipmap(GLenum target) {
  GLFN(driver_, glGenerateMipmapEXT)(target);
}

WebGLActiveInfo* WebGLRenderingContextWebGPUBase::getActiveAttrib(
    WebGLProgram* program,
    GLuint index) {
  if (!ValidateProgramOrShader("getActiveAttrib", program)) {
    return nullptr;
  }

  GLint max_name_length = -1;
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glGetProgramiv)(
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
      GLFN(driver_, glGetActiveAttrib)(
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
      GLFN(driver_, glGetProgramiv)(
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
      GLFN(driver_, glGetActiveUniform)(
          program->Object(), index, max_name_length, &length, &size, &type,
          reinterpret_cast<GLchar*>(name_buffer.data())),
      nullptr);
  DCHECK_GE(size, 0);
  return MakeGarbageCollected<WebGLActiveInfo>(name_impl->Substring(0, length),
                                               type, size);
}

std::optional<HeapVector<Member<WebGLShader>>>
WebGLRenderingContextWebGPUBase::getAttachedShaders(WebGLProgram* program) {
  if (!ValidateProgramOrShader("getAttachedShaders", program)) {
    return std::nullopt;
  }

  // Must return the JS wrappers, so use Blink's tracking.
  HeapVector<Member<WebGLShader>> shader_objects;
  for (GLenum shader_type : {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER}) {
    WebGLShader* shader = program->GetAttachedShader(shader_type);
    if (shader) {
      shader_objects.push_back(shader);
    }
  }
  return shader_objects;
}

GLint WebGLRenderingContextWebGPUBase::getAttribLocation(WebGLProgram* program,
                                                         const String& name) {
  if (!ValidateProgramOrShader("getAttribLocation", program)) {
    return -1;
  }

  return GLFN(driver_, glGetAttribLocation)(program->Object(),
                                            name.Utf8().c_str());
}

ScriptValue WebGLRenderingContextWebGPUBase::getBufferParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum pname) {
  // ANGLE also accepts the ES3 buffer mapping pnames (BUFFER_MAPPED,
  // BUFFER_ACCESS_FLAGS, ...), but WebGL 2.0 has no buffer mapping.
  if (pname != GL_BUFFER_USAGE && pname != GL_BUFFER_SIZE) {
    InsertGLError(GL_INVALID_ENUM, "getBufferParameter",
                  "invalid parameter name");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  // WebGL 2.0 spec, getBufferParameter:
  // > querying with the pname BUFFER_SIZE returns the buffer size as a value of
  // > type GLsizeiptr.
  // https://registry.khronos.org/webgl/specs/latest/2.0/
  // A 32-bit GLint is equivalent for us: bufferData rejects sizes above
  // INT32_MAX, and JS gets the same Number.
  GLint value = 0;
  RETURN_IF_GL_ERROR(GLFN(driver_, glGetBufferParameterivRobustANGLE)(
                         target, pname, 1, nullptr, &value),
                     ScriptValue::CreateNull(script_state->GetIsolate()));
  return WebGLAny(script_state, value);
}

WebGLContextAttributes* WebGLRenderingContextWebGPUBase::getContextAttributes()
    const {
  if (isContextLost()) {
    return nullptr;
  }

  // TODO(413078308): Adjust the returned attributes to match what the context
  // actually provides once implemented:
  // - depth / stencil: set to false if the default framebuffer failed to
  //   allocate them.
  // - antialias: set to whether the default framebuffer is multisampled.
  // - xrCompatible: set to the tracked XR compatibility state once
  //   makeXRCompatible() is implemented.
  // - desynchronized: set to Host()->LowLatencyEnabled() once low-latency
  //   presentation is supported.
  return ToWebGLContextAttributes(CreationAttributes());
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

  // WebGL 2 parameters that must be 100% handled in Blink (because they return
  // Blink JS wrapper objects or WebGL-specific values). Other WebGL 2
  // parameters are handled in the shared switch below, where ANGLE validates
  // the context version.
  if (IsWebGL2()) {
    switch (pname) {
      // --- Wrapper objects ---
      // WebGLBuffer:
      case GL_COPY_READ_BUFFER_BINDING:
        return WebGLAny(script_state, copy_read_buffer_binding_.Get());

      case GL_COPY_WRITE_BUFFER_BINDING:
        return WebGLAny(script_state, copy_write_buffer_binding_.Get());

      case GL_PIXEL_PACK_BUFFER_BINDING:
        return WebGLAny(script_state, pixel_pack_buffer_binding_.Get());

      case GL_PIXEL_UNPACK_BUFFER_BINDING:
        return WebGLAny(script_state, pixel_unpack_buffer_binding_.Get());

      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        return WebGLAny(script_state, transform_feedback_buffer_binding_.Get());

      case GL_UNIFORM_BUFFER_BINDING:
        return WebGLAny(script_state, uniform_buffer_binding_.Get());

      // WebGLFramebuffer:
      case GL_READ_FRAMEBUFFER_BINDING:
        return WebGLAny(script_state, read_framebuffer_binding_.Get());

      // WebGLTexture:
      case GL_TEXTURE_BINDING_2D_ARRAY:
        return WebGLAny(script_state,
                        bound_textures_[static_cast<size_t>(
                            TextureTarget::k2DArray)][active_texture_unit_]
                            .Get());

      case GL_TEXTURE_BINDING_3D:
        return WebGLAny(script_state,
                        bound_textures_[static_cast<size_t>(TextureTarget::k3D)]
                                       [active_texture_unit_]
                                           .Get());

      // --- Special cases (strings, WebGL-specific limits) ---
      // Blocking the JS thread in clientWaitSync is not allowed, so the
      // maximum timeout is 0.
      case GC3D_MAX_CLIENT_WAIT_TIMEOUT_WEBGL:
        return WebGLAny(script_state, int64_t{0});

      case GL_SHADING_LANGUAGE_VERSION:
        return WebGLAny(
            script_state,
            StrCat({"WebGL GLSL ES 3.00 (",
                    GetGLString(GL_SHADING_LANGUAGE_VERSION), ")"}));

      case GL_VERSION:
        return WebGLAny(script_state,
                        StrCat({"WebGL 2.0 (", GetGLString(GL_VERSION), ")"}));

      // --- Unimplemented parameters ---
      // TODO(413078308): Implement these parameters.
      case GL_READ_BUFFER:
      case GL_SAMPLER_BINDING:
      case GL_TRANSFORM_FEEDBACK_BINDING:
        NOTIMPLEMENTED();
        return ScriptValue::CreateNull(script_state->GetIsolate());
    }
  }

  switch (pname) {
    // --- Wrapper objects ---
    // WebGLBuffer:
    case GL_ARRAY_BUFFER_BINDING:
      return WebGLAny(script_state, array_buffer_binding_.Get());

    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      return WebGLAny(script_state, element_array_buffer_binding_.Get());

    // WebGLFramebuffer:
    case GL_FRAMEBUFFER_BINDING:  // Also GL_DRAW_FRAMEBUFFER_BINDING.
      return WebGLAny(script_state, draw_framebuffer_binding_.Get());

    // WebGLProgram:
    case GL_CURRENT_PROGRAM:
      return WebGLAny(script_state, program_binding_.Get());

    // WebGLTexture:
    case GL_TEXTURE_BINDING_2D:
      return WebGLAny(script_state,
                      bound_textures_[static_cast<size_t>(TextureTarget::k2D)]
                                     [active_texture_unit_]
                                         .Get());

    case GL_TEXTURE_BINDING_CUBE_MAP:
      return WebGLAny(script_state,
                      bound_textures_[static_cast<size_t>(
                          TextureTarget::kCubeMap)][active_texture_unit_]
                          .Get());

    // --- Special cases (compressed formats, strings, Blink state) ---
    case GL_COMPRESSED_TEXTURE_FORMATS: {
      GLint count = 0;
      RETURN_IF_GL_ERROR(
          GLFN(driver_, glGetIntegervRobustANGLE)(
              GL_NUM_COMPRESSED_TEXTURE_FORMATS, 1, nullptr, &count),
          ScriptValue::CreateNull(script_state->GetIsolate()));
      if (count <= 0) {
        return WebGLAny(script_state, DOMUint32Array::Create(0));
      }
      // GLenum values are returned through glGetIntegerv, reinterpret them as
      // unsigned.
      Vector<GLuint> formats(base::checked_cast<wtf_size_t>(count));
      RETURN_IF_GL_ERROR(GLFN(driver_, glGetIntegervRobustANGLE)(
                             pname, formats.size(), nullptr,
                             reinterpret_cast<GLint*>(formats.data())),
                         ScriptValue::CreateNull(script_state->GetIsolate()));
      return WebGLAny(script_state, DOMUint32Array::Create(formats));
    }

    case GL_RENDERER:
      return WebGLAny(script_state, String("WebKit WebGL"));

    case GL_SHADING_LANGUAGE_VERSION:
      return WebGLAny(script_state,
                      StrCat({"WebGL GLSL ES 1.0 (",
                              GetGLString(GL_SHADING_LANGUAGE_VERSION), ")"}));

    case GL_VENDOR:
      return WebGLAny(script_state, String("WebKit"));

    case GL_VERSION:
      return WebGLAny(script_state,
                      StrCat({"WebGL 1.0 (", GetGLString(GL_VERSION), ")"}));

    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      return WebGLAny(script_state, unpack_colorspace_conversion_);

    case GC3D_UNPACK_FLIP_Y_WEBGL:
      return WebGLAny(script_state, unpack_flip_y_);

    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
      return WebGLAny(script_state, unpack_premultiply_alpha_);

    // --- Unimplemented parameters ---
    // TODO(413078308): Implement these parameters.
    case GL_DRAW_BUFFER0:
    case GL_DRAW_BUFFER1:
    case GL_DRAW_BUFFER2:
    case GL_DRAW_BUFFER3:
    case GL_DRAW_BUFFER4:
    case GL_DRAW_BUFFER5:
    case GL_DRAW_BUFFER6:
    case GL_DRAW_BUFFER7:
    case GL_DRAW_BUFFER8:
    case GL_DRAW_BUFFER9:
    case GL_DRAW_BUFFER10:
    case GL_DRAW_BUFFER11:
    case GL_DRAW_BUFFER12:
    case GL_DRAW_BUFFER13:
    case GL_DRAW_BUFFER14:
    case GL_DRAW_BUFFER15:
    case GL_RENDERBUFFER_BINDING:
    case GL_TIMESTAMP_EXT:
    case GL_VERTEX_ARRAY_BINDING:
    case WebGLDebugRendererInfo::kUnmaskedRendererWebgl:
    case WebGLDebugRendererInfo::kUnmaskedVendorWebgl:
      NOTIMPLEMENTED();
      return ScriptValue::CreateNull(script_state->GetIsolate());

    // --- Values wrapping the ANGLE return value in a JS object ---
    case GL_BLEND:
    case GL_CLIP_DISTANCE0_ANGLE:
    case GL_CLIP_DISTANCE1_ANGLE:
    case GL_CLIP_DISTANCE2_ANGLE:
    case GL_CLIP_DISTANCE3_ANGLE:
    case GL_CLIP_DISTANCE4_ANGLE:
    case GL_CLIP_DISTANCE5_ANGLE:
    case GL_CLIP_DISTANCE6_ANGLE:
    case GL_CLIP_DISTANCE7_ANGLE:
    case GL_CULL_FACE:
    case GL_DEPTH_CLAMP_EXT:
    case GL_DEPTH_TEST:
    case GL_DEPTH_WRITEMASK:
    case GL_DITHER:
    case GL_GPU_DISJOINT_EXT:
    case GL_POLYGON_OFFSET_FILL:
    case GL_POLYGON_OFFSET_LINE_ANGLE:
    case GL_RASTERIZER_DISCARD:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE:
    case GL_SAMPLE_COVERAGE_INVERT:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST:
    case GL_TRANSFORM_FEEDBACK_ACTIVE:
    case GL_TRANSFORM_FEEDBACK_PAUSED:
      return GetBooleanParameter(script_state, pname);

    case GL_COLOR_WRITEMASK:
      return GetBooleanArrayParameter<4>(script_state, pname);

    case GL_ACTIVE_TEXTURE:
    case GL_BLEND_DST_ALPHA:
    case GL_BLEND_DST_RGB:
    case GL_BLEND_EQUATION_ALPHA:
    case GL_BLEND_EQUATION_RGB:  // Also GL_BLEND_EQUATION.
    case GL_BLEND_SRC_ALPHA:
    case GL_BLEND_SRC_RGB:
    case GL_CLIP_DEPTH_MODE_EXT:
    case GL_CLIP_ORIGIN_EXT:
    case GL_CULL_FACE_MODE:
    case GL_DEPTH_FUNC:
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
    case GL_FRONT_FACE:
    case GL_GENERATE_MIPMAP_HINT:
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
    case GL_POLYGON_MODE_ANGLE:
    case GL_PROVOKING_VERTEX_ANGLE:
    case GL_STENCIL_BACK_FAIL:
    case GL_STENCIL_BACK_FUNC:
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
    case GL_STENCIL_BACK_VALUE_MASK:
    case GL_STENCIL_BACK_WRITEMASK:
    case GL_STENCIL_FAIL:
    case GL_STENCIL_FUNC:
    case GL_STENCIL_PASS_DEPTH_FAIL:
    case GL_STENCIL_PASS_DEPTH_PASS:
    case GL_STENCIL_VALUE_MASK:
    case GL_STENCIL_WRITEMASK:
      return GetUnsignedIntParameter(script_state, pname);

    case GL_ALPHA_BITS:
    case GL_BLUE_BITS:
    case GL_DEPTH_BITS:
    case GL_FRAGMENT_INTERPOLATION_OFFSET_BITS_OES:
    case GL_GREEN_BITS:
    case GL_MAX_3D_TEXTURE_SIZE:
    case GL_MAX_ARRAY_TEXTURE_LAYERS:
    case GL_MAX_CLIP_DISTANCES_ANGLE:
    case GL_MAX_COLOR_ATTACHMENTS:
    case GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_ANGLE:
    case GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE:
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
    case GL_MAX_COMBINED_UNIFORM_BLOCKS:
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
    case GL_MAX_CULL_DISTANCES_ANGLE:
    case GL_MAX_DRAW_BUFFERS:
    case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT:
    case GL_MAX_ELEMENTS_INDICES:
    case GL_MAX_ELEMENTS_VERTICES:
    case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
    case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
    case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
    case GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE:
    case GL_MAX_PROGRAM_TEXEL_OFFSET:
    case GL_MAX_RENDERBUFFER_SIZE:
    case GL_MAX_SAMPLES:
    case GL_MAX_TEXTURE_IMAGE_UNITS:
    case GL_MAX_TEXTURE_SIZE:
    case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
    case GL_MAX_UNIFORM_BUFFER_BINDINGS:
    case GL_MAX_VARYING_COMPONENTS:
    case GL_MAX_VARYING_VECTORS:
    case GL_MAX_VERTEX_ATTRIBS:
    case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
    case GL_MAX_VERTEX_UNIFORM_BLOCKS:
    case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
    case GL_MAX_VIEWS_OVR:
    case GL_MIN_PROGRAM_TEXEL_OFFSET:
    case GL_PACK_ALIGNMENT:
    case GL_PACK_ROW_LENGTH:
    case GL_PACK_SKIP_PIXELS:
    case GL_PACK_SKIP_ROWS:
    case GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE:
    case GL_RED_BITS:
    case GL_SAMPLE_BUFFERS:
    case GL_SAMPLES:
    case GL_STENCIL_BACK_REF:
    case GL_STENCIL_BITS:
    case GL_STENCIL_CLEAR_VALUE:
    case GL_STENCIL_REF:
    case GL_SUBPIXEL_BITS:
    case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
    case GL_UNPACK_IMAGE_HEIGHT:
    case GL_UNPACK_ROW_LENGTH:
    case GL_UNPACK_SKIP_IMAGES:
    case GL_UNPACK_SKIP_PIXELS:
    case GL_UNPACK_SKIP_ROWS:
      return GetIntParameter(script_state, pname);

    case GL_MAX_VIEWPORT_DIMS:
      return GetIntArrayParameter<2>(script_state, pname);

    case GL_SCISSOR_BOX:
    case GL_VIEWPORT:
      return GetIntArrayParameter<4>(script_state, pname);

    case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
    case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
    case GL_MAX_ELEMENT_INDEX:
    case GL_MAX_SERVER_WAIT_TIMEOUT:
    case GL_MAX_UNIFORM_BLOCK_SIZE:
      return GetInt64Parameter(script_state, pname);

    case GL_DEPTH_CLEAR_VALUE:
    case GL_LINE_WIDTH:
    case GL_MAX_FRAGMENT_INTERPOLATION_OFFSET_OES:
    case GL_MAX_TEXTURE_LOD_BIAS:
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
    case GL_MIN_FRAGMENT_INTERPOLATION_OFFSET_OES:
    case GL_POLYGON_OFFSET_CLAMP_EXT:
    case GL_POLYGON_OFFSET_FACTOR:
    case GL_POLYGON_OFFSET_UNITS:
    case GL_SAMPLE_COVERAGE_VALUE:
      return GetFloatParameter(script_state, pname);

    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_DEPTH_RANGE:
      return GetFloatArrayParameter<2>(script_state, pname);

    case GL_BLEND_COLOR:
    case GL_COLOR_CLEAR_VALUE:
      return GetFloatArrayParameter<4>(script_state, pname);

    default:
      InsertGLError(GL_INVALID_ENUM, "getParameter", "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
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
      GLFN(driver_, glGetProgramiv)(program->Object(), pname, &value);
      return WebGLAny(script_state, static_cast<bool>(value));

    case GL_COMPLETION_STATUS_KHR:
      // Completion status queries always return true on a lost context. This is
      // intended to prevent applications from entering an infinite polling
      // loop.
      if (IsLost()) {
        return WebGLAny(script_state, true);
      }
      GLFN(driver_, glGetProgramiv)(program->Object(), GL_COMPLETION_STATUS_KHR,
                                    &value);
      return WebGLAny(script_state, bool(value));

    case GL_ACTIVE_UNIFORM_BLOCKS:
    case GL_TRANSFORM_FEEDBACK_VARYINGS:
    case GL_ATTACHED_SHADERS:
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_UNIFORMS:
    case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
      GLFN(driver_, glGetProgramiv)(program->Object(), pname, &value);
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
  RETURN_IF_GL_ERROR(GLFN(driver_, glGetProgramiv)(program->Object(),
                                                   GL_INFO_LOG_LENGTH, &length),
                     String());

  if (length == 0) {
    return String();
  }

  GLsizei write_length = 0;
  StringBuffer<LChar> log_buffer(length);
  RETURN_IF_GL_ERROR(GLFN(driver_, glGetProgramInfoLog)(
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
      GLFN(driver_, glGetShaderiv)(shader->Object(), GL_COMPILE_STATUS, &value);
      return WebGLAny(script_state, bool(value));

    case GL_COMPLETION_STATUS_KHR:
      // Completion status queries always return true on a lost context. This is
      // intended to prevent applications from entering an infinite polling
      // loop.
      if (IsLost()) {
        return WebGLAny(script_state, true);
      }
      GLFN(driver_, glGetShaderiv)(shader->Object(), GL_COMPLETION_STATUS_KHR,
                                   &value);
      return WebGLAny(script_state, bool(value));

    case GL_SHADER_TYPE:
      GLFN(driver_, glGetShaderiv)(shader->Object(), GL_SHADER_TYPE, &value);
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
  RETURN_IF_GL_ERROR(GLFN(driver_, glGetShaderiv)(shader->Object(),
                                                  GL_INFO_LOG_LENGTH, &length),
                     String());

  if (length == 0) {
    return String();
  }

  GLsizei write_length = 0;
  StringBuffer<LChar> log_buffer(length);
  RETURN_IF_GL_ERROR(GLFN(driver_, glGetShaderInfoLog)(
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
  std::array<GLint, 2> range = {0, 0};
  GLint precision = 0;
  RETURN_IF_GL_ERROR(GLFN(driver_, glGetShaderPrecisionFormat)(
                         shader_type, precision_type, range.data(), &precision),
                     nullptr);
  return MakeGarbageCollected<WebGLShaderPrecisionFormat>(range[0], range[1],
                                                          precision);
}

String WebGLRenderingContextWebGPUBase::getShaderSource(WebGLShader* shader) {
  if (!ValidateProgramOrShader("getShaderSource", shader)) {
    return String();
  }

  // ANGLE only has the ASCII-sanitized copy. Return the original source.
  // A null String would become JS null, which WebGL reserves for errors.
  return shader->Source().IsNull() ? g_empty_string : shader->Source();
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

  GLint location = GLFN(driver_, glGetUniformLocation)(program->Object(),
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
  void* pointer = nullptr;
  RETURN_IF_GL_ERROR(GLFN(driver_, glGetVertexAttribPointervRobustANGLE)(
                         index, pname, 1, nullptr, &pointer),
                     0);
  return static_cast<int64_t>(reinterpret_cast<intptr_t>(pointer));
}

void WebGLRenderingContextWebGPUBase::hint(GLenum target, GLenum mode) {
  GLFN(driver_, glHint)(target, mode);
}

bool WebGLRenderingContextWebGPUBase::isBuffer(WebGLBuffer* buffer) {
  return IsObject(this, buffer, GLFN(driver_, glIsBuffer));
}

bool WebGLRenderingContextWebGPUBase::isEnabled(GLenum cap) {
  NOTIMPLEMENTED();
  return false;
}

bool WebGLRenderingContextWebGPUBase::isFramebuffer(
    WebGLFramebuffer* framebuffer) {
  return IsObject(this, framebuffer, GLFN(driver_, glIsFramebufferEXT));
}

bool WebGLRenderingContextWebGPUBase::isProgram(WebGLProgram* program) {
  return IsObject(this, program, GLFN(driver_, glIsProgram));
}

bool WebGLRenderingContextWebGPUBase::isRenderbuffer(
    WebGLRenderbuffer* renderbuffer) {
  return IsObject(this, renderbuffer, GLFN(driver_, glIsRenderbufferEXT));
}

bool WebGLRenderingContextWebGPUBase::isShader(WebGLShader* shader) {
  return IsObject(this, shader, GLFN(driver_, glIsShader));
}

bool WebGLRenderingContextWebGPUBase::isTexture(WebGLTexture* texture) {
  return IsObject(this, texture, GLFN(driver_, glIsTexture));
}

void WebGLRenderingContextWebGPUBase::lineWidth(GLfloat width) {
  GLFN(driver_, glLineWidth)(width);
}

void WebGLRenderingContextWebGPUBase::linkProgram(WebGLProgram* program) {
  if (!ValidateProgramOrShader("linkProgram", program)) {
    return;
  }

  RETURN_IF_GL_ERROR(GLFN(driver_, glLinkProgram)(program->Object()));

  // TODO(413078308): Handle KHR_parallel_shader_compile.
  // Increase the link count so TFO can know which version it uses.
  program->IncreaseLinkCount();
}

void WebGLRenderingContextWebGPUBase::pixelStorei(GLenum pname, GLint param) {
  switch (pname) {
    // WebGL-specific parameters:
    // TODO(413078308): Apply these parameters in tex[Sub]Image2D and
    // tex[Sub]Image3D.
    case GC3D_UNPACK_FLIP_Y_WEBGL:
      unpack_flip_y_ = param != 0;
      return;

    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
      unpack_premultiply_alpha_ = param != 0;
      return;

    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      if (static_cast<GLenum>(param) != GC3D_BROWSER_DEFAULT_WEBGL &&
          param != GL_NONE) {
        InsertGLError(
            GL_INVALID_VALUE, "pixelStorei",
            "invalid parameter for UNPACK_COLORSPACE_CONVERSION_WEBGL");
        return;
      }
      unpack_colorspace_conversion_ = static_cast<GLenum>(param);
      return;

    // Standard parameters. ANGLE keeps their state.
    default:
      GLFN(driver_, glPixelStorei)(pname, param);
      return;
  }
}

void WebGLRenderingContextWebGPUBase::polygonOffset(GLfloat factor,
                                                    GLfloat units) {
  GLFN(driver_, glPolygonOffset)(factor, units);
}

void WebGLRenderingContextWebGPUBase::readPixels(
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  // Forward to the WebGL2 readPixels function that takes an offset. The WebGL2
  // readPixels doesn't validate that we are a WebGL2 context as that's done at
  // the type level in the WebGL IDL: the readPixels with offset can only be
  // called if a successful `getContext("webgl2")` happened.
  readPixels(x, y, width, height, format, type, pixels, 0);
}

void WebGLRenderingContextWebGPUBase::renderbufferStorage(GLenum target,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::sampleCoverage(GLfloat value,
                                                     GLboolean invert) {
  GLFN(driver_, glSampleCoverage)(value, invert);
}

void WebGLRenderingContextWebGPUBase::scissor(GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height) {
  GLFN(driver_, glScissor)(x, y, width, height);
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
    ascii_source.push_back(IsAscii(code_point) ? code_point : '?');
  }

  GLint c_ascii_size = base::checked_cast<GLint>(ascii_source.size());
  const char* c_ascii_source = ascii_source.data();
  RETURN_IF_GL_ERROR(GLFN(driver_, glShaderSource)(
      shader->Object(), 1, &c_ascii_source, &c_ascii_size));

  shader->SetSource(source);
}

void WebGLRenderingContextWebGPUBase::stencilFunc(GLenum func,
                                                  GLint ref,
                                                  GLuint mask) {
  GLFN(driver_, glStencilFunc)(func, ref, mask);
}

void WebGLRenderingContextWebGPUBase::stencilFuncSeparate(GLenum face,
                                                          GLenum func,
                                                          GLint ref,
                                                          GLuint mask) {
  GLFN(driver_, glStencilFuncSeparate)(face, func, ref, mask);
}

void WebGLRenderingContextWebGPUBase::stencilMask(GLuint mask) {
  GLFN(driver_, glStencilMask)(mask);
}

void WebGLRenderingContextWebGPUBase::stencilMaskSeparate(GLenum face,
                                                          GLuint mask) {
  GLFN(driver_, glStencilMaskSeparate)(face, mask);
}

void WebGLRenderingContextWebGPUBase::stencilOp(GLenum fail,
                                                GLenum zfail,
                                                GLenum zpass) {
  GLFN(driver_, glStencilOp)(fail, zfail, zpass);
}

void WebGLRenderingContextWebGPUBase::stencilOpSeparate(GLenum face,
                                                        GLenum fail,
                                                        GLenum zfail,
                                                        GLenum zpass) {
  GLFN(driver_, glStencilOpSeparate)(face, fail, zfail, zpass);
}

void WebGLRenderingContextWebGPUBase::texParameterf(GLenum target,
                                                    GLenum pname,
                                                    GLfloat param) {
  GLFN(driver_, glTexParameterf)(target, pname, param);
}

void WebGLRenderingContextWebGPUBase::texParameteri(GLenum target,
                                                    GLenum pname,
                                                    GLint param) {
  GLFN(driver_, glTexParameteri)(target, pname, param);
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

  GLFN(driver_, glTexImage2DRobustANGLE)(target, level, internalformat, width,
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

  GLFN(driver_, glTexSubImage2DRobustANGLE)(target, level, xoffset, yoffset,
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
  GLFN(driver_, glUniform1f)(location->Location(), x);
}

void WebGLRenderingContextWebGPUBase::uniform1fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniform1fv", location, 1, v.size())) {
    return;
  }
  GLFN(driver_, glUniform1fv)(location->Location(), static_cast<int>(v.size()),
                              v.data());
}

void WebGLRenderingContextWebGPUBase::uniform1i(
    const WebGLUniformLocation* location,
    GLint x) {
  if (!ValidateUniformLocation("uniform1i", location)) {
    return;
  }
  GLFN(driver_, glUniform1i)(location->Location(), x);
}

void WebGLRenderingContextWebGPUBase::uniform1iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v) {
  if (!ValidateUniformV("uniform1iv", location, 1, v.size())) {
    return;
  }
  GLFN(driver_, glUniform1iv)(location->Location(), static_cast<int>(v.size()),
                              v.data());
}

void WebGLRenderingContextWebGPUBase::uniform2f(
    const WebGLUniformLocation* location,
    GLfloat x,
    GLfloat y) {
  if (!ValidateUniformLocation("uniform2f", location)) {
    return;
  }
  GLFN(driver_, glUniform2f)(location->Location(), x, y);
}

void WebGLRenderingContextWebGPUBase::uniform2fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniform2fv", location, 2, v.size())) {
    return;
  }
  GLFN(driver_, glUniform2fv)(location->Location(),
                              static_cast<int>(v.size() / 2), v.data());
}

void WebGLRenderingContextWebGPUBase::uniform2i(
    const WebGLUniformLocation* location,
    GLint x,
    GLint y) {
  if (!ValidateUniformLocation("uniform2i", location)) {
    return;
  }
  GLFN(driver_, glUniform2i)(location->Location(), x, y);
}

void WebGLRenderingContextWebGPUBase::uniform2iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v) {
  if (!ValidateUniformV("uniform2iv", location, 2, v.size())) {
    return;
  }
  GLFN(driver_, glUniform2iv)(location->Location(),
                              static_cast<int>(v.size() / 2), v.data());
}

void WebGLRenderingContextWebGPUBase::uniform3f(
    const WebGLUniformLocation* location,
    GLfloat x,
    GLfloat y,
    GLfloat z) {
  if (!ValidateUniformLocation("uniform3f", location)) {
    return;
  }
  GLFN(driver_, glUniform3f)(location->Location(), x, y, z);
}

void WebGLRenderingContextWebGPUBase::uniform3fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniform3fv", location, 3, v.size())) {
    return;
  }
  GLFN(driver_, glUniform3fv)(location->Location(),
                              static_cast<int>(v.size() / 3), v.data());
}

void WebGLRenderingContextWebGPUBase::uniform3i(
    const WebGLUniformLocation* location,
    GLint x,
    GLint y,
    GLint z) {
  if (!ValidateUniformLocation("uniform3i", location)) {
    return;
  }
  GLFN(driver_, glUniform3i)(location->Location(), x, y, z);
}

void WebGLRenderingContextWebGPUBase::uniform3iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v) {
  if (!ValidateUniformV("uniform3iv", location, 3, v.size())) {
    return;
  }
  GLFN(driver_, glUniform3iv)(location->Location(),
                              static_cast<int>(v.size() / 3), v.data());
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
  GLFN(driver_, glUniform4f)(location->Location(), x, y, z, w);
}

void WebGLRenderingContextWebGPUBase::uniform4fv(
    const WebGLUniformLocation* location,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniform4fv", location, 4, v.size())) {
    return;
  }
  GLFN(driver_, glUniform4fv)(location->Location(),
                              static_cast<int>(v.size() / 4), v.data());
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
  GLFN(driver_, glUniform4i)(location->Location(), x, y, z, w);
}

void WebGLRenderingContextWebGPUBase::uniform4iv(
    const WebGLUniformLocation* location,
    base::span<const GLint> v) {
  if (!ValidateUniformV("uniform4iv", location, 4, v.size())) {
    return;
  }
  GLFN(driver_, glUniform4iv)(location->Location(),
                              static_cast<int>(v.size() / 4), v.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniformMatrix2fv", location, 4, v.size())) {
    return;
  }
  GLFN(driver_, glUniformMatrix2fv)(location->Location(),
                                    static_cast<int>(v.size() / 4), transpose,
                                    v.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniformMatrix3fv", location, 9, v.size())) {
    return;
  }
  GLFN(driver_, glUniformMatrix3fv)(location->Location(),
                                    static_cast<int>(v.size() / 9), transpose,
                                    v.data());
}

void WebGLRenderingContextWebGPUBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v) {
  if (!ValidateUniformV("uniformMatrix4fv", location, 16, v.size())) {
    return;
  }
  GLFN(driver_, glUniformMatrix4fv)(location->Location(),
                                    static_cast<int>(v.size() / 16), transpose,
                                    v.data());
}

void WebGLRenderingContextWebGPUBase::useProgram(WebGLProgram* program) {
  if (!ValidateNullableObject("useProgram", program)) {
    return;
  }

  RETURN_IF_GL_ERROR(GLFN(driver_, glUseProgram)(ObjectOrZero(program)));

  program_binding_ = program;
}

void WebGLRenderingContextWebGPUBase::validateProgram(WebGLProgram* program) {
  if (!ValidateProgramOrShader("useProgram", program)) {
    return;
  }

  GLFN(driver_, glValidateProgram)(program->Object());
}

void WebGLRenderingContextWebGPUBase::vertexAttrib1f(GLuint index, GLfloat x) {
  GLFN(driver_, glVertexAttrib1f)(index, x);
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttrib1fv(
    GLuint index,
    base::span<const GLfloat> values) {
  // WebGL allows larger spans (reusing an array for different sizes to
  // avoid repeated allocations). Extra elements are ignored.
  if (values.size() < 1) {
    InsertGLError(GL_INVALID_VALUE, "vertexAttrib1fv", "invalid size");
    return;
  }
  GLFN(driver_, glVertexAttrib1fv)(index, values.data());
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttrib2f(GLuint index,
                                                     GLfloat x,
                                                     GLfloat y) {
  GLFN(driver_, glVertexAttrib2f)(index, x, y);
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttrib2fv(
    GLuint index,
    base::span<const GLfloat> values) {
  // WebGL allows larger spans (reusing an array for different sizes to
  // avoid repeated allocations). Extra elements are ignored.
  if (values.size() < 2) {
    InsertGLError(GL_INVALID_VALUE, "vertexAttrib2fv", "invalid size");
    return;
  }
  GLFN(driver_, glVertexAttrib2fv)(index, values.data());
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttrib3f(GLuint index,
                                                     GLfloat x,
                                                     GLfloat y,
                                                     GLfloat z) {
  GLFN(driver_, glVertexAttrib3f)(index, x, y, z);
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttrib3fv(
    GLuint index,
    base::span<const GLfloat> values) {
  // WebGL allows larger spans (reusing an array for different sizes to
  // avoid repeated allocations). Extra elements are ignored.
  if (values.size() < 3) {
    InsertGLError(GL_INVALID_VALUE, "vertexAttrib3fv", "invalid size");
    return;
  }
  GLFN(driver_, glVertexAttrib3fv)(index, values.data());
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttrib4f(GLuint index,
                                                     GLfloat x,
                                                     GLfloat y,
                                                     GLfloat z,
                                                     GLfloat w) {
  GLFN(driver_, glVertexAttrib4f)(index, x, y, z, w);
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttrib4fv(
    GLuint index,
    base::span<const GLfloat> values) {
  // WebGL allows larger spans (reusing an array for different sizes to
  // avoid repeated allocations). Extra elements are ignored.
  if (values.size() < 4) {
    InsertGLError(GL_INVALID_VALUE, "vertexAttrib4fv", "invalid size");
    return;
  }
  GLFN(driver_, glVertexAttrib4fv)(index, values.data());
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
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

  GLFN(driver_, glVertexAttribPointer)(
      index, size, type, normalized, stride,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));

  // TODO(413078308): Update the tracked buffer in the current VAO if there was
  // no GL error.
}

void WebGLRenderingContextWebGPUBase::viewport(GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height) {
  GLFN(driver_, glViewport)(x, y, width, height);
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

void WebGLRenderingContextWebGPUBase::texElementSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    const V8UnionElementOrElementImage* element,
    const WebGLCopyElementImageConfig* config,
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
  GLFN(driver_, glUniform1fv)(location->Location(),
                              static_cast<int>(data.size()), data.data());
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
  GLFN(driver_, glUniform2fv)(location->Location(),
                              static_cast<int>(data.size() / 2), data.data());
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
  GLFN(driver_, glUniform3fv)(location->Location(),
                              static_cast<int>(data.size() / 3), data.data());
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
  GLFN(driver_, glUniform4fv)(location->Location(),
                              static_cast<int>(data.size() / 4), data.data());
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
  GLFN(driver_, glUniform1iv)(location->Location(),
                              static_cast<int>(data.size()), data.data());
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
  GLFN(driver_, glUniform2iv)(location->Location(),
                              static_cast<int>(data.size() / 2), data.data());
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
  GLFN(driver_, glUniform3iv)(location->Location(),
                              static_cast<int>(data.size() / 3), data.data());
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
  GLFN(driver_, glUniform4iv)(location->Location(),
                              static_cast<int>(data.size() / 4), data.data());
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
  GLFN(driver_, glUniform1uiv)(location->Location(),
                               static_cast<int>(data.size()), data.data());
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
  GLFN(driver_, glUniform2uiv)(location->Location(),
                               static_cast<int>(data.size() / 2), data.data());
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
  GLFN(driver_, glUniform3uiv)(location->Location(),
                               static_cast<int>(data.size() / 3), data.data());
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
  GLFN(driver_, glUniform4uiv)(location->Location(),
                               static_cast<int>(data.size() / 4), data.data());
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
  GLFN(driver_, glUniformMatrix2fv)(location->Location(),
                                    static_cast<int>(data.size() / 4),
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
  GLFN(driver_, glUniformMatrix3fv)(location->Location(),
                                    static_cast<int>(data.size() / 9),
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
  GLFN(driver_, glUniformMatrix4fv)(location->Location(),
                                    static_cast<int>(data.size() / 16),
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
  GLFN(driver_, glUniformMatrix2x3fv)(location->Location(),
                                      static_cast<int>(data.size() / 6),
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
  GLFN(driver_, glUniformMatrix3x2fv)(location->Location(),
                                      static_cast<int>(data.size() / 6),
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
  GLFN(driver_, glUniformMatrix2x4fv)(location->Location(),
                                      static_cast<int>(data.size() / 8),
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
  GLFN(driver_, glUniformMatrix4x2fv)(location->Location(),
                                      static_cast<int>(data.size() / 8),
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
  GLFN(driver_, glUniformMatrix3x4fv)(location->Location(),
                                      static_cast<int>(data.size() / 12),
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
  GLFN(driver_, glUniformMatrix4x3fv)(location->Location(),
                                      static_cast<int>(data.size() / 12),
                                      transpose, data.data());
}

void WebGLRenderingContextWebGPUBase::vertexAttribI4i(GLuint index,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z,
                                                      GLint w) {
  GLFN(driver_, glVertexAttribI4i)(index, x, y, z, w);
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttribI4iv(
    GLuint index,
    base::span<const GLint> v) {
  // WebGL allows larger spans (reusing an array for different sizes to
  // avoid repeated allocations). Extra elements are ignored.
  if (v.size() < 4) {
    InsertGLError(GL_INVALID_VALUE, "vertexAttribI4iv", "invalid size");
    return;
  }
  GLFN(driver_, glVertexAttribI4iv)(index, v.data());
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttribI4ui(GLuint index,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z,
                                                       GLuint w) {
  GLFN(driver_, glVertexAttribI4ui)(index, x, y, z, w);
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
}

void WebGLRenderingContextWebGPUBase::vertexAttribI4uiv(
    GLuint index,
    base::span<const GLuint> v) {
  // WebGL allows larger spans (reusing an array for different sizes to
  // avoid repeated allocations). Extra elements are ignored.
  if (v.size() < 4) {
    InsertGLError(GL_INVALID_VALUE, "vertexAttribI4uiv", "invalid size");
    return;
  }
  GLFN(driver_, glVertexAttribI4uiv)(index, v.data());
  // TODO(crbug.com/413078308): Track the type of the array so the correct
  // typed array is returned for getVertexAttrib(index, CURRENT_VERTEX_ATTRIB).
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
  GLFN(driver_, glVertexAttribDivisorANGLE)(index, divisor);
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
  return IsObject(this, query, GLFN(driver_, glIsQuery));
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
  return IsObject(this, sampler, GLFN(driver_, glIsSampler));
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
  // WebGLSync is managed entirely on the client side and does not require an
  // ANGLE query.
  return ValidateIsObject(this, sync) && sync->Object() != 0;
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
  return IsObject(this, feedback, GLFN(driver_, glIsTransformFeedback));
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
  return IsObject(this, vertex_array, GLFN(driver_, glIsVertexArrayOES));
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
  if (!ValidateFitsNonNegInt32("readPixels", "offset", offset)) {
    return;
  }

  // WebGL separates the entrypoints for readPixels back to CPU or to a
  // PIXEL_PACK buffer, so there is validation that the correct entrypoint is
  // used depending on whether the PIXEL_PACK buffer is present.
  if (!pixel_pack_buffer_binding_) {
    InsertGLError(GL_INVALID_OPERATION, "readPixels",
                  "no PIXEL_PACK buffer bound");
    return;
  }

  EnsureDefaultFramebuffer();
  GLFN(driver_, glReadPixels)(x, y, width, height, format, type,
                              reinterpret_cast<void*>(offset));
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
  // Due to WebGL's same-origin restrictions, it is not possible to taint the
  // origin using the WebGL API.
  DCHECK(Host()->OriginClean());

  // WebGL separates the entrypoints for readPixels back to CPU or to a
  // PIXEL_PACK buffer, so there is validation that the correct entrypoint is
  // used depending on whether the PIXEL_PACK buffer is present.
  if (pixel_pack_buffer_binding_) {
    InsertGLError(GL_INVALID_OPERATION, "readPixels",
                  "PIXEL_PACK buffer should not be bound");
    return;
  }

  // Validation specific to WebGL because it uses a DOMArrayBufferView instead
  // of a void* like in OpenGL ES.
  if (pixels.IsNull()) {
    InsertGLError(GL_INVALID_VALUE, "readPixels",
                  "no destination ArrayBufferView");
    return;
  }
  if (offset > int64_t(pixels->byteLength() / pixels->TypeSize())) {
    InsertGLError(GL_INVALID_VALUE, "readPixels",
                  "destination offset out of range");
    return;
  }
  size_t byte_offset = size_t(offset * pixels->TypeSize());
  base::span<uint8_t> data_at_offset = pixels->ByteSpan().subspan(byte_offset);

  // Validation specific to WebGL that the type of the DOMArrayBufferView
  // matches the type used to read back data.
  DOMArrayBufferView::ViewType pixels_type = pixels->GetType();
  switch (type) {
    case GL_UNSIGNED_BYTE:
      if (pixels_type != DOMArrayBufferView::kTypeUint8 &&
          pixels_type != DOMArrayBufferView::kTypeUint8Clamped) {
        InsertGLError(
            GL_INVALID_OPERATION, "readPixels",
            "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array or "
            "Uint8ClampedArray");
        return;
      }
      break;
    case GL_BYTE:
      if (pixels_type != DOMArrayBufferView::kTypeInt8) {
        InsertGLError(GL_INVALID_OPERATION, "readPixels",
                      "type BYTE but ArrayBufferView not Int8Array");
        return;
      }
      break;
    case GL_HALF_FLOAT:
      if (pixels_type != DOMArrayBufferView::kTypeUint16) {
        InsertGLError(GL_INVALID_OPERATION, "readPixels",
                      "type HALF_FLOAT but ArrayBufferView not Uint16Array");
        return;
      }
      break;
    case GL_FLOAT:
      if (pixels_type != DOMArrayBufferView::kTypeFloat32) {
        InsertGLError(GL_INVALID_OPERATION, "readPixels",
                      "type FLOAT but ArrayBufferView not Float32Array");
        return;
      }
      break;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      if (pixels_type != DOMArrayBufferView::kTypeUint16) {
        InsertGLError(
            GL_INVALID_OPERATION, "readPixels",
            "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
        return;
      }
      break;
    case GL_UNSIGNED_SHORT:
      if (pixels_type != DOMArrayBufferView::kTypeUint16) {
        InsertGLError(
            GL_INVALID_OPERATION, "readPixels",
            "type GL_UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
        return;
      }
      break;
    case GL_SHORT:
      if (pixels_type != DOMArrayBufferView::kTypeInt16) {
        InsertGLError(GL_INVALID_OPERATION, "readPixels",
                      "type SHORT but ArrayBufferView not Int16Array");
        return;
      }
      break;
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
      if (pixels_type != DOMArrayBufferView::kTypeUint32) {
        InsertGLError(GL_INVALID_OPERATION, "readPixels",
                      "type UNSIGNED_INT but ArrayBufferView not Uint32Array");
        return;
      }
      break;
    case GL_INT:
      if (pixels_type != DOMArrayBufferView::kTypeInt32) {
        InsertGLError(GL_INVALID_OPERATION, "readPixels",
                      "type INT but ArrayBufferView not Int32Array");
        return;
      }
      break;
    default:
      InsertGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return;
  }

  EnsureDefaultFramebuffer();

  // Use ReadPixelsRobustANGLE that will check that the bytes written don't go
  // past the end of the DOMArrayBufferView. We also need to ensure the size
  // fits in a GLsizei (the type used for the bufSize parameter) and doesn't go
  // past kMaximumSupportedArrayBufferSize (see comment for that constant).
  constexpr size_t kMaxBufSize =
      std::min(size_t(std::numeric_limits<GLsizei>::max()),
               kMaximumSupportedArrayBufferSize);
  size_t bufSizeSizeT = std::min(data_at_offset.size(), kMaxBufSize);
  // Safe with the min() above.
  GLsizei bufSize = static_cast<GLsizei>(bufSizeSizeT);

  GLFN(driver_, glReadPixelsRobustANGLE)(x, y, width, height, format, type,
                                         bufSize, nullptr, nullptr, nullptr,
                                         data_at_offset.data());
}

// **************************************************************************
// End of WebGL2RenderingContextBase's IDL methods
// **************************************************************************

// ****************************************************************************
// Start of CanvasRenderingContext implementation
// ****************************************************************************
bool WebGLRenderingContextWebGPUBase::IsOpaque() const {
  return !CreationAttributes().alpha;
}

base::ByteSize WebGLRenderingContextWebGPUBase::AllocatedBufferSize() const {
  base::ByteSize result;
  if (swap_buffers_) {
    result += swap_buffers_->EstimatedSizeInBytes();
  }
  return result;
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

void WebGLRenderingContextWebGPUBase::Reshape(gfx::Size old_size,
                                              gfx::Size new_size) {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::Stop() {
  NOTIMPLEMENTED();
}

void WebGLRenderingContextWebGPUBase::FinalizeFrame(FlushReason) {
  NOTIMPLEMENTED();
}

scoped_refptr<ExportedCanvasResource>
WebGLRenderingContextWebGPUBase::GetResourceForPushFrame(
    bool& should_call_push_frame) {
  should_call_push_frame = false;
  NOTIMPLEMENTED();
  return nullptr;
}

// ****************************************************************************
// End of CanvasRenderingContext implementation
// ****************************************************************************

void WebGLRenderingContextWebGPUBase::OnTextureTransferred() {
  GLFN(driver_, glFlush)();

  {
    ScopedBindFramebuffer bind_default_fbo(
        driver_, supports_separate_framebuffer_targets_, GL_FRAMEBUFFER,
        default_framebuffer_);
    GLFN(driver_, glFramebufferTexture2DEXT)(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
  }
  GLFN(driver_, glDeleteTextures)(1, &default_framebuffer_color_texture_);
  default_framebuffer_color_texture_ = 0;
  EGLFN(driver_, eglDestroyImageKHR)(driver_.display,
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
  visitor->Trace(copy_read_buffer_binding_);
  visitor->Trace(copy_write_buffer_binding_);
  visitor->Trace(pixel_pack_buffer_binding_);
  visitor->Trace(pixel_unpack_buffer_binding_);
  visitor->Trace(transform_feedback_buffer_binding_);
  visitor->Trace(uniform_buffer_binding_);
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
  WebGPUSwapBufferProvider::Client::Trace(visitor);
}

void WebGLRenderingContextWebGPUBase::OnDebugMessage(GLenum source,
                                                     GLenum type,
                                                     GLuint id,
                                                     GLenum severity,
                                                     GLsizei length,
                                                     const GLchar* message) {
  // SAFETY: GLDebugMessageCallback provides a pointer and length that are
  // guaranteed to be valid for the duration of the callback.
  base::span<const char> message_span =
      UNSAFE_BUFFERS(base::span(message, base::checked_cast<size_t>(length)));
  String message_str =
      String::FromUtf8WithLatin1Fallback(base::as_byte_span(message_span));

  if (type == GL_DEBUG_TYPE_ERROR && source == GL_DEBUG_SOURCE_API) {
    had_error_callback_ = true;
    PrintGLErrorToConsole(
        blink::Format("WebGL: {}: {}", GetErrorString(id), message_str));
  } else {
    PrintWarningToConsole(blink::Format(
        "WebGL: ({}, {}, {}, {}): {}", gl::GetDebugSourceString(source),
        gl::GetDebugTypeString(type), gl::GetDebugSeverityString(severity), id,
        message_str));
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

  SkAlphaType alpha_type =
      CreationAttributes().alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType;

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      swap_buffers_->GetNewTexture(texDesc, alpha_type);

  Host()->UpdateMemoryUsage();
  mailbox_texture->SetNeedsPresent(true);

  current_swap_buffer_ = mailbox_texture->GetTexture();

  // Create an EGL image of the swap buffer texture.
  const auto image_attribs = std::to_array<EGLint>({
      EGL_NONE,
  });
  default_framebuffer_color_image_ = EGLFN(driver_, eglCreateImageKHR)(
      driver_.display, EGL_NO_CONTEXT, EGL_WEBGPU_TEXTURE_ANGLE,
      current_swap_buffer_.Get(), image_attribs.data());
  CHECK(default_framebuffer_color_image_);

  // Import the EGL image to a GL texture and bind it to the default framebuffer
  // as color attachment 0.
  GLFN(driver_, glGenTextures)(1, &default_framebuffer_color_texture_);
  {
    ScopedBindTexture bind_color_texture(driver_, GL_TEXTURE_2D,
                                         default_framebuffer_color_texture_);
    GLFN(driver_, glEGLImageTargetTexture2DOES)(
        GL_TEXTURE_2D, default_framebuffer_color_image_);

    {
      ScopedBindFramebuffer bind_default_fbo(
          driver_, supports_separate_framebuffer_targets_, GL_FRAMEBUFFER,
          default_framebuffer_);
      GLFN(driver_, glFramebufferTexture2DEXT)(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
          default_framebuffer_color_texture_, 0);
    }
  }

  // TODO(413078308): Don't create a depth stencil when the user did not request
  // it. Also choose a format based on the request.
  if (!default_framebuffer_depth_stencil_renderbuffer_ ||
      framebuffer_size != default_framebuffer_size_) {
    GLFN(driver_, glDeleteRenderbuffersEXT)(
        1, &default_framebuffer_depth_stencil_renderbuffer_);
    GLFN(driver_, glGenRenderbuffersEXT)(
        1, &default_framebuffer_depth_stencil_renderbuffer_);

    {
      ScopedBindRenderbuffer bind_renderbuffer(
          driver_, default_framebuffer_depth_stencil_renderbuffer_);
      GLFN(driver_, glRenderbufferStorageEXT)(
          GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
          std::max(1, framebuffer_size.width()),
          std::max(1, framebuffer_size.height()));
    }

    {
      ScopedBindFramebuffer bind_default_fbo(
          driver_, supports_separate_framebuffer_targets_, GL_FRAMEBUFFER,
          default_framebuffer_);
      GLFN(driver_, glFramebufferRenderbufferEXT)(
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
  driver_.driver_egl.InitializeStaticBindings(get_proc_address);

  InitializeEGLDebugLogging(driver_, ui::LogEGLDebugMessage);

  // Initialize the EGL display using the device and the dawn wire client proc
  // table.
  const auto display_attribs = std::to_array<EGLAttrib>({
      EGL_PLATFORM_ANGLE_TYPE_ANGLE,
      EGL_PLATFORM_ANGLE_TYPE_WEBGPU_ANGLE,
      EGL_PLATFORM_ANGLE_DAWN_PROC_TABLE_ANGLE,
      proxy_instance_->GetProcTableForANGLE(),
      EGL_PLATFORM_ANGLE_WEBGPU_INSTANCE_ANGLE,
      proxy_instance_->GetInstanceForANGLE(),
      // Use a different EGLDisplay per context. ANGLE caches displays by their
      // attributes, but ignores the Dawn proc table and instance above. A
      // second context would otherwise get the first context's display, and
      // its wgpu::Device. That device belongs to the first context's Dawn wire
      // client, and can't be used with ours.
      // ANGLE never frees displays: a later context at the same address reuses
      // a terminated one. That's fine, it is initialized again.
      // TODO(413078308): Reuse the keys to avoid leaking a display per context.
      EGL_PLATFORM_ANGLE_DISPLAY_KEY_ANGLE,
      reinterpret_cast<EGLAttrib>(this),
      EGL_NONE,
  });
  driver_.display = EGLFN(driver_, eglGetPlatformDisplay)(
      EGL_PLATFORM_ANGLE_ANGLE, nullptr, display_attribs.data());
  CHECK(driver_.display != EGL_NO_DISPLAY);

  EGLint egl_version_major, egl_version_minor;
  EGLFN(driver_, eglInitialize)(driver_.display, &egl_version_major,
                                &egl_version_minor);

  // Setup the ANGLE platform for internal logging and trace events
  angle::InitializePlatform(driver_.display, get_proc_address);

  // Query the wgpu::Device that was created by ANGLE.
  EGLAttrib eglDevice = 0;
  EGLFN(driver_, eglQueryDisplayAttribEXT)(driver_.display, EGL_DEVICE_EXT,
                                           &eglDevice);
  CHECK_NE(0, eglDevice);

  EGLAttrib wgpuDevice = 0;
  EGLFN(driver_, eglQueryDeviceAttribEXT)(
      reinterpret_cast<EGLDeviceEXT>(eglDevice), EGL_WEBGPU_DEVICE_ANGLE,
      &wgpuDevice);
  CHECK_NE(0, wgpuDevice);
  device_ = wgpu::Device::Acquire(reinterpret_cast<WGPUDevice>(wgpuDevice));

  // Create a GL Context.
  // TODO(413078308): Request a WebGL compatibility context when requesting
  // extensions is supported both for basic functionality and WebGL extensions.
  const auto context_attribs = std::to_array<EGLint>({
      // WebGL 1.0 is based on OpenGL ES 2.0, WebGL 2.0 on OpenGL ES 3.0.
      // ANGLE then rejects the ES3-only enums on WebGL 1.0 contexts.
      EGL_CONTEXT_MAJOR_VERSION,
      IsWebGL2() ? 3 : 2,
      EGL_CONTEXT_MINOR_VERSION,
      0,
      EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE,
      EGL_FALSE,
      EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE,
      EGL_FALSE,
      EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE,
      EGL_FALSE,
      EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM,
      EGL_FALSE,
      EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE,
      EGL_TRUE,
      EGL_NONE,
  });
  driver_.context = EGLFN(driver_, eglCreateContext)(
      driver_.display, EGL_NO_CONFIG_KHR, nullptr, context_attribs.data());
  CHECK(driver_.context != EGL_NO_CONTEXT);

  EGLFN(driver_, eglMakeCurrent)(driver_.display, EGL_NO_SURFACE,
                                 EGL_NO_SURFACE, driver_.context);

  // With the context created, we can load the GL entry points
  driver_.driver_gl.InitializeStaticBindings(get_proc_address);

  // Explicitly call GL functions instead of using GLFN. GLFN calls the ANGLE
  // explicit context entry points which will not be loaded until
  // InitializeDynamicBindings below.
  const char* extensions_string = reinterpret_cast<const char*>(
      driver_.driver_gl.fn.glGetStringFn(GL_EXTENSIONS));
  gfx::ExtensionSet extensions = gfx::MakeExtensionSet(extensions_string);

  gl::GLVersionInfo version(
      reinterpret_cast<const char*>(
          driver_.driver_gl.fn.glGetStringFn(GL_VERSION)),
      reinterpret_cast<const char*>(
          driver_.driver_gl.fn.glGetStringFn(GL_RENDERER)),
      extensions);

  // Load the extension entry points based on the version and extension string.
  // TODO(413078308): Store the enabled extensions, update them after requesting
  // new extensions.
  driver_.driver_gl.InitializeDynamicBindings(get_proc_address, &version,
                                              extensions);
  LOG(ERROR) << "GL context extensions: " << extensions_string;

  // Just log all GL debug messages.
  // TODO(413078308): Forward debug messages to the JS console and use the
  // callback for determining when GL calls generate errors without having to
  // check glGetError
  InitializeGLDebugLogging(
      driver_, true, WebGLRenderingContextWebGPUBaseDebugMessageCallback, this);

  // Initialize the GLES2 interface used for WebGL objects that just proxies to
  // the gl::DriverGL.
  gles2_for_objects_ = std::make_unique<PartialGLES2ForObjects>(&driver_);
  WebGLContextObjectSupport::OnContextRestored(gles2_for_objects_.get());

  // The separate read and draw framebuffer bindings require ES3 or framebuffer
  // blit extensions
  supports_separate_framebuffer_targets_ =
      driver_.driver_gl.ext.b_GL_ANGLE_framebuffer_blit ||
      version.IsAtLeastGLES(3, 0);

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
  GLFN(driver_, glGenFramebuffersEXT)(1, &default_framebuffer_);
  GLFN(driver_, glBindFramebufferEXT)(GL_FRAMEBUFFER, default_framebuffer_);

  // Initialize the default fixed-function state.
  GLFN(driver_, glViewport)(0, 0, Host()->Size().width(),
                            Host()->Size().height());
  GLFN(driver_, glScissor)(0, 0, Host()->Size().width(),
                           Host()->Size().height());
}

void WebGLRenderingContextWebGPUBase::Destroy() {
  if (swap_buffers_) {
    swap_buffers_->Neuter();
  }

  if (driver_.context) {
    DCHECK(driver_.display != EGL_NO_DISPLAY);
    EGLFN(driver_, eglMakeCurrent)(driver_.display, EGL_NO_SURFACE,
                                   EGL_NO_SURFACE, EGL_NO_CONTEXT);
    EGLFN(driver_, eglDestroyContext)(driver_.display, driver_.context);
    driver_.context = EGL_NO_CONTEXT;
  }
  driver_.driver_gl.ClearBindings();

  // WebGLContextObjectSupport depends on `gles2_for_objects_`.
  if (!IsLost()) {
    WebGLContextObjectSupport::OnContextLost();
  }
  gles2_for_objects_ = nullptr;

  if (driver_.display) {
    angle::ResetPlatform(driver_.display,
                         driver_.driver_egl.fn.eglGetProcAddressFn);
    EGLFN(driver_, eglTerminate)(driver_.display);
    driver_.display = EGL_NO_DISPLAY;
  }
  driver_.driver_egl.ClearBindings();
  proxy_instance_ = nullptr;
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
  // Programs and shaders stay usable after deleteProgram/deleteShader while
  // they are still in use (attached or current). Only reject them once the GL
  // object is really gone.
  if (!object->HasObject()) {
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
  GLenum error = GLFN(driver_, glGetError)();
  while (error != GL_NO_ERROR) {
    if (!errors_.Contains(error)) {
      errors_.push_back(error);
    }

    // Check for context loss on out-of-memory errors
    if (error == GL_OUT_OF_MEMORY || error == GL_CONTEXT_LOST) {
      // TODO(6596816): Handle context loss and OOM
      break;
    }

    error = GLFN(driver_, glGetError)();
  }
}

void WebGLRenderingContextWebGPUBase::InsertGLError(GLenum error,
                                                    const char* function_name,
                                                    const char* description) {
  if (!errors_.Contains(error)) {
    errors_.push_back(error);
  }

  String error_type(GetErrorString(error));
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

String WebGLRenderingContextWebGPUBase::GetGLString(GLenum name) const {
  return String(
      reinterpret_cast<const char*>(GLFN(driver_, glGetString)(name)));
}

ScriptValue WebGLRenderingContextWebGPUBase::GetBooleanParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLboolean value = 0;
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glGetBooleanvRobustANGLE)(pname, 1, nullptr, &value),
      ScriptValue::CreateNull(script_state->GetIsolate()));
  return WebGLAny(script_state, static_cast<bool>(value));
}

ScriptValue WebGLRenderingContextWebGPUBase::GetFloatParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLfloat value = 0.0f;
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glGetFloatvRobustANGLE)(pname, 1, nullptr, &value),
      ScriptValue::CreateNull(script_state->GetIsolate()));
  return WebGLAny(script_state, value);
}

ScriptValue WebGLRenderingContextWebGPUBase::GetIntParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint value = 0;
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glGetIntegervRobustANGLE)(pname, 1, nullptr, &value),
      ScriptValue::CreateNull(script_state->GetIsolate()));
  return WebGLAny(script_state, value);
}

ScriptValue WebGLRenderingContextWebGPUBase::GetInt64Parameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint64 value = 0;
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glGetInteger64vRobustANGLE)(pname, 1, nullptr, &value),
      ScriptValue::CreateNull(script_state->GetIsolate()));
  return WebGLAny(script_state, value);
}

ScriptValue WebGLRenderingContextWebGPUBase::GetUnsignedIntParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint value = 0;
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glGetIntegervRobustANGLE)(pname, 1, nullptr, &value),
      ScriptValue::CreateNull(script_state->GetIsolate()));
  return WebGLAny(script_state, static_cast<GLuint>(value));
}

template <GLsizei N>
ScriptValue WebGLRenderingContextWebGPUBase::GetBooleanArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  std::array<GLboolean, N> values = {};
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glGetBooleanvRobustANGLE)(pname, N, nullptr, values.data()),
      ScriptValue::CreateNull(script_state->GetIsolate()));
  std::array<bool, N> bool_values;
  std::ranges::transform(values, bool_values.begin(),
                         [](GLboolean v) { return static_cast<bool>(v); });
  return WebGLAny(script_state, bool_values);
}

template <GLsizei N>
ScriptValue WebGLRenderingContextWebGPUBase::GetFloatArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  std::array<GLfloat, N> values = {};
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glGetFloatvRobustANGLE)(pname, N, nullptr, values.data()),
      ScriptValue::CreateNull(script_state->GetIsolate()));
  return WebGLAny(script_state, DOMFloat32Array::Create(values));
}

template <GLsizei N>
ScriptValue WebGLRenderingContextWebGPUBase::GetIntArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  std::array<GLint, N> values = {};
  RETURN_IF_GL_ERROR(
      GLFN(driver_, glGetIntegervRobustANGLE)(pname, N, nullptr, values.data()),
      ScriptValue::CreateNull(script_state->GetIsolate()));
  return WebGLAny(script_state, DOMInt32Array::Create(values));
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
