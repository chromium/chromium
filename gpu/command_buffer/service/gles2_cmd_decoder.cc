// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/heap_array.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/legacy_hash.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/gles2_cmd_copy_texture_chromium_utils.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_clear_framebuffer.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_tex_image.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/gles2_external_framebuffer.h"
#include "gpu/command_buffer/service/gles2_query_manager.h"
#include "gpu/command_buffer/service/gpu_fence_manager.h"
#include "gpu/command_buffer/service/gpu_state_tracer.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/multi_draw_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/sampler_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/shader_translator.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/transform_feedback_manager.h"
#include "gpu/command_buffer/service/vertex_array_manager.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/gpu_switching_observer.h"
#include "ui/gl/gpu_timing.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/scoped_make_current.h"

// Note: this undefs far and near so include this after other Windows headers.
#include "third_party/angle/src/image_util/loadimage.h"

namespace gpu {
namespace gles2 {

namespace {

const char kOESDerivativeExtension[] = "GL_OES_standard_derivatives";
const char kOESFboRenderMipmapExtension[] = "GL_OES_fbo_render_mipmap";
const char kEXTFragDepthExtension[] = "GL_EXT_frag_depth";
const char kEXTDrawBuffersExtension[] = "GL_EXT_draw_buffers";
const char kEXTShaderTextureLodExtension[] = "GL_EXT_shader_texture_lod";
const char kWEBGLMultiDrawExtension[] = "GL_WEBGL_multi_draw";
const char kWEBGLDrawInstancedBaseVertexBaseInstanceExtension[] =
    "GL_WEBGL_draw_instanced_base_vertex_base_instance";
const char kWEBGLMultiDrawInstancedBaseVertexBaseInstanceExtension[] =
    "GL_WEBGL_multi_draw_instanced_base_vertex_base_instance";

template <typename MANAGER_TYPE, typename OBJECT_TYPE>
GLuint GetClientId(const MANAGER_TYPE* manager, const OBJECT_TYPE* object) {
  DCHECK(manager);
  GLuint client_id = 0;
  if (object) {
    manager->GetClientId(object->service_id(), &client_id);
  }
  return client_id;
}

template <typename OBJECT_TYPE>
GLuint GetServiceId(const OBJECT_TYPE* object) {
  return object ? object->service_id() : 0;
}

struct Vec4f {
  explicit Vec4f(const Vec4& data) {
    data.GetValues(v);
  }

  GLfloat v[4];
};

struct TexSubCoord3D {
  TexSubCoord3D(int _xoffset, int _yoffset, int _zoffset,
                int _width, int _height, int _depth)
      : xoffset(_xoffset),
        yoffset(_yoffset),
        zoffset(_zoffset),
        width(_width),
        height(_height),
        depth(_depth) {}

  int xoffset;
  int yoffset;
  int zoffset;
  int width;
  int height;
  int depth;
};

// Check if all |ref| bits are set in |bits|.
bool AllBitsSet(GLbitfield bits, GLbitfield ref) {
  DCHECK_NE(0u, ref);
  return ((bits & ref) == ref);
}

// Check if any of |ref| bits are set in |bits|.
bool AnyBitsSet(GLbitfield bits, GLbitfield ref) {
  DCHECK_NE(0u, ref);
  return ((bits & ref) != 0);
}

// Check if any bits are set in |bits| other than the bits in |ref|.
bool AnyOtherBitsSet(GLbitfield bits, GLbitfield ref) {
  DCHECK_NE(0u, ref);
  GLbitfield mask = ~ref;
  return ((bits & mask) != 0);
}

void APIENTRY GLDebugMessageCallback(GLenum source,
                                     GLenum type,
                                     GLuint id,
                                     GLenum severity,
                                     GLsizei length,
                                     const GLchar* message,
                                     const GLvoid* user_param) {
  Logger* error_logger = static_cast<Logger*>(const_cast<void*>(user_param));
  LogGLDebugMessage(source, type, id, severity, length, message, error_logger);
}

}  // namespace

class GLES2DecoderImpl;

// Local versions of the SET_GL_ERROR macros
#define LOCAL_SET_GL_ERROR(error, function_name, msg) \
  ERRORSTATE_SET_GL_ERROR(error_state_.get(), error, function_name, msg)
#define LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, value, label)      \
  ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(error_state_.get(), function_name, \
                                       static_cast<uint32_t>(value), label)
#define LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name) \
  ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(error_state_.get(), function_name)
#define LOCAL_PEEK_GL_ERROR(function_name) \
  ERRORSTATE_PEEK_GL_ERROR(error_state_.get(), function_name)
#define LOCAL_CLEAR_REAL_GL_ERRORS(function_name) \
  ERRORSTATE_CLEAR_REAL_GL_ERRORS(error_state_.get(), function_name)
#define LOCAL_PERFORMANCE_WARNING(msg) \
    PerformanceWarning(__FILE__, __LINE__, msg)
#define LOCAL_RENDER_WARNING(msg) \
    RenderWarning(__FILE__, __LINE__, msg)

// Check that certain assumptions the code makes are true. There are places in
// the code where shared memory is passed direclty to GL. Example, glUniformiv,
// glShaderSource. The command buffer code assumes GLint and GLsizei (and maybe
// a few others) are 32bits. If they are not 32bits the code will have to change
// to call those GL functions with service side memory and then copy the results
// to shared memory, converting the sizes.
static_assert(sizeof(GLint) == sizeof(uint32_t),  // NOLINT
              "GLint should be the same size as uint32_t");
static_assert(sizeof(GLsizei) == sizeof(uint32_t),  // NOLINT
              "GLsizei should be the same size as uint32_t");
static_assert(sizeof(GLfloat) == sizeof(float),  // NOLINT
              "GLfloat should be the same size as float");

// TODO(kbr): the use of this anonymous namespace core dumps the
// linker on Mac OS X 10.6 when the symbol ordering file is used
// namespace {

// Return true if a character belongs to the ASCII subset as defined in
// GLSL ES 1.0 spec section 3.1.
static bool CharacterIsValidForGLES(unsigned char c) {
  // Printing characters are valid except " $ ` @ \ ' DEL.
  if (c >= 32 && c <= 126 &&
      c != '"' &&
      c != '$' &&
      c != '`' &&
      c != '@' &&
      c != '\\' &&
      c != '\'') {
    return true;
  }
  // Horizontal tab, line feed, vertical tab, form feed, carriage return
  // are also valid.
  if (c >= 9 && c <= 13) {
    return true;
  }

  return false;
}

static bool StringIsValidForGLES(const std::string& str) {
  return str.length() == 0 ||
         base::ranges::all_of(str, CharacterIsValidForGLES);
}

DisallowedFeatures::DisallowedFeatures() = default;
DisallowedFeatures::~DisallowedFeatures() = default;
DisallowedFeatures::DisallowedFeatures(const DisallowedFeatures&) = default;

// This class prevents any GL errors that occur when it is in scope from
// being reported to the client.
class ScopedGLErrorSuppressor {
 public:
  explicit ScopedGLErrorSuppressor(
      const char* function_name, ErrorState* error_state);

  ScopedGLErrorSuppressor(const ScopedGLErrorSuppressor&) = delete;
  ScopedGLErrorSuppressor& operator=(const ScopedGLErrorSuppressor&) = delete;

  ~ScopedGLErrorSuppressor();
 private:
  const char* function_name_;
  raw_ptr<ErrorState> error_state_;
};

// Temporarily changes a decoder's bound texture and restore it when this
// object goes out of scope. Also temporarily switches to using active texture
// unit zero in case the client has changed that to something invalid.
class ScopedTextureBinder {
 public:
  explicit ScopedTextureBinder(ContextState* state,
                               ErrorState* error_state,
                               GLuint id,
                               GLenum target);

  ScopedTextureBinder(const ScopedTextureBinder&) = delete;
  ScopedTextureBinder& operator=(const ScopedTextureBinder&) = delete;

  ~ScopedTextureBinder();

 private:
  raw_ptr<ContextState> state_;
  raw_ptr<ErrorState> error_state_;
  GLenum target_;
};

// Temporarily changes a decoder's bound frame buffer and restore it when this
// object goes out of scope.
class ScopedFramebufferBinder {
 public:
  explicit ScopedFramebufferBinder(GLES2DecoderImpl* decoder, GLuint id);

  ScopedFramebufferBinder(const ScopedFramebufferBinder&) = delete;
  ScopedFramebufferBinder& operator=(const ScopedFramebufferBinder&) = delete;

  ~ScopedFramebufferBinder();

 private:
  raw_ptr<GLES2DecoderImpl> decoder_;
};

// Temporarily create and bind a single sample copy of the currently bound
// framebuffer using CopyTexImage2D. This is useful as a workaround for drivers
// that have broken implementations of ReadPixels on multisampled framebuffers.
// See http://crbug.com/890002
class ScopedFramebufferCopyBinder {
 public:
  explicit ScopedFramebufferCopyBinder(GLES2DecoderImpl* decoder,
                                       GLint x = 0,
                                       GLint y = 0,
                                       GLint width = 0,
                                       GLint height = 0);

  ScopedFramebufferCopyBinder(const ScopedFramebufferCopyBinder&) = delete;
  ScopedFramebufferCopyBinder& operator=(const ScopedFramebufferCopyBinder&) =
      delete;

  ~ScopedFramebufferCopyBinder();

 private:
  raw_ptr<GLES2DecoderImpl> decoder_;
  std::unique_ptr<ScopedFramebufferBinder> framebuffer_binder_;
  GLuint temp_texture_;
  GLuint temp_framebuffer_;
};

// Temporarily changes a decoder's PIXEL_UNPACK_BUFFER to 0 and set pixel unpack
// params to default, and restore them when this object goes out of scope.
class ScopedPixelUnpackState {
 public:
  explicit ScopedPixelUnpackState(ContextState* state);

  ScopedPixelUnpackState(const ScopedPixelUnpackState&) = delete;
  ScopedPixelUnpackState& operator=(const ScopedPixelUnpackState&) = delete;

  ~ScopedPixelUnpackState();

 private:
  raw_ptr<ContextState> state_;
};

// Encapsulates an OpenGL texture.
class BackTexture {
 public:
  explicit BackTexture(GLES2DecoderImpl* decoder);

  BackTexture(const BackTexture&) = delete;
  BackTexture& operator=(const BackTexture&) = delete;

  ~BackTexture();

  // Create a new render texture.
  void Create();

  // Set the initial size and format of a render texture or resize it.
  bool AllocateStorage(const gfx::Size& size, GLenum format, bool zero);

  // Copy the contents of the currently bound frame buffer.
  void Copy();

  // Destroy the render texture. This must be explicitly called before
  // destroying this object.
  void Destroy();

  // Invalidate the texture. This can be used when a context is lost and it is
  // not possible to make it current in order to free the resource.
  void Invalidate();

  // The bind point for the texture.
  GLenum Target();

  scoped_refptr<TextureRef> texture_ref() { return texture_ref_; }

  GLuint id() const {
    return texture_ref_ ? texture_ref_->service_id() : 0;
  }

  gfx::Size size() const {
    return size_;
  }

  gl::GLApi* api() const;

 private:
  MemoryTypeTracker memory_tracker_;
  size_t bytes_allocated_;
  gfx::Size size_;
  raw_ptr<GLES2DecoderImpl> decoder_;

  scoped_refptr<TextureRef> texture_ref_;
};

// Encapsulates an OpenGL frame buffer.
class BackFramebuffer {
 public:
  explicit BackFramebuffer(GLES2DecoderImpl* decoder);

  BackFramebuffer(const BackFramebuffer&) = delete;
  BackFramebuffer& operator=(const BackFramebuffer&) = delete;

  ~BackFramebuffer();

  // Create a new frame buffer.
  void Create();

  // Attach a color render buffer to a frame buffer.
  void AttachRenderTexture(BackTexture* texture);

  // Destroy the frame buffer. This must be explicitly called before destroying
  // this object.
  void Destroy();

  // Invalidate the frame buffer. This can be used when a context is lost and it
  // is not possible to make it current in order to free the resource.
  void Invalidate();

  // See glCheckFramebufferStatusEXT.
  GLenum CheckStatus();

  GLuint id() const {
    return id_;
  }

  gl::GLApi* api() const;

 private:
  raw_ptr<GLES2DecoderImpl> decoder_;
  GLuint id_;
};

struct FenceCallback {
  FenceCallback() : fence(gl::GLFence::Create()) { DCHECK(fence); }
  FenceCallback(FenceCallback&&) = default;
  FenceCallback& operator=(FenceCallback&&) = default;
  std::vector<base::OnceClosure> callbacks;
  std::unique_ptr<gl::GLFence> fence;
};

// For ensuring that client-side glRenderbufferStorageMultisampleEXT
// calls go down the correct code path.
enum ForcedMultisampleMode {
  kForceExtMultisampledRenderToTexture,
  kDoNotForce
};

// }  // anonymous namespace.

// static
const unsigned int GLES2Decoder::kDefaultStencilMask =
    static_cast<unsigned int>(-1);

bool GLES2Decoder::GetServiceTextureId(uint32_t client_texture_id,
                                       uint32_t* service_texture_id) {
  return false;
}

uint32_t GLES2Decoder::GetAndClearBackbufferClearBitsForTest() {
  return 0;
}

GLES2Decoder::GLES2Decoder(DecoderClient* client,
                           CommandBufferServiceBase* command_buffer_service,
                           Outputter* outputter)
    : CommonDecoder(client, command_buffer_service), outputter_(outputter) {
  DCHECK(outputter_);
}

GLES2Decoder::~GLES2Decoder() = default;

bool GLES2Decoder::initialized() const {
  return initialized_;
}

TextureBase* GLES2Decoder::GetTextureBase(uint32_t client_id) {
  return nullptr;
}

void GLES2Decoder::SetLevelInfo(uint32_t client_id,
                                int level,
                                unsigned internal_format,
                                unsigned width,
                                unsigned height,
                                unsigned depth,
                                unsigned format,
                                unsigned type,
                                const gfx::Rect& cleared_rect) {}

void GLES2Decoder::BeginDecoding() {}

void GLES2Decoder::EndDecoding() {}

std::string_view GLES2Decoder::GetLogPrefix() {
  return GetLogger()->GetLogPrefix();
}

void GLES2Decoder::SetLogCommands(bool log_commands) {
  log_commands_ = log_commands;
}

Outputter* GLES2Decoder::outputter() const {
  return outputter_;
}

int GLES2Decoder::GetRasterDecoderId() const {
  NOTREACHED_IN_MIGRATION();
  return -1;
}

// This class implements GLES2Decoder so we don't have to expose all the GLES2
// cmd stuff to outside this class.
class GLES2DecoderImpl : public GLES2Decoder,
                         public ErrorStateClient,
                         public ui::GpuSwitchingObserver {
 public:
  GLES2DecoderImpl(DecoderClient* client,
                   CommandBufferServiceBase* command_buffer_service,
                   Outputter* outputter,
                   ContextGroup* group);

  GLES2DecoderImpl(const GLES2DecoderImpl&) = delete;
  GLES2DecoderImpl& operator=(const GLES2DecoderImpl&) = delete;

  ~GLES2DecoderImpl() override;

  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) override;

  template <bool DebugImpl>
  error::Error DoCommandsImpl(unsigned int num_commands,
                              const volatile void* buffer,
                              int num_entries,
                              int* entries_processed);

  // Overridden from GLES2Decoder.
  base::WeakPtr<DecoderContext> AsWeakPtr() override;
  gpu::ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const DisallowedFeatures& disallowed_features,
      const ContextCreationAttribs& attrib_helper) override;
  void Destroy(bool have_context) override;
  void SetSurface(const scoped_refptr<gl::GLSurface>& surface) override;
  void ReleaseSurface() override;
  void SetDefaultFramebufferSharedImage(const Mailbox& mailbox,
                                        int samples,
                                        bool preserve,
                                        bool needs_depth,
                                        bool needs_stencil) override;
  bool ResizeOffscreenFramebuffer(const gfx::Size& size);
  bool MakeCurrent() override;
  gl::GLApi* api() const { return state_.api(); }
  GLES2Util* GetGLES2Util() override { return &util_; }
  gl::GLContext* GetGLContext() override { return context_.get(); }
  gl::GLSurface* GetGLSurface() override { return surface_.get(); }
  ContextGroup* GetContextGroup() override { return group_.get(); }
  const FeatureInfo* GetFeatureInfo() const override {
    return feature_info_.get();
  }
  Capabilities GetCapabilities() override;
  GLCapabilities GetGLCapabilities() override;
  void RestoreState(const ContextState* prev_state) override;

  void RestoreActiveTexture() const override { state_.RestoreActiveTexture(); }
  void RestoreAllTextureUnitAndSamplerBindings(
      const ContextState* prev_state) const override {
    state_.RestoreAllTextureUnitAndSamplerBindings(prev_state);
  }
  void RestoreActiveTextureUnitBinding(unsigned int target) const override {
    state_.RestoreActiveTextureUnitBinding(target);
  }
  void RestoreBufferBindings() const override {
    state_.RestoreBufferBindings();
  }
  void RestoreGlobalState() const override {
    state_.RestoreGlobalState(nullptr);
  }
  void RestoreProgramBindings() const override {
    state_.RestoreProgramSettings(nullptr, false);
  }
  void RestoreTextureUnitBindings(unsigned unit) const override {
    state_.RestoreTextureUnitBindings(unit, nullptr);
  }
  void RestoreVertexAttribArray(unsigned index) override {
    RestoreStateForAttrib(index, true);
  }
  void RestoreBufferBinding(unsigned int target) override;
  void RestoreFramebufferBindings() const override;
  void RestoreRenderbufferBindings() override;
  void RestoreTextureState(unsigned service_id) override;

  void ClearDeviceWindowRectangles() const;
  void RestoreDeviceWindowRectangles() const override;

  void ClearAllAttributes() const override;
  void RestoreAllAttributes() const override;

  QueryManager* GetQueryManager() override { return query_manager_.get(); }
  void SetQueryCallback(unsigned int query_client_id,
                        base::OnceClosure callback) override;
  void CancelAllQueries() override;
  GpuFenceManager* GetGpuFenceManager() override {
    return gpu_fence_manager_.get();
  }
  FramebufferManager* GetFramebufferManager() override {
    return framebuffer_manager_.get();
  }
  TransformFeedbackManager* GetTransformFeedbackManager() override {
    return transform_feedback_manager_.get();
  }
  VertexArrayManager* GetVertexArrayManager() override {
    return vertex_array_manager_.get();
  }

  bool HasPendingQueries() const override;
  void ProcessPendingQueries(bool did_finish) override;

  bool HasMoreIdleWork() const override;
  void PerformIdleWork() override;

  bool HasPollingWork() const override;
  void PerformPollingWork() override;

  void WaitForReadPixels(base::OnceClosure callback) override;

  Logger* GetLogger() override;

  void BeginDecoding() override;
  void EndDecoding() override;

  ErrorState* GetErrorState() override;
  const ContextState* GetContextState() override { return &state_; }

  scoped_refptr<ShaderTranslatorInterface> GetTranslator(GLenum type) override;
  scoped_refptr<ShaderTranslatorInterface> GetOrCreateTranslator(GLenum type);

  void SetIgnoreCachedStateForTest(bool ignore) override;
  void SetForceShaderNameHashingForTest(bool force) override;
  uint32_t GetAndClearBackbufferClearBitsForTest() override;
  void ProcessFinishedAsyncTransfers();

  bool GetServiceTextureId(uint32_t client_texture_id,
                           uint32_t* service_texture_id) override;
  TextureBase* GetTextureBase(uint32_t client_id) override;
  void SetLevelInfo(uint32_t client_id,
                    int level,
                    unsigned internal_format,
                    unsigned width,
                    unsigned height,
                    unsigned depth,
                    unsigned format,
                    unsigned type,
                    const gfx::Rect& cleared_rect) override;

  // Implements GpuSwitchingObserver.
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override;

  // Restores the current state to the user's settings.
  void RestoreCurrentFramebufferBindings();

  // Sets DEPTH_TEST, STENCIL_TEST and color mask for the current framebuffer.
  void ApplyDirtyState();

  // These check the state of the currently bound framebuffer or the
  // backbuffer if no framebuffer is bound.
  // Check with all attached and enabled color attachments.
  bool BoundFramebufferAllowsChangesToAlphaChannel();
  bool BoundFramebufferHasDepthAttachment();
  bool BoundFramebufferHasStencilAttachment();

  // Overriden from ErrorStateClient.
  void OnContextLostError() override;
  void OnOutOfMemoryError() override;

  // Ensure Renderbuffer corresponding to last DoBindRenderbuffer() is bound.
  void EnsureRenderbufferBound();

  // Helpers to facilitate calling into compatible extensions.
  void RenderbufferStorageMultisampleWithWorkaround(GLenum target,
                                                    GLsizei samples,
                                                    GLenum internal_format,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    ForcedMultisampleMode mode);
  void RenderbufferStorageMultisampleHelper(GLenum target,
                                            GLsizei samples,
                                            GLenum internal_format,
                                            GLsizei width,
                                            GLsizei height,
                                            ForcedMultisampleMode mode);
  void RenderbufferStorageMultisampleHelperAMD(GLenum target,
                                               GLsizei samples,
                                               GLsizei storageSamples,
                                               GLenum internal_format,
                                               GLsizei width,
                                               GLsizei height,
                                               ForcedMultisampleMode mode);
  bool RegenerateRenderbufferIfNeeded(Renderbuffer* renderbuffer);

  void SetCopyTextureResourceManagerForTest(
      CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager)
      override {
    copy_texture_chromium_.reset(copy_texture_resource_manager);
  }

  void SetCopyTexImageBlitterForTest(
      CopyTexImageResourceManager* copy_tex_image_blit) override {
    copy_tex_image_blit_.reset(copy_tex_image_blit);
  }

  // ServiceFontManager::Client implementation.
  scoped_refptr<gpu::Buffer> GetShmBuffer(uint32_t shm_id);

 private:
  friend class ScopedFramebufferBinder;
  friend class ScopedResolvedFramebufferBinder;
  friend class ScopedFramebufferCopyBinder;
  friend class BackFramebuffer;
  friend class BackTexture;

  enum FramebufferOperation {
    kFramebufferDiscard,
    kFramebufferInvalidate,
    kFramebufferInvalidateSub
  };

  enum class BindIndexedBufferFunctionType {
    kBindBufferBase,
    kBindBufferRange
  };

  // Helper class to ensure that GLES2DecoderImpl::Destroy() is always called
  // unless we specifically call OnSuccess().
  class DestroyOnFailure {
   public:
    DestroyOnFailure(GLES2DecoderImpl* decoder) : decoder_(decoder) {}
    ~DestroyOnFailure() {
      if (!success_)
        decoder_->Destroy(has_context_);
    }

    void OnSuccess() { success_ = true; }
    void LoseContext() { has_context_ = false; }

   private:
    raw_ptr<GLES2DecoderImpl> decoder_ = nullptr;
    bool success_ = false;
    bool has_context_ = true;
  };

  const char* GetCommandName(unsigned int command_id) const;

  // Initialize or re-initialize the shader translator.
  bool InitializeShaderTranslator();
  void DestroyShaderTranslator();

  GLint ComputeMaxSamples();
  void UpdateCapabilities();

  // Helpers for the glGen and glDelete functions.
  bool GenTexturesHelper(GLsizei n, const GLuint* client_ids);
  void DeleteTexturesHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenBuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteBuffersHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenFramebuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteFramebuffersHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenRenderbuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteRenderbuffersHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenQueriesEXTHelper(GLsizei n, const GLuint* client_ids);
  void DeleteQueriesEXTHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenVertexArraysOESHelper(GLsizei n, const GLuint* client_ids);
  void DeleteVertexArraysOESHelper(GLsizei n,
                                   const volatile GLuint* client_ids);
  bool GenSamplersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteSamplersHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenTransformFeedbacksHelper(GLsizei n, const GLuint* client_ids);
  void DeleteTransformFeedbacksHelper(GLsizei n,
                                      const volatile GLuint* client_ids);
  void DeleteSyncHelper(GLuint sync);

  bool UnmapBufferHelper(Buffer* buffer, GLenum target);

  // Workarounds
  void OnFboChanged() const;
  void OnUseFramebuffer() const;
  void UpdateFramebufferSRGB(Framebuffer* framebuffer);

  // TODO(gman): Cache these pointers?
  BufferManager* buffer_manager() {
    return group_->buffer_manager();
  }

  RenderbufferManager* renderbuffer_manager() {
    return group_->renderbuffer_manager();
  }

  FramebufferManager* framebuffer_manager() {
    return framebuffer_manager_.get();
  }

  ProgramManager* program_manager() {
    return group_->program_manager();
  }

  SamplerManager* sampler_manager() {
    return group_->sampler_manager();
  }

  ShaderManager* shader_manager() {
    return group_->shader_manager();
  }

  ShaderTranslatorCache* shader_translator_cache() {
    return group_->shader_translator_cache();
  }

  const TextureManager* texture_manager() const {
    return group_->texture_manager();
  }

  TextureManager* texture_manager() {
    return group_->texture_manager();
  }

  VertexArrayManager* vertex_array_manager() {
    return vertex_array_manager_.get();
  }

  MemoryTracker* memory_tracker() {
    return group_->memory_tracker();
  }

  const gl::GLVersionInfo& gl_version_info() {
    return feature_info_->gl_version_info();
  }

  // Creates a Texture for the given texture.
  TextureRef* CreateTexture(
      GLuint client_id, GLuint service_id) {
    return texture_manager()->CreateTexture(client_id, service_id);
  }

  // Gets the texture info for the given texture. Returns nullptr if none
  // exists.
  TextureRef* GetTexture(GLuint client_id) const {
    return texture_manager()->GetTexture(client_id);
  }

  // Deletes the texture info for the given texture.
  void RemoveTexture(GLuint client_id) {
    texture_manager()->RemoveTexture(client_id);
  }

  // Creates a Sampler for the given sampler.
  Sampler* CreateSampler(
      GLuint client_id, GLuint service_id) {
    return sampler_manager()->CreateSampler(client_id, service_id);
  }

  // Gets the sampler info for the given sampler. Returns nullptr if none
  // exists.
  Sampler* GetSampler(GLuint client_id) {
    return sampler_manager()->GetSampler(client_id);
  }

  // Deletes the sampler info for the given sampler.
  void RemoveSampler(GLuint client_id) {
    sampler_manager()->RemoveSampler(client_id);
  }

  // Creates a TransformFeedback for the given transformfeedback.
  TransformFeedback* CreateTransformFeedback(
      GLuint client_id, GLuint service_id) {
    return transform_feedback_manager_->CreateTransformFeedback(
        client_id, service_id);
  }

  // Gets the TransformFeedback info for the given transformfeedback.
  // Returns nullptr if none exists.
  TransformFeedback* GetTransformFeedback(GLuint client_id) {
    return transform_feedback_manager_->GetTransformFeedback(client_id);
  }

  // Deletes the TransformFeedback info for the given transformfeedback.
  void RemoveTransformFeedback(GLuint client_id) {
    transform_feedback_manager_->RemoveTransformFeedback(client_id);
  }

  // Get the size (in pixels) of the currently bound frame buffer (either FBO
  // or regular back buffer).
  gfx::Size GetBoundReadFramebufferSize();
  gfx::Size GetBoundDrawFramebufferSize();

  // Get the service side ID for the bound read framebuffer.
  // If it's back buffer, 0 is returned.
  GLuint GetBoundReadFramebufferServiceId();

  // Get the service side ID for the bound draw framebuffer.
  // If it's back buffer, 0 is returned.
  GLuint GetBoundDrawFramebufferServiceId() const;

  // Get the format/type of the currently bound frame buffer (either FBO or
  // regular back buffer).
  // If the color image is a renderbuffer, returns 0 for type.
  GLenum GetBoundReadFramebufferTextureType();
  GLenum GetBoundReadFramebufferInternalFormat();

  // Get the i-th draw buffer's internal format/type from the bound framebuffer.
  // If no framebuffer is bound, or no image is attached, or the DrawBuffers
  // setting for that image is GL_NONE, return 0.
  GLenum GetBoundColorDrawBufferType(GLint drawbuffer_i);
  GLenum GetBoundColorDrawBufferInternalFormat(GLint drawbuffer_i);

  GLsizei GetBoundFramebufferSamples(GLenum target);

  // Return 0 if no depth attachment.
  GLenum GetBoundFramebufferDepthFormat(GLenum target);
  // Return 0 if no stencil attachment.
  GLenum GetBoundFramebufferStencilFormat(GLenum target);

  void MarkDrawBufferAsCleared(GLenum buffer, GLint drawbuffer_i);

  // Wrapper for CompressedTexImage{2|3}D commands.
  error::Error DoCompressedTexImage(
      GLenum target,
      GLint level,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLint border,
      GLsizei image_size,
      const void* data,
      ContextState::Dimension dimension);

  // Wrapper for CompressedTexSubImage{2|3}D.
  error::Error DoCompressedTexSubImage(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLint zoffset,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLenum format,
      GLsizei imageSize,
      const void* data,
      ContextState::Dimension dimension);

  // Validate if |format| is valid for CopyTex{Sub}Image functions.
  // If not, generate a GL error and return false.
  bool ValidateCopyTexFormat(const char* func_name, GLenum internal_format,
                             GLenum read_format, GLenum read_type);

  // Wrapper for CopyTexImage2D.
  void DoCopyTexImage2D(
      GLenum target,
      GLint level,
      GLenum internal_format,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLint border);

  // Wrapper for SwapBuffers.
  void DoSwapBuffers(uint64_t swap_id, GLbitfield flags);

  // Callback for async SwapBuffers.
  void FinishAsyncSwapBuffers(uint64_t swap_id,
                              gfx::SwapCompletionResult result);
  void FinishSwapBuffers(gfx::SwapResult result);

  // Wrapper for CopyTexSubImage2D.
  void DoCopyTexSubImage2D(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height);

  // Wrapper for CopyTexSubImage3D.
  void DoCopyTexSubImage3D(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLint zoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height);

  // Wrapper for glCopyBufferSubData.
  void DoCopyBufferSubData(GLenum readtarget,
                           GLenum writetarget,
                           GLintptr readoffset,
                           GLintptr writeoffset,
                           GLsizeiptr size);

  void DoCopyTextureCHROMIUM(GLuint source_id,
                             GLint source_level,
                             GLenum dest_target,
                             GLuint dest_id,
                             GLint dest_level,
                             GLenum internal_format,
                             GLenum dest_type,
                             GLboolean unpack_flip_y,
                             GLboolean unpack_premultiply_alpha,
                             GLboolean unpack_unmultiply_alpha);

  void DoCopySubTextureCHROMIUM(GLuint source_id,
                                GLint source_level,
                                GLenum dest_target,
                                GLuint dest_id,
                                GLint dest_level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLboolean unpack_flip_y,
                                GLboolean unpack_premultiply_alpha,
                                GLboolean unpack_unmultiply_alpha);

  // Helper for DoTexStorage2DEXT and DoTexStorage3D.
  void TexStorageImpl(GLenum target,
                      GLsizei levels,
                      GLenum internal_format,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth,
                      ContextState::Dimension dimension,
                      const char* function_name);

  // Wrapper for TexStorage2DEXT.
  void DoTexStorage2DEXT(GLenum target,
                         GLsizei levels,
                         GLenum internal_format,
                         GLsizei width,
                         GLsizei height);

  // Wrapper for TexStorage3D.
  void DoTexStorage3D(GLenum target,
                      GLsizei levels,
                      GLenum internal_format,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth);

  void DoTexStorage2DImageCHROMIUM(GLenum target,
                                   GLenum internal_format,
                                   GLenum buffer_usage,
                                   GLsizei width,
                                   GLsizei height);

  void DoCreateAndTexStorage2DSharedImageINTERNAL(
      GLuint client_id,
      const volatile GLbyte* mailbox);
  void DoBeginSharedImageAccessDirectCHROMIUM(GLuint client_id, GLenum mode);
  void DoEndSharedImageAccessDirectCHROMIUM(GLuint client_id);

  void DoCopySharedImageINTERNAL(GLint xoffset,
                                 GLint yoffset,
                                 GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height,
                                 GLboolean unpack_flip_y,
                                 const volatile GLbyte* mailboxes);
  void DoCopySharedImageToTextureINTERNAL(GLuint texture,
                                          GLenum target,
                                          GLuint internal_format,
                                          GLenum type,
                                          GLint src_x,
                                          GLint src_y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLboolean flip_y,
                                          const volatile GLbyte* src_mailbox);

  void DoTraceEndCHROMIUM(void);

  void DoDrawBuffersEXT(GLsizei count, const volatile GLenum* bufs);

  void DoLoseContextCHROMIUM(GLenum current, GLenum other);

  void DoFlushDriverCachesCHROMIUM(void);

  void DoFlushMappedBufferRange(
      GLenum target, GLintptr offset, GLsizeiptr size);

  // Wrappers for ANGLE_shader_pixel_local_storage.
  void DoFramebufferMemorylessPixelLocalStorageANGLE(GLint plane,
                                                     GLenum internalformat);
  void DoFramebufferTexturePixelLocalStorageANGLE(GLint plane,
                                                  GLuint backingtexture,
                                                  GLint level,
                                                  GLint layer);
  void DoFramebufferPixelLocalClearValuefvANGLE(GLint plane,
                                                const volatile GLfloat* value);
  void DoFramebufferPixelLocalClearValueivANGLE(GLint plane,
                                                const volatile GLint* value);
  void DoFramebufferPixelLocalClearValueuivANGLE(GLint plane,
                                                 const volatile GLuint* value);
  void DoBeginPixelLocalStorageANGLE(GLsizei n, const volatile GLenum* loadops);
  void DoEndPixelLocalStorageANGLE(GLsizei n, const volatile GLenum* storeops);
  void DoPixelLocalStorageBarrierANGLE();
  void DoFramebufferPixelLocalStorageInterruptANGLE();
  void DoFramebufferPixelLocalStorageRestoreANGLE();
  void DoGetFramebufferPixelLocalStorageParameterfvANGLE(GLint plane,
                                                         GLenum pname,
                                                         GLfloat* params,
                                                         GLsizei params_size);
  void DoGetFramebufferPixelLocalStorageParameterivANGLE(GLint plane,
                                                         GLenum pname,
                                                         GLint* params,
                                                         GLsizei params_size);

  // Creates a Program for the given program.
  Program* CreateProgram(GLuint client_id, GLuint service_id) {
    return program_manager()->CreateProgram(client_id, service_id);
  }

  // Gets the program info for the given program. Returns nullptr if none
  // exists.
  Program* GetProgram(GLuint client_id) {
    return program_manager()->GetProgram(client_id);
  }

#if defined(NDEBUG)
  void LogClientServiceMapping(
      const char* /* function_name */,
      GLuint /* client_id */,
      GLuint /* service_id */) {
  }
  template<typename T>
  void LogClientServiceForInfo(
      T* /* info */, GLuint /* client_id */, const char* /* function_name */) {
  }
#else
  void LogClientServiceMapping(
      const char* function_name, GLuint client_id, GLuint service_id) {
    if (service_logging_) {
      VLOG(1) << "[" << logger_.GetLogPrefix() << "] " << function_name
              << ": client_id = " << client_id
              << ", service_id = " << service_id;
    }
  }
  template<typename T>
  void LogClientServiceForInfo(
      T* info, GLuint client_id, const char* function_name) {
    if (info) {
      LogClientServiceMapping(function_name, client_id, info->service_id());
    }
  }
#endif

  // Gets the program info for the given program. If it's not a program
  // generates a GL error. Returns nullptr if not program.
  Program* GetProgramInfoNotShader(
      GLuint client_id, const char* function_name) {
    Program* program = GetProgram(client_id);
    if (!program) {
      if (GetShader(client_id)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name, "shader passed for program");
      } else {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "unknown program");
      }
    }
    LogClientServiceForInfo(program, client_id, function_name);
    return program;
  }


  // Creates a Shader for the given shader.
  Shader* CreateShader(
      GLuint client_id,
      GLuint service_id,
      GLenum shader_type) {
    return shader_manager()->CreateShader(
        client_id, service_id, shader_type);
  }

  // Gets the shader info for the given shader. Returns nullptr if none exists.
  Shader* GetShader(GLuint client_id) {
    return shader_manager()->GetShader(client_id);
  }

  // Gets the shader info for the given shader. If it's not a shader generates a
  // GL error. Returns nullptr if not shader.
  Shader* GetShaderInfoNotProgram(
      GLuint client_id, const char* function_name) {
    Shader* shader = GetShader(client_id);
    if (!shader) {
      if (GetProgram(client_id)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name, "program passed for shader");
      } else {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_VALUE, function_name, "unknown shader");
      }
    }
    LogClientServiceForInfo(shader, client_id, function_name);
    return shader;
  }

  // Creates a buffer info for the given buffer.
  void CreateBuffer(GLuint client_id, GLuint service_id) {
    return buffer_manager()->CreateBuffer(client_id, service_id);
  }

  // Gets the buffer info for the given buffer.
  Buffer* GetBuffer(GLuint client_id) {
    Buffer* buffer = buffer_manager()->GetBuffer(client_id);
    return buffer;
  }

  // Creates a framebuffer info for the given framebuffer.
  void CreateFramebuffer(GLuint client_id, GLuint service_id) {
    return framebuffer_manager()->CreateFramebuffer(client_id, service_id);
  }

  // Gets the framebuffer info for the given framebuffer.
  Framebuffer* GetFramebuffer(GLuint client_id) {
    return framebuffer_manager()->GetFramebuffer(client_id);
  }

  // Removes the framebuffer info for the given framebuffer.
  void RemoveFramebuffer(GLuint client_id) {
    framebuffer_manager()->RemoveFramebuffer(client_id);
  }

  // Creates a renderbuffer info for the given renderbuffer.
  void CreateRenderbuffer(GLuint client_id, GLuint service_id) {
    return renderbuffer_manager()->CreateRenderbuffer(
        client_id, service_id);
  }

  // Gets the renderbuffer info for the given renderbuffer.
  Renderbuffer* GetRenderbuffer(GLuint client_id) {
    return renderbuffer_manager()->GetRenderbuffer(client_id);
  }

  // Removes the renderbuffer info for the given renderbuffer.
  void RemoveRenderbuffer(GLuint client_id) {
    renderbuffer_manager()->RemoveRenderbuffer(client_id);
  }

  // Gets the vertex attrib manager for the given vertex array.
  VertexAttribManager* GetVertexAttribManager(GLuint client_id) {
    VertexAttribManager* info =
        vertex_array_manager()->GetVertexAttribManager(client_id);
    return info;
  }

  // Removes the vertex attrib manager for the given vertex array.
  void RemoveVertexAttribManager(GLuint client_id) {
    vertex_array_manager()->RemoveVertexAttribManager(client_id);
  }

  // Creates a vertex attrib manager for the given vertex array.
  scoped_refptr<VertexAttribManager> CreateVertexAttribManager(
      GLuint client_id,
      GLuint service_id,
      bool client_visible) {
    return vertex_array_manager()->CreateVertexAttribManager(
        client_id, service_id, group_->max_vertex_attribs(), client_visible,
        feature_info_->IsWebGL2OrES3Context());
  }

  void DoBindAttribLocation(GLuint client_id,
                            GLuint index,
                            const std::string& name);

  error::Error DoBindFragDataLocation(GLuint program_id,
                                      GLuint colorName,
                                      const std::string& name);

  error::Error DoBindFragDataLocationIndexed(GLuint program_id,
                                             GLuint colorName,
                                             GLuint index,
                                             const std::string& name);

  void DoBindUniformLocationCHROMIUM(GLuint client_id,
                                     GLint location,
                                     const std::string& name);

  error::Error GetAttribLocationHelper(GLuint client_id,
                                       uint32_t location_shm_id,
                                       uint32_t location_shm_offset,
                                       const std::string& name_str);

  error::Error GetUniformLocationHelper(GLuint client_id,
                                        uint32_t location_shm_id,
                                        uint32_t location_shm_offset,
                                        const std::string& name_str);

  error::Error GetFragDataLocationHelper(GLuint client_id,
                                         uint32_t location_shm_id,
                                         uint32_t location_shm_offset,
                                         const std::string& name_str);

  error::Error GetFragDataIndexHelper(GLuint program_id,
                                      uint32_t index_shm_id,
                                      uint32_t index_shm_offset,
                                      const std::string& name_str);

  // Wrapper for glShaderSource.
  void DoShaderSource(
      GLuint client_id, GLsizei count, const char** data, const GLint* length);

  // Wrapper for glTransformFeedbackVaryings.
  void DoTransformFeedbackVaryings(
      GLuint client_program_id, GLsizei count, const char* const* varyings,
      GLenum buffer_mode);

  // Clear any textures used by the current program.
  bool ClearUnclearedTextures();

  // Clears any uncleared attachments attached to the given frame buffer.
  // Returns false if there was a generated GL error.
  void ClearUnclearedAttachments(GLenum target, Framebuffer* framebuffer);

  // overridden from GLES2Decoder
  bool ClearLevel(Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override;

  // Helper function for ClearLevel that attempts to clear using a glClear call
  // to a temporary FBO, rather than using glTexSubImage2D.
  bool ClearLevelUsingGL(Texture* texture,
                         uint32_t channels,
                         unsigned target,
                         int level,
                         int xoffset,
                         int yoffset,
                         int width,
                         int height);

  // overridden from GLES2Decoder
  bool ClearCompressedTextureLevel(Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override;
  bool ClearCompressedTextureLevel3D(Texture* texture,
                                     unsigned target,
                                     int level,
                                     unsigned format,
                                     int width,
                                     int height,
                                     int depth) override;
  bool IsCompressedTextureFormat(unsigned format) override;

  // overridden from GLES2Decoder
  bool ClearLevel3D(Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override;

  // Restore all GL state that affects clearing.
  void RestoreClearState();

  // Remembers the state of some capabilities.
  // Returns: true if glEnable/glDisable should actually be called.
  bool SetCapabilityState(GLenum cap, bool enabled);

  // Check that the currently bound read framebuffer's color image
  // isn't the target texture of the glCopyTex{Sub}Image{2D|3D}.
  bool FormsTextureCopyingFeedbackLoop(
      TextureRef* texture,
      GLint level,
      GLint layer);

  // Check if a framebuffer meets our requirements.
  // Generates |gl_error| if the framebuffer is incomplete.
  bool CheckFramebufferValid(
      Framebuffer* framebuffer,
      GLenum target,
      GLenum gl_error,
      const char* func_name);

  bool CheckBoundDrawFramebufferValid(const char* func_name,
                                      bool check_float_blending = false);
  // Generates |gl_error| if the bound read fbo is incomplete.
  bool CheckBoundReadFramebufferValid(const char* func_name, GLenum gl_error);
  // This is only used by DoBlitFramebufferCHROMIUM which operates read/draw
  // framebuffer at the same time.
  bool CheckBoundFramebufferValid(const char* func_name);

  // Checks if the current program exists and is valid. If not generates the
  // appropriate GL error.  Returns true if the current program is in a usable
  // state.
  bool CheckCurrentProgram(const char* function_name);

  // Checks if the current program exists and is valid and that location is not
  // -1. If the current program is not valid generates the appropriate GL
  // error. Returns true if the current program is in a usable state and
  // location is not -1.
  bool CheckCurrentProgramForUniform(GLint location, const char* function_name);

  // Helper for CheckDrawingFeedbackLoops. Returns true if the attachment is
  // the same one where it samples from during drawing.
  bool CheckDrawingFeedbackLoopsHelper(
      const Framebuffer::Attachment* attachment,
      TextureRef* texture_ref,
      const char* function_name);

  bool SupportsDrawBuffers() const;

  // Checks if a draw buffer's format and its corresponding fragment shader
  // output's type are compatible, i.e., a signed integer typed variable is
  // incompatible with a float or unsigned integer buffer.
  // If incompaticle, generates an INVALID_OPERATION to avoid undefined buffer
  // contents and return false.
  // Otherwise, filter out the draw buffers that are not written to but are not
  // NONE through DrawBuffers, to be on the safe side. Return true.
  bool ValidateAndAdjustDrawBuffers(const char* function_name);

  // Filter out the draw buffers that have no images attached but are not NONE
  // through DrawBuffers, to be on the safe side.
  void AdjustDrawBuffers();

  // Checks if all active uniform blocks in the current program are backed by
  // a buffer of sufficient size.
  // If not, generates an INVALID_OPERATION to avoid undefined behavior in
  // shader execution and return false.
  bool ValidateUniformBlockBackings(const char* function_name);

  // Checks if |api_type| is valid for the given uniform
  // If the api type is not valid generates the appropriate GL
  // error. Returns true if |api_type| is valid for the uniform
  bool CheckUniformForApiType(const Program::UniformInfo* info,
                              const char* function_name,
                              UniformApiType api_type);

  // Gets the type of a uniform for a location in the current program. Sets GL
  // errors if the current program is not valid. Returns true if the current
  // program is valid and the location exists. Adjusts count so it
  // does not overflow the uniform.
  bool PrepForSetUniformByLocation(GLint fake_location,
                                   const char* function_name,
                                   UniformApiType api_type,
                                   GLint* real_location,
                                   GLenum* type,
                                   GLsizei* count);

  // Gets the service id for any simulated backbuffer fbo.
  GLuint GetBackbufferServiceId() const;

  // Helper for glGetBooleanv, glGetFloatv and glGetIntegerv.  Returns
  // false if pname is unhandled.
  bool GetHelper(GLenum pname, GLint* params, GLsizei* num_written);

  // Helper for glGetVertexAttrib
  void GetVertexAttribHelper(
    const VertexAttrib* attrib, GLenum pname, GLint* param);

  // Wrapper for glActiveTexture
  void DoActiveTexture(GLenum texture_unit);

  // Wrapper for glAttachShader
  void DoAttachShader(GLuint client_program_id, GLint client_shader_id);

  // Wrapper for glBindBuffer since we need to track the current targets.
  void DoBindBuffer(GLenum target, GLuint buffer);

  // Wrapper for glBindBufferBase since we need to track the current targets.
  void DoBindBufferBase(GLenum target, GLuint index, GLuint buffer);

  // Wrapper for glBindBufferRange since we need to track the current targets.
  void DoBindBufferRange(GLenum target, GLuint index, GLuint buffer,
      GLintptr offset, GLsizeiptr size);

  // Helper for DoBindBufferBase and DoBindBufferRange.
  void BindIndexedBufferImpl(GLenum target, GLuint index, GLuint buffer,
                             GLintptr offset, GLsizeiptr size,
                             BindIndexedBufferFunctionType function_type,
                             const char* function_name);

  // Wrapper for glBindFramebuffer since we need to track the current targets.
  void DoBindFramebuffer(GLenum target, GLuint framebuffer);

  // Wrapper for glBindRenderbuffer since we need to track the current targets.
  void DoBindRenderbuffer(GLenum target, GLuint renderbuffer);

  // Updates any targets to which `client_id` is bound to be bound to `texture`.
  void UpdateTextureBinding(GLenum target,
                            GLuint client_id,
                            TextureRef* texture_ref);

  // Wrapper for glBindTexture since we need to track the current targets.
  void DoBindTexture(GLenum target, GLuint texture);

  // Wrapper for glBindSampler since we need to track the current targets.
  void DoBindSampler(GLuint unit, GLuint sampler);

  // Wrapper for glBindTransformFeedback since we need to emulate ES3 behaviors
  // for BindBufferRange on Desktop GL lower than 4.2.
  void DoBindTransformFeedback(GLenum target, GLuint transform_feedback);

  // Wrapper for glBeginTransformFeedback.
  void DoBeginTransformFeedback(GLenum primitive_mode);

  // Wrapper for glEndTransformFeedback.
  void DoEndTransformFeedback();

  // Wrapper for glPauseTransformFeedback.
  void DoPauseTransformFeedback();

  // Wrapper for glResumeTransformFeedback.
  void DoResumeTransformFeedback();

  // Wrapper for glBindVertexArrayOES
  void DoBindVertexArrayOES(GLuint array);
  void EmulateVertexArrayState();

  // Wrapper for glBlitFramebufferCHROMIUM.
  void DoBlitFramebufferCHROMIUM(
      GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
      GLbitfield mask, GLenum filter);

  // Wrapper for glBufferSubData.
  void DoBufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);

  // Wrapper for glCheckFramebufferStatus
  GLenum DoCheckFramebufferStatus(GLenum target);

  // Wrapper for glClear*()
  error::Error DoClear(GLbitfield mask);
  void DoClearBufferiv(GLenum buffer,
                       GLint drawbuffer,
                       const volatile GLint* value);
  void DoClearBufferuiv(GLenum buffer,
                        GLint drawbuffer,
                        const volatile GLuint* value);
  void DoClearBufferfv(GLenum buffer,
                       GLint drawbuffer,
                       const volatile GLfloat* value);
  void DoClearBufferfi(
      GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);

  // Wrappers for various state.
  void DoDepthRangef(GLclampf znear, GLclampf zfar);
  void DoSampleCoverage(GLclampf value, GLboolean invert);

  // Wrapper for glCompileShader.
  void DoCompileShader(GLuint shader);

  // Wrapper for glDetachShader
  void DoDetachShader(GLuint client_program_id, GLint client_shader_id);

  // Wrapper for glDisable
  void DoDisable(GLenum cap);

  // Wrapper for glDisableiOES
  void DoDisableiOES(GLenum target, GLuint index);

  // Wrapper for glDisableVertexAttribArray.
  void DoDisableVertexAttribArray(GLuint index);

  // Wrapper for glDiscardFramebufferEXT, since we need to track undefined
  // attachments.
  void DoDiscardFramebufferEXT(GLenum target,
                               GLsizei count,
                               const volatile GLenum* attachments);

  void DoInvalidateFramebuffer(GLenum target,
                               GLsizei count,
                               const volatile GLenum* attachments);
  void DoInvalidateSubFramebuffer(GLenum target,
                                  GLsizei count,
                                  const volatile GLenum* attachments,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height);

  // Helper for DoDiscardFramebufferEXT, DoInvalidate{Sub}Framebuffer.
  void InvalidateFramebufferImpl(GLenum target,
                                 GLsizei count,
                                 const volatile GLenum* attachments,
                                 GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height,
                                 const char* function_name,
                                 FramebufferOperation op);

  // Wrapper for glEnable
  void DoEnable(GLenum cap);

  // Wrapper for glEnableiOES
  void DoEnableiOES(GLenum target, GLuint index);

  // Wrapper for glEnableVertexAttribArray.
  void DoEnableVertexAttribArray(GLuint index);

  // Wrapper for glFinish.
  void DoFinish();

  // Wrapper for glFlush.
  void DoFlush();

  // Wrapper for glFramebufferParameteri.
  void DoFramebufferParameteri(GLenum target, GLenum pname, GLint param);

  // Wrapper for glFramebufferRenderbufffer.
  void DoFramebufferRenderbuffer(
      GLenum target, GLenum attachment, GLenum renderbuffertarget,
      GLuint renderbuffer);

  // Wrapper for glFramebufferTexture2D.
  void DoFramebufferTexture2D(
      GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
      GLint level);

  // Wrapper for glFramebufferTexture2DMultisampleEXT.
  void DoFramebufferTexture2DMultisample(
      GLenum target, GLenum attachment, GLenum textarget,
      GLuint texture, GLint level, GLsizei samples);

  // Common implementation for both DoFramebufferTexture2D wrappers.
  void DoFramebufferTexture2DCommon(const char* name,
      GLenum target, GLenum attachment, GLenum textarget,
      GLuint texture, GLint level, GLsizei samples);

  // Wrapper for glFramebufferTextureLayer.
  void DoFramebufferTextureLayer(
      GLenum target, GLenum attachment, GLuint texture, GLint level,
      GLint layer);

  // Wrapper for glFramebufferTextureLayer.
  void DoFramebufferTextureMultiviewOVR(GLenum target,
                                        GLenum attachment,
                                        GLuint texture,
                                        GLint level,
                                        GLint base_view_index,
                                        GLsizei num_views);

  // Wrapper for glGenerateMipmap
  void DoGenerateMipmap(GLenum target);

  // Helper for DoGetBooleanv, Floatv, and Intergerv to adjust pname
  // to account for different pname values defined in different extension
  // variants.
  GLenum AdjustGetPname(GLenum pname);

  // Wrapper for DoGetBooleanv.
  void DoGetBooleanv(GLenum pname, GLboolean* params, GLsizei params_size);

  // Wrapper for DoGetFloatv.
  void DoGetFloatv(GLenum pname, GLfloat* params, GLsizei params_size);

  // Wrapper for glGetFramebufferAttachmentParameteriv.
  void DoGetFramebufferAttachmentParameteriv(GLenum target,
                                             GLenum attachment,
                                             GLenum pname,
                                             GLint* params,
                                             GLsizei params_size);

  // Wrapper for glGetInteger64v.
  void DoGetInteger64v(GLenum pname, GLint64* params, GLsizei params_size);

  // Wrapper for glGetIntegerv.
  void DoGetIntegerv(GLenum pname, GLint* params, GLsizei params_size);

  // Helper for DoGetBooleani_v, DoGetIntegeri_v and DoGetInteger64i_v.
  template <typename TYPE>
  void GetIndexedIntegerImpl(
      const char* function_name, GLenum target, GLuint index, TYPE* data);

  // Wrapper for glGetBooleani_v.
  void DoGetBooleani_v(GLenum target,
                       GLuint index,
                       GLboolean* params,
                       GLsizei params_size);

  // Wrapper for glGetIntegeri_v.
  void DoGetIntegeri_v(GLenum target,
                       GLuint index,
                       GLint* params,
                       GLsizei params_size);

  // Wrapper for glGetInteger64i_v.
  void DoGetInteger64i_v(GLenum target,
                         GLuint index,
                         GLint64* params,
                         GLsizei params_size);

  // Gets the max value in a range in a buffer.
  GLuint DoGetMaxValueInBufferCHROMIUM(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset);

  // Wrapper for glGetBufferParameteri64v.
  void DoGetBufferParameteri64v(GLenum target,
                                GLenum pname,
                                GLint64* params,
                                GLsizei params_size);

  // Wrapper for glGetBufferParameteriv.
  void DoGetBufferParameteriv(GLenum target,
                              GLenum pname,
                              GLint* params,
                              GLsizei params_size);

  // Wrapper for glGetProgramiv.
  void DoGetProgramiv(GLuint program_id,
                      GLenum pname,
                      GLint* params,
                      GLsizei params_size);

  // Wrapper for glRenderbufferParameteriv.
  void DoGetRenderbufferParameteriv(GLenum target,
                                    GLenum pname,
                                    GLint* params,
                                    GLsizei params_size);

  // Wrappers for glGetSamplerParameter.
  void DoGetSamplerParameterfv(GLuint client_id,
                               GLenum pname,
                               GLfloat* params,
                               GLsizei params_size);
  void DoGetSamplerParameteriv(GLuint client_id,
                               GLenum pname,
                               GLint* params,
                               GLsizei params_size);

  // Wrapper for glGetShaderiv
  void DoGetShaderiv(GLuint shader,
                     GLenum pname,
                     GLint* params,
                     GLsizei params_size);

  // Wrapper for glGetSynciv.
  void DoGetSynciv(GLuint sync_id,
                   GLenum pname,
                   GLsizei num_values,
                   GLsizei* length,
                   GLint* values);

  // Helper for DoGetTexParameter{f|i}v.
  void GetTexParameterImpl(
      GLenum target, GLenum pname, GLfloat* fparams, GLint* iparams,
      const char* function_name);

  // Wrappers for glGetTexParameter.
  void DoGetTexParameterfv(GLenum target,
                           GLenum pname,
                           GLfloat* params,
                           GLsizei params_size);
  void DoGetTexParameteriv(GLenum target,
                           GLenum pname,
                           GLint* params,
                           GLsizei params_size);

  // Wrappers for glGetVertexAttrib.
  template <typename T>
  void DoGetVertexAttribImpl(GLuint index, GLenum pname, T* params);
  void DoGetVertexAttribfv(GLuint index,
                           GLenum pname,
                           GLfloat* params,
                           GLsizei params_size);
  void DoGetVertexAttribiv(GLuint index,
                           GLenum pname,
                           GLint* params,
                           GLsizei params_size);
  void DoGetVertexAttribIiv(GLuint index,
                            GLenum pname,
                            GLint* params,
                            GLsizei params_size);
  void DoGetVertexAttribIuiv(GLuint index,
                             GLenum pname,
                             GLuint* params,
                             GLsizei params_size);

  // Wrappers for glIsXXX functions.
  bool DoIsEnabled(GLenum cap);
  bool DoIsBuffer(GLuint client_id);
  bool DoIsFramebuffer(GLuint client_id);
  bool DoIsProgram(GLuint client_id);
  bool DoIsRenderbuffer(GLuint client_id);
  bool DoIsShader(GLuint client_id);
  bool DoIsTexture(GLuint client_id);
  bool DoIsSampler(GLuint client_id);
  bool DoIsTransformFeedback(GLuint client_id);
  bool DoIsVertexArrayOES(GLuint client_id);
  bool DoIsSync(GLuint client_id);

  bool DoIsEnablediOES(GLenum target, GLuint index);

  void DoLineWidth(GLfloat width);

  // Wrapper for glLinkProgram
  void DoLinkProgram(GLuint program);

  void DoMultiDrawBeginCHROMIUM(GLsizei drawcount);
  void DoMultiDrawEndCHROMIUM();

  // Wrapper for glReadBuffer
  void DoReadBuffer(GLenum src);

  // Wrapper for glRenderbufferStorage.
  void DoRenderbufferStorage(
      GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

  // Handler for glRenderbufferStorageMultisampleCHROMIUM.
  void DoRenderbufferStorageMultisampleCHROMIUM(
      GLenum target, GLsizei samples, GLenum internalformat,
      GLsizei width, GLsizei height);

  // Handler for glRenderbufferStorageMultisampleAdvancedAMD.
  void DoRenderbufferStorageMultisampleAdvancedAMD(GLenum target,
                                                   GLsizei samples,
                                                   GLsizei storageSamples,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height);

  // Handler for glRenderbufferStorageMultisampleEXT
  // (multisampled_render_to_texture).
  void DoRenderbufferStorageMultisampleEXT(
      GLenum target, GLsizei samples, GLenum internalformat,
      GLsizei width, GLsizei height);

  // Wrapper for glFenceSync.
  GLsync DoFenceSync(GLenum condition, GLbitfield flags);

  GLsizei InternalFormatSampleCountsHelper(
      GLenum target,
      GLenum format,
      std::vector<GLint>* out_sample_counts);

  // Common validation for multisample extensions.
  bool ValidateRenderbufferStorageMultisample(GLsizei samples,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height);

  // validation for multisample AMD extension.
  bool ValidateRenderbufferStorageMultisampleAMD(GLsizei samples,
                                                 GLsizei storageSamples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height);

  // Wrapper for glReleaseShaderCompiler.
  void DoReleaseShaderCompiler();

  // Wrappers for glSamplerParameter functions.
  void DoSamplerParameterf(GLuint client_id, GLenum pname, GLfloat param);
  void DoSamplerParameteri(GLuint client_id, GLenum pname, GLint param);
  void DoSamplerParameterfv(GLuint client_id,
                            GLenum pname,
                            const volatile GLfloat* params);
  void DoSamplerParameteriv(GLuint client_id,
                            GLenum pname,
                            const volatile GLint* params);

  // Wrappers for glTexParameter functions.
  void DoTexParameterf(GLenum target, GLenum pname, GLfloat param);
  void DoTexParameteri(GLenum target, GLenum pname, GLint param);
  void DoTexParameterfv(GLenum target,
                        GLenum pname,
                        const volatile GLfloat* params);
  void DoTexParameteriv(GLenum target,
                        GLenum pname,
                        const volatile GLint* params);

  // Wrappers for glUniform1i and glUniform1iv as according to the GLES2
  // spec only these 2 functions can be used to set sampler uniforms.
  void DoUniform1i(GLint fake_location, GLint v0);
  void DoUniform1iv(GLint fake_location,
                    GLsizei count,
                    const volatile GLint* value);
  void DoUniform2iv(GLint fake_location,
                    GLsizei count,
                    const volatile GLint* value);
  void DoUniform3iv(GLint fake_location,
                    GLsizei count,
                    const volatile GLint* value);
  void DoUniform4iv(GLint fake_location,
                    GLsizei count,
                    const volatile GLint* value);

  void DoUniform1ui(GLint fake_location, GLuint v0);
  void DoUniform1uiv(GLint fake_location,
                     GLsizei count,
                     const volatile GLuint* value);
  void DoUniform2uiv(GLint fake_location,
                     GLsizei count,
                     const volatile GLuint* value);
  void DoUniform3uiv(GLint fake_location,
                     GLsizei count,
                     const volatile GLuint* value);
  void DoUniform4uiv(GLint fake_location,
                     GLsizei count,
                     const volatile GLuint* value);

  // Wrappers for glUniformfv because some drivers don't correctly accept
  // bool uniforms.
  void DoUniform1fv(GLint fake_location,
                    GLsizei count,
                    const volatile GLfloat* value);
  void DoUniform2fv(GLint fake_location,
                    GLsizei count,
                    const volatile GLfloat* value);
  void DoUniform3fv(GLint fake_location,
                    GLsizei count,
                    const volatile GLfloat* value);
  void DoUniform4fv(GLint fake_location,
                    GLsizei count,
                    const volatile GLfloat* value);

  void DoUniformMatrix2fv(GLint fake_location,
                          GLsizei count,
                          GLboolean transpose,
                          const volatile GLfloat* value);
  void DoUniformMatrix3fv(GLint fake_location,
                          GLsizei count,
                          GLboolean transpose,
                          const volatile GLfloat* value);
  void DoUniformMatrix4fv(GLint fake_location,
                          GLsizei count,
                          GLboolean transpose,
                          const volatile GLfloat* value);
  void DoUniformMatrix2x3fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix2x4fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix3x2fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix3x4fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix4x2fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix4x3fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);

  template <typename T>
  bool SetVertexAttribValue(
    const char* function_name, GLuint index, const T* value);

  // Wrappers for glVertexAttrib??
  void DoVertexAttrib1f(GLuint index, GLfloat v0);
  void DoVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1);
  void DoVertexAttrib3f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
  void DoVertexAttrib4f(
      GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
  void DoVertexAttrib1fv(GLuint index, const volatile GLfloat* v);
  void DoVertexAttrib2fv(GLuint index, const volatile GLfloat* v);
  void DoVertexAttrib3fv(GLuint index, const volatile GLfloat* v);
  void DoVertexAttrib4fv(GLuint index, const volatile GLfloat* v);
  void DoVertexAttribI4i(GLuint index, GLint v0, GLint v1, GLint v2, GLint v3);
  void DoVertexAttribI4iv(GLuint index, const volatile GLint* v);
  void DoVertexAttribI4ui(
      GLuint index, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
  void DoVertexAttribI4uiv(GLuint index, const volatile GLuint* v);

  // Wrapper for glViewport
  void DoViewport(GLint x, GLint y, GLsizei width, GLsizei height);

  // Wrapper for glScissor
  void DoScissor(GLint x, GLint y, GLsizei width, GLsizei height);

  // Wrapper for glUseProgram
  void DoUseProgram(GLuint program);

  // Wrapper for glValidateProgram.
  void DoValidateProgram(GLuint program_client_id);

  void DoInsertEventMarkerEXT(GLsizei length, const GLchar* marker);
  void DoPushGroupMarkerEXT(GLsizei length, const GLchar* group);
  void DoPopGroupMarkerEXT(void);

  // Wrapper for ContextVisibilityHintCHROMIUM.
  void DoContextVisibilityHintCHROMIUM(GLboolean visibility);

  // Gets the number of values that will be returned by glGetXXX. Returns
  // false if pname is unknown.
  bool GetNumValuesReturnedForGLGet(GLenum pname, GLsizei* num_values);

  // Checks if every attribute's type set by vertexAttrib API match
  // the type of corresponding attribute in vertex shader.
  bool AttribsTypeMatch();

  // Verifies that front/back stencil settings match, per WebGL specification:
  // https://www.khronos.org/registry/webgl/specs/latest/1.0/#6.11
  bool ValidateStencilStateForDraw(const char* function_name);

  // Checks if the current program and vertex attributes are valid for drawing.
  bool IsDrawValid(const char* function_name,
                   GLuint max_vertex_accessed,
                   bool instanced,
                   GLsizei primcount,
                   GLint basevertex,
                   GLuint baseinstance);

  // Returns true if successful, simulated will be true if attrib0 was
  // simulated.
  void RestoreStateForAttrib(GLuint attrib, bool restore_array_binding);

  void DoWindowRectanglesEXT(GLenum mode, GLsizei n, const volatile GLint* box);

  void DoSetReadbackBufferShadowAllocationINTERNAL(GLuint buffer_id,
                                                   GLuint shm_id,
                                                   GLuint shm_offset,
                                                   GLuint size);

  // Returns false if a GL error occurred. textures_set is always modified
  // appropriately to indicate whether textures were set, even on failure.
  bool PrepareTexturesForRender(bool* textures_set, const char* function_name);
  void RestoreStateForTextures();

  // Having extra base vertex and base instance parameters and run-time if else
  // for heavily called DoMultiDrawArrays/DoMultiDrawElements caused
  // performance regression, thus use non-type template draw functions
  enum class DrawArraysOption { Default = 0, UseBaseInstance };
  enum class DrawElementsOption { Default = 0, UseBaseVertexBaseInstance };

  template <DrawArraysOption option>
  bool CheckMultiDrawArraysVertices(const char* function_name,
                                    bool instanced,
                                    const GLint* firsts,
                                    const GLsizei* counts,
                                    const GLsizei* primcounts,
                                    const GLuint* baseinstances,
                                    GLsizei drawcount,
                                    GLuint* total_max_vertex_accessed,
                                    GLsizei* total_max_primcount);
  template <DrawElementsOption option>
  bool CheckMultiDrawElementsVertices(const char* function_name,
                                      bool instanced,
                                      const GLsizei* counts,
                                      GLenum type,
                                      const int32_t* offsets,
                                      const GLsizei* primcounts,
                                      const GLint* basevertices,
                                      const GLuint* baseinstances,
                                      GLsizei drawcount,
                                      Buffer* element_array_buffer,
                                      GLuint* total_max_vertex_accessed,
                                      GLsizei* total_max_primcount);
  bool CheckTransformFeedback(const char* function_name,
                              bool instanced,
                              GLenum mode,
                              const GLsizei* counts,
                              const GLsizei* primcounts,
                              GLsizei drawcount,
                              GLsizei* transform_feedback_vertices);

  // Handle MultiDrawArrays and MultiDrawElements for both instanced and
  // non-instanced cases (primcount is always 1 for non-instanced).
  // (basevertex and baseinstance are always 0 for non-basevertex-baseinstance
  // draws)
  template <DrawArraysOption option>
  error::Error DoMultiDrawArrays(const char* function_name,
                                 bool instanced,
                                 GLenum mode,
                                 const GLint* firsts,
                                 const GLsizei* counts,
                                 const GLsizei* primcounts,
                                 const GLuint* baseinstances,
                                 GLsizei drawcount);
  template <DrawElementsOption option>
  error::Error DoMultiDrawElements(const char* function_name,
                                   bool instanced,
                                   GLenum mode,
                                   const GLsizei* counts,
                                   GLenum type,
                                   const int32_t* offsets,
                                   const GLsizei* primcounts,
                                   const GLint* basevertices,
                                   const GLuint* baseinstances,
                                   GLsizei drawcount);

  GLenum GetBindTargetForSamplerType(GLenum type) {
    switch (type) {
      case GL_SAMPLER_2D:
      case GL_SAMPLER_2D_SHADOW:
      case GL_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_2D:
        return GL_TEXTURE_2D;
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_CUBE_SHADOW:
      case GL_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
        return GL_TEXTURE_CUBE_MAP;
      case GL_SAMPLER_EXTERNAL_OES:
        return GL_TEXTURE_EXTERNAL_OES;
      case GL_SAMPLER_2D_RECT_ARB:
        return GL_TEXTURE_RECTANGLE_ARB;
      case GL_SAMPLER_3D:
      case GL_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
        return GL_TEXTURE_3D;
      case GL_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        return GL_TEXTURE_2D_ARRAY;
      default:
        NOTREACHED_IN_MIGRATION();
        return 0;
    }
  }

  // Gets the framebuffer info for a particular target.
  Framebuffer* GetFramebufferInfoForTarget(GLenum target) const {
    Framebuffer* framebuffer = nullptr;
    switch (target) {
      case GL_FRAMEBUFFER:
      case GL_DRAW_FRAMEBUFFER_EXT:
        framebuffer = framebuffer_state_.bound_draw_framebuffer.get();
        break;
      case GL_READ_FRAMEBUFFER_EXT:
        framebuffer = framebuffer_state_.bound_read_framebuffer.get();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    return framebuffer;
  }

  Renderbuffer* GetRenderbufferInfoForTarget(
      GLenum target) {
    Renderbuffer* renderbuffer = nullptr;
    switch (target) {
      case GL_RENDERBUFFER:
        renderbuffer = state_.bound_renderbuffer.get();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    return renderbuffer;
  }

  // Validates the program and location for a glGetUniform call and returns
  // a SizeResult setup to receive the result. Returns true if glGetUniform
  // should be called.
  template <class T>
  bool GetUniformSetup(GLuint program,
                       GLint fake_location,
                       uint32_t shm_id,
                       uint32_t shm_offset,
                       error::Error* error,
                       GLint* real_location,
                       GLuint* service_id,
                       SizedResult<T>** result,
                       GLenum* result_type,
                       GLsizei* result_size);

  bool WasContextLost() const override;
  bool WasContextLostByRobustnessExtension() const override;
  void MarkContextLost(error::ContextLostReason reason) override;
  bool CheckResetStatus() override;

  bool ValidateCompressedTexDimensions(
      const char* function_name, GLenum target, GLint level,
      GLsizei width, GLsizei height, GLsizei depth, GLenum format);
  bool ValidateCompressedTexFuncData(const char* function_name,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLenum format,
                                     GLsizei size,
                                     const GLvoid* data);
  bool ValidateCompressedTexSubDimensions(
    const char* function_name,
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    Texture* texture);
  bool ValidateCopyTextureCHROMIUMTextures(const char* function_name,
                                           GLenum dest_target,
                                           TextureRef* source_texture_ref,
                                           TextureRef* dest_texture_ref);
  bool CanUseCopyTextureCHROMIUMInternalFormat(GLenum dest_internal_format);
  void CopySubTextureHelper(const char* function_name,
                            GLuint source_id,
                            GLint source_level,
                            GLenum dest_target,
                            GLuint dest_id,
                            GLint dest_level,
                            GLint xoffset,
                            GLint yoffset,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLboolean unpack_flip_y,
                            GLboolean unpack_premultiply_alpha,
                            GLboolean unpack_unmultiply_alpha);

  void RenderWarning(const char* filename, int line, const std::string& msg);
  void PerformanceWarning(
      const char* filename, int line, const std::string& msg);

  const FeatureInfo::FeatureFlags& features() const {
    return feature_info_->feature_flags();
  }

  const GpuDriverBugWorkarounds& workarounds() const {
    return feature_info_->workarounds();
  }

  // Whether the back buffer exposed to the client has an alpha channel. Note
  // that this is potentially different from whether the implementation of the
  // back buffer has an alpha channel.
  bool ClientExposedBackBufferHasAlpha() const {
    if (back_buffer_draw_buffer_ == GL_NONE)
      return false;

    if (external_default_framebuffer_ &&
        external_default_framebuffer_->IsSharedImageAttached()) {
      return external_default_framebuffer_->HasAlpha();
    }

    if (offscreen_target_frame_buffer_.get()) {
      return false;
    }
    return (back_buffer_color_format_ == GL_RGBA ||
            back_buffer_color_format_ == GL_RGBA8);
  }

  // Set remaining commands to process to 0 to force DoCommands to return
  // and allow context preemption and GPU watchdog checks in CommandExecutor().
  void ExitCommandProcessingEarly() override;

  void ProcessPendingReadPixels(bool did_finish);
  void FinishReadPixels(GLsizei width,
                        GLsizei height,
                        GLsizei format,
                        GLsizei type,
                        uint32_t pixels_shm_id,
                        uint32_t pixels_shm_offset,
                        uint32_t result_shm_id,
                        uint32_t result_shm_offset,
                        GLint pack_alignment,
                        GLenum read_format,
                        GLuint buffer);

  // Checks to see if the inserted fence has completed.
  void ProcessDescheduleUntilFinished();

  // If |texture_manager_version_| doesn't match the current version, then this
  // will rebind all external textures to match their current service_id.
  void RestoreAllExternalTextureBindingsIfNeeded() override;

  const SamplerState& GetSamplerStateForTextureUnit(GLenum target, GLuint unit);

  // Helper method to call glClear workaround.
  void ClearFramebufferForWorkaround(GLbitfield mask);

  bool SupportsSeparateFramebufferBinds() const {
    return (feature_info_->feature_flags().chromium_framebuffer_multisample ||
            feature_info_->IsWebGL2OrES3Context());
  }

  GLenum GetDrawFramebufferTarget() const {
    return SupportsSeparateFramebufferBinds() ?
        GL_DRAW_FRAMEBUFFER : GL_FRAMEBUFFER;
  }

  GLenum GetReadFramebufferTarget() const {
    return SupportsSeparateFramebufferBinds() ?
        GL_READ_FRAMEBUFFER : GL_FRAMEBUFFER;
  }

  Framebuffer* GetBoundDrawFramebuffer() const {
    return framebuffer_state_.bound_draw_framebuffer.get();
  }

  Framebuffer* GetBoundReadFramebuffer() const {
    GLenum target = GetReadFramebufferTarget();
    return GetFramebufferInfoForTarget(target);
  }

  bool InitializeCopyTexImageBlitter(const char* function_name);
  bool InitializeCopyTextureCHROMIUM(const char* function_name);

  void UnbindTexture(TextureRef* texture_ref,
                     bool supports_separate_framebuffer_binds);

  void ReadBackBuffersIntoShadowCopies(
      base::flat_set<scoped_refptr<Buffer>> buffers_to_shadow_copy);

  // Compiles the given shader and exits command processing early.
  void CompileShaderAndExitCommandProcessingEarly(Shader* shader);

  // Notify the watchdog thread of progress, preventing time-outs when a
  // command takes a long time. May be no-op when using in-process command
  // buffer.
  void ReportProgress();

  // Generate a member function prototype for each command in an automated and
  // typesafe way.
#define GLES2_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP

  // The GL context this decoder renders to on behalf of the client.
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  // The ContextGroup for this decoder uses to track resources.
  scoped_refptr<ContextGroup> group_;

  DebugMarkerManager debug_marker_manager_;
  Logger logger_;

  std::unique_ptr<ErrorState> error_state_;

  // All the state for this context.
  ContextState state_;

  std::unique_ptr<TransformFeedbackManager> transform_feedback_manager_;

  // Current width and height of the offscreen frame buffer.
  gfx::Size offscreen_size_;

  // Util to help with GL.
  GLES2Util util_;

  // The buffer we bind to attrib 0 since OpenGL requires it (ES does not).
  GLuint attrib_0_buffer_id_;

  // The value currently in attrib_0.
  Vec4 attrib_0_value_;

  // Whether or not the attrib_0 buffer holds the attrib_0_value.
  bool attrib_0_buffer_matches_value_;

  // The size of attrib 0.
  GLsizei attrib_0_size_;

  // The buffer used to simulate GL_FIXED attribs.
  GLuint fixed_attrib_buffer_id_;

  // The size of fiixed attrib buffer.
  GLsizei fixed_attrib_buffer_size_;

  // The offscreen frame buffer that the client renders to.
  std::unique_ptr<BackFramebuffer> offscreen_target_frame_buffer_;
  std::unique_ptr<BackTexture> offscreen_target_color_texture_;

  std::unique_ptr<GLES2ExternalFramebuffer> external_default_framebuffer_;

  // The format of the texture or renderbuffer backing the offscreen
  // framebuffer.
  GLenum offscreen_target_color_format_;

  GLint max_offscreen_framebuffer_size_;

  std::unique_ptr<FramebufferManager> framebuffer_manager_;

  std::unique_ptr<GLES2QueryManager> query_manager_;

  std::unique_ptr<GpuFenceManager> gpu_fence_manager_;

  std::unique_ptr<MultiDrawManager> multi_draw_manager_;

  std::unique_ptr<VertexArrayManager> vertex_array_manager_;

  base::flat_set<scoped_refptr<Buffer>> writes_submitted_but_not_completed_;

  // The format of the back buffer_
  GLenum back_buffer_color_format_;
  bool back_buffer_has_depth_;
  bool back_buffer_has_stencil_;

  // Tracks read buffer and draw buffer for backbuffer, whether it's onscreen
  // or offscreen.
  // TODO(zmo): when ES3 APIs are exposed to Nacl, make sure read_buffer_
  // setting is set correctly when SwapBuffers().
  GLenum back_buffer_read_buffer_;
  GLenum back_buffer_draw_buffer_;

  bool surfaceless_;

  // Backbuffer attachments that are currently undefined.
  uint32_t backbuffer_needs_clear_bits_;

  uint64_t swaps_since_resize_;

  // The current decoder error communicates the decoder error through command
  // processing functions that do not return the error value. Should be set only
  // if not returning an error.
  error::Error current_decoder_error_;

  bool has_fragment_precision_high_ = false;
  scoped_refptr<ShaderTranslatorInterface> vertex_translator_;
  scoped_refptr<ShaderTranslatorInterface> fragment_translator_;

  // Cached from ContextGroup
  raw_ptr<const Validators, DanglingUntriaged> validators_;
  scoped_refptr<FeatureInfo> feature_info_;

  int frame_number_;

  // Number of commands remaining to be processed in DoCommands().
  int commands_to_process_;

  bool context_was_lost_;
  bool reset_by_robustness_extension_;
  bool supports_async_swap_;

  // These flags are used to override the state of the shared feature_info_
  // member.  Because the same FeatureInfo instance may be shared among many
  // contexts, the assumptions on the availablity of extensions in WebGL
  // contexts may be broken.  These flags override the shared state to preserve
  // WebGL semantics.
  bool derivatives_explicitly_enabled_;
  bool fbo_render_mipmap_explicitly_enabled_;
  bool frag_depth_explicitly_enabled_;
  bool draw_buffers_explicitly_enabled_;
  bool shader_texture_lod_explicitly_enabled_;
  bool multi_draw_explicitly_enabled_;
  bool draw_instanced_base_vertex_base_instance_explicitly_enabled_;
  bool multi_draw_instanced_base_vertex_base_instance_explicitly_enabled_;
  bool arb_texture_rectangle_enabled_;
  bool oes_egl_image_external_enabled_;
  bool nv_egl_stream_consumer_external_enabled_;

  bool compile_shader_always_succeeds_;

  // An optional behaviour to lose the context and group when OOM.
  bool lose_context_when_out_of_memory_;

  // Log extra info.
  bool service_logging_;

  std::unique_ptr<CopyTexImageResourceManager> copy_tex_image_blit_;
  std::unique_ptr<CopyTextureCHROMIUMResourceManager> copy_texture_chromium_;
  std::unique_ptr<ClearFramebufferResourceManager> clear_framebuffer_blit_;

  // Cached values of the currently assigned viewport dimensions.
  GLsizei viewport_max_width_;
  GLsizei viewport_max_height_;

  // Cached value for the number of stencil bits for the default framebuffer.
  GLint num_stencil_bits_;

  // States related to each manager.
  DecoderTextureState texture_state_;
  DecoderFramebufferState framebuffer_state_;

  std::unique_ptr<GPUTracer> gpu_tracer_;
  std::unique_ptr<GPUStateTracer> gpu_state_tracer_;
  raw_ptr<const unsigned char> gpu_decoder_category_;
  int gpu_trace_level_;
  bool gpu_trace_commands_;
  bool gpu_debug_commands_;

  base::queue<FenceCallback> pending_readpixel_fences_;

  // After a second fence is inserted, both the GpuChannelMessageQueue and
  // CommandExecutor are descheduled. Once the first fence has completed, both
  // get rescheduled.
  std::vector<std::unique_ptr<gl::GLFence>> deschedule_until_finished_fences_;

  // Used to validate multisample renderbuffers if needed
  typedef std::unordered_map<GLenum, GLuint> TextureMap;
  TextureMap validation_textures_;
  GLuint validation_fbo_multisample_;
  GLuint validation_fbo_;

  typedef gpu::gles2::GLES2Decoder::Error (GLES2DecoderImpl::*CmdHandler)(
      uint32_t immediate_data_size,
      const volatile void* data);

  // A struct to hold info about each command.
  struct CommandInfo {
    CmdHandler cmd_handler;
    uint8_t arg_flags;   // How to handle the arguments for this command
    uint8_t cmd_flags;   // How to handle this command
    uint16_t arg_count;  // How many arguments are expected for this command.
  };

  // A table of CommandInfo for all the commands.
  static const CommandInfo command_info[kNumCommands - kFirstGLES2Command];

  // Most recent generation of the TextureManager.  If this no longer matches
  // the current generation when our context becomes current, then we'll rebind
  // all the textures to stay up to date with Texture::service_id() changes.
  uint32_t texture_manager_service_id_generation_;

  bool force_shader_name_hashing_for_test = false;

  GLfloat line_width_range_[2] = {0.0, 1.0};

  SamplerState default_sampler_state_;

  base::WeakPtrFactory<GLES2DecoderImpl> weak_ptr_factory_{this};
};

constexpr GLES2DecoderImpl::CommandInfo GLES2DecoderImpl::command_info[] = {
#define GLES2_CMD_OP(name)                                   \
  {                                                          \
    &GLES2DecoderImpl::Handle##name, cmds::name::kArgFlags,  \
        cmds::name::cmd_flags,                               \
        sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1, \
  }                                                          \
  , /* NOLINT */
    GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP
};

ScopedGLErrorSuppressor::ScopedGLErrorSuppressor(
    const char* function_name, ErrorState* error_state)
    : function_name_(function_name),
      error_state_(error_state) {
  ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(error_state_, function_name_);
}

ScopedGLErrorSuppressor::~ScopedGLErrorSuppressor() {
  ERRORSTATE_CLEAR_REAL_GL_ERRORS(error_state_, function_name_);
}

static void RestoreCurrentTextureBindings(ContextState* state,
                                          GLenum target,
                                          GLuint texture_unit) {
  DCHECK(!state->texture_units.empty());
  DCHECK_LT(texture_unit, state->texture_units.size());
  TextureUnit& info = state->texture_units[texture_unit];
  GLuint last_id;
  TextureRef* texture_ref = info.GetInfoForTarget(target);
  if (texture_ref) {
    last_id = texture_ref->service_id();
  } else {
    last_id = 0;
  }

  state->api()->glBindTextureFn(target, last_id);
}

ScopedTextureBinder::ScopedTextureBinder(ContextState* state,
                                         ErrorState* error_state,
                                         GLuint id,
                                         GLenum target)
    : state_(state), error_state_(error_state), target_(target) {
  ScopedGLErrorSuppressor suppressor("ScopedTextureBinder::ctor", error_state_);

  // TODO(apatrick): Check if there are any other states that need to be reset
  // before binding a new texture.
  auto* api = state->api();
  api->glActiveTextureFn(GL_TEXTURE0);
  api->glBindTextureFn(target, id);
}

ScopedTextureBinder::~ScopedTextureBinder() {
  ScopedGLErrorSuppressor suppressor("ScopedTextureBinder::dtor", error_state_);
  RestoreCurrentTextureBindings(state_, target_, 0);
  state_->RestoreActiveTexture();
}

ScopedFramebufferBinder::ScopedFramebufferBinder(GLES2DecoderImpl* decoder,
                                                 GLuint id)
    : decoder_(decoder) {
  ScopedGLErrorSuppressor suppressor("ScopedFramebufferBinder::ctor",
                                     decoder_->error_state_.get());
  decoder->api()->glBindFramebufferEXTFn(GL_FRAMEBUFFER, id);
  decoder->OnFboChanged();
}

ScopedFramebufferBinder::~ScopedFramebufferBinder() {
  ScopedGLErrorSuppressor suppressor("ScopedFramebufferBinder::dtor",
                                     decoder_->error_state_.get());
  decoder_->RestoreCurrentFramebufferBindings();
}

ScopedFramebufferCopyBinder::ScopedFramebufferCopyBinder(
    GLES2DecoderImpl* decoder,
    GLint x,
    GLint y,
    GLint width,
    GLint height)
    : decoder_(decoder) {
  const Framebuffer::Attachment* attachment =
      decoder->framebuffer_state_.bound_read_framebuffer.get()
          ->GetReadBufferAttachment();
  DCHECK(attachment);
  auto* api = decoder_->api();
  api->glGenTexturesFn(1, &temp_texture_);

  ScopedTextureBinder texture_binder(&decoder->state_,
                                     decoder->error_state_.get(), temp_texture_,
                                     GL_TEXTURE_2D);
  if (width == 0 || height == 0) {
    // Copy the whole framebuffer if a rectangle isn't specified.
    api->glCopyTexImage2DFn(GL_TEXTURE_2D, 0, attachment->internal_format(), 0,
                            0, attachment->width(), attachment->height(), 0);
  } else {
    api->glCopyTexImage2DFn(GL_TEXTURE_2D, 0, attachment->internal_format(), x,
                            y, width, height, 0);
  }

  api->glGenFramebuffersEXTFn(1, &temp_framebuffer_);
  framebuffer_binder_ =
      std::make_unique<ScopedFramebufferBinder>(decoder, temp_framebuffer_);
  api->glFramebufferTexture2DEXTFn(GL_READ_FRAMEBUFFER_EXT,
                                   GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   temp_texture_, 0);
  api->glReadBufferFn(GL_COLOR_ATTACHMENT0);
}

ScopedFramebufferCopyBinder::~ScopedFramebufferCopyBinder() {
  auto* api = decoder_->api();
  framebuffer_binder_.reset();
  api->glDeleteFramebuffersEXTFn(1, &temp_framebuffer_);
  api->glDeleteTexturesFn(1, &temp_texture_);
  api->glReadBufferFn(
      decoder_->framebuffer_state_.bound_read_framebuffer.get()->read_buffer());
}

ScopedPixelUnpackState::ScopedPixelUnpackState(ContextState* state)
    : state_(state) {
  DCHECK(state_);
  state_->PushTextureUnpackState();
}

ScopedPixelUnpackState::~ScopedPixelUnpackState() {
  state_->RestoreUnpackState();
}

BackTexture::BackTexture(GLES2DecoderImpl* decoder)
    : memory_tracker_(decoder->memory_tracker()),
      bytes_allocated_(0),
      decoder_(decoder) {}

BackTexture::~BackTexture() {
  // This does not destroy the render texture because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id(), 0u);
}

inline gl::GLApi* BackTexture::api() const {
  return decoder_->api();
}

void BackTexture::Create() {
  DCHECK_EQ(id(), 0u);
  ScopedGLErrorSuppressor suppressor("BackTexture::Create",
                                     decoder_->error_state_.get());
  GLuint id;
  api()->glGenTexturesFn(1, &id);

  GLenum target = Target();
  ScopedTextureBinder binder(&decoder_->state_, decoder_->error_state_.get(),
                             id, target);

  // No client id is necessary because this texture will never be directly
  // accessed by a client, only indirectly via a mailbox.
  texture_ref_ = TextureRef::Create(decoder_->texture_manager(), 0, id);
  decoder_->texture_manager()->SetTarget(texture_ref_.get(), target);
  decoder_->texture_manager()->SetParameteri(
      "BackTexture::Create", decoder_->error_state_.get(), texture_ref_.get(),
      GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  decoder_->texture_manager()->SetParameteri(
      "BackTexture::Create", decoder_->error_state_.get(), texture_ref_.get(),
      GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  decoder_->texture_manager()->SetParameteri(
      "BackTexture::Create", decoder_->error_state_.get(), texture_ref_.get(),
      GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  decoder_->texture_manager()->SetParameteri(
      "BackTexture::Create", decoder_->error_state_.get(), texture_ref_.get(),
      GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool BackTexture::AllocateStorage(
    const gfx::Size& size, GLenum format, bool zero) {
  DCHECK_NE(id(), 0u);
  ScopedGLErrorSuppressor suppressor("BackTexture::AllocateStorage",
                                     decoder_->error_state_.get());
  ScopedTextureBinder binder(&decoder_->state_, decoder_->error_state_.get(),
                             id(), Target());
  uint32_t image_size = 0;
  GLES2Util::ComputeImageDataSizes(size.width(), size.height(), 1, format,
                                   GL_UNSIGNED_BYTE, 8, &image_size, nullptr,
                                   nullptr);

  size_ = size;
  {
    // Add extra scope to destroy zero_data and the object it owns right
    // after its usage.
    base::HeapArray<char> zero_data;
    if (zero) {
      zero_data = base::HeapArray<char>::WithSize(image_size);
    }

    api()->glTexImage2DFn(Target(),
                          0,  // mip level
                          format, size.width(), size.height(),
                          0,  // border
                          format, GL_UNSIGNED_BYTE, zero_data.data());
  }

  decoder_->texture_manager()->SetLevelInfo(
      texture_ref_.get(), Target(),
      0,  // level
      GL_RGBA, size.width(), size.height(),
      1,  // depth
      0,  // border
      GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(size));
  const bool success = api()->glGetErrorFn() == GL_NO_ERROR;

  if (success) {
    memory_tracker_.TrackMemFree(bytes_allocated_);
    bytes_allocated_ = image_size;
    memory_tracker_.TrackMemAlloc(bytes_allocated_);
  }
  return success;
}

void BackTexture::Copy() {
  DCHECK_NE(id(), 0u);
  ScopedGLErrorSuppressor suppressor("BackTexture::Copy",
                                     decoder_->error_state_.get());
  ScopedTextureBinder binder(&decoder_->state_, decoder_->error_state_.get(),
                             id(), Target());
  api()->glCopyTexSubImage2DFn(Target(),
                               0,  // level
                               0, 0, 0, 0, size_.width(), size_.height());
}

void BackTexture::Destroy() {
  if (texture_ref_) {
    ScopedGLErrorSuppressor suppressor("BackTexture::Destroy",
                                       decoder_->error_state_.get());
    texture_ref_ = nullptr;
  }
  memory_tracker_.TrackMemFree(bytes_allocated_);
  bytes_allocated_ = 0;
}

void BackTexture::Invalidate() {
  if (texture_ref_) {
    texture_ref_->ForceContextLost();
    texture_ref_ = nullptr;
  }
  memory_tracker_.TrackMemFree(bytes_allocated_);
  bytes_allocated_ = 0;
}

GLenum BackTexture::Target() {
  return GL_TEXTURE_2D;
}

BackFramebuffer::BackFramebuffer(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      id_(0) {
}

BackFramebuffer::~BackFramebuffer() {
  // This does not destroy the frame buffer because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id_, 0u);
}

inline gl::GLApi* BackFramebuffer::api() const {
  return decoder_->api();
}

void BackFramebuffer::Create() {
  ScopedGLErrorSuppressor suppressor("BackFramebuffer::Create",
                                     decoder_->error_state_.get());
  Destroy();
  api()->glGenFramebuffersEXTFn(1, &id_);
}

void BackFramebuffer::AttachRenderTexture(BackTexture* texture) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor("BackFramebuffer::AttachRenderTexture",
                                     decoder_->error_state_.get());
  ScopedFramebufferBinder binder(decoder_, id_);
  GLuint attach_id = texture ? texture->id() : 0;
  api()->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     texture->Target(), attach_id, 0);
}

void BackFramebuffer::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor("BackFramebuffer::Destroy",
                                       decoder_->error_state_.get());
    api()->glDeleteFramebuffersEXTFn(1, &id_);
    id_ = 0;
  }
}

void BackFramebuffer::Invalidate() {
  id_ = 0;
}

GLenum BackFramebuffer::CheckStatus() {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor("BackFramebuffer::CheckStatus",
                                     decoder_->error_state_.get());
  ScopedFramebufferBinder binder(decoder_, id_);
  return api()->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER);
}

GLES2Decoder* GLES2Decoder::Create(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    Outputter* outputter,
    ContextGroup* group) {
  if (group->use_passthrough_cmd_decoder()) {
    return new GLES2DecoderPassthroughImpl(client, command_buffer_service,
                                           outputter, group);
  }

// Allow linux to run fuzzers.
#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER) || BUILDFLAG(IS_LINUX)
  return new GLES2DecoderImpl(client, command_buffer_service, outputter, group);
#else
  LOG(FATAL) << "Validating command decoder is not supported.";
#endif
}

GLES2DecoderImpl::GLES2DecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    Outputter* outputter,
    ContextGroup* group)
    : GLES2Decoder(client, command_buffer_service, outputter),
      group_(group),
      logger_(&debug_marker_manager_,
              base::BindRepeating(&DecoderClient::OnConsoleMessage,
                                  base::Unretained(client),
                                  0),
              group->gpu_preferences().disable_gl_error_limit),
      error_state_(ErrorState::Create(this, &logger_)),
      state_(group_->feature_info()),
      attrib_0_buffer_id_(0),
      attrib_0_buffer_matches_value_(true),
      attrib_0_size_(0),
      fixed_attrib_buffer_id_(0),
      fixed_attrib_buffer_size_(0),
      offscreen_target_color_format_(0),
      max_offscreen_framebuffer_size_(0),
      back_buffer_color_format_(0),
      back_buffer_has_depth_(false),
      back_buffer_has_stencil_(false),
      back_buffer_read_buffer_(GL_BACK),
      back_buffer_draw_buffer_(GL_BACK),
      surfaceless_(false),
      backbuffer_needs_clear_bits_(0),
      swaps_since_resize_(0),
      current_decoder_error_(error::kNoError),
      validators_(group_->feature_info()->validators()),
      feature_info_(group_->feature_info()),
      frame_number_(0),
      context_was_lost_(false),
      reset_by_robustness_extension_(false),
      supports_async_swap_(false),
      derivatives_explicitly_enabled_(false),
      fbo_render_mipmap_explicitly_enabled_(false),
      frag_depth_explicitly_enabled_(false),
      draw_buffers_explicitly_enabled_(false),
      shader_texture_lod_explicitly_enabled_(false),
      multi_draw_explicitly_enabled_(false),
      draw_instanced_base_vertex_base_instance_explicitly_enabled_(false),
      multi_draw_instanced_base_vertex_base_instance_explicitly_enabled_(false),
      arb_texture_rectangle_enabled_(false),
      oes_egl_image_external_enabled_(false),
      nv_egl_stream_consumer_external_enabled_(false),
      compile_shader_always_succeeds_(false),
      lose_context_when_out_of_memory_(false),
      service_logging_(
          group_->gpu_preferences().enable_gpu_service_logging_gpu),
      viewport_max_width_(0),
      viewport_max_height_(0),
      num_stencil_bits_(0),
      texture_state_(group_->feature_info()->workarounds()),
      gpu_decoder_category_(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.decoder"))),
      gpu_trace_level_(2),
      gpu_trace_commands_(false),
      gpu_debug_commands_(false),
      validation_fbo_multisample_(0),
      validation_fbo_(0),
      texture_manager_service_id_generation_(0) {
  DCHECK(client);
  DCHECK(group);
}

GLES2DecoderImpl::~GLES2DecoderImpl() = default;

base::WeakPtr<DecoderContext> GLES2DecoderImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

gpu::ContextResult GLES2DecoderImpl::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const DisallowedFeatures& disallowed_features,
    const ContextCreationAttribs& attrib_helper) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::Initialize");
  DCHECK(context->IsCurrent(surface.get()));
  DCHECK(!context_.get());
  state_.set_api(gl::g_current_gl_context);

  surfaceless_ = surface->IsSurfaceless() && !offscreen;

  set_initialized();
  // At this point we are partially initialized and must Destroy() in any
  // failure case.
  DestroyOnFailure destroy_on_failure(this);

  gpu_state_tracer_ = GPUStateTracer::Create(&state_);

  if (group_->gpu_preferences().enable_gpu_debugging)
    set_debug(true);

  if (group_->gpu_preferences().enable_gpu_command_logging)
    SetLogCommands(true);

  compile_shader_always_succeeds_ =
      group_->gpu_preferences().compile_shader_always_succeeds;

  // Take ownership of the context and surface. The surface can be replaced with
  // SetSurface.
  context_ = context;
  surface_ = surface;

  // Create GPU Tracer for timing values.
  gpu_tracer_ = std::make_unique<GPUTracer>(this);

  if (workarounds().disable_timestamp_queries) {
    // Forcing time elapsed query for any GPU Timing Client forces it for all
    // clients in the context.
    GetGLContext()->CreateGPUTimingClient()->ForceTimeElapsedQuery();
  }

  // Save the loseContextWhenOutOfMemory context creation attribute.
  lose_context_when_out_of_memory_ =
      attrib_helper.lose_context_when_out_of_memory;

  // If the failIfMajorPerformanceCaveat context creation attribute was true
  // and we are using a software renderer, fail.
  if (attrib_helper.fail_if_major_perf_caveat &&
      feature_info_->feature_flags().is_swiftshader_for_webgl) {
    // Must not destroy ContextGroup if it is not initialized.
    group_ = nullptr;
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "fail_if_major_perf_caveat + swiftshader";
    return gpu::ContextResult::kFatalFailure;
  }

  // Only create ES 3.1 contexts with the passthrough cmd decoder.
  if (attrib_helper.context_type == CONTEXT_TYPE_OPENGLES31_FOR_TESTING) {
    // Must not destroy ContextGroup if it is not initialized.
    group_ = nullptr;
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "ES 3.1 is not supported on validating command decoder.";
    return gpu::ContextResult::kFatalFailure;
  }

  auto result =
      group_->Initialize(this, attrib_helper.context_type, disallowed_features);
  if (result != gpu::ContextResult::kSuccess) {
    // Must not destroy ContextGroup if it is not initialized.
    group_ = nullptr;
    return result;
  }
  CHECK_GL_ERROR();

  // In theory |needs_emulation| needs to be true on Desktop GL 4.1 or lower.
  // However, we set it to true everywhere, not to trust drivers to handle
  // out-of-bounds buffer accesses.
  bool needs_emulation = true;
  transform_feedback_manager_ = std::make_unique<TransformFeedbackManager>(
      group_->max_transform_feedback_separate_attribs(), needs_emulation);

  // Register this object as a GPU switching observer.
  if (feature_info_->IsWebGLContext()) {
    ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  }

  if (feature_info_->IsWebGL2OrES3Context()) {
    // Verified in ContextGroup.
    DCHECK(feature_info_->IsES3Capable());
    feature_info_->EnableES3Validators();

    frag_depth_explicitly_enabled_ = true;
    draw_buffers_explicitly_enabled_ = true;
    // TODO(zmo): Look into shader_texture_lod_explicitly_enabled_ situation.

    // Create a fake default transform feedback and bind to it.
    GLuint default_transform_feedback = 0;
    api()->glGenTransformFeedbacksFn(1, &default_transform_feedback);
    state_.default_transform_feedback =
        transform_feedback_manager_->CreateTransformFeedback(
            0, default_transform_feedback);
    api()->glBindTransformFeedbackFn(GL_TRANSFORM_FEEDBACK,
                                     default_transform_feedback);
    state_.bound_transform_feedback = state_.default_transform_feedback.get();
    state_.bound_transform_feedback->SetIsBound(true);
  }
  state_.indexed_uniform_buffer_bindings =
      base::MakeRefCounted<gles2::IndexedBufferBindingHost>(
          group_->max_uniform_buffer_bindings(), GL_UNIFORM_BUFFER,
          needs_emulation,
          workarounds().round_down_uniform_bind_buffer_range_size);
  state_.indexed_uniform_buffer_bindings->SetIsBound(true);

  state_.InitGenericAttribs(group_->max_vertex_attribs());
  vertex_array_manager_ = std::make_unique<VertexArrayManager>();

  GLuint default_vertex_attrib_service_id = 0;
  if (features().native_vertex_array_object) {
    api()->glGenVertexArraysOESFn(1, &default_vertex_attrib_service_id);
    api()->glBindVertexArrayOESFn(default_vertex_attrib_service_id);
  }

  state_.default_vertex_attrib_manager =
      CreateVertexAttribManager(0, default_vertex_attrib_service_id, false);

  state_.default_vertex_attrib_manager->Initialize(
      group_->max_vertex_attribs());

  // vertex_attrib_manager is set to default_vertex_attrib_manager by this call
  DoBindVertexArrayOES(0);

  framebuffer_manager_ = std::make_unique<FramebufferManager>(
      group_->max_draw_buffers(), group_->max_color_attachments(),
      group_->framebuffer_completeness_cache());
  group_->texture_manager()->AddFramebufferManager(framebuffer_manager_.get());

  query_manager_ =
      std::make_unique<GLES2QueryManager>(this, feature_info_.get());

  gpu_fence_manager_ = std::make_unique<GpuFenceManager>();

  multi_draw_manager_ = std::make_unique<MultiDrawManager>(
      MultiDrawManager::IndexStorageType::Offset);

  util_.set_num_compressed_texture_formats(
      validators_->compressed_texture_format.GetValues().size());

  api()->glGenBuffersARBFn(1, &attrib_0_buffer_id_);
  api()->glBindBufferFn(GL_ARRAY_BUFFER, attrib_0_buffer_id_);
  api()->glVertexAttribPointerFn(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
  api()->glBindBufferFn(GL_ARRAY_BUFFER, 0);
  api()->glGenBuffersARBFn(1, &fixed_attrib_buffer_id_);

  state_.texture_units.resize(group_->max_texture_units());
  state_.sampler_units.resize(group_->max_texture_units());
  for (uint32_t tt = 0; tt < state_.texture_units.size(); ++tt) {
    api()->glActiveTextureFn(GL_TEXTURE0 + tt);
    // We want the last bind to be 2D.
    TextureRef* ref;
    if (features().oes_egl_image_external ||
        features().nv_egl_stream_consumer_external) {
      ref = texture_manager()->GetDefaultTextureInfo(
          GL_TEXTURE_EXTERNAL_OES);
      state_.texture_units[tt].bound_texture_external_oes = ref;
      api()->glBindTextureFn(GL_TEXTURE_EXTERNAL_OES,
                             ref ? ref->service_id() : 0);
    }
    if (features().arb_texture_rectangle) {
      ref = texture_manager()->GetDefaultTextureInfo(
          GL_TEXTURE_RECTANGLE_ARB);
      state_.texture_units[tt].bound_texture_rectangle_arb = ref;
      api()->glBindTextureFn(GL_TEXTURE_RECTANGLE_ARB,
                             ref ? ref->service_id() : 0);
    }
    ref = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_CUBE_MAP);
    state_.texture_units[tt].bound_texture_cube_map = ref;
    api()->glBindTextureFn(GL_TEXTURE_CUBE_MAP, ref ? ref->service_id() : 0);
    ref = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_2D);
    state_.texture_units[tt].bound_texture_2d = ref;
    api()->glBindTextureFn(GL_TEXTURE_2D, ref ? ref->service_id() : 0);
  }
  api()->glActiveTextureFn(GL_TEXTURE0);
  CHECK_GL_ERROR();

  // cache ALPHA_BITS result for re-use with clear behaviour
  GLint alpha_bits = 0;

  if (offscreen) {
    offscreen_target_color_format_ =
        workarounds().disable_gl_rgb_format ? GL_RGBA : GL_RGB;

    max_offscreen_framebuffer_size_ =
        std::min(renderbuffer_manager()->max_renderbuffer_size(),
                 texture_manager()->MaxSizeForTarget(GL_TEXTURE_2D));

    // Use 64x64 instead of 1x1 to handle minimum framebuffer size
    // requirement on some platforms: b/265847440.
    gfx::Size initial_size = gfx::Size(64, 64);

    state_.viewport_width = initial_size.width();
    state_.viewport_height = initial_size.height();
  } else {
    api()->glBindFramebufferEXTFn(GL_FRAMEBUFFER, GetBackbufferServiceId());
    // These are NOT if the back buffer has these proprorties. They are
    // if we want the command buffer to enforce them regardless of what
    // the real backbuffer is assuming the real back buffer gives us more than
    // we ask for. In other words, if we ask for RGB and we get RGBA then we'll
    // make it appear RGB. If on the other hand we ask for RGBA nd get RGB we
    // can't do anything about that.

    if (!surfaceless_) {
      GLint depth_bits = 0;
      GLint stencil_bits = 0;

      api()->glGetIntegervFn(GL_ALPHA_BITS, &alpha_bits);
      api()->glGetIntegervFn(GL_DEPTH_BITS, &depth_bits);
      api()->glGetIntegervFn(GL_STENCIL_BITS, &stencil_bits);

      back_buffer_color_format_ = alpha_bits > 0 ? GL_RGBA : GL_RGB;
      back_buffer_has_depth_ = depth_bits > 0;
      back_buffer_has_stencil_ = stencil_bits > 0;
      num_stencil_bits_ = stencil_bits;
    } else {
      num_stencil_bits_ = 0;
    }

    state_.viewport_width = surface->GetSize().width();
    state_.viewport_height = surface->GetSize().height();
  }

  GLint range[2] = {0, 0};
  GLint precision = 0;
  QueryShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range,
                             &precision);
  has_fragment_precision_high_ =
      PrecisionMeetsSpecForHighpFloat(range[0], range[1], precision);

  GLint viewport_params[4] = { 0 };
  api()->glGetIntegervFn(GL_MAX_VIEWPORT_DIMS, viewport_params);
  viewport_max_width_ = viewport_params[0];
  viewport_max_height_ = viewport_params[1];

  api()->glGetFloatvFn(GL_ALIASED_LINE_WIDTH_RANGE, line_width_range_);
  state_.SetLineWidthBounds(line_width_range_[0], line_width_range_[1]);

  if (feature_info_->feature_flags().ext_window_rectangles) {
    GLint max_window_rectangles = -1;
    api()->glGetIntegervFn(GL_MAX_WINDOW_RECTANGLES_EXT,
                           &max_window_rectangles);
    state_.SetMaxWindowRectangles(max_window_rectangles);
  }

  state_.scissor_width = state_.viewport_width;
  state_.scissor_height = state_.viewport_height;

  // Set all the default state because some GL drivers get it wrong.
  state_.InitCapabilities(nullptr);
  state_.InitState(nullptr);

  // Default state must be set before offscreen resources can be created.
  if (offscreen) {
    // Create the target frame buffer. This is the one that the client renders
    // directly to.
    offscreen_target_frame_buffer_ = std::make_unique<BackFramebuffer>(this);
    offscreen_target_frame_buffer_->Create();
    offscreen_target_color_texture_ = std::make_unique<BackTexture>(this);
    offscreen_target_color_texture_->Create();

    // Allocate the render buffers at their initial size and check the status
    // of the frame buffers is okay.
    if (!ResizeOffscreenFramebuffer(
            gfx::Size(state_.viewport_width, state_.viewport_height))) {
      LOG(ERROR) << "ContextResult::kFatalFailure: "
                    "Could not allocate offscreen buffer storage.";
      return gpu::ContextResult::kFatalFailure;
    }
  }

  api()->glActiveTextureFn(GL_TEXTURE0 + state_.active_texture_unit);

  DoBindBuffer(GL_ARRAY_BUFFER, 0);
  DoBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  DoBindFramebuffer(GL_FRAMEBUFFER, 0);
  DoBindRenderbuffer(GL_RENDERBUFFER, 0);
  UpdateFramebufferSRGB(nullptr);

  bool call_gl_clear = !surfaceless_ && !offscreen;
  // Temporary workaround for Android WebView because this clear ignores the
  // clip and corrupts that external UI of the App. Not calling glClear is ok
  // because the system already clears the buffer before each draw. Proper
  // fix might be setting the scissor clip properly before initialize. See
  // crbug.com/259023 for details.
  call_gl_clear = surface_->GetHandle();
  if (call_gl_clear) {
    // On configs where we report no alpha, if the underlying surface has
    // alpha, clear the surface alpha to 1.0 to be correct on ReadPixels/etc.
    bool clear_alpha = back_buffer_color_format_ == GL_RGB && alpha_bits > 0;
    if (clear_alpha) {
      api()->glClearColorFn(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Clear the backbuffer.
    api()->glClearFn(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                     GL_STENCIL_BUFFER_BIT);

    // Restore alpha clear value if we changed it.
    if (clear_alpha) {
      api()->glClearColorFn(0.0f, 0.0f, 0.0f, 0.0f);
    }
  }

  supports_async_swap_ = surface->SupportsAsyncSwap();

  if (workarounds().unbind_fbo_on_context_switch) {
    context_->SetUnbindFboOnMakeCurrent();
  }

  if (workarounds().gl_clear_broken) {
    DCHECK(!clear_framebuffer_blit_.get());
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glClearWorkaroundInit");
    clear_framebuffer_blit_ =
        std::make_unique<ClearFramebufferResourceManager>(this);
    if (LOCAL_PEEK_GL_ERROR("glClearWorkaroundInit") != GL_NO_ERROR) {
      LOG(ERROR) << "ContextResult::kFatalFailure: "
                    "glClearWorkaroundInit failed";
      return gpu::ContextResult::kFatalFailure;
    }
  }

  if (group_->gpu_preferences().enable_gpu_driver_debug_logging &&
      feature_info_->feature_flags().khr_debug) {
    InitializeGLDebugLogging(true, GLDebugMessageCallback, &logger_);
  }

  if (CheckResetStatus()) {
    // If the context was lost at any point before or during initialization, the
    // values queried from the driver could be bogus, and potentially
    // inconsistent between various ContextStates on the same underlying real GL
    // context. Make sure to report the failure early, to not allow virtualized
    // context switches in that case.
    LOG(ERROR)
        << "  GLES2DecoderImpl: Context reset detected after initialization.";
    group_->LoseContexts(error::kUnknown);
    destroy_on_failure.LoseContext();
    return gpu::ContextResult::kTransientFailure;
  }

  destroy_on_failure.OnSuccess();
  return gpu::ContextResult::kSuccess;
}

Capabilities GLES2DecoderImpl::GetCapabilities() {
  DCHECK(initialized());
  Capabilities caps;

  DoGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps.max_texture_size, 1);
  if (workarounds().webgl_or_caps_max_texture_size) {
    caps.max_texture_size =
        std::min(caps.max_texture_size,
                 feature_info_->workarounds().webgl_or_caps_max_texture_size);
  }

  caps.egl_image_external =
      feature_info_->feature_flags().oes_egl_image_external;
  caps.egl_image_external_essl3 =
      feature_info_->feature_flags().oes_egl_image_external_essl3;
  caps.texture_format_bgra8888 =
      feature_info_->feature_flags().ext_texture_format_bgra8888;
  caps.texture_format_etc1_npot =
      feature_info_->feature_flags().oes_compressed_etc1_rgb8_texture &&
      !workarounds().etc1_power_of_two_only;
  // Vulkan currently doesn't support single-component cross-thread shared
  // images.
  caps.disable_one_component_textures =
      group_->shared_image_manager() &&
      group_->shared_image_manager()->display_context_on_another_thread() &&
      (workarounds().avoid_one_component_egl_images ||
       features::IsUsingVulkan());
  caps.sync_query = feature_info_->feature_flags().chromium_sync_query;

  // Only query the kEnableMSAAOnNewIntelGPUs feature flag if the host device
  // is affected by the experiment.
  bool eligible_for_experiment =
      workarounds().msaa_is_slow && !workarounds().msaa_is_slow_2;
  caps.msaa_is_slow =
      eligible_for_experiment
          ? !base::FeatureList::IsEnabled(features::kEnableMSAAOnNewIntelGPUs)
          : workarounds().msaa_is_slow;
  caps.avoid_stencil_buffers = workarounds().avoid_stencil_buffers;
  caps.texture_rg = feature_info_->feature_flags().ext_texture_rg;
  caps.texture_norm16 = feature_info_->feature_flags().ext_texture_norm16;
  caps.texture_half_float_linear =
      feature_info_->oes_texture_half_float_linear_available();
  caps.image_ycbcr_420v =
      feature_info_->feature_flags().chromium_image_ycbcr_420v;
  caps.image_ar30 = feature_info_->feature_flags().chromium_image_ar30;
  caps.image_ab30 = feature_info_->feature_flags().chromium_image_ab30;
  caps.image_ycbcr_p010 =
      feature_info_->feature_flags().chromium_image_ycbcr_p010;
  caps.max_copy_texture_chromium_size =
      workarounds().max_copy_texture_chromium_size;
  caps.render_buffer_format_bgra8888 =
      feature_info_->feature_flags().ext_render_buffer_format_bgra8888;
  caps.gpu_rasterization = false;
  if (workarounds().broken_egl_image_ref_counting &&
      group_->gpu_preferences().enable_threaded_texture_mailboxes) {
    caps.disable_2d_canvas_copy_on_write = true;
  }
  caps.chromium_gpu_fence = feature_info_->feature_flags().chromium_gpu_fence;
  caps.mesa_framebuffer_flip_y =
      feature_info_->feature_flags().mesa_framebuffer_flip_y;

  caps.gpu_memory_buffer_formats =
      feature_info_->feature_flags().gpu_memory_buffer_formats;

  caps.angle_rgbx_internal_format =
      feature_info_->feature_flags().angle_rgbx_internal_format;

  // Technically, YUV readback is handled on the client side, but enable it here
  // so that clients can use this to detect support.
  caps.supports_yuv_readback = true;

  return caps;
}

GLCapabilities GLES2DecoderImpl::GetGLCapabilities() {
  CHECK(initialized());
  GLCapabilities caps;

  caps.VisitPrecisions([](GLenum shader, GLenum type,
                          GLCapabilities::ShaderPrecision* shader_precision) {
    GLint range[2] = {0, 0};
    GLint precision = 0;
    QueryShaderPrecisionFormat(shader, type, range, &precision);
    shader_precision->min_range = range[0];
    shader_precision->max_range = range[1];
    shader_precision->precision = precision;
  });

  if (feature_info_->IsWebGL2OrES3Context()) {
    // TODO(zmo): once we switch to MANGLE, we should query version numbers.
    caps.major_version = 3;
    caps.minor_version = 0;
  }
  DoGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps.max_texture_size, 1);
  if (workarounds().webgl_or_caps_max_texture_size) {
    caps.max_texture_size =
        std::min(caps.max_texture_size,
                 feature_info_->workarounds().webgl_or_caps_max_texture_size);
  }

  DoGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                &caps.max_combined_texture_image_units, 1);
  DoGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &caps.max_cube_map_texture_size,
                1);
  DoGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS,
                &caps.max_fragment_uniform_vectors, 1);
  DoGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &caps.max_renderbuffer_size, 1);
  DoGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &caps.max_texture_image_units, 1);

  DoGetIntegerv(GL_MAX_VARYING_VECTORS, &caps.max_varying_vectors, 1);
  DoGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &caps.max_vertex_attribs, 1);
  DoGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
                &caps.max_vertex_texture_image_units, 1);
  DoGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &caps.max_vertex_uniform_vectors,
                1);
  {
    GLint dims[2] = {0, 0};
    DoGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims, 2);
    caps.max_viewport_width = dims[0];
    caps.max_viewport_height = dims[1];
  }
  DoGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS,
                &caps.num_compressed_texture_formats, 1);
  DoGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &caps.num_shader_binary_formats,
                1);
  DoGetIntegerv(GL_BIND_GENERATES_RESOURCE_CHROMIUM,
                &caps.bind_generates_resource_chromium, 1);
  if (feature_info_->IsWebGL2OrES3Context()) {
    // TODO(zmo): Note that some parameter values could be more than 32-bit,
    // but for now we clamp them to 32-bit max.
    DoGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &caps.max_3d_texture_size, 1);
    DoGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &caps.max_array_texture_layers,
                  1);
    DoGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &caps.max_color_attachments, 1);
    DoGetInteger64v(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,
                    &caps.max_combined_fragment_uniform_components, 1);
    DoGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS,
                  &caps.max_combined_uniform_blocks, 1);
    DoGetInteger64v(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,
                    &caps.max_combined_vertex_uniform_components, 1);
    DoGetIntegerv(GL_MAX_DRAW_BUFFERS, &caps.max_draw_buffers, 1);
    DoGetInteger64v(GL_MAX_ELEMENT_INDEX, &caps.max_element_index, 1);
    DoGetIntegerv(GL_MAX_ELEMENTS_INDICES, &caps.max_elements_indices, 1);
    DoGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &caps.max_elements_vertices, 1);
    DoGetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS,
                  &caps.max_fragment_input_components, 1);
    DoGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS,
                  &caps.max_fragment_uniform_blocks, 1);
    DoGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
                  &caps.max_fragment_uniform_components, 1);
    DoGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, &caps.max_program_texel_offset,
                  1);
    DoGetInteger64v(GL_MAX_SERVER_WAIT_TIMEOUT, &caps.max_server_wait_timeout,
                    1);
    // Work around Linux NVIDIA driver bug where GL_TIMEOUT_IGNORED is
    // returned.
    if (caps.max_server_wait_timeout < 0) {
      caps.max_server_wait_timeout = 0;
    }
    DoGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &caps.max_texture_lod_bias, 1);
    DoGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
                  &caps.max_transform_feedback_interleaved_components, 1);
    DoGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
                  &caps.max_transform_feedback_separate_attribs, 1);
    DoGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
                  &caps.max_transform_feedback_separate_components, 1);
    DoGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &caps.max_uniform_block_size, 1);
    DoGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS,
                  &caps.max_uniform_buffer_bindings, 1);
    DoGetIntegerv(GL_MAX_VARYING_COMPONENTS, &caps.max_varying_components, 1);
    DoGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS,
                  &caps.max_vertex_output_components, 1);
    DoGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &caps.max_vertex_uniform_blocks,
                  1);
    DoGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,
                  &caps.max_vertex_uniform_components, 1);
    DoGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, &caps.min_program_texel_offset,
                  1);
    // TODO(vmiura): Remove GL_NUM_PROGRAM_BINARY_FORMATS.
    DoGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS,
                  &caps.num_program_binary_formats, 1);
    DoGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                  &caps.uniform_buffer_offset_alignment, 1);
  }
  if (feature_info_->feature_flags().multisampled_render_to_texture ||
      feature_info_->feature_flags().chromium_framebuffer_multisample ||
      feature_info_->IsWebGL2OrES3Context()) {
    caps.max_samples = ComputeMaxSamples();
  }
  caps.occlusion_query_boolean =
      feature_info_->feature_flags().occlusion_query_boolean;
  caps.timer_queries = query_manager_->GPUTimingAvailable();

  caps.sync_query = feature_info_->feature_flags().chromium_sync_query;

  return caps;
}

GLint GLES2DecoderImpl::ComputeMaxSamples() {
  GLint max_samples = 0;
  DoGetIntegerv(GL_MAX_SAMPLES, &max_samples, 1);

  if (feature_info_->IsWebGLContext() &&
      feature_info_->feature_flags().nv_internalformat_sample_query) {
    std::vector<GLint> temp;

    auto minWithSamplesForFormat = [&](GLenum internalformat) {
      temp.clear();
      InternalFormatSampleCountsHelper(GL_RENDERBUFFER, internalformat, &temp);
      max_samples = std::min(max_samples, temp[0]);
    };

    // OpenGL ES 3.0.5, section 4.4.2.2: "Implementations must support creation
    // of renderbuffers in these required formats with up to the value of
    // MAX_SAMPLES multisamples, with the exception of signed and unsigned
    // integer formats."

    // OpenGL ES 3.0.5, section 3.8.3.1
    minWithSamplesForFormat(GL_RGBA8);
    minWithSamplesForFormat(GL_SRGB8_ALPHA8);
    minWithSamplesForFormat(GL_RGB10_A2);
    minWithSamplesForFormat(GL_RGBA4);
    minWithSamplesForFormat(GL_RGB5_A1);
    minWithSamplesForFormat(GL_RGB8);
    minWithSamplesForFormat(GL_RGB565);
    minWithSamplesForFormat(GL_RG8);
    minWithSamplesForFormat(GL_R8);
  }

  return max_samples;
}

void GLES2DecoderImpl::UpdateCapabilities() {
  util_.set_num_compressed_texture_formats(
      validators_->compressed_texture_format.GetValues().size());
  util_.set_num_shader_binary_formats(
      validators_->shader_binary_format.GetValues().size());
}

bool GLES2DecoderImpl::InitializeShaderTranslator() {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::InitializeShaderTranslator");
  if (feature_info_->disable_shader_translator()) {
    return true;
  }
  if (vertex_translator_ || fragment_translator_) {
    DCHECK(vertex_translator_ && fragment_translator_);
    return true;
  }
  ShBuiltInResources resources;
  sh::InitBuiltInResources(&resources);
  resources.MaxVertexAttribs = group_->max_vertex_attribs();
  resources.MaxVertexUniformVectors =
      group_->max_vertex_uniform_vectors();
  resources.MaxVaryingVectors = group_->max_varying_vectors();
  resources.MaxVertexTextureImageUnits =
      group_->max_vertex_texture_image_units();
  resources.MaxCombinedTextureImageUnits = group_->max_texture_units();
  resources.MaxTextureImageUnits = group_->max_texture_image_units();
  resources.MaxFragmentUniformVectors =
      group_->max_fragment_uniform_vectors();
  resources.MaxDrawBuffers = group_->max_draw_buffers();
  resources.MaxExpressionComplexity = 256;
  resources.MaxCallStackDepth = 256;
  resources.MaxDualSourceDrawBuffers = group_->max_dual_source_draw_buffers();

  if (!feature_info_->IsWebGL1OrES2Context()) {
    resources.MaxVertexOutputVectors =
        group_->max_vertex_output_components() / 4;
    resources.MaxFragmentInputVectors =
        group_->max_fragment_input_components() / 4;
    resources.MaxProgramTexelOffset = group_->max_program_texel_offset();
    resources.MinProgramTexelOffset = group_->min_program_texel_offset();
  }

  resources.FragmentPrecisionHigh = has_fragment_precision_high_;
  resources.EXT_YUV_target = features().ext_yuv_target ? 1 : 0;

  ShShaderSpec shader_spec;
  switch (feature_info_->context_type()) {
    case CONTEXT_TYPE_WEBGL1:
      shader_spec = SH_WEBGL_SPEC;
      resources.OES_standard_derivatives = derivatives_explicitly_enabled_;
      resources.ARB_texture_rectangle =
          (features().arb_texture_rectangle && arb_texture_rectangle_enabled_)
              ? 1
              : 0;
      resources.OES_EGL_image_external =
          (features().oes_egl_image_external && oes_egl_image_external_enabled_)
              ? 1
              : 0;
      resources.NV_EGL_stream_consumer_external =
          (features().nv_egl_stream_consumer_external &&
           nv_egl_stream_consumer_external_enabled_)
              ? 1
              : 0;
      resources.EXT_frag_depth = frag_depth_explicitly_enabled_;
      resources.EXT_draw_buffers = draw_buffers_explicitly_enabled_;
      if (!draw_buffers_explicitly_enabled_)
        resources.MaxDrawBuffers = 1;
      resources.EXT_shader_texture_lod = shader_texture_lod_explicitly_enabled_;
      resources.NV_draw_buffers =
          draw_buffers_explicitly_enabled_ && features().nv_draw_buffers;
      break;
    case CONTEXT_TYPE_WEBGL2:
      shader_spec = SH_WEBGL2_SPEC;
      resources.ARB_texture_rectangle =
          (features().arb_texture_rectangle && arb_texture_rectangle_enabled_)
              ? 1
              : 0;
      resources.OES_EGL_image_external =
          (features().oes_egl_image_external && oes_egl_image_external_enabled_)
              ? 1
              : 0;
      resources.NV_EGL_stream_consumer_external =
          (features().nv_egl_stream_consumer_external &&
           nv_egl_stream_consumer_external_enabled_)
              ? 1
              : 0;
      break;
    case CONTEXT_TYPE_OPENGLES2:
      shader_spec = SH_GLES2_SPEC;
      resources.OES_standard_derivatives =
          features().oes_standard_derivatives ? 1 : 0;
      resources.ARB_texture_rectangle =
          features().arb_texture_rectangle ? 1 : 0;
      resources.OES_EGL_image_external =
          features().oes_egl_image_external ? 1 : 0;
      resources.NV_EGL_stream_consumer_external =
          features().nv_egl_stream_consumer_external ? 1 : 0;
      resources.EXT_draw_buffers =
          features().ext_draw_buffers ? 1 : 0;
      resources.EXT_frag_depth =
          features().ext_frag_depth ? 1 : 0;
      resources.EXT_shader_texture_lod =
          features().ext_shader_texture_lod ? 1 : 0;
      resources.NV_draw_buffers =
          features().nv_draw_buffers ? 1 : 0;
      resources.EXT_blend_func_extended =
          features().ext_blend_func_extended ? 1 : 0;
      break;
    case CONTEXT_TYPE_OPENGLES3:
      shader_spec = SH_GLES3_SPEC;
      resources.ARB_texture_rectangle =
          features().arb_texture_rectangle ? 1 : 0;
      resources.OES_EGL_image_external =
          features().oes_egl_image_external ? 1 : 0;
      resources.NV_EGL_stream_consumer_external =
          features().nv_egl_stream_consumer_external ? 1 : 0;
      resources.EXT_blend_func_extended =
          features().ext_blend_func_extended ? 1 : 0;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      shader_spec = SH_GLES2_SPEC;
      break;
  }

  if (shader_spec == SH_WEBGL_SPEC || shader_spec == SH_WEBGL2_SPEC) {
    resources.ANGLE_multi_draw =
        multi_draw_explicitly_enabled_ && features().webgl_multi_draw;
  }

  if (shader_spec == SH_WEBGL2_SPEC) {
    // The gl_BaseVertex/BaseInstance shader builtins is disabled in ANGLE for
    // WebGL As they are removed in
    // https://github.com/KhronosGroup/WebGL/pull/3278
    // To re-enable the shader
    // builtins add back SH_EMULATE_GL_BASE_VERTEX_BASE_INSTANCE to
    // ShCompileOptions in ANGLE
    resources.ANGLE_base_vertex_base_instance_shader_builtin =
        (draw_instanced_base_vertex_base_instance_explicitly_enabled_ &&
         features().webgl_draw_instanced_base_vertex_base_instance) ||
        (multi_draw_instanced_base_vertex_base_instance_explicitly_enabled_ &&
         features().webgl_multi_draw_instanced_base_vertex_base_instance);
  }

  if (((shader_spec == SH_WEBGL_SPEC || shader_spec == SH_WEBGL2_SPEC) &&
       features().enable_shader_name_hashing) ||
      force_shader_name_hashing_for_test) {
    // TODO(crbug.com/40601370): In theory, it should be OK to change this
    // hash. However, in practice, this seems to cause some tests to fail. See
    // https://crbug.com/963889.
    resources.HashFunction = +[](const char* data, size_t size) {
      return base::legacy::CityHash64(
          base::as_bytes(base::make_span(data, size)));
    };
  } else {
    resources.HashFunction = nullptr;
  }

  ShCompileOptions driver_bug_workarounds{};
  if (workarounds().init_gl_position_in_vertex_shader)
    driver_bug_workarounds.initGLPosition = true;
  if (workarounds().scalarize_vec_and_mat_constructor_args)
    driver_bug_workarounds.scalarizeVecAndMatConstructorArgs = true;
  if (workarounds().dont_use_loops_to_initialize_variables)
    driver_bug_workarounds.dontUseLoopsToInitializeVariables = true;
  if (workarounds().remove_dynamic_indexing_of_swizzled_vector)
    driver_bug_workarounds.removeDynamicIndexingOfSwizzledVector = true;

  // Initialize uninitialized locals by default
  driver_bug_workarounds.initializeUninitializedLocals = true;

  vertex_translator_ = shader_translator_cache()->GetTranslator(
      GL_VERTEX_SHADER, shader_spec, &resources, SH_ESSL_OUTPUT,
      driver_bug_workarounds);
  if (!vertex_translator_.get()) {
    LOG(ERROR) << "Could not initialize vertex shader translator.";
    Destroy(true);
    return false;
  }

  fragment_translator_ = shader_translator_cache()->GetTranslator(
      GL_FRAGMENT_SHADER, shader_spec, &resources, SH_ESSL_OUTPUT,
      driver_bug_workarounds);
  if (!fragment_translator_.get()) {
    LOG(ERROR) << "Could not initialize fragment shader translator.";
    Destroy(true);
    return false;
  }
  return true;
}

void GLES2DecoderImpl::DestroyShaderTranslator() {
  vertex_translator_ = nullptr;
  fragment_translator_ = nullptr;
}

bool GLES2DecoderImpl::GenBuffersHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetBuffer(client_ids[ii])) {
      return false;
    }
  }
  auto service_ids = base::HeapArray<GLuint>::Uninit(n);
  api()->glGenBuffersARBFn(n, service_ids.data());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateBuffer(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenFramebuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetFramebuffer(client_ids[ii])) {
      return false;
    }
  }
  auto service_ids = base::HeapArray<GLuint>::Uninit(n);
  api()->glGenFramebuffersEXTFn(n, service_ids.data());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateFramebuffer(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenRenderbuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetRenderbuffer(client_ids[ii])) {
      return false;
    }
  }
  auto service_ids = base::HeapArray<GLuint>::Uninit(n);
  api()->glGenRenderbuffersEXTFn(n, service_ids.data());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateRenderbuffer(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenTexturesHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetTexture(client_ids[ii])) {
      return false;
    }
  }
  auto service_ids = base::HeapArray<GLuint>::Uninit(n);
  api()->glGenTexturesFn(n, service_ids.data());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateTexture(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenSamplersHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetSampler(client_ids[ii])) {
      return false;
    }
  }
  auto service_ids = base::HeapArray<GLuint>::Uninit(n);
  api()->glGenSamplersFn(n, service_ids.data());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateSampler(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenTransformFeedbacksHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetTransformFeedback(client_ids[ii])) {
      return false;
    }
  }
  auto service_ids = base::HeapArray<GLuint>::Uninit(n);
  api()->glGenTransformFeedbacksFn(n, service_ids.data());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateTransformFeedback(client_ids[ii], service_ids[ii]);
  }
  return true;
}

void GLES2DecoderImpl::DeleteBuffersHelper(GLsizei n,
                                           const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    Buffer* buffer = GetBuffer(client_id);
    if (buffer && !buffer->IsDeleted()) {
      if (buffer->GetMappedRange()) {
        // The buffer is not guaranteed to still be bound to any binding point,
        // even though it is mapped. If it is not bound, we will need to bind
        // it temporarily in order to unmap it.
        GLenum target = buffer->initial_target();
        Buffer* currently_bound =
            buffer_manager()->GetBufferInfoForTarget(&state_, target);
        if (currently_bound != buffer) {
          api()->glBindBufferFn(target, buffer->service_id());
        }
        UnmapBufferHelper(buffer, target);
        if (currently_bound != buffer) {
          api()->glBindBufferFn(
              target, currently_bound ? currently_bound->service_id() : 0);
        }
      }
      state_.RemoveBoundBuffer(buffer);
      buffer_manager()->RemoveBuffer(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteFramebuffersHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    Framebuffer* framebuffer = GetFramebuffer(client_id);
    if (framebuffer && !framebuffer->IsDeleted()) {
      if (framebuffer == framebuffer_state_.bound_draw_framebuffer.get()) {
        GLenum target = GetDrawFramebufferTarget();

        // Unbind attachments on FBO before deletion.
        if (workarounds().unbind_attachments_on_bound_render_fbo_delete)
          framebuffer->DoUnbindGLAttachmentsForWorkaround(target);

        api()->glBindFramebufferEXTFn(target, GetBackbufferServiceId());
        state_.UpdateWindowRectanglesForBoundDrawFramebufferClientID(0);
        framebuffer_state_.bound_draw_framebuffer = nullptr;
        framebuffer_state_.clear_state_dirty = true;
      }
      if (framebuffer == framebuffer_state_.bound_read_framebuffer.get()) {
        framebuffer_state_.bound_read_framebuffer = nullptr;
        GLenum target = GetReadFramebufferTarget();
        api()->glBindFramebufferEXTFn(target, GetBackbufferServiceId());
      }
      OnFboChanged();
      RemoveFramebuffer(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteRenderbuffersHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  bool supports_separate_framebuffer_binds =
     features().chromium_framebuffer_multisample;
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    Renderbuffer* renderbuffer = GetRenderbuffer(client_id);
    if (renderbuffer && !renderbuffer->IsDeleted()) {
      if (state_.bound_renderbuffer.get() == renderbuffer) {
        state_.bound_renderbuffer = nullptr;
      }
      // Unbind from current framebuffers.
      if (supports_separate_framebuffer_binds) {
        if (framebuffer_state_.bound_read_framebuffer.get()) {
          framebuffer_state_.bound_read_framebuffer
              ->UnbindRenderbuffer(GL_READ_FRAMEBUFFER_EXT, renderbuffer);
        }
        if (framebuffer_state_.bound_draw_framebuffer.get()) {
          framebuffer_state_.bound_draw_framebuffer
              ->UnbindRenderbuffer(GL_DRAW_FRAMEBUFFER_EXT, renderbuffer);
        }
      } else {
        if (framebuffer_state_.bound_draw_framebuffer.get()) {
          framebuffer_state_.bound_draw_framebuffer
              ->UnbindRenderbuffer(GL_FRAMEBUFFER, renderbuffer);
        }
      }
      framebuffer_state_.clear_state_dirty = true;
      RemoveRenderbuffer(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteTexturesHelper(GLsizei n,
                                            const volatile GLuint* client_ids) {
  bool supports_separate_framebuffer_binds = SupportsSeparateFramebufferBinds();
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    TextureRef* texture_ref = GetTexture(client_id);
    if (texture_ref) {
      UnbindTexture(texture_ref, supports_separate_framebuffer_binds);
      RemoveTexture(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteSamplersHelper(GLsizei n,
                                            const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    Sampler* sampler = GetSampler(client_id);
    if (sampler && !sampler->IsDeleted()) {
      // Unbind from current sampler units.
      state_.UnbindSampler(sampler);

      RemoveSampler(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteTransformFeedbacksHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    TransformFeedback* transform_feedback = GetTransformFeedback(client_id);
    if (transform_feedback) {
      if (transform_feedback->active()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glDeleteTransformFeedbacks",
                           "Deleting transform feedback is active");
        return;
      }
      if (state_.bound_transform_feedback.get() == transform_feedback) {
        // Bind to the default transform feedback.
        DCHECK(state_.default_transform_feedback.get());
        state_.default_transform_feedback->DoBindTransformFeedback(
            GL_TRANSFORM_FEEDBACK, state_.bound_transform_feedback.get(),
            state_.bound_transform_feedback_buffer.get());
        state_.bound_transform_feedback =
            state_.default_transform_feedback.get();
      }
      RemoveTransformFeedback(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteSyncHelper(GLuint sync) {
  GLsync service_id = 0;
  if (group_->GetSyncServiceId(sync, &service_id)) {
    api()->glDeleteSyncFn(service_id);
    group_->RemoveSyncId(sync);
  } else if (sync != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDeleteSync", "unknown sync");
  }
}

bool GLES2DecoderImpl::MakeCurrent() {
  if (!context_.get())
    return false;

  if (WasContextLost()) {
    LOG(ERROR) << "  GLES2DecoderImpl: Trying to make lost context current.";
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "  GLES2DecoderImpl: Context lost during MakeCurrent.";
    MarkContextLost(error::kMakeCurrentFailed);
    group_->LoseContexts(error::kUnknown);
    return false;
  }
  DCHECK_EQ(api(), gl::g_current_gl_context);

  if (CheckResetStatus()) {
    LOG(ERROR)
        << "  GLES2DecoderImpl: Context reset detected after MakeCurrent.";
    group_->LoseContexts(error::kUnknown);
    return false;
  }

  ProcessFinishedAsyncTransfers();

  // Rebind the FBO if it was unbound by the context.
  if (workarounds().unbind_fbo_on_context_switch)
    RestoreFramebufferBindings();

  framebuffer_state_.clear_state_dirty = true;
  state_.stencil_state_changed_since_validation = true;

  // Rebind textures if the service ids may have changed.
  RestoreAllExternalTextureBindingsIfNeeded();

  return true;
}

void GLES2DecoderImpl::ProcessFinishedAsyncTransfers() {
  ProcessPendingReadPixels(false);
}

static void RebindCurrentFramebuffer(gl::GLApi* api,
                                     GLenum target,
                                     Framebuffer* framebuffer,
                                     GLuint back_buffer_service_id) {
  GLuint framebuffer_id = framebuffer ? framebuffer->service_id() : 0;

  if (framebuffer_id == 0) {
    framebuffer_id = back_buffer_service_id;
  }

  api->glBindFramebufferEXTFn(target, framebuffer_id);
}

void GLES2DecoderImpl::RestoreCurrentFramebufferBindings() {
  framebuffer_state_.clear_state_dirty = true;

  if (!SupportsSeparateFramebufferBinds()) {
    RebindCurrentFramebuffer(api(), GL_FRAMEBUFFER,
                             framebuffer_state_.bound_draw_framebuffer.get(),
                             GetBackbufferServiceId());
  } else {
    RebindCurrentFramebuffer(api(), GL_READ_FRAMEBUFFER_EXT,
                             framebuffer_state_.bound_read_framebuffer.get(),
                             GetBackbufferServiceId());
    RebindCurrentFramebuffer(api(), GL_DRAW_FRAMEBUFFER_EXT,
                             framebuffer_state_.bound_draw_framebuffer.get(),
                             GetBackbufferServiceId());
  }
  OnFboChanged();
}

bool GLES2DecoderImpl::CheckFramebufferValid(
    Framebuffer* framebuffer,
    GLenum target,
    GLenum gl_error,
    const char* func_name) {
  if (!framebuffer) {
    if (surfaceless_)
      return false;
    if (backbuffer_needs_clear_bits_) {
      api()->glClearColorFn(0, 0, 0, 1.0f);
      state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      api()->glClearStencilFn(0);
      state_.SetDeviceStencilMaskSeparate(GL_FRONT, kDefaultStencilMask);
      state_.SetDeviceStencilMaskSeparate(GL_BACK, kDefaultStencilMask);
      api()->glClearDepthFn(1.0f);
      state_.SetDeviceDepthMask(GL_TRUE);
      state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
      ClearDeviceWindowRectangles();
      bool reset_draw_buffer = false;
      if ((backbuffer_needs_clear_bits_ & GL_COLOR_BUFFER_BIT) != 0 &&
          back_buffer_draw_buffer_ == GL_NONE) {
        reset_draw_buffer = true;
        GLenum buf = GL_BACK;
        if (GetBackbufferServiceId() != 0)  // emulated backbuffer
          buf = GL_COLOR_ATTACHMENT0;
        api()->glDrawBuffersARBFn(1, &buf);
      }
      if (workarounds().gl_clear_broken) {
        ClearFramebufferForWorkaround(backbuffer_needs_clear_bits_);
      } else {
        api()->glClearFn(backbuffer_needs_clear_bits_);
      }
      if (reset_draw_buffer) {
        GLenum buf = GL_NONE;
        api()->glDrawBuffersARBFn(1, &buf);
      }
      backbuffer_needs_clear_bits_ = 0;
      RestoreClearState();
    }
    return true;
  }

  if (!framebuffer_manager()->IsComplete(framebuffer)) {
    GLenum completeness = framebuffer->IsPossiblyComplete(feature_info_.get());
    if (completeness != GL_FRAMEBUFFER_COMPLETE) {
      LOCAL_SET_GL_ERROR(gl_error, func_name, "framebuffer incomplete");
      return false;
    }

    if (framebuffer->GetStatus(texture_manager(), target) !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOCAL_SET_GL_ERROR(
          gl_error, func_name, "framebuffer incomplete (check)");
      return false;
    }
    framebuffer_manager()->MarkAsComplete(framebuffer);
  }

  // Are all the attachments cleared?
  if (renderbuffer_manager()->HaveUnclearedRenderbuffers() ||
      texture_manager()->HaveUnclearedMips()) {
    if (!framebuffer->IsCleared()) {
      ClearUnclearedAttachments(target, framebuffer);
    }
  }
  return true;
}

bool GLES2DecoderImpl::CheckBoundDrawFramebufferValid(
    const char* func_name,
    bool check_float_blending) {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  bool valid = CheckFramebufferValid(
      framebuffer, GetDrawFramebufferTarget(),
      GL_INVALID_FRAMEBUFFER_OPERATION, func_name);
  if (!valid)
    return false;

  if (check_float_blending) {
    // only is true when called by DoMultiDrawArrays or DoMultiDrawElements
    if (framebuffer && state_.GetEnabled(GL_BLEND) &&
        !features().ext_float_blend) {
      if (framebuffer->HasActiveFloat32ColorAttachment()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                           "GL_BLEND with floating-point color attachments "
                           "requires the EXT_float_blend extension");
        return false;
      }
    }
  }

  if (!SupportsSeparateFramebufferBinds())
    OnUseFramebuffer();

  UpdateFramebufferSRGB(framebuffer);
  return true;
}

void GLES2DecoderImpl::UpdateFramebufferSRGB(Framebuffer* framebuffer) {
  // Manually set the value of FRAMEBUFFER_SRGB based on the state that was set
  // by the client.
  bool needs_enable_disable_framebuffer_srgb = false;
  bool enable_framebuffer_srgb = true;
  if (feature_info_->feature_flags().ext_srgb_write_control) {
    needs_enable_disable_framebuffer_srgb = true;
    enable_framebuffer_srgb &= state_.GetEnabled(GL_FRAMEBUFFER_SRGB);
  }
  if (needs_enable_disable_framebuffer_srgb)
    state_.EnableDisableFramebufferSRGB(enable_framebuffer_srgb);
}

bool GLES2DecoderImpl::CheckBoundReadFramebufferValid(
    const char* func_name, GLenum gl_error) {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  bool valid = CheckFramebufferValid(
      framebuffer, GetReadFramebufferTarget(), gl_error, func_name);
  return valid;
}

bool GLES2DecoderImpl::CheckBoundFramebufferValid(const char* func_name) {
  GLenum gl_error = GL_INVALID_FRAMEBUFFER_OPERATION;
  Framebuffer* draw_framebuffer = GetBoundDrawFramebuffer();
  bool valid = CheckFramebufferValid(
      draw_framebuffer, GetDrawFramebufferTarget(), gl_error, func_name);

  Framebuffer* read_framebuffer = GetBoundReadFramebuffer();
  valid = valid && CheckFramebufferValid(
      read_framebuffer, GetReadFramebufferTarget(), gl_error, func_name);
  return valid;
}

bool GLES2DecoderImpl::FormsTextureCopyingFeedbackLoop(
    TextureRef* texture, GLint level, GLint layer) {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (!framebuffer)
    return false;
  const Framebuffer::Attachment* attachment =
      framebuffer->GetReadBufferAttachment();
  if (!attachment)
    return false;
  return attachment->FormsFeedbackLoop(texture, level, layer);
}

gfx::Size GLES2DecoderImpl::GetBoundReadFramebufferSize() {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (framebuffer) {
    return framebuffer->GetFramebufferValidSize();
  } else if (external_default_framebuffer_ &&
             external_default_framebuffer_->IsSharedImageAttached()) {
    return external_default_framebuffer_->GetSize();
  } else if (offscreen_target_frame_buffer_.get()) {
    return offscreen_size_;
  } else {
    return surface_->GetSize();
  }
}

gfx::Size GLES2DecoderImpl::GetBoundDrawFramebufferSize() {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer) {
    return framebuffer->GetFramebufferValidSize();
  } else if (external_default_framebuffer_ &&
             external_default_framebuffer_->IsSharedImageAttached()) {
    return external_default_framebuffer_->GetSize();
  } else if (offscreen_target_frame_buffer_.get()) {
    return offscreen_size_;
  } else {
    return surface_->GetSize();
  }
}

GLuint GLES2DecoderImpl::GetBoundReadFramebufferServiceId() {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (framebuffer) {
    return framebuffer->service_id();
  }
  if (external_default_framebuffer_ &&
      external_default_framebuffer_->IsSharedImageAttached()) {
    return external_default_framebuffer_->GetFramebufferId();
  }
  if (surface_.get()) {
    return surface_->GetBackingFramebufferObject();
  }
  return 0;
}

GLuint GLES2DecoderImpl::GetBoundDrawFramebufferServiceId() const {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer) {
    return framebuffer->service_id();
  }
  if (external_default_framebuffer_ &&
      external_default_framebuffer_->IsSharedImageAttached()) {
    return external_default_framebuffer_->GetFramebufferId();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_frame_buffer_->id();
  }
  if (surface_.get()) {
    return surface_->GetBackingFramebufferObject();
  }
  return 0;
}

GLenum GLES2DecoderImpl::GetBoundReadFramebufferTextureType() {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (framebuffer) {
    return framebuffer->GetReadBufferTextureType();
  } else {  // Back buffer.
    if (back_buffer_read_buffer_ == GL_NONE)
      return 0;
    return GL_UNSIGNED_BYTE;
  }
}

GLenum GLES2DecoderImpl::GetBoundReadFramebufferInternalFormat() {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (framebuffer) {
    return framebuffer->GetReadBufferInternalFormat();
  } else {  // Back buffer.
    if (back_buffer_read_buffer_ == GL_NONE)
      return 0;
    if (external_default_framebuffer_ &&
        external_default_framebuffer_->IsSharedImageAttached()) {
      return external_default_framebuffer_->GetColorFormat();
    }
    if (offscreen_target_frame_buffer_.get()) {
      return offscreen_target_color_format_;
    }
    return back_buffer_color_format_;
  }
}

GLenum GLES2DecoderImpl::GetBoundColorDrawBufferType(GLint drawbuffer_i) {
  DCHECK(drawbuffer_i >= 0 &&
         drawbuffer_i < static_cast<GLint>(group_->max_draw_buffers()));
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (!framebuffer) {
    return 0;
  }
  GLenum drawbuffer = static_cast<GLenum>(GL_DRAW_BUFFER0 + drawbuffer_i);
  if (framebuffer->GetDrawBuffer(drawbuffer) == GL_NONE) {
    return 0;
  }
  GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + drawbuffer_i);
  const Framebuffer::Attachment* buffer =
      framebuffer->GetAttachment(attachment);
  if (!buffer) {
    return 0;
  }
  return buffer->texture_type();
}

GLenum GLES2DecoderImpl::GetBoundColorDrawBufferInternalFormat(
    GLint drawbuffer_i) {
  DCHECK(drawbuffer_i >= 0 &&
         drawbuffer_i < static_cast<GLint>(group_->max_draw_buffers()));
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (!framebuffer) {
    return 0;
  }
  GLenum drawbuffer = static_cast<GLenum>(GL_DRAW_BUFFER0 + drawbuffer_i);
  if (framebuffer->GetDrawBuffer(drawbuffer) == GL_NONE) {
    return 0;
  }
  GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + drawbuffer_i);
  const Framebuffer::Attachment* buffer =
      framebuffer->GetAttachment(attachment);
  if (!buffer) {
    return 0;
  }
  return buffer->internal_format();
}

GLsizei GLES2DecoderImpl::GetBoundFramebufferSamples(GLenum target) {
  DCHECK(target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER ||
         target == GL_FRAMEBUFFER);
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (framebuffer) {
    return framebuffer->GetSamples();
  } else {  // Back buffer.
    if (external_default_framebuffer_ &&
        external_default_framebuffer_->IsSharedImageAttached()) {
      return external_default_framebuffer_->GetSamplesCount();
    }
    return 0;
  }
}

GLenum GLES2DecoderImpl::GetBoundFramebufferDepthFormat(
    GLenum target) {
  DCHECK(target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER ||
         target == GL_FRAMEBUFFER);
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (framebuffer) {
    return framebuffer->GetDepthFormat();
  } else {  // Back buffer.
    if (external_default_framebuffer_ &&
        external_default_framebuffer_->IsSharedImageAttached()) {
      return external_default_framebuffer_->GetDepthFormat();
    }
    if (offscreen_target_frame_buffer_.get()) {
      return 0;
    }
    if (back_buffer_has_depth_)
      return GL_DEPTH;
    return 0;
  }
}

GLenum GLES2DecoderImpl::GetBoundFramebufferStencilFormat(
    GLenum target) {
  DCHECK(target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER ||
         target == GL_FRAMEBUFFER);
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (framebuffer) {
    return framebuffer->GetStencilFormat();
  } else {  // Back buffer.
    if (external_default_framebuffer_ &&
        external_default_framebuffer_->IsSharedImageAttached()) {
      return external_default_framebuffer_->GetStencilFormat();
    }
    if (offscreen_target_frame_buffer_.get()) {
      return 0;
    }
    if (back_buffer_has_stencil_)
      return GL_STENCIL;
    return 0;
  }
}

void GLES2DecoderImpl::MarkDrawBufferAsCleared(
    GLenum buffer, GLint drawbuffer_i) {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (!framebuffer)
    return;
  GLenum attachment  = 0;
  switch (buffer) {
    case GL_COLOR:
      DCHECK(drawbuffer_i >= 0 &&
             drawbuffer_i < static_cast<GLint>(group_->max_draw_buffers()));
      attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + drawbuffer_i);
      break;
    case GL_DEPTH:
      attachment = GL_DEPTH_ATTACHMENT;
      break;
    case GL_STENCIL:
      attachment = GL_STENCIL_ATTACHMENT;
      break;
    default:
      // Caller is responsible for breaking GL_DEPTH_STENCIL into GL_DEPTH and
      // GL_STENCIL.
      NOTREACHED_IN_MIGRATION();
  }
  framebuffer->MarkAttachmentAsCleared(
      renderbuffer_manager(), texture_manager(), attachment, true);
}

Logger* GLES2DecoderImpl::GetLogger() {
  return &logger_;
}

void GLES2DecoderImpl::BeginDecoding() {
  gpu_tracer_->BeginDecoding();
  gpu_trace_commands_ = gpu_tracer_->IsTracing() && *gpu_decoder_category_;
  gpu_debug_commands_ = log_commands() || debug() || gpu_trace_commands_;
  query_manager_->ProcessFrameBeginUpdates();
  query_manager_->BeginProcessingCommands();
}

void GLES2DecoderImpl::EndDecoding() {
  gpu_tracer_->EndDecoding();
  query_manager_->EndProcessingCommands();
}

ErrorState* GLES2DecoderImpl::GetErrorState() {
  return error_state_.get();
}

bool GLES2DecoderImpl::GetServiceTextureId(uint32_t client_texture_id,
                                           uint32_t* service_texture_id) {
  TextureRef* texture_ref = texture_manager()->GetTexture(client_texture_id);
  if (texture_ref) {
    *service_texture_id = texture_ref->service_id();
    return true;
  }
  return false;
}

TextureBase* GLES2DecoderImpl::GetTextureBase(uint32_t client_id) {
  TextureRef* texture_ref = texture_manager()->GetTexture(client_id);
  return texture_ref ? texture_ref->texture() : nullptr;
}

void GLES2DecoderImpl::SetLevelInfo(uint32_t client_id,
                                    int level,
                                    unsigned internal_format,
                                    unsigned width,
                                    unsigned height,
                                    unsigned depth,
                                    unsigned format,
                                    unsigned type,
                                    const gfx::Rect& cleared_rect) {
  TextureRef* texture_ref = texture_manager()->GetTexture(client_id);
  texture_manager()->SetLevelInfo(texture_ref, texture_ref->texture()->target(),
                                  level, internal_format, width, height, depth,
                                  0 /* border */, format, type, cleared_rect);
}

void GLES2DecoderImpl::OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) {
  // Send OnGpuSwitched notification to renderer process via decoder client.
  client()->OnGpuSwitched(active_gpu_heuristic);
}

void GLES2DecoderImpl::Destroy(bool have_context) {
  if (!initialized())
    return;

  DCHECK(!have_context || context_->IsCurrent(nullptr));

  if (external_default_framebuffer_) {
    external_default_framebuffer_->Destroy(have_context);
    external_default_framebuffer_.reset();
  }

  if (have_context) {
    if (copy_tex_image_blit_.get()) {
      copy_tex_image_blit_->Destroy();
      copy_tex_image_blit_.reset();
    }

    if (copy_texture_chromium_.get()) {
      copy_texture_chromium_->Destroy();
      copy_texture_chromium_.reset();
    }

    if (clear_framebuffer_blit_.get()) {
      clear_framebuffer_blit_->Destroy();
      clear_framebuffer_blit_.reset();
    }

    if (state_.current_program.get()) {
      program_manager()->UnuseProgram(shader_manager(),
                                      state_.current_program.get());
    }

    if (attrib_0_buffer_id_) {
      api()->glDeleteBuffersARBFn(1, &attrib_0_buffer_id_);
    }
    if (fixed_attrib_buffer_id_) {
      api()->glDeleteBuffersARBFn(1, &fixed_attrib_buffer_id_);
    }

    if (validation_fbo_) {
      api()->glDeleteFramebuffersEXTFn(1, &validation_fbo_multisample_);
      api()->glDeleteFramebuffersEXTFn(1, &validation_fbo_);
    }
    while (!validation_textures_.empty()) {
      GLuint tex;
      tex = validation_textures_.begin()->second;
      api()->glDeleteTexturesFn(1, &tex);
      validation_textures_.erase(validation_textures_.begin());
    }

    if (offscreen_target_frame_buffer_.get())
      offscreen_target_frame_buffer_->Destroy();
    if (offscreen_target_color_texture_.get())
      offscreen_target_color_texture_->Destroy();
  } else {
    if (offscreen_target_frame_buffer_.get())
      offscreen_target_frame_buffer_->Invalidate();
    if (offscreen_target_color_texture_.get())
      offscreen_target_color_texture_->Invalidate();
    for (auto& fence : deschedule_until_finished_fences_) {
      fence->Invalidate();
    }

    if (group_ && group_->texture_manager())
      group_->texture_manager()->MarkContextLost();
    state_.MarkContextLost();
  }
  deschedule_until_finished_fences_.clear();

  ReportProgress();

  // Unbind everything.
  state_.vertex_attrib_manager = nullptr;
  state_.default_vertex_attrib_manager = nullptr;
  state_.texture_units.clear();
  state_.sampler_units.clear();
  state_.bound_array_buffer = nullptr;
  state_.bound_copy_read_buffer = nullptr;
  state_.bound_copy_write_buffer = nullptr;
  state_.bound_pixel_pack_buffer = nullptr;
  state_.bound_pixel_unpack_buffer = nullptr;
  state_.bound_transform_feedback_buffer = nullptr;
  state_.bound_uniform_buffer = nullptr;
  framebuffer_state_.bound_read_framebuffer = nullptr;
  framebuffer_state_.bound_draw_framebuffer = nullptr;
  state_.current_draw_framebuffer_client_id = 0;
  state_.bound_renderbuffer = nullptr;
  state_.bound_transform_feedback = nullptr;
  state_.default_transform_feedback = nullptr;
  state_.indexed_uniform_buffer_bindings = nullptr;

  // Current program must be cleared after calling ProgramManager::UnuseProgram.
  // Otherwise, we can leak objects. http://crbug.com/258772.
  // state_.current_program must be reset before group_ is reset because
  // the later deletes the ProgramManager object that referred by
  // state_.current_program object.
  state_.current_program = nullptr;

  copy_tex_image_blit_.reset();
  copy_texture_chromium_.reset();
  clear_framebuffer_blit_.reset();

  ReportProgress();

  if (framebuffer_manager_.get()) {
    framebuffer_manager_->Destroy(have_context);
    if (group_->texture_manager())
      group_->texture_manager()->RemoveFramebufferManager(
          framebuffer_manager_.get());
    framebuffer_manager_.reset();
  }

  multi_draw_manager_.reset();

  if (query_manager_.get()) {
    query_manager_->Destroy(have_context);
    query_manager_.reset();
  }

  if (gpu_fence_manager_.get()) {
    gpu_fence_manager_->Destroy(have_context);
    gpu_fence_manager_.reset();
  }

  if (vertex_array_manager_ .get()) {
    vertex_array_manager_->Destroy(have_context);
    vertex_array_manager_.reset();
  }

  if (transform_feedback_manager_.get()) {
    if (!have_context) {
      transform_feedback_manager_->MarkContextLost();
    }
    transform_feedback_manager_->Destroy();
    transform_feedback_manager_.reset();
  }

  ReportProgress();

  offscreen_target_frame_buffer_.reset();
  offscreen_target_color_texture_.reset();

  // Release all fences now, because some fence types need the context to be
  // current on destruction.
  pending_readpixel_fences_ = base::queue<FenceCallback>();

  // Need to release these before releasing |group_| which may own the
  // ShaderTranslatorCache.
  DestroyShaderTranslator();

  ReportProgress();

  // Destroy the GPU Tracer which may own some in process GPU Timings.
  if (gpu_tracer_) {
    gpu_tracer_->Destroy(have_context);
    gpu_tracer_.reset();
  }

  // Unregister this object as a GPU switching observer.
  if (feature_info_->IsWebGLContext()) {
    ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  }

  if (group_.get()) {
    group_->Destroy(this, have_context);
    group_ = nullptr;
  }

  if (context_.get()) {
    context_->ReleaseCurrent(nullptr);
    context_ = nullptr;
  }
  surface_ = nullptr;
}

void GLES2DecoderImpl::SetSurface(const scoped_refptr<gl::GLSurface>& surface) {
  DCHECK(context_->IsCurrent(nullptr));
  DCHECK(surface);
  surface_ = surface;
  RestoreCurrentFramebufferBindings();
}

void GLES2DecoderImpl::ReleaseSurface() {
  if (!context_.get())
    return;
  if (WasContextLost()) {
    DLOG(ERROR) << "  GLES2DecoderImpl: Trying to release lost context.";
    return;
  }
  context_->ReleaseCurrent(surface_.get());
  surface_ = nullptr;
}

void GLES2DecoderImpl::SetDefaultFramebufferSharedImage(const Mailbox& mailbox,
                                                        int samples,
                                                        bool preserve,
                                                        bool needs_depth,
                                                        bool needs_stencil) {
  if (!external_default_framebuffer_) {
    external_default_framebuffer_ = std::make_unique<GLES2ExternalFramebuffer>(
        /*passthrough=*/false, *group_->feature_info(),
        group_->shared_image_representation_factory());
  }

  if (!external_default_framebuffer_->AttachSharedImage(
          mailbox, samples, preserve, needs_depth, needs_stencil)) {
    return;
  }
  RestoreCurrentFramebufferBindings();
}

error::Error GLES2DecoderImpl::HandleCreateGpuFenceINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CreateGpuFenceINTERNAL& c =
      *static_cast<const volatile gles2::cmds::CreateGpuFenceINTERNAL*>(
          cmd_data);
  if (!features().chromium_gpu_fence) {
    return error::kUnknownCommand;
  }
  GLuint gpu_fence_id = static_cast<GLuint>(c.gpu_fence_id);
  if (!GetGpuFenceManager()->CreateGpuFence(gpu_fence_id))
    return error::kInvalidArguments;
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleWaitGpuFenceCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::WaitGpuFenceCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::WaitGpuFenceCHROMIUM*>(cmd_data);
  if (!features().chromium_gpu_fence) {
    return error::kUnknownCommand;
  }
  GLuint gpu_fence_id = static_cast<GLuint>(c.gpu_fence_id);
  if (!GetGpuFenceManager()->GpuFenceServerWait(gpu_fence_id))
    return error::kInvalidArguments;
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDestroyGpuFenceCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DestroyGpuFenceCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::DestroyGpuFenceCHROMIUM*>(
          cmd_data);
  if (!features().chromium_gpu_fence) {
    return error::kUnknownCommand;
  }
  GLuint gpu_fence_id = static_cast<GLuint>(c.gpu_fence_id);
  if (!GetGpuFenceManager()->RemoveGpuFence(gpu_fence_id))
    return error::kInvalidArguments;
  return error::kNoError;
}

bool GLES2DecoderImpl::ResizeOffscreenFramebuffer(const gfx::Size& size) {
  bool is_offscreen = !!offscreen_target_frame_buffer_.get();
  if (!is_offscreen) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer called "
               << " with an onscreen framebuffer.";
    return false;
  }

  if (offscreen_size_ == size)
    return true;

  offscreen_size_ = size;
  int w = offscreen_size_.width();
  int h = offscreen_size_.height();
  if (w < 0 || h < 0 || w > max_offscreen_framebuffer_size_ ||
      h > max_offscreen_framebuffer_size_) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
               << "to allocate storage due to excessive dimensions.";
    return false;
  }

  // Reallocate the offscreen target buffers.
  DCHECK(offscreen_target_color_format_);
  if (!offscreen_target_color_texture_->AllocateStorage(
          offscreen_size_, offscreen_target_color_format_, false)) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
               << "to allocate storage for offscreen target color texture.";
    return false;
  }

  // Attach the offscreen target buffers to the target frame buffer.
  offscreen_target_frame_buffer_->AttachRenderTexture(
      offscreen_target_color_texture_.get());

  if (offscreen_target_frame_buffer_->CheckStatus() !=
      GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
                 << "because offscreen FBO was incomplete.";
    return false;
  }

  // Clear the target frame buffer.
  {
    ScopedFramebufferBinder binder(this, offscreen_target_frame_buffer_->id());
    api()->glClearColorFn(0, 0, 0, 1.0f);
    state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    api()->glClearStencilFn(0);
    state_.SetDeviceStencilMaskSeparate(GL_FRONT, kDefaultStencilMask);
    state_.SetDeviceStencilMaskSeparate(GL_BACK, kDefaultStencilMask);
    api()->glClearDepthFn(0);
    state_.SetDeviceDepthMask(GL_TRUE);
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
    ClearDeviceWindowRectangles();
    api()->glClearFn(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                     GL_STENCIL_BUFFER_BIT);
    RestoreClearState();
  }

  return true;
}

error::Error GLES2DecoderImpl::HandleResizeCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ResizeCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ResizeCHROMIUM*>(cmd_data);

  GLuint width = static_cast<GLuint>(c.width);
  GLuint height = static_cast<GLuint>(c.height);
  GLfloat scale_factor = c.scale_factor;
  GLboolean has_alpha = c.alpha;
  gfx::ColorSpace color_space;
  if (!ReadColorSpace(c.shm_id, c.shm_offset, c.color_space_size,
                      &color_space)) {
    return error::kOutOfBounds;
  }
  TRACE_EVENT2("gpu", "glResizeChromium", "width", width, "height", height);

  // gfx::Size uses integers, make sure width and height do not overflow
  static_assert(sizeof(GLuint) >= sizeof(int), "Unexpected GLuint size.");
  static const GLuint kMaxDimension =
      static_cast<GLuint>(std::numeric_limits<int>::max());
  width = std::clamp(width, 1U, kMaxDimension);
  height = std::clamp(height, 1U, kMaxDimension);

  bool is_offscreen = !!offscreen_target_frame_buffer_.get();
  if (is_offscreen) {
    // We don't support Resize on the offscreen contexts.
    LOG(ERROR) << "Resize called for the offscreen context";
    return error::kUnknownCommand;
  } else {
    if (!surface_->Resize(gfx::Size(width, height), scale_factor, color_space,
                          !!has_alpha)) {
      LOG(ERROR) << "GLES2DecoderImpl: Context lost because resize failed.";
      return error::kLostContext;
    }
    DCHECK(context_->IsCurrent(surface_.get()));
    if (!context_->IsCurrent(surface_.get())) {
      LOG(ERROR) << "GLES2DecoderImpl: Context lost because context no longer "
                 << "current after resize callback.";
      return error::kLostContext;
    }
    if (surface_->BuffersFlipped()) {
      backbuffer_needs_clear_bits_ |= GL_COLOR_BUFFER_BIT;
    }
  }

  swaps_since_resize_ = 0;

  return error::kNoError;
}

const char* GLES2DecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstGLES2Command && command_id < kNumCommands) {
    return gles2::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

// Decode multiple commands, and call the corresponding GL functions.
// NOTE: 'buffer' is a pointer to the command buffer. As such, it could be
// changed by a (malicious) client at any time, so if validation has to happen,
// it should operate on a copy of them.
// NOTE: This is duplicating code from AsyncAPIInterface::DoCommands() in the
// interest of performance in this critical execution loop.
template <bool DebugImpl>
error::Error GLES2DecoderImpl::DoCommandsImpl(unsigned int num_commands,
                                              const volatile void* buffer,
                                              int num_entries,
                                              int* entries_processed) {
  DCHECK(entries_processed);
  commands_to_process_ = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;
  unsigned int command = 0;

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process_--) {
    const unsigned int size = cmd_data->value_header.size;
    command = cmd_data->value_header.command;

    if (size == 0) {
      result = error::kInvalidSize;
      break;
    }

    if (static_cast<int>(size) + process_pos > num_entries) {
      result = error::kOutOfBounds;
      break;
    }

    if (DebugImpl && log_commands()) {
      LOG(ERROR) << "[" << logger_.GetLogPrefix() << "]"
                 << "cmd: " << GetCommandName(command);
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstGLES2Command;
    if (command_index < std::size(command_info)) {
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        bool doing_gpu_trace = false;
        if (DebugImpl && gpu_trace_commands_) {
          if (CMD_FLAG_GET_TRACE_LEVEL(info.cmd_flags) <= gpu_trace_level_) {
            doing_gpu_trace = true;
            gpu_tracer_->Begin(TRACE_DISABLED_BY_DEFAULT("gpu.decoder"),
                               GetCommandName(command), kTraceDecoder);
          }
        }

        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT

        result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);

        if (DebugImpl && doing_gpu_trace)
          gpu_tracer_->End(kTraceDecoder);

        if (DebugImpl && debug() && !WasContextLost()) {
          GLenum error;
          while ((error = api()->glGetErrorFn()) != GL_NO_ERROR) {
            LOG(ERROR) << "[" << logger_.GetLogPrefix() << "] "
                       << "GL ERROR: " << GLES2Util::GetStringEnum(error)
                       << " : " << GetCommandName(command);
            LOCAL_SET_GL_ERROR(error, "DoCommand", "GL error from driver");
          }
        }
      } else {
        result = error::kInvalidArguments;
      }
    } else {
      result = DoCommonCommand(command, arg_count, cmd_data);
    }

    if (result == error::kNoError &&
        current_decoder_error_ != error::kNoError) {
      result = current_decoder_error_;
      current_decoder_error_ = error::kNoError;
    }

    if (result != error::kDeferCommandUntilLater) {
      process_pos += size;
      cmd_data += size;
    }
  }

  *entries_processed = process_pos;

  if (error::IsError(result)) {
    LOG(ERROR) << "Error: " << result << " for Command "
               << GetCommandName(command);
  }

  return result;
}

error::Error GLES2DecoderImpl::DoCommands(unsigned int num_commands,
                                          const volatile void* buffer,
                                          int num_entries,
                                          int* entries_processed) {
  if (gpu_debug_commands_) {
    return DoCommandsImpl<true>(
        num_commands, buffer, num_entries, entries_processed);
  } else {
    return DoCommandsImpl<false>(
        num_commands, buffer, num_entries, entries_processed);
  }
}

void GLES2DecoderImpl::ExitCommandProcessingEarly() {
  commands_to_process_ = 0;
}

void GLES2DecoderImpl::DoFinish() {
  api()->glFinishFn();
  ProcessPendingReadPixels(true);
  ProcessPendingQueries(true);
}

void GLES2DecoderImpl::DoFlush() {
  api()->glFlushFn();
  ProcessPendingQueries(false);
}

void GLES2DecoderImpl::DoActiveTexture(GLenum texture_unit) {
  GLuint texture_index = texture_unit - GL_TEXTURE0;
  if (texture_index >= state_.texture_units.size()) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glActiveTexture", texture_unit, "texture_unit");
    return;
  }
  state_.active_texture_unit = texture_index;
  api()->glActiveTextureFn(texture_unit);
}

void GLES2DecoderImpl::DoBindBuffer(GLenum target, GLuint client_id) {
  Buffer* buffer = nullptr;
  GLuint service_id = 0;
  if (client_id != 0) {
    buffer = GetBuffer(client_id);
    if (!buffer) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                           "glBindBuffer",
                           "id not generated by glGenBuffers");
        return;
      }

      // It's a new id so make a buffer buffer for it.
      api()->glGenBuffersARBFn(1, &service_id);
      CreateBuffer(client_id, service_id);
      buffer = GetBuffer(client_id);
    }
  }
  LogClientServiceForInfo(buffer, client_id, "glBindBuffer");
  if (buffer) {
    if (!buffer_manager()->SetTarget(buffer, target)) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "glBindBuffer", "buffer bound to more than 1 target");
      return;
    }
    service_id = buffer->service_id();
  }
  state_.SetBoundBuffer(target, buffer);
  api()->glBindBufferFn(target, service_id);
}

void GLES2DecoderImpl::BindIndexedBufferImpl(
    GLenum target, GLuint index, GLuint client_id,
    GLintptr offset, GLsizeiptr size,
    BindIndexedBufferFunctionType function_type, const char* function_name) {
  switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER: {
      if (index >= group_->max_transform_feedback_separate_attribs()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                           "index out of range");
        return;
      }
      DCHECK(state_.bound_transform_feedback.get());
      if (state_.bound_transform_feedback->active()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                           "bound transform feedback is active");
        return;
      }
      break;
    }
    case GL_UNIFORM_BUFFER: {
      if (index >= group_->max_uniform_buffer_bindings()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                           "index out of range");
        return;
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (function_type == BindIndexedBufferFunctionType::kBindBufferRange) {
    switch (target) {
      case GL_TRANSFORM_FEEDBACK_BUFFER:
        if ((size % 4 != 0) || (offset % 4 != 0)) {
          LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                             "size or offset are not multiples of 4");
          return;
        }
        break;
      case GL_UNIFORM_BUFFER: {
        if (offset % group_->uniform_buffer_offset_alignment() != 0) {
          LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
              "offset is not a multiple of UNIFORM_BUFFER_OFFSET_ALIGNMENT");
          return;
        }
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    if (client_id != 0) {
      if (size <= 0) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "size <= 0");
        return;
      }
      if (offset < 0) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "offset < 0");
        return;
      }
    }
  }

  Buffer* buffer = nullptr;
  GLuint service_id = 0;
  if (client_id != 0) {
    buffer = GetBuffer(client_id);
    if (!buffer) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                           "id not generated by glGenBuffers");
        return;
      }

      // It's a new id so make a buffer for it.
      api()->glGenBuffersARBFn(1, &service_id);
      CreateBuffer(client_id, service_id);
      buffer = GetBuffer(client_id);
      DCHECK(buffer);
    }
    if (!buffer_manager()->SetTarget(buffer, target)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "buffer bound to more than 1 target");
      return;
    }
    service_id = buffer->service_id();
  }
  LogClientServiceForInfo(buffer, client_id, function_name);

  scoped_refptr<IndexedBufferBindingHost> bindings;
  switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      bindings = state_.bound_transform_feedback.get();
      break;
    case GL_UNIFORM_BUFFER:
      bindings = state_.indexed_uniform_buffer_bindings.get();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  DCHECK(bindings);
  switch (function_type) {
    case BindIndexedBufferFunctionType::kBindBufferBase:
      bindings->DoBindBufferBase(index, buffer);
      break;
    case BindIndexedBufferFunctionType::kBindBufferRange:
      bindings->DoBindBufferRange(index, buffer, offset, size);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  state_.SetBoundBuffer(target, buffer);
}

void GLES2DecoderImpl::DoBindBufferBase(GLenum target, GLuint index,
                                        GLuint client_id) {
  BindIndexedBufferImpl(target, index, client_id, 0, 0,
                        BindIndexedBufferFunctionType::kBindBufferBase,
                        "glBindBufferBase");
}

void GLES2DecoderImpl::DoBindBufferRange(GLenum target, GLuint index,
                                         GLuint client_id,
                                         GLintptr offset,
                                         GLsizeiptr size) {
  BindIndexedBufferImpl(target, index, client_id, offset, size,
                        BindIndexedBufferFunctionType::kBindBufferRange,
                        "glBindBufferRange");
}

bool GLES2DecoderImpl::BoundFramebufferAllowsChangesToAlphaChannel() {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer)
    return framebuffer->HasAlphaMRT();
  if (back_buffer_draw_buffer_ == GL_NONE)
    return false;
  if (external_default_framebuffer_ &&
      external_default_framebuffer_->IsSharedImageAttached()) {
    return external_default_framebuffer_->HasAlpha();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return false;
  }
  return (back_buffer_color_format_ == GL_RGBA ||
          back_buffer_color_format_ == GL_RGBA8);
}

bool GLES2DecoderImpl::BoundFramebufferHasDepthAttachment() {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer) {
    return framebuffer->HasDepthAttachment();
  }
  if (external_default_framebuffer_ &&
      external_default_framebuffer_->IsSharedImageAttached()) {
    return external_default_framebuffer_->HasDepth();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return false;
  }
  return back_buffer_has_depth_;
}

bool GLES2DecoderImpl::BoundFramebufferHasStencilAttachment() {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer) {
    return framebuffer->HasStencilAttachment();
  }
  if (external_default_framebuffer_ &&
      external_default_framebuffer_->IsSharedImageAttached()) {
    return external_default_framebuffer_->HasStencil();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return false;
  }
  return back_buffer_has_stencil_;
}

void GLES2DecoderImpl::ApplyDirtyState() {
  if (framebuffer_state_.clear_state_dirty) {
    bool allows_alpha_change = BoundFramebufferAllowsChangesToAlphaChannel();
    state_.SetDeviceColorMask(state_.color_mask_red, state_.color_mask_green,
                              state_.color_mask_blue,
                              state_.color_mask_alpha && allows_alpha_change);

    bool have_depth = BoundFramebufferHasDepthAttachment();
    state_.SetDeviceDepthMask(state_.depth_mask && have_depth);

    bool have_stencil = BoundFramebufferHasStencilAttachment();
    state_.SetDeviceStencilMaskSeparate(
        GL_FRONT, have_stencil ? state_.stencil_front_writemask : 0);
    state_.SetDeviceStencilMaskSeparate(
        GL_BACK, have_stencil ? state_.stencil_back_writemask : 0);

    state_.SetDeviceCapabilityState(
        GL_DEPTH_TEST, state_.enable_flags.depth_test && have_depth);
    state_.SetDeviceCapabilityState(
        GL_STENCIL_TEST, state_.enable_flags.stencil_test && have_stencil);
    framebuffer_state_.clear_state_dirty = false;
  }
}

GLuint GLES2DecoderImpl::GetBackbufferServiceId() const {
  if (external_default_framebuffer_ &&
      external_default_framebuffer_->IsSharedImageAttached())
    return external_default_framebuffer_->GetFramebufferId();

  return (offscreen_target_frame_buffer_.get())
             ? offscreen_target_frame_buffer_->id()
             : (surface_.get() ? surface_->GetBackingFramebufferObject() : 0);
}

void GLES2DecoderImpl::RestoreState(const ContextState* prev_state) {
  TRACE_EVENT1("gpu", "GLES2DecoderImpl::RestoreState",
               "context", logger_.GetLogPrefix());
  // Restore the Framebuffer first because of bugs in Intel drivers.
  // Intel drivers incorrectly clip the viewport settings to
  // the size of the current framebuffer object.
  RestoreFramebufferBindings();
  state_.RestoreState(prev_state);
}

void GLES2DecoderImpl::RestoreBufferBinding(unsigned int target) {
  if (target == GL_PIXEL_PACK_BUFFER) {
    state_.UpdatePackParameters();
  } else if (target == GL_PIXEL_UNPACK_BUFFER) {
    state_.UpdateUnpackParameters();
  }
  Buffer* bound_buffer =
      buffer_manager()->GetBufferInfoForTarget(&state_, target);
  api()->glBindBufferFn(target, bound_buffer ? bound_buffer->service_id() : 0);
}

void GLES2DecoderImpl::RestoreFramebufferBindings() const {
  GLuint service_id =
      framebuffer_state_.bound_draw_framebuffer.get()
          ? framebuffer_state_.bound_draw_framebuffer->service_id()
          : GetBackbufferServiceId();
  if (!SupportsSeparateFramebufferBinds()) {
    api()->glBindFramebufferEXTFn(GL_FRAMEBUFFER, service_id);
  } else {
    api()->glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER, service_id);
    service_id = framebuffer_state_.bound_read_framebuffer.get()
                     ? framebuffer_state_.bound_read_framebuffer->service_id()
                     : GetBackbufferServiceId();
    api()->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, service_id);
  }
  OnFboChanged();
}

void GLES2DecoderImpl::RestoreRenderbufferBindings() {
  state_.RestoreRenderbufferBindings();
}

void GLES2DecoderImpl::RestoreTextureState(unsigned service_id) {
  Texture* texture = texture_manager()->GetTextureForServiceId(service_id);
  if (texture) {
    GLenum target = texture->target();
    api()->glBindTextureFn(target, service_id);
    api()->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, texture->wrap_s());
    api()->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, texture->wrap_t());
    api()->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER,
                             texture->min_filter());
    api()->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER,
                             texture->mag_filter());
    if (feature_info_->IsWebGL2OrES3Context()) {
      api()->glTexParameteriFn(target, GL_TEXTURE_BASE_LEVEL,
                               texture->base_level());
    }
    RestoreTextureUnitBindings(state_.active_texture_unit);
  }
}

void GLES2DecoderImpl::ClearDeviceWindowRectangles() const {
  if (!feature_info_->feature_flags().ext_window_rectangles) {
    return;
  }
  api()->glWindowRectanglesEXTFn(GL_EXCLUSIVE_EXT, 0, nullptr);
}

void GLES2DecoderImpl::RestoreDeviceWindowRectangles() const {
  state_.UpdateWindowRectangles();
}

void GLES2DecoderImpl::ClearAllAttributes() const {
  // Must use native VAO 0, as RestoreAllAttributes can't fully restore
  // other VAOs.
  if (feature_info_->feature_flags().native_vertex_array_object)
    api()->glBindVertexArrayOESFn(0);

  for (uint32_t i = 0; i < group_->max_vertex_attribs(); ++i) {
    if (i != 0)  // Never disable attribute 0
      state_.vertex_attrib_manager->SetDriverVertexAttribEnabled(i, false);
    if (features().angle_instanced_arrays)
      api()->glVertexAttribDivisorANGLEFn(i, 0);
  }
}

void GLES2DecoderImpl::RestoreAllAttributes() const {
  state_.RestoreVertexAttribs(nullptr);
}

void GLES2DecoderImpl::SetIgnoreCachedStateForTest(bool ignore) {
  state_.SetIgnoreCachedStateForTest(ignore);
}

void GLES2DecoderImpl::SetForceShaderNameHashingForTest(bool force) {
  force_shader_name_hashing_for_test = force;
}

// Added specifically for testing backbuffer_needs_clear_bits unittests.
uint32_t GLES2DecoderImpl::GetAndClearBackbufferClearBitsForTest() {
  uint32_t clear_bits = backbuffer_needs_clear_bits_;
  backbuffer_needs_clear_bits_ = 0;
  return clear_bits;
}

void GLES2DecoderImpl::OnFboChanged() const {
  state_.fbo_binding_for_scissor_workaround_dirty = true;
  state_.stencil_state_changed_since_validation = true;

  if (workarounds().flush_on_framebuffer_change)
    api()->glFlushFn();
}

// Called after the FBO is checked for completeness.
void GLES2DecoderImpl::OnUseFramebuffer() const {
  if (!state_.fbo_binding_for_scissor_workaround_dirty)
    return;
  state_.fbo_binding_for_scissor_workaround_dirty = false;

  if (workarounds().force_update_scissor_state_when_binding_fbo0 &&
      GetBoundDrawFramebufferServiceId() == 0) {
    // The theory is that FBO0 keeps some internal (in HW regs maybe?) scissor
    // test state, but the driver forgets to update it with GL_SCISSOR_TEST
    // when FBO0 gets bound. (So it stuck with whatever state we last switched
    // from it.)
    // If the internal scissor test state was enabled, it does update its
    // internal scissor rect with GL_SCISSOR_BOX though.
    if (state_.enable_flags.cached_scissor_test) {
      // The driver early outs if the new state matches previous state so some
      // shake up is needed.
      api()->glDisableFn(GL_SCISSOR_TEST);
      api()->glEnableFn(GL_SCISSOR_TEST);
    } else {
      // Ditto.
      api()->glEnableFn(GL_SCISSOR_TEST);
      api()->glDisableFn(GL_SCISSOR_TEST);
    }
  }
}

void GLES2DecoderImpl::DoBindFramebuffer(GLenum target, GLuint client_id) {
  Framebuffer* framebuffer = nullptr;
  GLuint service_id = 0;
  if (client_id != 0) {
    framebuffer = GetFramebuffer(client_id);
    if (!framebuffer) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                           "glBindFramebuffer",
                           "id not generated by glGenFramebuffers");
        return;
      }

      // It's a new id so make a framebuffer framebuffer for it.
      api()->glGenFramebuffersEXTFn(1, &service_id);
      CreateFramebuffer(client_id, service_id);
      framebuffer = GetFramebuffer(client_id);
    } else {
      service_id = framebuffer->service_id();
    }
    framebuffer->MarkAsValid();
  }
  LogClientServiceForInfo(framebuffer, client_id, "glBindFramebuffer");

  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER_EXT) {
    framebuffer_state_.bound_draw_framebuffer = framebuffer;
    state_.UpdateWindowRectanglesForBoundDrawFramebufferClientID(client_id);
  }

  // vmiura: This looks like dup code
  if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER_EXT) {
    framebuffer_state_.bound_read_framebuffer = framebuffer;
  }

  framebuffer_state_.clear_state_dirty = true;

  // If we are rendering to the backbuffer get the FBO id for any simulated
  // backbuffer.
  if (framebuffer == nullptr) {
    service_id = GetBackbufferServiceId();
  }

  api()->glBindFramebufferEXTFn(target, service_id);
  OnFboChanged();
}

void GLES2DecoderImpl::DoBindRenderbuffer(GLenum target, GLuint client_id) {
  DCHECK_EQ(target, (GLenum)GL_RENDERBUFFER);
  Renderbuffer* renderbuffer = nullptr;
  GLuint service_id = 0;
  if (client_id != 0) {
    renderbuffer = GetRenderbuffer(client_id);
    if (!renderbuffer) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                           "glBindRenderbuffer",
                           "id not generated by glGenRenderbuffers");
        return;
      }

      // It's a new id so make a renderbuffer for it.
      api()->glGenRenderbuffersEXTFn(1, &service_id);
      CreateRenderbuffer(client_id, service_id);
      renderbuffer = GetRenderbuffer(client_id);
    } else {
      service_id = renderbuffer->service_id();
    }
    renderbuffer->MarkAsValid();
  }
  LogClientServiceForInfo(renderbuffer, client_id, "glBindRenderbuffer");
  state_.bound_renderbuffer = renderbuffer;
  state_.bound_renderbuffer_valid = true;
  api()->glBindRenderbufferEXTFn(GL_RENDERBUFFER, service_id);
}

void GLES2DecoderImpl::UpdateTextureBinding(GLenum target,
                                            GLuint client_id,
                                            TextureRef* texture) {
  CHECK(texture);
  size_t last_active_unit_index = state_.active_texture_unit;

  for (size_t curr_unit_index = 0;
       curr_unit_index < state_.texture_units.size(); curr_unit_index++) {
    auto curr_unit = state_.texture_units[curr_unit_index];
    auto* curr_texture = curr_unit.GetInfoForTarget(target);

    if (!curr_texture) {
      continue;
    }

    if (curr_texture->client_id() != client_id) {
      continue;
    }

    // `target` is bound to `client_id` in this unit, so we need to update the
    // service-side binding.

    // First update the active texture unit if needed.
    if (last_active_unit_index != curr_unit_index) {
      api()->glActiveTextureFn(
          static_cast<GLenum>(GL_TEXTURE0 + curr_unit_index));
      last_active_unit_index = curr_unit_index;
    }

    // Now update the texture binding.
    api()->glBindTextureFn(target, texture->service_id());
    curr_unit.SetInfoForTarget(target, texture);
  }

  // Reset the active texture unit if it was changed.
  if (last_active_unit_index != state_.active_texture_unit) {
    api()->glActiveTextureFn(
        static_cast<GLenum>(GL_TEXTURE0 + state_.active_texture_unit));
  }
}

void GLES2DecoderImpl::DoBindTexture(GLenum target, GLuint client_id) {
  TextureRef* texture_ref = nullptr;
  GLuint service_id = 0;
  if (client_id != 0) {
    texture_ref = GetTexture(client_id);
    if (!texture_ref) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                           "glBindTexture",
                           "id not generated by glGenTextures");
        return;
      }

      // It's a new id so make a texture texture for it.
      api()->glGenTexturesFn(1, &service_id);
      DCHECK_NE(0u, service_id);
      CreateTexture(client_id, service_id);
      texture_ref = GetTexture(client_id);
    }
  } else {
    texture_ref = texture_manager()->GetDefaultTextureInfo(target);
  }

  // Check the texture exists
  if (texture_ref) {
    Texture* texture = texture_ref->texture();
    // Check that we are not trying to bind it to a different target.
    if (texture->target() != 0 && texture->target() != target) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                         "glBindTexture",
                         "texture bound to more than 1 target.");
      return;
    }
    LogClientServiceForInfo(texture, client_id, "glBindTexture");
    api()->glBindTextureFn(target, texture->service_id());
    if (texture->target() == 0) {
      texture_manager()->SetTarget(texture_ref, target);
    }
  } else {
    api()->glBindTextureFn(target, 0);
  }

  TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
  unit.bind_target = target;
  unit.SetInfoForTarget(target, texture_ref);
}

void GLES2DecoderImpl::DoBindSampler(GLuint unit, GLuint client_id) {
  if (unit >= group_->max_texture_units()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glBindSampler", "unit out of bounds");
    return;
  }
  Sampler* sampler = nullptr;
  if (client_id != 0) {
    sampler = GetSampler(client_id);
    if (!sampler) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                         "glBindSampler",
                         "id not generated by glGenSamplers");
      return;
    }
  }

  // Check the sampler exists
  if (sampler) {
    LogClientServiceForInfo(sampler, client_id, "glBindSampler");
    api()->glBindSamplerFn(unit, sampler->service_id());
  } else {
    api()->glBindSamplerFn(unit, 0);
  }

  state_.sampler_units[unit] = sampler;
}

void GLES2DecoderImpl::DoBindTransformFeedback(
    GLenum target, GLuint client_id) {
  const char* function_name = "glBindTransformFeedback";

  TransformFeedback* transform_feedback = nullptr;
  if (client_id != 0) {
    transform_feedback = GetTransformFeedback(client_id);
    if (!transform_feedback) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "id not generated by glGenTransformFeedbacks");
      return;
    }
  } else {
    transform_feedback = state_.default_transform_feedback.get();
  }
  DCHECK(transform_feedback);
  if (transform_feedback == state_.bound_transform_feedback.get())
    return;
  if (state_.bound_transform_feedback->active() &&
      !state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "currently bound transform feedback is active");
    return;
  }
  LogClientServiceForInfo(transform_feedback, client_id, function_name);
  transform_feedback->DoBindTransformFeedback(
      target, state_.bound_transform_feedback.get(),
      state_.bound_transform_feedback_buffer.get());
  state_.bound_transform_feedback = transform_feedback;
}

void GLES2DecoderImpl::DoBeginTransformFeedback(GLenum primitive_mode) {
  const char* function_name = "glBeginTransformFeedback";
  TransformFeedback* transform_feedback = state_.bound_transform_feedback.get();
  DCHECK(transform_feedback);
  if (transform_feedback->active()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "transform feedback is already active");
    return;
  }
  if (!CheckCurrentProgram(function_name)) {
    return;
  }
  Program* program = state_.current_program.get();
  DCHECK(program);
  size_t required_buffer_count =
      program->effective_transform_feedback_varyings().size();
  if (required_buffer_count == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "no active transform feedback varyings");
    return;
  }
  if (required_buffer_count > 1 &&
      GL_INTERLEAVED_ATTRIBS ==
          program->effective_transform_feedback_buffer_mode()) {
    required_buffer_count = 1;
  }
  for (size_t ii = 0; ii < required_buffer_count; ++ii) {
    Buffer* buffer = transform_feedback->GetBufferBinding(ii);
    if (!buffer) {
      std::string msg = base::StringPrintf("missing buffer bound at index %i",
                                           static_cast<int>(ii));
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name, msg.c_str());
      return;
    }
    if (buffer->GetMappedRange()) {
      std::string msg = base::StringPrintf(
          "bound buffer bound at index %i is mapped", static_cast<int>(ii));
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name, msg.c_str());
      return;
    }
    if (buffer->IsDoubleBoundForTransformFeedback()) {
      std::string msg = base::StringPrintf(
          "buffer at index %i is bound for multiple transform feedback outputs",
          static_cast<int>(ii));
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name, msg.c_str());
      return;
    }
  }
  transform_feedback->DoBeginTransformFeedback(primitive_mode);
  DCHECK(transform_feedback->active());
}

void GLES2DecoderImpl::DoEndTransformFeedback() {
  const char* function_name = "glEndTransformFeedback";
  DCHECK(state_.bound_transform_feedback.get());
  if (!state_.bound_transform_feedback->active()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "transform feedback is not active");
    return;
  }
  // TODO(zmo): Validate binding points.
  state_.bound_transform_feedback->DoEndTransformFeedback();
}

void GLES2DecoderImpl::DoPauseTransformFeedback() {
  DCHECK(state_.bound_transform_feedback.get());
  if (!state_.bound_transform_feedback->active() ||
      state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glPauseTransformFeedback",
                       "transform feedback is not active or already paused");
    return;
  }
  state_.bound_transform_feedback->DoPauseTransformFeedback();
}

void GLES2DecoderImpl::DoResumeTransformFeedback() {
  DCHECK(state_.bound_transform_feedback.get());
  if (!state_.bound_transform_feedback->active() ||
      !state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glResumeTransformFeedback",
                       "transform feedback is not active or not paused");
    return;
  }
  state_.bound_transform_feedback->DoResumeTransformFeedback();
}

void GLES2DecoderImpl::DoDisableVertexAttribArray(GLuint index) {
  if (state_.vertex_attrib_manager->Enable(index, false)) {
    state_.vertex_attrib_manager->SetDriverVertexAttribEnabled(index, false);
  } else {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glDisableVertexAttribArray", "index out of range");
  }
}

void GLES2DecoderImpl::InvalidateFramebufferImpl(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    const char* function_name,
    FramebufferOperation op) {
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);

  // Because of performance issues, no-op if the format of the attachment is
  // DEPTH_STENCIL and only one part is intended to be invalidated.
  bool has_depth_stencil_format = framebuffer &&
      framebuffer->HasDepthStencilFormatAttachment();
  bool invalidate_depth = false;
  bool invalidate_stencil = false;
  auto validated_attachments = base::HeapArray<GLenum>::Uninit(count + 1);
  GLsizei validated_count = 0;

  // Validates the attachments. If one of them fails, the whole command fails.
  GLenum thresh0 = GL_COLOR_ATTACHMENT0 + group_->max_color_attachments();
  GLenum thresh1 = GL_COLOR_ATTACHMENT15;
  for (GLsizei i = 0; i < count; ++i) {
    GLenum attachment = attachments[i];
    if (framebuffer) {
      if (attachment >= thresh0 && attachment <= thresh1) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name, "invalid attachment");
        return;
      }
      if (!validators_->attachment.IsValid(attachment)) {
        LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, attachment,
                                        "attachments");
        return;
      }
      if (has_depth_stencil_format) {
        switch (attachment) {
          case GL_DEPTH_ATTACHMENT:
            invalidate_depth = true;
            continue;
          case GL_STENCIL_ATTACHMENT:
            invalidate_stencil = true;
            continue;
          case GL_DEPTH_STENCIL_ATTACHMENT:
            invalidate_depth = true;
            invalidate_stencil = true;
            continue;
        }
      }
    } else {
      if (!validators_->backbuffer_attachment.IsValid(attachment)) {
        LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, attachment,
                                        "attachments");
        return;
      }
    }
    validated_attachments[validated_count++] = attachment;
  }
  if (invalidate_depth && invalidate_stencil) {
    // We do not use GL_DEPTH_STENCIL_ATTACHMENT here because
    // it is not a valid token for glDiscardFramebufferEXT.
    validated_attachments[validated_count++] = GL_DEPTH_ATTACHMENT;
    validated_attachments[validated_count++] = GL_STENCIL_ATTACHMENT;
  }

  // If the default framebuffer is bound but we are still rendering to an
  // FBO, translate attachment names that refer to default framebuffer
  // channels to corresponding framebuffer attachments.
  auto translated_attachments =
      base::HeapArray<GLenum>::Uninit(validated_count);
  for (GLsizei i = 0; i < validated_count; ++i) {
    GLenum attachment = validated_attachments[i];
    if (!framebuffer && GetBackbufferServiceId()) {
      switch (attachment) {
        case GL_COLOR_EXT:
          attachment = GL_COLOR_ATTACHMENT0;
          break;
        case GL_DEPTH_EXT:
          attachment = GL_DEPTH_ATTACHMENT;
          break;
        case GL_STENCIL_EXT:
          attachment = GL_STENCIL_ATTACHMENT;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          return;
      }
    }
    translated_attachments[i] = attachment;
  }

  bool dirty = false;
  switch (op) {
    case kFramebufferDiscard:
      if (gl_version_info().is_es3) {
        api()->glInvalidateFramebufferFn(target, validated_count,
                                         translated_attachments.data());
      } else {
        api()->glDiscardFramebufferEXTFn(target, validated_count,
                                         translated_attachments.data());
      }
      dirty = true;
      break;
    case kFramebufferInvalidate:
      api()->glInvalidateFramebufferFn(target, validated_count,
                                       translated_attachments.data());
      dirty = true;
      break;
    case kFramebufferInvalidateSub:
      // Make it an no-op because we don't have a mechanism to mark partial
      // pixels uncleared yet.
      // TODO(zmo): Revisit this.
      break;
  }

  if (!dirty)
    return;

  // Marks each one of them as not cleared.
  for (GLsizei i = 0; i < validated_count; ++i) {
    if (framebuffer) {
      if (validated_attachments[i] == GL_DEPTH_STENCIL_ATTACHMENT) {
        framebuffer->MarkAttachmentAsCleared(renderbuffer_manager(),
                                             texture_manager(),
                                             GL_DEPTH_ATTACHMENT,
                                             false);
        framebuffer->MarkAttachmentAsCleared(renderbuffer_manager(),
                                             texture_manager(),
                                             GL_STENCIL_ATTACHMENT,
                                             false);
      } else {
        framebuffer->MarkAttachmentAsCleared(renderbuffer_manager(),
                                             texture_manager(),
                                             validated_attachments[i],
                                             false);
      }
    } else {
      switch (validated_attachments[i]) {
        case GL_COLOR_EXT:
          backbuffer_needs_clear_bits_ |= GL_COLOR_BUFFER_BIT;
          break;
        case GL_DEPTH_EXT:
          backbuffer_needs_clear_bits_ |= GL_DEPTH_BUFFER_BIT;
          break;
        case GL_STENCIL_EXT:
          backbuffer_needs_clear_bits_ |= GL_STENCIL_BUFFER_BIT;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
  }
}

void GLES2DecoderImpl::DoDiscardFramebufferEXT(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  if (workarounds().disable_discard_framebuffer)
    return;

  const GLsizei kWidthNotUsed = 1;
  const GLsizei kHeightNotUsed = 1;
  InvalidateFramebufferImpl(
      target, count, attachments, 0, 0, kWidthNotUsed, kHeightNotUsed,
      "glDiscardFramebufferEXT", kFramebufferDiscard);
}

void GLES2DecoderImpl::DoInvalidateFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  const GLsizei kWidthNotUsed = 1;
  const GLsizei kHeightNotUsed = 1;
  InvalidateFramebufferImpl(
      target, count, attachments, 0, 0, kWidthNotUsed, kHeightNotUsed,
      "glInvalidateFramebuffer", kFramebufferInvalidate);
}

void GLES2DecoderImpl::DoInvalidateSubFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  InvalidateFramebufferImpl(
      target, count, attachments, x, y, width, height,
      "glInvalidateSubFramebuffer", kFramebufferInvalidateSub);
}

void GLES2DecoderImpl::DoEnableVertexAttribArray(GLuint index) {
  if (state_.vertex_attrib_manager->Enable(index, true)) {
    state_.vertex_attrib_manager->SetDriverVertexAttribEnabled(index, true);
  } else {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glEnableVertexAttribArray", "index out of range");
  }
}

void GLES2DecoderImpl::DoGenerateMipmap(GLenum target) {
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref ||
      !texture_manager()->CanGenerateMipmaps(texture_ref)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGenerateMipmap", "Can not generate mips");
    return;
  }
  Texture* tex = texture_ref->texture();
  GLint base_level = tex->base_level();

  if (target == GL_TEXTURE_CUBE_MAP) {
    for (int i = 0; i < 6; ++i) {
      GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
      if (!texture_manager()->ClearTextureLevel(this, texture_ref, face,
                                                base_level)) {
        LOCAL_SET_GL_ERROR(
            GL_OUT_OF_MEMORY, "glGenerateMipmap", "dimensions too big");
        return;
      }
    }
  } else {
    if (!texture_manager()->ClearTextureLevel(this, texture_ref, target,
                                              base_level)) {
      LOCAL_SET_GL_ERROR(
          GL_OUT_OF_MEMORY, "glGenerateMipmap", "dimensions too big");
      return;
    }
  }

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glGenerateMipmap");
  api()->glGenerateMipmapEXTFn(target);

  GLenum error = LOCAL_PEEK_GL_ERROR("glGenerateMipmap");
  if (error == GL_NO_ERROR) {
    texture_manager()->MarkMipmapsGenerated(texture_ref);
  }
}

bool GLES2DecoderImpl::GetHelper(
    GLenum pname, GLint* params, GLsizei* num_written) {
  DCHECK(num_written);
  switch (pname) {
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      *num_written = 1;
      {
        Framebuffer* framebuffer = GetBoundReadFramebuffer();
        if (framebuffer &&
            framebuffer->IsPossiblyComplete(feature_info_.get()) !=
            GL_FRAMEBUFFER_COMPLETE) {
          // Here we avoid querying the driver framebuffer status because the
          // above should cover most cases. This is an effort to reduce crashes
          // on MacOSX. See crbug.com/662802.
          LOCAL_SET_GL_ERROR(
              GL_INVALID_OPERATION, "glGetIntegerv", "incomplete framebuffer");
          if (params) {
            *params = 0;
          }
          return true;
        }
      }
      if (params) {
        api()->glGetIntegervFn(pname, params);
        if (*params == GL_HALF_FLOAT && feature_info_->IsWebGL1OrES2Context()) {
          *params = GL_HALF_FLOAT_OES;
        }
        if (*params == GL_SRGB_ALPHA_EXT) {
          *params = GL_RGBA;
        }
        if (*params == GL_SRGB_EXT) {
          *params = GL_RGB;
        }
      }
      return true;
    default:
      break;
  }

  if (feature_info_->IsWebGL2OrES3Context()) {
    switch (pname) {
      case GL_MAX_VARYING_COMPONENTS:
        // We can just delegate this query to the driver.
        *num_written = 1;
        break;
      case GL_READ_BUFFER:
        *num_written = 1;
        if (params) {
          Framebuffer* framebuffer = GetBoundReadFramebuffer();
          GLenum read_buffer;
          if (framebuffer) {
            read_buffer = framebuffer->read_buffer();
          } else {
            read_buffer = back_buffer_read_buffer_;
          }
          *params = static_cast<GLint>(read_buffer);
        }
        return true;
      case GL_TRANSFORM_FEEDBACK_ACTIVE:
        *num_written = 1;
        if (params) {
          *params =
              static_cast<GLint>(state_.bound_transform_feedback->active());
        }
        return true;
      case GL_TRANSFORM_FEEDBACK_PAUSED:
        *num_written = 1;
        if (params) {
          *params =
              static_cast<GLint>(state_.bound_transform_feedback->paused());
        }
        return true;
      case GL_WINDOW_RECTANGLE_EXT:
        *num_written = 4;
        // This is only used for glGetIntegeri_v and similar, so params will
        // always be null - the only path here is through
        // GetNumValuesReturnedForGLGet.
        DCHECK(!params);
        return true;
    }
  }
  switch (pname) {
    case GL_MAX_VIEWPORT_DIMS:
      *num_written = 2;
      if (offscreen_target_frame_buffer_.get()) {
        if (params) {
          params[0] = renderbuffer_manager()->max_renderbuffer_size();
          params[1] = renderbuffer_manager()->max_renderbuffer_size();
        }
        return true;
      }
      break;
    case GL_MAX_SAMPLES:
      *num_written = 1;
      if (params) {
        params[0] = renderbuffer_manager()->max_samples();
      }
      return true;
    case GL_MAX_RENDERBUFFER_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = renderbuffer_manager()->max_renderbuffer_size();
      }
      return true;
    case GL_MAX_TEXTURE_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = texture_manager()->MaxSizeForTarget(GL_TEXTURE_2D);
      }
      return true;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = texture_manager()->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP);
      }
      return true;
    case GL_MAX_3D_TEXTURE_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = texture_manager()->MaxSizeForTarget(GL_TEXTURE_3D);
      }
      return true;
    case GL_MAX_ARRAY_TEXTURE_LAYERS:
      *num_written = 1;
      if (params) {
        params[0] = texture_manager()->max_array_texture_layers();
      }
      return true;
    case GL_MAX_COLOR_ATTACHMENTS_EXT:
      *num_written = 1;
      if (params) {
        params[0] = group_->max_color_attachments();
      }
      return true;
    case GL_MAX_DRAW_BUFFERS_ARB:
      *num_written = 1;
      if (params) {
        params[0] = group_->max_draw_buffers();
      }
      return true;
    case GL_ALPHA_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        Framebuffer* framebuffer = GetBoundDrawFramebuffer();
        if (framebuffer) {
          if (framebuffer->HasAlphaMRT() &&
              framebuffer->HasSameInternalFormatsMRT()) {
            api()->glGetIntegervFn(GL_ALPHA_BITS, &v);
          }
        } else {
          v = (ClientExposedBackBufferHasAlpha() ? 8 : 0);
        }
        params[0] = v;
      }
      return true;
    case GL_DEPTH_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        api()->glGetIntegervFn(GL_DEPTH_BITS, &v);
        params[0] = BoundFramebufferHasDepthAttachment() ? v : 0;
      }
      return true;
    case GL_RED_BITS:
    case GL_GREEN_BITS:
    case GL_BLUE_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        api()->glGetIntegervFn(pname, &v);
        params[0] = v;
      }
      return true;
    case GL_STENCIL_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        api()->glGetIntegervFn(GL_STENCIL_BITS, &v);
        params[0] = BoundFramebufferHasStencilAttachment() ? v : 0;
      }
      return true;
    case GL_COMPRESSED_TEXTURE_FORMATS:
      *num_written = validators_->compressed_texture_format.GetValues().size();
      if (params) {
        for (GLint ii = 0; ii < *num_written; ++ii) {
          params[ii] = validators_->compressed_texture_format.GetValues()[ii];
        }
      }
      return true;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      *num_written = 1;
      if (params) {
        *params = validators_->compressed_texture_format.GetValues().size();
      }
      return true;
    case GL_NUM_SHADER_BINARY_FORMATS:
      *num_written = 1;
      if (params) {
        *params = validators_->shader_binary_format.GetValues().size();
      }
      return true;
    case GL_SHADER_BINARY_FORMATS:
      *num_written = validators_->shader_binary_format.GetValues().size();
      if (params) {
        for (GLint ii = 0; ii <  *num_written; ++ii) {
          params[ii] = validators_->shader_binary_format.GetValues()[ii];
        }
      }
      return true;
    case GL_SHADER_COMPILER:
      *num_written = 1;
      if (params) {
        *params = GL_TRUE;
      }
      return true;
    case GL_ARRAY_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_array_buffer.get());
      }
      return true;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(),
            state_.vertex_attrib_manager->element_array_buffer());
      }
      return true;
    case GL_COPY_READ_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_copy_read_buffer.get());
      }
      return true;
    case GL_COPY_WRITE_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_copy_write_buffer.get());
      }
      return true;
    case GL_PIXEL_PACK_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_pixel_pack_buffer.get());
      }
      return true;
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_pixel_unpack_buffer.get());
      }
      return true;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_transform_feedback_buffer.get());
      }
      return true;
    case GL_UNIFORM_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_uniform_buffer.get());
      }
      return true;
    case GL_FRAMEBUFFER_BINDING:
    // case GL_DRAW_FRAMEBUFFER_BINDING_EXT: (same as GL_FRAMEBUFFER_BINDING)
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            framebuffer_manager(),
            GetFramebufferInfoForTarget(GL_FRAMEBUFFER));
      }
      return true;
    case GL_READ_FRAMEBUFFER_BINDING_EXT:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            framebuffer_manager(),
            GetFramebufferInfoForTarget(GL_READ_FRAMEBUFFER_EXT));
      }
      return true;
    case GL_RENDERBUFFER_BINDING:
      *num_written = 1;
      if (params) {
        Renderbuffer* renderbuffer =
            GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
        if (renderbuffer) {
          *params = renderbuffer->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_CURRENT_PROGRAM:
      *num_written = 1;
      if (params) {
        *params = GetClientId(program_manager(), state_.current_program.get());
      }
      return true;
    case GL_VERTEX_ARRAY_BINDING_OES:
      *num_written = 1;
      if (params) {
        if (state_.vertex_attrib_manager.get() !=
            state_.default_vertex_attrib_manager.get()) {
          GLuint client_id = 0;
          vertex_array_manager_->GetClientId(
              state_.vertex_attrib_manager->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_2D:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_2d.get()) {
          *params = unit.bound_texture_2d->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_CUBE_MAP:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_cube_map.get()) {
          *params = unit.bound_texture_cube_map->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_EXTERNAL_OES:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_external_oes.get()) {
          *params = unit.bound_texture_external_oes->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_RECTANGLE_ARB:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_rectangle_arb.get()) {
          *params = unit.bound_texture_rectangle_arb->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_BIND_GENERATES_RESOURCE_CHROMIUM:
      *num_written = 1;
      if (params) {
        params[0] = group_->bind_generates_resource() ? 1 : 0;
      }
      return true;
    case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT:
      *num_written = 1;
      if (params) {
        params[0] = group_->max_dual_source_draw_buffers();
      }
      return true;

    case GL_MAJOR_VERSION:
      *num_written = 1;
      if (params) {
        // TODO(zmo): once we switch to MANGLE, we should query version numbers.
        params[0] = 3;
      }
      return true;
    case GL_MINOR_VERSION:
      // TODO(zmo): once we switch to MANGLE, we should query version numbers.
      *num_written = 1;
      if (params) {
        params[0] = 0;
      }
      return true;

    case GL_NUM_EXTENSIONS:
      // TODO(vmiura): Should the command buffer support this?
      *num_written = 1;
      if (params) {
        params[0] = 0;
      }
      return true;
    case GL_GPU_DISJOINT_EXT:
      // TODO(vmiura): Should the command buffer support this?
      *num_written = 1;
      if (params) {
        params[0] = 0;
      }
      return true;
    case GL_TIMESTAMP_EXT:
      // TODO(vmiura): Should the command buffer support this?
      *num_written = 1;
      if (params) {
        params[0] = 0;
      }
      return true;
    case GL_TEXTURE_BINDING_2D_ARRAY:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_2d_array.get()) {
          *params = unit.bound_texture_2d_array->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_3D:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_3d.get()) {
          *params = unit.bound_texture_3d->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_SAMPLER_BINDING:
      *num_written = 1;
      if (params) {
        DCHECK_LT(state_.active_texture_unit, state_.sampler_units.size());
        Sampler* sampler =
            state_.sampler_units[state_.active_texture_unit].get();
        *params = sampler ? sampler->client_id() : 0;

#if DCHECK_IS_ON()
        if (sampler) {
          GLint bound_sampler = 0;
          glGetIntegerv(GL_SAMPLER_BINDING, &bound_sampler);
          DCHECK_EQ(static_cast<GLuint>(bound_sampler), sampler->service_id());
        }
#endif
      }
      return true;
    case GL_TRANSFORM_FEEDBACK_BINDING:
      *num_written = 1;
      if (params) {
        *params = state_.bound_transform_feedback->client_id();
      }
      return true;
    case GL_NUM_PROGRAM_BINARY_FORMATS:
      *num_written = 1;
      if (params) {
        *params = 0;
      }
      return true;
    case GL_PROGRAM_BINARY_FORMATS:
      *num_written = 0;
      return true;
    default:
      if (pname >= GL_DRAW_BUFFER0_ARB && pname <= GL_DRAW_BUFFER15_ARB) {
        *num_written = 1;
        if (params) {
          if (pname < GL_DRAW_BUFFER0_ARB + group_->max_draw_buffers()) {
            Framebuffer* framebuffer =
                GetFramebufferInfoForTarget(GL_FRAMEBUFFER);
            if (framebuffer) {
              *params = framebuffer->GetDrawBuffer(pname);
            } else {  // backbuffer
              if (pname == GL_DRAW_BUFFER0_ARB)
                *params = back_buffer_draw_buffer_;
              else
                *params = GL_NONE;
            }
          } else {
            *params = GL_NONE;
          }
        }
        return true;
      }

      *num_written = util_.GLGetNumValuesReturned(pname);
      if (*num_written)
        break;

      return false;
  }

  if (params) {
    DCHECK(*num_written);
    pname = AdjustGetPname(pname);
    api()->glGetIntegervFn(pname, params);
  }
  return true;
}

bool GLES2DecoderImpl::GetNumValuesReturnedForGLGet(
    GLenum pname, GLsizei* num_values) {
  *num_values = 0;
  if (state_.GetStateAsGLint(pname, nullptr, num_values)) {
    return true;
  }
  return GetHelper(pname, nullptr, num_values);
}

GLenum GLES2DecoderImpl::AdjustGetPname(GLenum pname) {
  if (GL_MAX_SAMPLES == pname &&
      features().use_img_for_multisampled_render_to_texture) {
    return GL_MAX_SAMPLES_IMG;
  }
  return pname;
}

void GLES2DecoderImpl::DoGetBooleanv(GLenum pname,
                                     GLboolean* params,
                                     GLsizei params_size) {
  DCHECK(params);
  auto values = base::HeapArray<GLint>::WithSize(params_size);
  DoGetIntegerv(pname, values.data(), params_size);
  for (GLsizei ii = 0; ii < params_size; ++ii) {
    params[ii] = static_cast<GLboolean>(values[ii]);
  }
}

void GLES2DecoderImpl::DoGetFloatv(GLenum pname,
                                   GLfloat* params,
                                   GLsizei params_size) {
  DCHECK(params);
  GLsizei num_written = 0;
  if (state_.GetStateAsGLfloat(pname, params, &num_written)) {
    DCHECK_EQ(num_written, params_size);
    return;
  }

  switch (pname) {
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
      DCHECK_EQ(params_size, util_.GLGetNumValuesReturned(pname));
      pname = AdjustGetPname(pname);
      api()->glGetFloatvFn(pname, params);
      return;
  }

  auto values = base::HeapArray<GLint>::WithSize(params_size);
  DoGetIntegerv(pname, values.data(), params_size);
  for (GLsizei ii = 0; ii < params_size; ++ii) {
    params[ii] = static_cast<GLfloat>(values[ii]);
  }
}

void GLES2DecoderImpl::DoGetInteger64v(GLenum pname,
                                       GLint64* params,
                                       GLsizei params_size) {
  DCHECK(params);
  if (feature_info_->IsWebGL2OrES3Context()) {
    switch (pname) {
      case GL_MAX_ELEMENT_INDEX: {
        DCHECK_EQ(params_size, 1);
        if (gl_version_info().IsAtLeastGLES(3, 0)) {
          api()->glGetInteger64vFn(GL_MAX_ELEMENT_INDEX, params);
        } else {
          // Assume that desktop GL implementations can generally support
          // 32-bit indices.
          if (params) {
            *params = std::numeric_limits<unsigned int>::max();
          }
        }
        return;
      }
    }
  }

  auto values = base::HeapArray<GLint>::WithSize(params_size);
  DoGetIntegerv(pname, values.data(), params_size);
  for (GLsizei ii = 0; ii < params_size; ++ii) {
    params[ii] = static_cast<GLint64>(values[ii]);
  }
}

void GLES2DecoderImpl::DoGetIntegerv(GLenum pname,
                                     GLint* params,
                                     GLsizei params_size) {
  DCHECK(params);
  GLsizei num_written = 0;
  if (state_.GetStateAsGLint(pname, params, &num_written) ||
      GetHelper(pname, params, &num_written)) {
    DCHECK_EQ(num_written, params_size);
    return;
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled enum " << pname;
}

template <typename TYPE>
void GLES2DecoderImpl::GetIndexedIntegerImpl(
    const char* function_name, GLenum target, GLuint index, TYPE* data) {
  DCHECK(data);

  if (features().ext_window_rectangles && target == GL_WINDOW_RECTANGLE_EXT) {
    if (index >= state_.GetMaxWindowRectangles()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                         "window rectangle index out of bounds");
    }
    // This must be queried from the state tracker because the driver state is
    // "wrong" if the bound framebuffer is 0 (backbuffer).
    state_.GetWindowRectangle(index, data);
    return;
  }

  scoped_refptr<IndexedBufferBindingHost> bindings;
  switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
      if (index >= group_->max_transform_feedback_separate_attribs()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid index");
        return;
      }
      bindings = state_.bound_transform_feedback.get();
      break;
    case GL_UNIFORM_BUFFER_BINDING:
    case GL_UNIFORM_BUFFER_SIZE:
    case GL_UNIFORM_BUFFER_START:
      if (index >= group_->max_uniform_buffer_bindings()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid index");
        return;
      }
      bindings = state_.indexed_uniform_buffer_bindings.get();
      break;
    case GL_BLEND_SRC_RGB:
    case GL_BLEND_SRC_ALPHA:
    case GL_BLEND_DST_RGB:
    case GL_BLEND_DST_ALPHA:
    case GL_BLEND_EQUATION_RGB:
    case GL_BLEND_EQUATION_ALPHA:
    case GL_COLOR_WRITEMASK:
      // Note (crbug.com/1058744): not implemented for validating command
      // decoder
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  DCHECK(bindings);
  switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GL_UNIFORM_BUFFER_BINDING:
      {
        Buffer* buffer = bindings->GetBufferBinding(index);
        *data = static_cast<TYPE>(buffer ? buffer->service_id() : 0);
      }
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GL_UNIFORM_BUFFER_SIZE:
      *data = static_cast<TYPE>(bindings->GetBufferSize(index));
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case GL_UNIFORM_BUFFER_START:
      *data = static_cast<TYPE>(bindings->GetBufferStart(index));
      break;
    case GL_BLEND_SRC_RGB:
    case GL_BLEND_SRC_ALPHA:
    case GL_BLEND_DST_RGB:
    case GL_BLEND_DST_ALPHA:
    case GL_BLEND_EQUATION_RGB:
    case GL_BLEND_EQUATION_ALPHA:
    case GL_COLOR_WRITEMASK:
      // Note (crbug.com/1058744): not implemented for validating command
      // decoder
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void GLES2DecoderImpl::DoGetBooleani_v(GLenum target,
                                       GLuint index,
                                       GLboolean* params,
                                       GLsizei params_size) {
  GetIndexedIntegerImpl<GLboolean>("glGetBooleani_v", target, index, params);
}

void GLES2DecoderImpl::DoGetIntegeri_v(GLenum target,
                                       GLuint index,
                                       GLint* params,
                                       GLsizei params_size) {
  GetIndexedIntegerImpl<GLint>("glGetIntegeri_v", target, index, params);
}

void GLES2DecoderImpl::DoGetInteger64i_v(GLenum target,
                                         GLuint index,
                                         GLint64* params,
                                         GLsizei params_size) {
  GetIndexedIntegerImpl<GLint64>("glGetInteger64i_v", target, index, params);
}

void GLES2DecoderImpl::DoGetProgramiv(GLuint program_id,
                                      GLenum pname,
                                      GLint* params,
                                      GLsizei params_size) {
  Program* program = GetProgramInfoNotShader(program_id, "glGetProgramiv");
  if (!program) {
    return;
  }
  program->GetProgramiv(pname, params);
}

void GLES2DecoderImpl::DoGetSynciv(GLuint sync_id,
                                   GLenum pname,
                                   GLsizei num_values,
                                   GLsizei* length,
                                   GLint* values) {
  GLsync service_sync = 0;
  if (!group_->GetSyncServiceId(sync_id, &service_sync)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glGetSynciv", "invalid sync id");
    return;
  }
  api()->glGetSyncivFn(service_sync, pname, num_values, nullptr, values);
}

void GLES2DecoderImpl::DoGetBufferParameteri64v(GLenum target,
                                                GLenum pname,
                                                GLint64* params,
                                                GLsizei params_size) {
  // Just delegate it. Some validation is actually done before this.
  buffer_manager()->ValidateAndDoGetBufferParameteri64v(
      &state_, error_state_.get(), target, pname, params);
}

void GLES2DecoderImpl::DoGetBufferParameteriv(GLenum target,
                                              GLenum pname,
                                              GLint* params,
                                              GLsizei params_size) {
  // Just delegate it. Some validation is actually done before this.
  buffer_manager()->ValidateAndDoGetBufferParameteriv(
      &state_, error_state_.get(), target, pname, params);
}

void GLES2DecoderImpl::DoBindAttribLocation(GLuint program_id,
                                            GLuint index,
                                            const std::string& name) {
  if (!StringIsValidForGLES(name)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glBindAttribLocation", "Invalid character");
    return;
  }
  if (ProgramManager::HasBuiltInPrefix(name)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glBindAttribLocation", "reserved prefix");
    return;
  }
  if (index >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glBindAttribLocation", "index out of range");
    return;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glBindAttribLocation");
  if (!program) {
    return;
  }
  // At this point, the program's shaders may not be translated yet,
  // therefore, we may not find the hashed attribute name.
  // glBindAttribLocation call with original name is useless.
  // So instead, we simply cache the binding, and then call
  // Program::ExecuteBindAttribLocationCalls() right before link.
  program->SetAttribLocationBinding(name, static_cast<GLint>(index));
}

error::Error GLES2DecoderImpl::HandleBindAttribLocationBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindAttribLocationBucket& c =
      *static_cast<const volatile gles2::cmds::BindAttribLocationBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  DoBindAttribLocation(program, index, name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::DoBindFragDataLocation(GLuint program_id,
                                                      GLuint colorName,
                                                      const std::string& name) {
  const char kFunctionName[] = "glBindFragDataLocationEXT";
  if (!StringIsValidForGLES(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "invalid character");
    return error::kNoError;
  }
  if (ProgramManager::HasBuiltInPrefix(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName, "reserved prefix");
    return error::kNoError;
  }
  if (colorName >= group_->max_draw_buffers()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "colorName out of range");
    return error::kNoError;
  }
  Program* program = GetProgramInfoNotShader(program_id, kFunctionName);
  if (!program) {
    return error::kNoError;
  }
  program->SetProgramOutputLocationBinding(name, colorName);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindFragDataLocationEXTBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_blend_func_extended) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::BindFragDataLocationEXTBucket& c =
      *static_cast<const volatile gles2::cmds::BindFragDataLocationEXTBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint colorNumber = static_cast<GLuint>(c.colorNumber);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return DoBindFragDataLocation(program, colorNumber, name_str);
}

error::Error GLES2DecoderImpl::DoBindFragDataLocationIndexed(
    GLuint program_id,
    GLuint colorName,
    GLuint index,
    const std::string& name) {
  const char kFunctionName[] = "glBindFragDataLocationIndexEXT";
  if (!StringIsValidForGLES(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "invalid character");
    return error::kNoError;
  }
  if (ProgramManager::HasBuiltInPrefix(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName, "reserved prefix");
    return error::kNoError;
  }
  if (index != 0 && index != 1) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "index out of range");
    return error::kNoError;
  }
  if ((index == 0 && colorName >= group_->max_draw_buffers()) ||
      (index == 1 && colorName >= group_->max_dual_source_draw_buffers())) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "colorName out of range for the color index");
    return error::kNoError;
  }
  Program* program = GetProgramInfoNotShader(program_id, kFunctionName);
  if (!program) {
    return error::kNoError;
  }
  program->SetProgramOutputLocationIndexedBinding(name, colorName, index);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindFragDataLocationIndexedEXTBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_blend_func_extended) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::BindFragDataLocationIndexedEXTBucket& c =
      *static_cast<
          const volatile gles2::cmds::BindFragDataLocationIndexedEXTBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint colorNumber = static_cast<GLuint>(c.colorNumber);
  GLuint index = static_cast<GLuint>(c.index);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return DoBindFragDataLocationIndexed(program, colorNumber, index, name_str);
}

void GLES2DecoderImpl::DoBindUniformLocationCHROMIUM(GLuint program_id,
                                                     GLint location,
                                                     const std::string& name) {
  if (!StringIsValidForGLES(name)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glBindUniformLocationCHROMIUM", "Invalid character");
    return;
  }
  if (ProgramManager::HasBuiltInPrefix(name)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glBindUniformLocationCHROMIUM", "reserved prefix");
    return;
  }
  if (location < 0 ||
      static_cast<uint32_t>(location) >=
          (group_->max_fragment_uniform_vectors() +
           group_->max_vertex_uniform_vectors()) *
              4) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glBindUniformLocationCHROMIUM", "location out of range");
    return;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glBindUniformLocationCHROMIUM");
  if (!program) {
    return;
  }
  if (!program->SetUniformLocationBinding(name, location)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glBindUniformLocationCHROMIUM", "location out of range");
  }
}

error::Error GLES2DecoderImpl::HandleBindUniformLocationCHROMIUMBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindUniformLocationCHROMIUMBucket& c =
      *static_cast<
          const volatile gles2::cmds::BindUniformLocationCHROMIUMBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  DoBindUniformLocationCHROMIUM(program, location, name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteShader& c =
      *static_cast<const volatile gles2::cmds::DeleteShader*>(cmd_data);
  GLuint client_id = c.shader;
  if (client_id) {
    Shader* shader = GetShader(client_id);
    if (shader) {
      if (!shader->IsDeleted()) {
        shader_manager()->Delete(shader);
      }
    } else {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDeleteShader", "unknown shader");
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteProgram& c =
      *static_cast<const volatile gles2::cmds::DeleteProgram*>(cmd_data);
  GLuint client_id = c.program;
  if (client_id) {
    Program* program = GetProgram(client_id);
    if (program) {
      if (!program->IsDeleted()) {
        program_manager()->MarkAsDeleted(shader_manager(), program);
      }
    } else {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_VALUE, "glDeleteProgram", "unknown program");
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::DoClear(GLbitfield mask) {
  const char* func_name = "glClear";
  if (mask &
      ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid mask");
    return error::kNoError;
  }
  if (CheckBoundDrawFramebufferValid(func_name)) {
    ApplyDirtyState();
    if (workarounds().gl_clear_broken) {
      if (!BoundFramebufferHasDepthAttachment())
        mask &= ~GL_DEPTH_BUFFER_BIT;
      if (!BoundFramebufferHasStencilAttachment())
        mask &= ~GL_STENCIL_BUFFER_BIT;
      ClearFramebufferForWorkaround(mask);
      return error::kNoError;
    }
    if (mask & GL_COLOR_BUFFER_BIT) {
      Framebuffer* framebuffer =
          framebuffer_state_.bound_draw_framebuffer.get();
      if (framebuffer && framebuffer->ContainsActiveIntegerAttachments()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
            "can't be called on integer buffers");
        return error::kNoError;
      }
    }
    AdjustDrawBuffers();
    api()->glClearFn(mask);
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoClearBufferiv(GLenum buffer,
                                       GLint drawbuffer,
                                       const volatile GLint* value) {
  const char* func_name = "glClearBufferiv";
  if (!CheckBoundDrawFramebufferValid(func_name))
    return;
  ApplyDirtyState();

  if (buffer == GL_COLOR) {
    if (drawbuffer < 0 ||
        drawbuffer >= static_cast<GLint>(group_->max_draw_buffers())) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
      return;
    }
    GLenum internal_format =
        GetBoundColorDrawBufferInternalFormat(drawbuffer);
    if (!GLES2Util::IsSignedIntegerFormat(internal_format)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "can only be called on signed integer buffers");
      return;
    }
  } else {
    DCHECK(buffer == GL_STENCIL);
    if (drawbuffer != 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
      return;
    }
    if (!BoundFramebufferHasStencilAttachment()) {
      return;
    }
  }
  MarkDrawBufferAsCleared(buffer, drawbuffer);
  api()->glClearBufferivFn(buffer, drawbuffer, const_cast<const GLint*>(value));
}

void GLES2DecoderImpl::DoClearBufferuiv(GLenum buffer,
                                        GLint drawbuffer,
                                        const volatile GLuint* value) {
  const char* func_name = "glClearBufferuiv";
  if (!CheckBoundDrawFramebufferValid(func_name))
    return;
  ApplyDirtyState();

  if (drawbuffer < 0 ||
      drawbuffer >= static_cast<GLint>(group_->max_draw_buffers())) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
    return;
  }
  GLenum internal_format =
      GetBoundColorDrawBufferInternalFormat(drawbuffer);
  if (!GLES2Util::IsUnsignedIntegerFormat(internal_format)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "can only be called on unsigned integer buffers");
    return;
  }
  MarkDrawBufferAsCleared(buffer, drawbuffer);
  api()->glClearBufferuivFn(buffer, drawbuffer,
                            const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoClearBufferfv(GLenum buffer,
                                       GLint drawbuffer,
                                       const volatile GLfloat* value) {
  const char* func_name = "glClearBufferfv";
  if (!CheckBoundDrawFramebufferValid(func_name))
    return;
  ApplyDirtyState();

  if (buffer == GL_COLOR) {
    if (drawbuffer < 0 ||
        drawbuffer >= static_cast<GLint>(group_->max_draw_buffers())) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
      return;
    }
    GLenum internal_format =
        GetBoundColorDrawBufferInternalFormat(drawbuffer);
    if (GLES2Util::IsIntegerFormat(internal_format)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "can only be called on float buffers");
      return;
    }
  } else {
    DCHECK(buffer == GL_DEPTH);
    if (drawbuffer != 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
      return;
    }
    if (!BoundFramebufferHasDepthAttachment()) {
      return;
    }
  }
  MarkDrawBufferAsCleared(buffer, drawbuffer);
  api()->glClearBufferfvFn(buffer, drawbuffer,
                           const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoClearBufferfi(
    GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
  const char* func_name = "glClearBufferfi";
  if (!CheckBoundDrawFramebufferValid(func_name))
    return;
  ApplyDirtyState();

  if (drawbuffer != 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, func_name, "invalid drawBuffer");
    return;
  }
  if (!BoundFramebufferHasDepthAttachment() &&
      !BoundFramebufferHasStencilAttachment()) {
    return;
  }
  MarkDrawBufferAsCleared(GL_DEPTH, drawbuffer);
  MarkDrawBufferAsCleared(GL_STENCIL, drawbuffer);
  api()->glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

void GLES2DecoderImpl::DoFramebufferParameteri(GLenum target,
                                               GLenum pname,
                                               GLint param) {
  const char* func_name = "glFramebufferParameteri";
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "no framebuffer bound");
    return;
  }
  api()->glFramebufferParameteriFn(target, pname, param);
}

void GLES2DecoderImpl::DoFramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint client_renderbuffer_id) {
  const char* func_name = "glFramebufferRenderbuffer";
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "no framebuffer bound");
    return;
  }
  GLuint service_id = 0;
  Renderbuffer* renderbuffer = nullptr;
  if (client_renderbuffer_id) {
    renderbuffer = GetRenderbuffer(client_renderbuffer_id);
    if (!renderbuffer) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "unknown renderbuffer");
      return;
    }
    if (!renderbuffer->IsValid()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "renderbuffer never bound or deleted");
      return;
    }
    service_id = renderbuffer->service_id();
  }
  std::vector<GLenum> attachments;
  if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
    attachments.push_back(GL_DEPTH_ATTACHMENT);
    attachments.push_back(GL_STENCIL_ATTACHMENT);
  } else {
    attachments.push_back(attachment);
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(func_name);
  for (GLenum attachment_point : attachments) {
    api()->glFramebufferRenderbufferEXTFn(target, attachment_point,
                                          renderbuffertarget, service_id);
    GLenum error = LOCAL_PEEK_GL_ERROR(func_name);
    if (error == GL_NO_ERROR) {
      framebuffer->AttachRenderbuffer(attachment_point, renderbuffer);
    }
  }
  if (framebuffer == framebuffer_state_.bound_draw_framebuffer.get()) {
    framebuffer_state_.clear_state_dirty = true;
  }
  OnFboChanged();
}

void GLES2DecoderImpl::DoDisable(GLenum cap) {
  if (SetCapabilityState(cap, false)) {
    if (cap == GL_PRIMITIVE_RESTART_FIXED_INDEX &&
        feature_info_->feature_flags().emulate_primitive_restart_fixed_index) {
      // Enable and Disable PRIMITIVE_RESTART only before and after
      // DrawElements* for old desktop GL.
      return;
    }
    if (cap == GL_FRAMEBUFFER_SRGB) {
      // Enable and Disable GL_FRAMEBUFFER_SRGB is done manually in
      // CheckBoundDrawFramebufferValid.
      return;
    }
    api()->glDisableFn(cap);
  }
}

void GLES2DecoderImpl::DoDisableiOES(GLenum target, GLuint index) {
  api()->glDisableiOESFn(target, index);
}

void GLES2DecoderImpl::DoEnable(GLenum cap) {
  if (SetCapabilityState(cap, true)) {
    if (cap == GL_PRIMITIVE_RESTART_FIXED_INDEX &&
        feature_info_->feature_flags().emulate_primitive_restart_fixed_index) {
      // Enable and Disable PRIMITIVE_RESTART only before and after
      // DrawElements* for old desktop GL.
      return;
    }
    if (cap == GL_FRAMEBUFFER_SRGB) {
      // Enable and Disable GL_FRAMEBUFFER_SRGB is done manually in
      // CheckBoundDrawFramebufferValid.
      return;
    }
    api()->glEnableFn(cap);
  }
}

void GLES2DecoderImpl::DoEnableiOES(GLenum target, GLuint index) {
  api()->glEnableiOESFn(target, index);
}

void GLES2DecoderImpl::DoDepthRangef(GLclampf znear, GLclampf zfar) {
  state_.z_near = std::clamp(znear, 0.0f, 1.0f);
  state_.z_far = std::clamp(zfar, 0.0f, 1.0f);
  api()->glDepthRangeFn(znear, zfar);
}

void GLES2DecoderImpl::DoSampleCoverage(GLclampf value, GLboolean invert) {
  state_.sample_coverage_value = std::clamp(value, 0.0f, 1.0f);
  state_.sample_coverage_invert = (invert != 0);
  api()->glSampleCoverageFn(state_.sample_coverage_value, invert);
}

// Assumes framebuffer is complete.
void GLES2DecoderImpl::ClearUnclearedAttachments(
    GLenum target, Framebuffer* framebuffer) {
  // Clear textures that we can't use glClear first. These textures will be
  // marked as cleared after the call and no longer be part of the following
  // code.
  framebuffer->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      this, texture_manager());

  bool cleared_int_renderbuffers = false;
  Framebuffer* draw_framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer->HasUnclearedIntRenderbufferAttachments()) {
    if (target == GL_READ_FRAMEBUFFER && draw_framebuffer != framebuffer) {
      // TODO(zmo): There is no guarantee that an FBO that is complete on the
      // READ attachment will be complete as a DRAW attachment.
      api()->glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER,
                                    framebuffer->service_id());
    }
    state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
    ClearDeviceWindowRectangles();

    // TODO(zmo): Assume DrawBuffers() does not affect ClearBuffer().
    framebuffer->ClearUnclearedIntRenderbufferAttachments(
        renderbuffer_manager());

    cleared_int_renderbuffers = true;
  }

  GLbitfield clear_bits = 0;
  bool reset_draw_buffers = false;
  if (framebuffer->HasUnclearedColorAttachments()) {
    // We should always use alpha == 0 here, because 1) some draw buffers may
    // have alpha and some may not; 2) we won't have the same situation as the
    // back buffer where alpha channel exists but is not requested.
    api()->glClearColorFn(0.0f, 0.0f, 0.0f, 0.0f);
    state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    clear_bits |= GL_COLOR_BUFFER_BIT;

    if (SupportsDrawBuffers()) {
      reset_draw_buffers =
          framebuffer->PrepareDrawBuffersForClearingUninitializedAttachments();
    }
  }

  if (framebuffer->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT)) {
    api()->glClearStencilFn(0);
    state_.SetDeviceStencilMaskSeparate(GL_FRONT, kDefaultStencilMask);
    state_.SetDeviceStencilMaskSeparate(GL_BACK, kDefaultStencilMask);
    clear_bits |= GL_STENCIL_BUFFER_BIT;
  }

  if (framebuffer->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT)) {
    api()->glClearDepthFn(1.0f);
    state_.SetDeviceDepthMask(GL_TRUE);
    clear_bits |= GL_DEPTH_BUFFER_BIT;
  }

  if (clear_bits) {
    if (!cleared_int_renderbuffers &&
        target == GL_READ_FRAMEBUFFER && draw_framebuffer != framebuffer) {
      // TODO(zmo): There is no guarantee that an FBO that is complete on the
      // READ attachment will be complete as a DRAW attachment.
      api()->glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER,
                                    framebuffer->service_id());
    }
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
    ClearDeviceWindowRectangles();
    if (workarounds().gl_clear_broken) {
      ClearFramebufferForWorkaround(clear_bits);
    } else {
      api()->glClearFn(clear_bits);
    }
  }

  if (cleared_int_renderbuffers || clear_bits) {
    if (reset_draw_buffers)
      framebuffer->RestoreDrawBuffers();
    RestoreClearState();
    if (target == GL_READ_FRAMEBUFFER && draw_framebuffer != framebuffer) {
      GLuint service_id = draw_framebuffer ? draw_framebuffer->service_id() :
                                             GetBackbufferServiceId();
      api()->glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER, service_id);
    }
  }

  framebuffer_manager()->MarkAttachmentsAsCleared(
      framebuffer, renderbuffer_manager(), texture_manager());
}

void GLES2DecoderImpl::RestoreClearState() {
  framebuffer_state_.clear_state_dirty = true;
  api()->glClearColorFn(state_.color_clear_red, state_.color_clear_green,
                        state_.color_clear_blue, state_.color_clear_alpha);
  api()->glClearStencilFn(state_.stencil_clear);
  api()->glClearDepthFn(state_.depth_clear);
  state_.SetDeviceCapabilityState(GL_SCISSOR_TEST,
                                  state_.enable_flags.scissor_test);
  RestoreDeviceWindowRectangles();
  api()->glScissorFn(state_.scissor_x, state_.scissor_y, state_.scissor_width,
                     state_.scissor_height);
}

GLenum GLES2DecoderImpl::DoCheckFramebufferStatus(GLenum target) {
  Framebuffer* framebuffer =
      GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    return GL_FRAMEBUFFER_COMPLETE;
  }
  GLenum completeness = framebuffer->IsPossiblyComplete(feature_info_.get());
  if (completeness != GL_FRAMEBUFFER_COMPLETE) {
    return completeness;
  }
  return framebuffer->GetStatus(texture_manager(), target);
}

void GLES2DecoderImpl::DoFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget,
    GLuint client_texture_id, GLint level) {
  DoFramebufferTexture2DCommon(
    "glFramebufferTexture2D", target, attachment,
    textarget, client_texture_id, level, 0);
}

void GLES2DecoderImpl::DoFramebufferTexture2DMultisample(
    GLenum target, GLenum attachment, GLenum textarget,
    GLuint client_texture_id, GLint level, GLsizei samples) {
  DoFramebufferTexture2DCommon(
    "glFramebufferTexture2DMultisample", target, attachment,
    textarget, client_texture_id, level, samples);
}

void GLES2DecoderImpl::DoFramebufferTexture2DCommon(
    const char* name, GLenum target, GLenum attachment, GLenum textarget,
    GLuint client_texture_id, GLint level, GLsizei samples) {
  if (samples > renderbuffer_manager()->max_samples()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glFramebufferTexture2DMultisample", "samples too large");
    return;
  }
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        name, "no framebuffer bound.");
    return;
  }
  GLuint service_id = 0;
  TextureRef* texture_ref = nullptr;
  if (client_texture_id) {
    texture_ref = GetTexture(client_texture_id);
    if (!texture_ref) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          name, "unknown texture_ref");
      return;
    }
    GLenum texture_target = texture_ref->texture()->target();
    if (texture_target != GLES2Util::GLFaceTargetToTextureTarget(textarget)) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          name, "Attachment textarget doesn't match texture target");
      return;
    }
    service_id = texture_ref->service_id();
  }

  bool valid_target = false;
  if (texture_ref) {
    valid_target = texture_manager()->ValidForTextureTarget(
        texture_ref->texture(), level, 0, 0, 1);
  } else {
    valid_target = texture_manager()->ValidForTarget(textarget, level, 0, 0, 1);
  }

  if ((level > 0 && !feature_info_->IsWebGL2OrES3Context() &&
       !(fbo_render_mipmap_explicitly_enabled_ &&
         feature_info_->feature_flags().oes_fbo_render_mipmap)) ||
      !valid_target) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        name, "level out of range");
    return;
  }

  std::vector<GLenum> attachments;
  if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
    attachments.push_back(GL_DEPTH_ATTACHMENT);
    attachments.push_back(GL_STENCIL_ATTACHMENT);
  } else {
    attachments.push_back(attachment);
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(name);
  for (size_t ii = 0; ii < attachments.size(); ++ii) {
    if (0 == samples) {
      api()->glFramebufferTexture2DEXTFn(target, attachments[ii], textarget,
                                         service_id, level);
    } else {
      api()->glFramebufferTexture2DMultisampleEXTFn(
          target, attachments[ii], textarget, service_id, level, samples);
    }
    GLenum error = LOCAL_PEEK_GL_ERROR(name);
    if (error == GL_NO_ERROR) {
      framebuffer->AttachTexture(attachments[ii], texture_ref, textarget, level,
                                 samples);
    }
  }
  if (framebuffer == framebuffer_state_.bound_draw_framebuffer.get()) {
    framebuffer_state_.clear_state_dirty = true;
  }

  OnFboChanged();
}

void GLES2DecoderImpl::DoFramebufferTextureLayer(
    GLenum target, GLenum attachment, GLuint client_texture_id,
    GLint level, GLint layer) {
  const char* function_name = "glFramebufferTextureLayer";

  TextureRef* texture_ref = nullptr;
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "no framebuffer bound.");
    return;
  }
  GLuint service_id = 0;
  GLenum texture_target = 0;
  if (client_texture_id) {
    texture_ref = GetTexture(client_texture_id);
    if (!texture_ref) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_VALUE, function_name, "unknown texture");
      return;
    }
    service_id = texture_ref->service_id();

    texture_target = texture_ref->texture()->target();
    switch (texture_target) {
      case GL_TEXTURE_3D:
      case GL_TEXTURE_2D_ARRAY:
        break;
      default:
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
            "texture is neither TEXTURE_3D nor TEXTURE_2D_ARRAY");
        return;
    }
    if (!texture_manager()->ValidForTextureTarget(texture_ref->texture(), level,
                                                  0, 0, layer)) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_VALUE, function_name, "invalid level or layer");
      return;
    }
  }
  api()->glFramebufferTextureLayerFn(target, attachment, service_id, level,
                                     layer);
  if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
    framebuffer->AttachTextureLayer(
        GL_DEPTH_ATTACHMENT, texture_ref, texture_target, level, layer);
    framebuffer->AttachTextureLayer(
        GL_STENCIL_ATTACHMENT, texture_ref, texture_target, level, layer);
  } else {
    framebuffer->AttachTextureLayer(
        attachment, texture_ref, texture_target, level, layer);
  }
  if (framebuffer == framebuffer_state_.bound_draw_framebuffer.get()) {
    framebuffer_state_.clear_state_dirty = true;
  }
}

void GLES2DecoderImpl::DoFramebufferTextureMultiviewOVR(
    GLenum target,
    GLenum attachment,
    GLuint client_texture_id,
    GLint level,
    GLint base_view_index,
    GLsizei num_views) {
  // This is only supported in passthrough command buffer.
  NOTREACHED_IN_MIGRATION();
}

void GLES2DecoderImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint* params,
    GLsizei params_size) {
  const char kFunctionName[] = "glGetFramebufferAttachmentParameteriv";
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    if (!feature_info_->IsWebGL2OrES3Context()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
          "no framebuffer bound");
      return;
    }
    if (!validators_->backbuffer_attachment.IsValid(attachment)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
          "invalid attachment for backbuffer");
      return;
    }
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        *params = static_cast<GLint>(GL_FRAMEBUFFER_DEFAULT);
        return;
      case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
      case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
        // Delegate to underlying driver.
        break;
      default:
        LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, kFunctionName,
            "invalid pname for backbuffer");
        return;
    }
    if (GetBackbufferServiceId() != 0) {  // Emulated backbuffer.
      switch (attachment) {
        case GL_BACK:
          attachment = GL_COLOR_ATTACHMENT0;
          break;
        case GL_DEPTH:
          attachment = GL_DEPTH_ATTACHMENT;
          break;
        case GL_STENCIL:
          attachment = GL_STENCIL_ATTACHMENT;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
  } else {
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
      const Framebuffer::Attachment* depth =
          framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT);
      const Framebuffer::Attachment* stencil =
          framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT);
      if ((!depth && !stencil) ||
          (depth && stencil && depth->IsSameAttachment(stencil))) {
        attachment = GL_DEPTH_ATTACHMENT;
      } else {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                           "depth and stencil attachment mismatch");
        return;
      }
    }
  }
  if (pname == GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT &&
      features().use_img_for_multisampled_render_to_texture) {
    pname = GL_TEXTURE_SAMPLES_IMG;
  }
  if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
    DCHECK(framebuffer);
    // If we query from the driver, it will be service ID; however, we need to
    // return the client ID here.
    const Framebuffer::Attachment* attachment_object =
        framebuffer->GetAttachment(attachment);
    *params = attachment_object ? attachment_object->object_name() : 0;
    return;
  }

  api()->glGetFramebufferAttachmentParameterivEXTFn(target, attachment, pname,
                                                    params);
  // We didn't perform a full error check before gl call.
  LOCAL_PEEK_GL_ERROR(kFunctionName);
}

void GLES2DecoderImpl::DoGetRenderbufferParameteriv(GLenum target,
                                                    GLenum pname,
                                                    GLint* params,
                                                    GLsizei params_size) {
  Renderbuffer* renderbuffer =
      GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glGetRenderbufferParameteriv", "no renderbuffer bound");
    return;
  }

  EnsureRenderbufferBound();
  switch (pname) {
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      *params = renderbuffer->internal_format();
      break;
    case GL_RENDERBUFFER_WIDTH:
      *params = renderbuffer->width();
      break;
    case GL_RENDERBUFFER_HEIGHT:
      *params = renderbuffer->height();
      break;
    case GL_RENDERBUFFER_SAMPLES_EXT:
      if (features().use_img_for_multisampled_render_to_texture) {
        api()->glGetRenderbufferParameterivEXTFn(
            target, GL_RENDERBUFFER_SAMPLES_IMG, params);
      } else {
        api()->glGetRenderbufferParameterivEXTFn(
            target, GL_RENDERBUFFER_SAMPLES_EXT, params);
      }
      break;
    default:
      api()->glGetRenderbufferParameterivEXTFn(target, pname, params);
      break;
  }
}

void GLES2DecoderImpl::DoBlitFramebufferCHROMIUM(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
    GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
    GLbitfield mask, GLenum filter) {
  const char* func_name = "glBlitFramebufferCHROMIUM";

  if (!CheckFramebufferValid(GetBoundDrawFramebuffer(),
                             GetDrawFramebufferTarget(),
                             GL_INVALID_FRAMEBUFFER_OPERATION, func_name)) {
    return;
  }

  // We need to get this before checking if the read framebuffer is valid.
  // Checking the read framebuffer may clear attachments which would mark the
  // draw framebuffer as incomplete. Framebuffer::GetFramebufferValidSize()
  // requires the framebuffer to be complete.
  gfx::Size draw_size = GetBoundDrawFramebufferSize();

  if (!CheckFramebufferValid(GetBoundReadFramebuffer(),
                             GetReadFramebufferTarget(),
                             GL_INVALID_FRAMEBUFFER_OPERATION, func_name)) {
    return;
  }

  if (GetBoundFramebufferSamples(GL_DRAW_FRAMEBUFFER) > 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                       "destination framebuffer is multisampled");
    return;
  }

  GLsizei read_buffer_samples = GetBoundFramebufferSamples(GL_READ_FRAMEBUFFER);
  if (read_buffer_samples > 0 &&
      (srcX0 != dstX0 || srcY0 != dstY0 || srcX1 != dstX1 || srcY1 != dstY1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "src framebuffer is multisampled, but src/dst regions are different");
    return;
  }

  // If color/depth/stencil buffer have no image, we can remove corresponding
  // bitfield from mask and return early if mask equals to 0.
  // But validations should be done against the original mask.
  GLbitfield mask_blit = mask;

  // Detect that designated read/depth/stencil buffer in read framebuffer miss
  // image, and the corresponding buffers in draw framebuffer have image.
  bool read_framebuffer_miss_image = false;

  // Check whether read framebuffer and draw framebuffer have identical image
  // TODO(yunchao): consider doing something like CheckFramebufferStatus().
  // We cache the validation results, and if read_framebuffer doesn't change,
  // draw_framebuffer doesn't change, then use the cached status.
  enum FeedbackLoopState {
    FeedbackLoopTrue,
    FeedbackLoopFalse,
    FeedbackLoopUnknown
  };

  FeedbackLoopState is_feedback_loop = FeedbackLoopUnknown;
  Framebuffer* read_framebuffer =
      framebuffer_state_.bound_read_framebuffer.get();
  Framebuffer* draw_framebuffer =
      framebuffer_state_.bound_draw_framebuffer.get();
  // If both read framebuffer and draw framebuffer are default framebuffer,
  // They always have identical image. Otherwise, if one of read framebuffer
  // and draw framebuffe is default framebuffer, but the other is fbo, they
  // always have no identical image.
  if (!read_framebuffer && !draw_framebuffer) {
    is_feedback_loop = FeedbackLoopTrue;
  } else if (!read_framebuffer || !draw_framebuffer) {
    is_feedback_loop = FeedbackLoopFalse;
    if (read_framebuffer) {
      if (((mask & GL_COLOR_BUFFER_BIT) != 0 &&
          !GetBoundReadFramebufferInternalFormat()) ||
          ((mask & GL_DEPTH_BUFFER_BIT) != 0 &&
          !read_framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT) &&
          BoundFramebufferHasDepthAttachment()) ||
          ((mask & GL_STENCIL_BUFFER_BIT) != 0 &&
          !read_framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT) &&
          BoundFramebufferHasStencilAttachment())) {
        read_framebuffer_miss_image = true;
      }
    }
  } else {
    DCHECK(read_framebuffer && draw_framebuffer);
    if ((mask & GL_DEPTH_BUFFER_BIT) != 0) {
      const Framebuffer::Attachment* depth_buffer_read =
          read_framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT);
      const Framebuffer::Attachment* depth_buffer_draw =
          draw_framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT);
      if (!depth_buffer_draw || !depth_buffer_read) {
        mask_blit &= ~GL_DEPTH_BUFFER_BIT;
        if (depth_buffer_draw) {
          read_framebuffer_miss_image = true;
        }
      } else if (depth_buffer_draw->IsSameAttachment(depth_buffer_read)) {
        is_feedback_loop = FeedbackLoopTrue;
      }
    }
    if ((mask & GL_STENCIL_BUFFER_BIT) != 0) {
      const Framebuffer::Attachment* stencil_buffer_read =
          read_framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT);
      const Framebuffer::Attachment* stencil_buffer_draw =
          draw_framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT);
      if (!stencil_buffer_draw || !stencil_buffer_read) {
        mask_blit &= ~GL_STENCIL_BUFFER_BIT;
        if (stencil_buffer_draw) {
          read_framebuffer_miss_image = true;
        }
      } else if (stencil_buffer_draw->IsSameAttachment(stencil_buffer_read)) {
        is_feedback_loop = FeedbackLoopTrue;
      }
    }
  }

  GLenum src_internal_format = GetBoundReadFramebufferInternalFormat();
  GLenum src_type = GetBoundReadFramebufferTextureType();

  bool read_buffer_has_srgb = GLES2Util::GetColorEncodingFromInternalFormat(
                                  src_internal_format) == GL_SRGB;
  bool draw_buffers_has_srgb = false;
  if ((mask & GL_COLOR_BUFFER_BIT) != 0) {
    bool is_src_signed_int =
        GLES2Util::IsSignedIntegerFormat(src_internal_format);
    bool is_src_unsigned_int =
        GLES2Util::IsUnsignedIntegerFormat(src_internal_format);
    DCHECK(!is_src_signed_int || !is_src_unsigned_int);

    if ((is_src_signed_int || is_src_unsigned_int) && filter == GL_LINEAR) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "invalid filter for integer format");
      return;
    }

    GLenum src_sized_format =
        GLES2Util::ConvertToSizedFormat(src_internal_format, src_type);
    DCHECK(read_framebuffer || (is_feedback_loop != FeedbackLoopUnknown));
    const Framebuffer::Attachment* read_buffer =
        is_feedback_loop == FeedbackLoopUnknown ?
        read_framebuffer->GetReadBufferAttachment() : nullptr;
    bool draw_buffer_has_image = false;
    for (uint32_t ii = 0; ii < group_->max_draw_buffers(); ++ii) {
      GLenum dst_format = GetBoundColorDrawBufferInternalFormat(
          static_cast<GLint>(ii));
      GLenum dst_type = GetBoundColorDrawBufferType(static_cast<GLint>(ii));
      if (dst_format == 0)
        continue;
      draw_buffer_has_image = true;
      if (!src_internal_format) {
        read_framebuffer_miss_image = true;
      }
      if (GLES2Util::GetColorEncodingFromInternalFormat(dst_format) == GL_SRGB)
        draw_buffers_has_srgb = true;
      if (read_buffer_samples > 0 &&
          (src_sized_format !=
           GLES2Util::ConvertToSizedFormat(dst_format, dst_type))) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                           "src and dst formats differ for color");
        return;
      }
      bool is_dst_signed_int = GLES2Util::IsSignedIntegerFormat(dst_format);
      bool is_dst_unsigned_int = GLES2Util::IsUnsignedIntegerFormat(dst_format);
      DCHECK(!is_dst_signed_int || !is_dst_unsigned_int);
      if (is_src_signed_int != is_dst_signed_int ||
          is_src_unsigned_int != is_dst_unsigned_int) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                           "incompatible src/dst color formats");
        return;
      }
      // Check whether draw buffers have identical color image with read buffer
      if (is_feedback_loop == FeedbackLoopUnknown) {
        GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + ii);
        DCHECK(draw_framebuffer);
        const Framebuffer::Attachment* draw_buffer =
            draw_framebuffer->GetAttachment(attachment);
        if (!draw_buffer || !read_buffer) {
          continue;
        }
        if (draw_buffer->IsSameAttachment(read_buffer)) {
          is_feedback_loop = FeedbackLoopTrue;
          break;
        }
      }
    }
    if (draw_framebuffer && !draw_buffer_has_image)
      mask_blit &= ~GL_COLOR_BUFFER_BIT;
  }
  if (is_feedback_loop == FeedbackLoopTrue) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "source buffer and destination buffers are identical");
    return;
  }
  if (read_framebuffer_miss_image == true) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
       "The designated attachment point(s) in read framebuffer miss image");
    return;
  }

  if ((mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0) {
    if (filter != GL_NEAREST) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "invalid filter for depth/stencil");
      return;
    }
  }

  mask = mask_blit;
  if (!mask)
    return;

  if (((mask & GL_DEPTH_BUFFER_BIT) != 0 &&
      (GetBoundFramebufferDepthFormat(GL_READ_FRAMEBUFFER) !=
      GetBoundFramebufferDepthFormat(GL_DRAW_FRAMEBUFFER))) ||
      ((mask & GL_STENCIL_BUFFER_BIT) != 0 &&
      ((GetBoundFramebufferStencilFormat(GL_READ_FRAMEBUFFER) !=
      GetBoundFramebufferStencilFormat(GL_DRAW_FRAMEBUFFER))))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                       "src and dst formats differ for depth/stencil");
    return;
  }

  base::CheckedNumeric<GLint> src_width_temp = srcX1;
  src_width_temp -= srcX0;
  base::CheckedNumeric<GLint> src_height_temp = srcY1;
  src_height_temp -= srcY0;
  base::CheckedNumeric<GLint> dst_width_temp = dstX1;
  dst_width_temp -= dstX0;
  base::CheckedNumeric<GLint> dst_height_temp = dstY1;
  dst_height_temp -= dstY0;
  if (!src_width_temp.Abs().IsValid() || !src_height_temp.Abs().IsValid() ||
      !dst_width_temp.Abs().IsValid() || !dst_height_temp.Abs().IsValid()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name,
                       "the width or height of src or dst region overflowed");
    return;
  }

  if (workarounds().adjust_src_dst_region_for_blitframebuffer) {
    gfx::Size read_size = GetBoundReadFramebufferSize();
    GLint src_x = srcX1 > srcX0 ? srcX0 : srcX1;
    GLint src_y = srcY1 > srcY0 ? srcY0 : srcY1;
    unsigned int src_width = base::checked_cast<unsigned int>(
        src_width_temp.Abs().ValueOrDefault(0));
    unsigned int src_height = base::checked_cast<unsigned int>(
        src_height_temp.Abs().ValueOrDefault(0));

    GLint dst_x = dstX1 > dstX0 ? dstX0 : dstX1;
    GLint dst_y = dstY1 > dstY0 ? dstY0 : dstY1;
    unsigned int dst_width = base::checked_cast<unsigned int>(
        dst_width_temp.Abs().ValueOrDefault(0));
    unsigned int dst_height = base::checked_cast<unsigned int>(
        dst_height_temp.Abs().ValueOrDefault(0));

    if (dst_width == 0 || src_width == 0 || dst_height == 0 ||
        src_height == 0) {
      return;
    }

    gfx::Rect src_bounds(0, 0, read_size.width(), read_size.height());
    gfx::Rect src_region(src_x, src_y, src_width, src_height);

    gfx::Rect dst_bounds(0, 0, draw_size.width(), draw_size.height());
    gfx::Rect dst_region(dst_x, dst_y, dst_width, dst_height);

    if (gfx::IntersectRects(dst_bounds, dst_region).IsEmpty()) {
      return;
    }

    bool x_flipped = ((srcX1 > srcX0) && (dstX1 < dstX0)) ||
                     ((srcX1 < srcX0) && (dstX1 > dstX0));
    bool y_flipped = ((srcY1 > srcY0) && (dstY1 < dstY0)) ||
                     ((srcY1 < srcY0) && (dstY1 > dstY0));

    if (!dst_bounds.Contains(dst_region)) {
      // dst_region is not within dst_bounds. We want to adjust it to a
      // reasonable size. This is done by halving the dst_region until it is at
      // most twice the size of the framebuffer. We cut it in half instead
      // of arbitrarily shrinking it to fit so that we don't end up with
      // non-power-of-two scale factors which could mess up pixel interpolation.
      // Naively clipping the dst rect and then proportionally sizing the
      // src rect yields incorrect results.

      unsigned int dst_x_halvings = 0;
      unsigned int dst_y_halvings = 0;
      int dst_origin_x = dst_x;
      int dst_origin_y = dst_y;

      int dst_clipped_width = dst_region.width();
      while (dst_clipped_width > 2 * dst_bounds.width()) {
        dst_clipped_width = dst_clipped_width >> 1;
        dst_x_halvings++;
      }

      int dst_clipped_height = dst_region.height();
      while (dst_clipped_height > 2 * dst_bounds.height()) {
        dst_clipped_height = dst_clipped_height >> 1;
        dst_y_halvings++;
      }

      // Before this block, we check that the two rectangles intersect.
      // Now, compute the location of a new region origin such that we use the
      // scaled dimensions but the new region has the same intersection as the
      // original region.

      int left = dst_region.x();
      int right = dst_region.right();
      int top = dst_region.y();
      int bottom = dst_region.bottom();

      if (left >= 0 && left < dst_bounds.width()) {
        // Left edge is in-bounds
        dst_origin_x = dst_x;
      } else if (right > 0 && right <= dst_bounds.width()) {
        // Right edge is in-bounds
        dst_origin_x = right - dst_clipped_width;
      } else {
        // Region completely spans bounds
        dst_origin_x = dst_x;
      }

      if (top >= 0 && top < dst_bounds.height()) {
        // Top edge is in-bounds
        dst_origin_y = dst_y;
      } else if (bottom > 0 && bottom <= dst_bounds.height()) {
        // Bottom edge is in-bounds
        dst_origin_y = bottom - dst_clipped_height;
      } else {
        // Region completely spans bounds
        dst_origin_y = dst_y;
      }

      dst_region.SetRect(dst_origin_x, dst_origin_y, dst_clipped_width,
                         dst_clipped_height);

      // Offsets from the bottom left corner of the original region to
      // the bottom left corner of the clipped region.
      // This value (after it is scaled) is the respective offset we will apply
      // to the src origin.
      base::CheckedNumeric<unsigned int> checked_xoffset(dst_region.x() -
                                                         dst_x);
      base::CheckedNumeric<unsigned int> checked_yoffset(dst_region.y() -
                                                         dst_y);

      // if X/Y is reversed, use the top/right out-of-bounds region to compute
      // the origin offset instead of the left/bottom out-of-bounds region
      if (x_flipped) {
        checked_xoffset = (dst_x + dst_width - dst_region.right());
      }
      if (y_flipped) {
        checked_yoffset = (dst_y + dst_height - dst_region.bottom());
      }

      // These offsets should never overflow.
      unsigned int xoffset, yoffset;
      if (!checked_xoffset.AssignIfValid(&xoffset) ||
          !checked_yoffset.AssignIfValid(&yoffset)) {
        NOTREACHED_IN_MIGRATION();
        LOCAL_SET_GL_ERROR(
            GL_INVALID_VALUE, func_name,
            "the width or height of src or dst region overflowed");
        return;
      }

      // Adjust the src region by the same factor
      src_region.SetRect(src_x + (xoffset >> dst_x_halvings),
                         src_y + (yoffset >> dst_y_halvings),
                         src_region.width() >> dst_x_halvings,
                         src_region.height() >> dst_y_halvings);

      // If the src was scaled to 0, set it to 1 so the src is non-empty.
      if (src_region.width() == 0) {
        src_region.set_width(1);
      }
      if (src_region.height() == 0) {
        src_region.set_height(1);
      }
    }

    if (!src_bounds.Contains(src_region)) {
      // If pixels lying outside the read framebuffer, adjust src region
      // and dst region to appropriate in-bounds regions respectively.
      src_region.Intersect(src_bounds);
      GLuint src_real_width = src_region.width();
      GLuint src_real_height = src_region.height();
      GLuint xoffset = src_region.x() - src_x;
      GLuint yoffset = src_region.y() - src_y;
      // if X/Y is reversed, use the top/right out-of-bounds region for mapping
      // to dst region, instead of left/bottom out-of-bounds region for mapping.
      if (x_flipped) {
        xoffset = src_x + src_width - src_region.x() - src_region.width();
      }
      if (y_flipped) {
        yoffset = src_y + src_height - src_region.y() - src_region.height();
      }

      GLfloat dst_mapping_width =
          static_cast<GLfloat>(src_real_width) * dst_width / src_width;
      GLfloat dst_mapping_height =
          static_cast<GLfloat>(src_real_height) * dst_height / src_height;
      GLfloat dst_mapping_xoffset =
          static_cast<GLfloat>(xoffset) * dst_width / src_width;
      GLfloat dst_mapping_yoffset =
          static_cast<GLfloat>(yoffset) * dst_height / src_height;

      GLuint dst_mapping_x0 =
          std::round(dst_x + dst_mapping_xoffset);
      GLuint dst_mapping_y0 =
          std::round(dst_y + dst_mapping_yoffset);

      GLuint dst_mapping_x1 =
          std::round(dst_x + dst_mapping_xoffset + dst_mapping_width);
      GLuint dst_mapping_y1 =
          std::round(dst_y + dst_mapping_yoffset + dst_mapping_height);

      dst_region.SetRect(dst_mapping_x0, dst_mapping_y0,
                         dst_mapping_x1 - dst_mapping_x0,
                         dst_mapping_y1 - dst_mapping_y0);
    }

    // Set the src and dst endpoints. If they were previously flipped,
    // set them as flipped.
    srcX0 = srcX0 < srcX1 ? src_region.x() : src_region.right();
    srcY0 = srcY0 < srcY1 ? src_region.y() : src_region.bottom();
    srcX1 = srcX0 < srcX1 ? src_region.right() : src_region.x();
    srcY1 = srcY0 < srcY1 ? src_region.bottom() : src_region.y();

    dstX0 = dstX0 < dstX1 ? dst_region.x() : dst_region.right();
    dstY0 = dstY0 < dstY1 ? dst_region.y() : dst_region.bottom();
    dstX1 = dstX0 < dstX1 ? dst_region.right() : dst_region.x();
    dstY1 = dstY0 < dstY1 ? dst_region.bottom() : dst_region.y();
  }

  bool enable_srgb =
      (read_buffer_has_srgb || draw_buffers_has_srgb) &&
      ((mask & GL_COLOR_BUFFER_BIT) != 0);
  if (enable_srgb && feature_info_->feature_flags().ext_srgb_write_control) {
    state_.EnableDisableFramebufferSRGB(enable_srgb);
  }

  api()->glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                             dstY1, mask, filter);
}

void GLES2DecoderImpl::UnbindTexture(TextureRef* texture_ref,
                                     bool supports_separate_framebuffer_binds) {
  Texture* texture = texture_ref->texture();
  if (texture->IsAttachedToFramebuffer()) {
    framebuffer_state_.clear_state_dirty = true;
  }
  // Unbind texture_ref from texture_ref units.
  state_.UnbindTexture(texture_ref);

  // Unbind from current framebuffers.
  if (supports_separate_framebuffer_binds) {
    if (framebuffer_state_.bound_read_framebuffer.get()) {
      framebuffer_state_.bound_read_framebuffer->UnbindTexture(
          GL_READ_FRAMEBUFFER_EXT, texture_ref);
    }
    if (framebuffer_state_.bound_draw_framebuffer.get()) {
      framebuffer_state_.bound_draw_framebuffer->UnbindTexture(
          GL_DRAW_FRAMEBUFFER_EXT, texture_ref);
    }
  } else {
    if (framebuffer_state_.bound_draw_framebuffer.get()) {
      framebuffer_state_.bound_draw_framebuffer->UnbindTexture(GL_FRAMEBUFFER,
                                                               texture_ref);
    }
  }
}

void GLES2DecoderImpl::EnsureRenderbufferBound() {
  if (!state_.bound_renderbuffer_valid) {
    state_.bound_renderbuffer_valid = true;
    api()->glBindRenderbufferEXTFn(GL_RENDERBUFFER,
                                   state_.bound_renderbuffer.get()
                                       ? state_.bound_renderbuffer->service_id()
                                       : 0);
  }
}

void GLES2DecoderImpl::RenderbufferStorageMultisampleWithWorkaround(
    GLenum target,
    GLsizei samples,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    ForcedMultisampleMode mode) {
  RegenerateRenderbufferIfNeeded(state_.bound_renderbuffer.get());
  EnsureRenderbufferBound();
  RenderbufferStorageMultisampleHelper(target, samples, internal_format, width,
                                       height, mode);
}

void GLES2DecoderImpl::RenderbufferStorageMultisampleHelper(
    GLenum target,
    GLsizei samples,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    ForcedMultisampleMode mode) {
  if (samples == 0) {
    api()->glRenderbufferStorageEXTFn(target, internal_format, width, height);
    return;
  }

  // TODO(sievers): This could be resolved at the GL binding level, but the
  // binding process is currently a bit too 'brute force'.

  // Note that when this is called via the
  // RenderbufferStorageMultisampleEXT handler,
  // kForceExtMultisampledRenderToTexture is passed in order to
  // prevent the core ES 3.0 multisampling code path from being used.
  if (mode == kForceExtMultisampledRenderToTexture) {
    api()->glRenderbufferStorageMultisampleEXTFn(
        target, samples, internal_format, width, height);
  } else {
    api()->glRenderbufferStorageMultisampleFn(target, samples, internal_format,
                                              width, height);
  }
}

void GLES2DecoderImpl::RenderbufferStorageMultisampleHelperAMD(
    GLenum target,
    GLsizei samples,
    GLsizei storageSamples,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    ForcedMultisampleMode mode) {
  api()->glRenderbufferStorageMultisampleAdvancedAMDFn(
      target, samples, storageSamples, internal_format, width, height);
}

bool GLES2DecoderImpl::RegenerateRenderbufferIfNeeded(
    Renderbuffer* renderbuffer) {
  if (!renderbuffer->RegenerateAndBindBackingObjectIfNeeded(workarounds())) {
    return false;
  }

  if (renderbuffer != state_.bound_renderbuffer.get()) {
    // The renderbuffer bound in the driver has changed to the new
    // renderbuffer->service_id(). If that isn't state_.bound_renderbuffer,
    // then state_.bound_renderbuffer is no longer bound in the driver.
    state_.bound_renderbuffer_valid = false;
  }

  return true;
}

bool GLES2DecoderImpl::ValidateRenderbufferStorageMultisample(
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  // Must check against the internal format's maximum number of samples
  // first in order to generate the correct INVALID_OPERATION rather than
  // INVALID_VALUE, below.
  if (feature_info_->IsES3Capable() &&
      !GLES2Util::IsIntegerFormat(internalformat)) {
    std::vector<GLint> sample_counts;
    GLsizei num_sample_counts = InternalFormatSampleCountsHelper(
        GL_RENDERBUFFER, internalformat, &sample_counts);
    // SwiftShader reports 0 samples for GL_DEPTH24_STENCIL8; be robust to this.
    if (num_sample_counts > 0) {
      if (samples > sample_counts[0]) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                           "glRenderbufferStorageMultisample",
                           "samples out of range for internalformat");
        return false;
      }
    }
  }

  if (samples > renderbuffer_manager()->max_samples()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glRenderbufferStorageMultisample", "samples too large");
    return false;
  }

  if (width > renderbuffer_manager()->max_renderbuffer_size() ||
      height > renderbuffer_manager()->max_renderbuffer_size()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glRenderbufferStorageMultisample", "dimensions too large");
    return false;
  }

  uint32_t estimated_size = 0;
  if (!renderbuffer_manager()->ComputeEstimatedRenderbufferSize(
           width, height, samples, internalformat, &estimated_size)) {
    LOCAL_SET_GL_ERROR(
        GL_OUT_OF_MEMORY,
        "glRenderbufferStorageMultisample", "dimensions too large");
    return false;
  }

  return true;
}

bool GLES2DecoderImpl::ValidateRenderbufferStorageMultisampleAMD(
    GLsizei samples,
    GLsizei storageSamples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  if (samples > renderbuffer_manager()->max_samples()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRenderbufferStorageMultisample",
                       "samples too large");
    return false;
  }

  if (width > renderbuffer_manager()->max_renderbuffer_size() ||
      height > renderbuffer_manager()->max_renderbuffer_size()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRenderbufferStorageMultisample",
                       "dimensions too large");
    return false;
  }

  uint32_t estimated_size = 0;
  if (!renderbuffer_manager()->ComputeEstimatedRenderbufferSize(
          width, height, samples, internalformat, &estimated_size)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "glRenderbufferStorageMultisample",
                       "dimensions too large");
    return false;
  }

  return true;
}

void GLES2DecoderImpl::DoRenderbufferStorageMultisampleCHROMIUM(
    GLenum target, GLsizei samples, GLenum internalformat,
    GLsizei width, GLsizei height) {
  Renderbuffer* renderbuffer = GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glRenderbufferStorageMultisampleCHROMIUM",
                       "no renderbuffer bound");
    return;
  }

  if (!ValidateRenderbufferStorageMultisample(
      samples, internalformat, width, height)) {
    return;
  }

  GLenum impl_format =
      renderbuffer_manager()->InternalRenderbufferFormatToImplFormat(
          internalformat);
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(
      "glRenderbufferStorageMultisampleCHROMIUM");
  RenderbufferStorageMultisampleWithWorkaround(target, samples, impl_format,
                                               width, height, kDoNotForce);
  GLenum error =
      LOCAL_PEEK_GL_ERROR("glRenderbufferStorageMultisampleCHROMIUM");
  if (error == GL_NO_ERROR) {
    renderbuffer_manager()->SetInfoAndInvalidate(renderbuffer, samples,
                                                 internalformat, width, height);
  }
}

void GLES2DecoderImpl::DoRenderbufferStorageMultisampleAdvancedAMD(
    GLenum target,
    GLsizei samples,
    GLsizei storageSamples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  Renderbuffer* renderbuffer = GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glRenderbufferStorageMultisampleAdvancedAMD",
                       "no renderbuffer bound");
    return;
  }

  if (!ValidateRenderbufferStorageMultisampleAMD(
          samples, storageSamples, internalformat, width, height)) {
    return;
  }

  GLenum impl_format =
      renderbuffer_manager()->InternalRenderbufferFormatToImplFormat(
          internalformat);
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(
      "glRenderbufferStorageMultisampleAdvancedAMD");
  RenderbufferStorageMultisampleHelperAMD(
      target, samples, storageSamples, impl_format, width, height, kDoNotForce);
  GLenum error =
      LOCAL_PEEK_GL_ERROR("glRenderbufferStorageMultisampleAdvancedAMD");
  if (error == GL_NO_ERROR) {
    renderbuffer_manager()->SetInfoAndInvalidate(renderbuffer, samples,
                                                 internalformat, width, height);
  }
}

// This is the handler for multisampled_render_to_texture extensions.
void GLES2DecoderImpl::DoRenderbufferStorageMultisampleEXT(
    GLenum target, GLsizei samples, GLenum internalformat,
    GLsizei width, GLsizei height) {
  Renderbuffer* renderbuffer = GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glRenderbufferStorageMultisampleEXT",
                       "no renderbuffer bound");
    return;
  }

  if (!ValidateRenderbufferStorageMultisample(
      samples, internalformat, width, height)) {
    return;
  }

  GLenum impl_format =
      renderbuffer_manager()->InternalRenderbufferFormatToImplFormat(
          internalformat);
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glRenderbufferStorageMultisampleEXT");
  RenderbufferStorageMultisampleWithWorkaround(
      target, samples, impl_format, width, height,
      kForceExtMultisampledRenderToTexture);
  GLenum error = LOCAL_PEEK_GL_ERROR("glRenderbufferStorageMultisampleEXT");
  if (error == GL_NO_ERROR) {
    renderbuffer_manager()->SetInfoAndInvalidate(renderbuffer, samples,
                                                 internalformat, width, height);
  }
}

void GLES2DecoderImpl::DoReleaseShaderCompiler() {
  DestroyShaderTranslator();
}

void GLES2DecoderImpl::DoRenderbufferStorage(
  GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  Renderbuffer* renderbuffer =
      GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glRenderbufferStorage", "no renderbuffer bound");
    return;
  }

  if (width > renderbuffer_manager()->max_renderbuffer_size() ||
      height > renderbuffer_manager()->max_renderbuffer_size()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glRenderbufferStorage", "dimensions too large");
    return;
  }

  uint32_t estimated_size = 0;
  if (!renderbuffer_manager()->ComputeEstimatedRenderbufferSize(
           width, height, 1, internalformat, &estimated_size)) {
    LOCAL_SET_GL_ERROR(
        GL_OUT_OF_MEMORY, "glRenderbufferStorage", "dimensions too large");
    return;
  }

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glRenderbufferStorage");
  RenderbufferStorageMultisampleWithWorkaround(
      target, 0,
      renderbuffer_manager()->InternalRenderbufferFormatToImplFormat(
          internalformat),
      width, height, kDoNotForce);
  GLenum error = LOCAL_PEEK_GL_ERROR("glRenderbufferStorage");
  if (error == GL_NO_ERROR) {
    renderbuffer_manager()->SetInfoAndInvalidate(renderbuffer, 0,
                                                 internalformat, width, height);
  }
}

void GLES2DecoderImpl::DoLineWidth(GLfloat width) {
  api()->glLineWidthFn(
      std::clamp(width, line_width_range_[0], line_width_range_[1]));
}

void GLES2DecoderImpl::DoLinkProgram(GLuint program_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoLinkProgram");
  Program* program = GetProgramInfoNotShader(
      program_id, "glLinkProgram");
  if (!program) {
    return;
  }

  LogClientServiceForInfo(program, program_id, "glLinkProgram");
  if (program->Link(shader_manager(),
                    client())) {
    if (features().webgl_multi_draw)
      program_manager()->UpdateDrawIDUniformLocation(program);
    if (features().webgl_draw_instanced_base_vertex_base_instance ||
        features().webgl_multi_draw_instanced_base_vertex_base_instance) {
      program_manager()->UpdateBaseVertexUniformLocation(program);
      program_manager()->UpdateBaseInstanceUniformLocation(program);
    }
  }

  // LinkProgram can be very slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

void GLES2DecoderImpl::DoReadBuffer(GLenum src) {
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(GL_READ_FRAMEBUFFER);
  if (framebuffer) {
    if (src == GL_BACK) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_ENUM, "glReadBuffer",
          "invalid src for a named framebuffer");
      return;
    }
    framebuffer->set_read_buffer(src);
  } else {
    if (src != GL_NONE && src != GL_BACK) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_ENUM, "glReadBuffer",
          "invalid src for the default framebuffer");
      return;
    }
    back_buffer_read_buffer_ = src;
    if (GetBackbufferServiceId() && src == GL_BACK)
      src = GL_COLOR_ATTACHMENT0;
  }
  api()->glReadBufferFn(src);
}

void GLES2DecoderImpl::DoSamplerParameterf(
    GLuint client_id, GLenum pname, GLfloat param) {
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glSamplerParameterf", "unknown sampler");
    return;
  }
  sampler_manager()->SetParameterf("glSamplerParameterf", error_state_.get(),
                                   sampler, pname, param);
}

void GLES2DecoderImpl::DoSamplerParameteri(
    GLuint client_id, GLenum pname, GLint param) {
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glSamplerParameteri", "unknown sampler");
    return;
  }
  sampler_manager()->SetParameteri("glSamplerParameteri", error_state_.get(),
                                   sampler, pname, param);
}

void GLES2DecoderImpl::DoSamplerParameterfv(GLuint client_id,
                                            GLenum pname,
                                            const volatile GLfloat* params) {
  DCHECK(params);
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glSamplerParameterfv", "unknown sampler");
    return;
  }
  sampler_manager()->SetParameterf("glSamplerParameterfv", error_state_.get(),
                                   sampler, pname, params[0]);
}

void GLES2DecoderImpl::DoSamplerParameteriv(GLuint client_id,
                                            GLenum pname,
                                            const volatile GLint* params) {
  DCHECK(params);
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glSamplerParameteriv", "unknown sampler");
    return;
  }
  sampler_manager()->SetParameteri("glSamplerParameteriv", error_state_.get(),
                                   sampler, pname, params[0]);
}

void GLES2DecoderImpl::DoTexParameterf(
    GLenum target, GLenum pname, GLfloat param) {
  TextureRef* texture = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexParameterf", "unknown texture");
    return;
  }

  texture_manager()->SetParameterf("glTexParameterf", error_state_.get(),
                                   texture, pname, param);
}

void GLES2DecoderImpl::DoTexParameteri(
    GLenum target, GLenum pname, GLint param) {
  TextureRef* texture = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexParameteri", "unknown texture");
    return;
  }

  texture_manager()->SetParameteri("glTexParameteri", error_state_.get(),
                                   texture, pname, param);
}

void GLES2DecoderImpl::DoTexParameterfv(GLenum target,
                                        GLenum pname,
                                        const volatile GLfloat* params) {
  TextureRef* texture = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexParameterfv", "unknown texture");
    return;
  }

  texture_manager()->SetParameterf("glTexParameterfv", error_state_.get(),
                                   texture, pname, *params);
}

void GLES2DecoderImpl::DoTexParameteriv(GLenum target,
                                        GLenum pname,
                                        const volatile GLint* params) {
  TextureRef* texture = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glTexParameteriv", "unknown texture");
    return;
  }

  texture_manager()->SetParameteri("glTexParameteriv", error_state_.get(),
                                   texture, pname, *params);
}

bool GLES2DecoderImpl::CheckCurrentProgram(const char* function_name) {
  if (!state_.current_program.get()) {
    // The program does not exist.
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "no program in use");
    return false;
  }
  if (!state_.current_program->InUse()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "program not linked");
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::CheckCurrentProgramForUniform(
    GLint location, const char* function_name) {
  if (!CheckCurrentProgram(function_name)) {
    return false;
  }
  return !state_.current_program->IsInactiveUniformLocationByFakeLocation(
      location);
}

bool GLES2DecoderImpl::CheckDrawingFeedbackLoopsHelper(
    const Framebuffer::Attachment* attachment,
    TextureRef* texture_ref,
    const char* function_name) {
  if (attachment && attachment->IsTexture(texture_ref)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name,
        "Source and destination textures of the draw are the same.");
    return true;
  }
  return false;
}

bool GLES2DecoderImpl::SupportsDrawBuffers() const {
  return feature_info_->IsWebGL1OrES2Context() ?
      feature_info_->feature_flags().ext_draw_buffers : true;
}

bool GLES2DecoderImpl::ValidateAndAdjustDrawBuffers(const char* func_name) {
  if (state_.GetEnabled(GL_RASTERIZER_DISCARD)) {
    return true;
  }
  if (!SupportsDrawBuffers()) {
    return true;
  }
  Framebuffer* framebuffer = framebuffer_state_.bound_draw_framebuffer.get();
  if (!state_.current_program.get() || !framebuffer) {
    return true;
  }
  if (!state_.color_mask_red && !state_.color_mask_green &&
      !state_.color_mask_blue && !state_.color_mask_alpha) {
    return true;
  }
  uint32_t fragment_output_type_mask =
      state_.current_program->fragment_output_type_mask();
  uint32_t fragment_output_written_mask =
      state_.current_program->fragment_output_written_mask();
  if (!framebuffer->ValidateAndAdjustDrawBuffers(
      fragment_output_type_mask, fragment_output_written_mask)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "buffer format and fragment output variable type incompatible");
      return false;
  }
  return true;
}

void GLES2DecoderImpl::AdjustDrawBuffers() {
  if (!SupportsDrawBuffers()) {
    return;
  }
  Framebuffer* framebuffer = framebuffer_state_.bound_draw_framebuffer.get();
  if (framebuffer) {
    framebuffer->AdjustDrawBuffers();
  }
}

bool GLES2DecoderImpl::ValidateUniformBlockBackings(const char* func_name) {
  DCHECK(feature_info_->IsWebGL2OrES3Context());
  if (!state_.current_program.get())
    return true;
  int32_t max_index = -1;
  for (auto info : state_.current_program->uniform_block_size_info()) {
    int32_t index = static_cast<int32_t>(info.binding);
    if (index > max_index)
      max_index = index;
  }
  if (max_index < 0)
    return true;
  std::vector<GLsizeiptr> uniform_block_sizes(max_index + 1);
  for (int32_t ii = 0; ii <= max_index; ++ii)
    uniform_block_sizes[ii] = 0;
  for (auto info : state_.current_program->uniform_block_size_info()) {
    uint32_t index = info.binding;
    uniform_block_sizes[index] = static_cast<GLsizeiptr>(info.data_size);
  }
  return buffer_manager()->RequestBuffersAccess(
      error_state_.get(), state_.indexed_uniform_buffer_bindings.get(),
      uniform_block_sizes, 1, func_name, "uniform buffers");
}

bool GLES2DecoderImpl::CheckUniformForApiType(const Program::UniformInfo* info,
                                              const char* function_name,
                                              UniformApiType api_type) {
  DCHECK(info);
  if ((api_type & info->accepts_api_type) == UniformApiType::kUniformNone) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "wrong uniform function for type");
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::PrepForSetUniformByLocation(GLint fake_location,
                                                   const char* function_name,
                                                   UniformApiType api_type,
                                                   GLint* real_location,
                                                   GLenum* type,
                                                   GLsizei* count) {
  DCHECK(type);
  DCHECK(count);
  DCHECK(real_location);

  if (!CheckCurrentProgramForUniform(fake_location, function_name)) {
    return false;
  }
  GLint array_index = -1;
  const Program::UniformInfo* info =
      state_.current_program->GetUniformInfoByFakeLocation(
          fake_location, real_location, &array_index);
  if (!info) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "unknown location");
    return false;
  }
  if (!CheckUniformForApiType(info, function_name, api_type)) {
    return false;
  }
  if (*count > 1 && !info->is_array) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "count > 1 for non-array");
    return false;
  }
  *count = std::min(info->size - array_index, *count);
  if (*count <= 0) {
    return false;
  }
  *type = info->type;
  return true;
}

void GLES2DecoderImpl::DoUniform1i(GLint fake_location, GLint v0) {
  GLenum type = 0;
  GLsizei count = 1;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform1i",
                                   UniformApiType::kUniform1i, &real_location,
                                   &type, &count)) {
    return;
  }
  if (!state_.current_program->SetSamplers(
      state_.texture_units.size(), fake_location, 1, &v0)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUniform1i", "texture unit out of range");
    return;
  }
  api()->glUniform1iFn(real_location, v0);
}

void GLES2DecoderImpl::DoUniform1iv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLint* values) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform1iv",
                                   UniformApiType::kUniform1i, &real_location,
                                   &type, &count)) {
    return;
  }
  auto values_copy = std::make_unique<GLint[]>(count);
  GLint* safe_values = values_copy.get();
  std::copy(values, values + count, safe_values);
  if (type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_RECT_ARB ||
      type == GL_SAMPLER_CUBE || type == GL_SAMPLER_EXTERNAL_OES) {
    if (!state_.current_program->SetSamplers(
            state_.texture_units.size(), fake_location, count, safe_values)) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_VALUE, "glUniform1iv", "texture unit out of range");
      return;
    }
  }
  api()->glUniform1ivFn(real_location, count, safe_values);
}

void GLES2DecoderImpl::DoUniform1uiv(GLint fake_location,
                                     GLsizei count,
                                     const volatile GLuint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform1uiv",
                                   UniformApiType::kUniform1ui, &real_location,
                                   &type, &count)) {
    return;
  }
  api()->glUniform1uivFn(real_location, count,
                         const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoUniform1fv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform1fv",
                                   UniformApiType::kUniform1f, &real_location,
                                   &type, &count)) {
    return;
  }
  if (type == GL_BOOL) {
    auto temp = base::HeapArray<GLint>::Uninit(count);
    for (GLsizei ii = 0; ii < count; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    api()->glUniform1ivFn(real_location, count, temp.data());
  } else {
    api()->glUniform1fvFn(real_location, count,
                          const_cast<const GLfloat*>(value));
  }
}

void GLES2DecoderImpl::DoUniform2fv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform2fv",
                                   UniformApiType::kUniform2f, &real_location,
                                   &type, &count)) {
    return;
  }
  if (type == GL_BOOL_VEC2) {
    GLsizei num_values = count * 2;
    auto temp = base::HeapArray<GLint>::Uninit(num_values);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    api()->glUniform2ivFn(real_location, count, temp.data());
  } else {
    api()->glUniform2fvFn(real_location, count,
                          const_cast<const GLfloat*>(value));
  }
}

void GLES2DecoderImpl::DoUniform3fv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform3fv",
                                   UniformApiType::kUniform3f, &real_location,
                                   &type, &count)) {
    return;
  }
  if (type == GL_BOOL_VEC3) {
    GLsizei num_values = count * 3;
    auto temp = base::HeapArray<GLint>::Uninit(num_values);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    api()->glUniform3ivFn(real_location, count, temp.data());
  } else {
    api()->glUniform3fvFn(real_location, count,
                          const_cast<const GLfloat*>(value));
  }
}

void GLES2DecoderImpl::DoUniform4fv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform4fv",
                                   UniformApiType::kUniform4f, &real_location,
                                   &type, &count)) {
    return;
  }
  if (type == GL_BOOL_VEC4) {
    GLsizei num_values = count * 4;
    auto temp = base::HeapArray<GLint>::Uninit(num_values);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    api()->glUniform4ivFn(real_location, count, temp.data());
  } else {
    api()->glUniform4fvFn(real_location, count,
                          const_cast<const GLfloat*>(value));
  }
}

void GLES2DecoderImpl::DoUniform2iv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform2iv",
                                   UniformApiType::kUniform2i, &real_location,
                                   &type, &count)) {
    return;
  }
  api()->glUniform2ivFn(real_location, count, const_cast<const GLint*>(value));
}

void GLES2DecoderImpl::DoUniform2uiv(GLint fake_location,
                                     GLsizei count,
                                     const volatile GLuint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform2uiv",
                                   UniformApiType::kUniform2ui, &real_location,
                                   &type, &count)) {
    return;
  }
  api()->glUniform2uivFn(real_location, count,
                         const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoUniform3iv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform3iv",
                                   UniformApiType::kUniform3i, &real_location,
                                   &type, &count)) {
    return;
  }
  api()->glUniform3ivFn(real_location, count, const_cast<const GLint*>(value));
}

void GLES2DecoderImpl::DoUniform3uiv(GLint fake_location,
                                     GLsizei count,
                                     const volatile GLuint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform3uiv",
                                   UniformApiType::kUniform3ui, &real_location,
                                   &type, &count)) {
    return;
  }
  api()->glUniform3uivFn(real_location, count,
                         const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoUniform4iv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform4iv",
                                   UniformApiType::kUniform4i, &real_location,
                                   &type, &count)) {
    return;
  }
  api()->glUniform4ivFn(real_location, count, const_cast<const GLint*>(value));
}

void GLES2DecoderImpl::DoUniform4uiv(GLint fake_location,
                                     GLsizei count,
                                     const volatile GLuint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniform4uiv",
                                   UniformApiType::kUniform4ui, &real_location,
                                   &type, &count)) {
    return;
  }
  api()->glUniform4uivFn(real_location, count,
                         const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix2fv(GLint fake_location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (transpose && !feature_info_->IsWebGL2OrES3Context()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUniformMatrix2fv", "transpose not FALSE");
    return;
  }
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix2fv",
                                   UniformApiType::kUniformMatrix2f,
                                   &real_location, &type, &count)) {
    return;
  }
  api()->glUniformMatrix2fvFn(real_location, count, transpose,
                              const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix3fv(GLint fake_location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (transpose && !feature_info_->IsWebGL2OrES3Context()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUniformMatrix3fv", "transpose not FALSE");
    return;
  }
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix3fv",
                                   UniformApiType::kUniformMatrix3f,
                                   &real_location, &type, &count)) {
    return;
  }
  api()->glUniformMatrix3fvFn(real_location, count, transpose,
                              const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix4fv(GLint fake_location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (transpose && !feature_info_->IsWebGL2OrES3Context()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUniformMatrix4fv", "transpose not FALSE");
    return;
  }
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix4fv",
                                   UniformApiType::kUniformMatrix4f,
                                   &real_location, &type, &count)) {
    return;
  }
  api()->glUniformMatrix4fvFn(real_location, count, transpose,
                              const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix2x3fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix2x3fv",
                                   UniformApiType::kUniformMatrix2x3f,
                                   &real_location, &type, &count)) {
    return;
  }
  api()->glUniformMatrix2x3fvFn(real_location, count, transpose,
                                const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix2x4fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix2x4fv",
                                   UniformApiType::kUniformMatrix2x4f,
                                   &real_location, &type, &count)) {
    return;
  }
  api()->glUniformMatrix2x4fvFn(real_location, count, transpose,
                                const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix3x2fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix3x2fv",
                                   UniformApiType::kUniformMatrix3x2f,
                                   &real_location, &type, &count)) {
    return;
  }
  api()->glUniformMatrix3x2fvFn(real_location, count, transpose,
                                const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix3x4fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix3x4fv",
                                   UniformApiType::kUniformMatrix3x4f,
                                   &real_location, &type, &count)) {
    return;
  }
  api()->glUniformMatrix3x4fvFn(real_location, count, transpose,
                                const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix4x2fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix4x2fv",
                                   UniformApiType::kUniformMatrix4x2f,
                                   &real_location, &type, &count)) {
    return;
  }
  api()->glUniformMatrix4x2fvFn(real_location, count, transpose,
                                const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix4x3fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix4x3fv",
                                   UniformApiType::kUniformMatrix4x3f,
                                   &real_location, &type, &count)) {
    return;
  }
  api()->glUniformMatrix4x3fvFn(real_location, count, transpose,
                                const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUseProgram(GLuint program_id) {
  const char* function_name = "glUseProgram";
  GLuint service_id = 0;
  Program* program = nullptr;
  if (program_id) {
    program = GetProgramInfoNotShader(program_id, function_name);
    if (!program) {
      return;
    }
    if (!program->IsValid()) {
      // Program was not linked successfully. (ie, glLinkProgram)
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, function_name, "program not linked");
      return;
    }
    service_id = program->service_id();
  }
  if (state_.bound_transform_feedback.get() &&
      state_.bound_transform_feedback->active() &&
      !state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
        "transformfeedback is active and not paused");
    return;
  }
  if (program == state_.current_program.get())
    return;
  if (state_.current_program.get()) {
    program_manager()->UnuseProgram(shader_manager(),
                                    state_.current_program.get());
  }
  state_.current_program = program;
  LogClientServiceMapping(function_name, program_id, service_id);
  api()->glUseProgramFn(service_id);
  if (state_.current_program.get()) {
    program_manager()->UseProgram(state_.current_program.get());
  }
}

void GLES2DecoderImpl::RenderWarning(
    const char* filename, int line, const std::string& msg) {
  logger_.LogMessage(filename, line, std::string("RENDER WARNING: ") + msg);
}

void GLES2DecoderImpl::PerformanceWarning(
    const char* filename, int line, const std::string& msg) {
  logger_.LogMessage(filename, line,
                     std::string("PERFORMANCE WARNING: ") + msg);
}

void GLES2DecoderImpl::DoCopyBufferSubData(GLenum readtarget,
                                           GLenum writetarget,
                                           GLintptr readoffset,
                                           GLintptr writeoffset,
                                           GLsizeiptr size) {
  // Just delegate it. Some validation is actually done before this.
  buffer_manager()->ValidateAndDoCopyBufferSubData(
      &state_, error_state_.get(), readtarget, writetarget, readoffset,
      writeoffset, size);
}

bool GLES2DecoderImpl::PrepareTexturesForRender(bool* textures_set,
                                                const char* function_name) {
  DCHECK(state_.current_program.get());
  *textures_set = false;
  const Program::SamplerIndices& sampler_indices =
     state_.current_program->sampler_indices();
  for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
    const Program::UniformInfo* uniform_info =
        state_.current_program->GetUniformInfo(sampler_indices[ii]);
    DCHECK(uniform_info);
    for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
      GLuint texture_unit_index = uniform_info->texture_units[jj];
      if (texture_unit_index < state_.texture_units.size()) {
        TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
        TextureRef* texture_ref =
            texture_unit.GetInfoForSamplerType(uniform_info->type);

        // Find if the texture is also a depth or stencil attachment
        // Regardless of whether depth/stencil writes and masks are enabled
        // If so, there's a drawing feedback loop.

        Framebuffer* framebuffer =
            GetFramebufferInfoForTarget(GL_DRAW_FRAMEBUFFER);
        if (framebuffer) {
          if (CheckDrawingFeedbackLoopsHelper(
                  framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT), texture_ref,
                  function_name)) {
            return false;
          }

          if (CheckDrawingFeedbackLoopsHelper(
                  framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT),
                  texture_ref, function_name)) {
            return false;
          }
        }

        GLenum textarget = GetBindTargetForSamplerType(uniform_info->type);
        const SamplerState& sampler_state = GetSamplerStateForTextureUnit(
            uniform_info->type, texture_unit_index);
        if (!texture_ref ||
            !texture_manager()->CanRenderWithSampler(
                texture_ref, sampler_state)) {
          *textures_set = true;
          api()->glActiveTextureFn(GL_TEXTURE0 + texture_unit_index);
          api()->glBindTextureFn(textarget, texture_manager()->black_texture_id(
                                                uniform_info->type));
          if (!texture_ref) {
            LOCAL_RENDER_WARNING(
                std::string("there is no texture bound to the unit ") +
                base::NumberToString(texture_unit_index));
          } else {
            LOCAL_RENDER_WARNING(
                std::string("texture bound to texture unit ") +
                base::NumberToString(texture_unit_index) +
                " is not renderable. It might be non-power-of-2 or have"
                " incompatible texture filtering (maybe)?");
          }
          continue;
        } else if (!texture_ref->texture()->CompatibleWithSamplerUniformType(
                       uniform_info->type, sampler_state)) {
          LOCAL_SET_GL_ERROR(
              GL_INVALID_OPERATION, function_name,
              (std::string("Texture bound to texture unit ") +
               base::NumberToString(texture_unit_index) +
               " with internal format " +
               GLES2Util::GetStringEnum(
                   texture_ref->texture()->GetInternalFormatOfBaseLevel()) +
               " is not compatible with sampler type " +
               GLES2Util::GetStringEnum(uniform_info->type))
                  .c_str());
          return false;
        }

        // Find if the texture is also a color attachment
        // If so, there's a drawing feedback loop.

        if (framebuffer) {
          for (GLsizei kk = 0; kk <= framebuffer->last_color_attachment_id();
               ++kk) {
            GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + kk);
            if (CheckDrawingFeedbackLoopsHelper(
                    framebuffer->GetAttachment(attachment), texture_ref,
                    function_name)) {
              return false;
            }
          }
        }
      }
      // else: should this be an error?
    }
  }
  return true;
}

void GLES2DecoderImpl::RestoreStateForTextures() {
  DCHECK(state_.current_program.get());
  const Program::SamplerIndices& sampler_indices =
      state_.current_program->sampler_indices();
  for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
    const Program::UniformInfo* uniform_info =
        state_.current_program->GetUniformInfo(sampler_indices[ii]);
    DCHECK(uniform_info);
    for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
      GLuint texture_unit_index = uniform_info->texture_units[jj];
      if (texture_unit_index < state_.texture_units.size()) {
        TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
        TextureRef* texture_ref =
            texture_unit.GetInfoForSamplerType(uniform_info->type);
        const SamplerState& sampler_state = GetSamplerStateForTextureUnit(
            uniform_info->type, texture_unit_index);
        if (!texture_ref ||
            !texture_manager()->CanRenderWithSampler(
                texture_ref, sampler_state)) {
          api()->glActiveTextureFn(GL_TEXTURE0 + texture_unit_index);
          // Get the texture_ref info that was previously bound here.
          texture_ref =
              texture_unit.GetInfoForTarget(texture_unit.bind_target);
          api()->glBindTextureFn(texture_unit.bind_target,
                                 texture_ref ? texture_ref->service_id() : 0);
          continue;
        }
      }
    }
  }
  // Set the active texture back to whatever the user had it as.
  api()->glActiveTextureFn(GL_TEXTURE0 + state_.active_texture_unit);
}

bool GLES2DecoderImpl::ClearUnclearedTextures() {
  // Only check if there are some uncleared textures.
  if (!texture_manager()->HaveUnsafeTextures()) {
    return true;
  }

  // 1: Check all textures we are about to render with.
  if (state_.current_program.get()) {
    const Program::SamplerIndices& sampler_indices =
        state_.current_program->sampler_indices();
    for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
      const Program::UniformInfo* uniform_info =
          state_.current_program->GetUniformInfo(sampler_indices[ii]);
      DCHECK(uniform_info);
      for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
        GLuint texture_unit_index = uniform_info->texture_units[jj];
        if (texture_unit_index < state_.texture_units.size()) {
          TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
          TextureRef* texture_ref =
              texture_unit.GetInfoForSamplerType(uniform_info->type);
          if (texture_ref && !texture_ref->texture()->SafeToRenderFrom()) {
            if (!texture_manager()->ClearRenderableLevels(this, texture_ref)) {
              return false;
            }
          }
        }
      }
    }
  }
  return true;
}

bool GLES2DecoderImpl::ValidateStencilStateForDraw(const char* function_name) {
  if (!state_.stencil_state_changed_since_validation) {
    return true;
  }

  GLenum stencil_format = GetBoundFramebufferStencilFormat(GL_DRAW_FRAMEBUFFER);
  uint8_t stencil_bits = GLES2Util::StencilBitsPerPixel(stencil_format);

  if (state_.enable_flags.stencil_test && stencil_bits > 0) {
    DCHECK_LE(stencil_bits, 8U);

    GLuint max_stencil_value = (1 << stencil_bits) - 1;
    GLint max_stencil_ref = static_cast<GLint>(max_stencil_value);
    bool different_refs =
        std::clamp(state_.stencil_front_ref, 0, max_stencil_ref) !=
        std::clamp(state_.stencil_back_ref, 0, max_stencil_ref);
    bool different_writemasks =
        (state_.stencil_front_writemask & max_stencil_value) !=
        (state_.stencil_back_writemask & max_stencil_value);
    bool different_value_masks =
        (state_.stencil_front_mask & max_stencil_value) !=
        (state_.stencil_back_mask & max_stencil_value);
    if (different_refs || different_writemasks || different_value_masks) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "Front/back stencil settings do not match.");
      return false;
    }
  }

  state_.stencil_state_changed_since_validation = false;
  return true;
}

bool GLES2DecoderImpl::IsDrawValid(const char* function_name,
                                   GLuint max_vertex_accessed,
                                   bool instanced,
                                   GLsizei primcount,
                                   GLint basevertex,
                                   GLuint baseinstance) {
  DCHECK(instanced || primcount == 1);

  // NOTE: We specifically do not check current_program->IsValid() because
  // it could never be invalid since glUseProgram would have failed. While
  // glLinkProgram could later mark the program as invalid the previous
  // valid program will still function if it is still the current program.
  if (!state_.current_program.get()) {
    // The program does not exist.
    // But GL says no ERROR.
    LOCAL_RENDER_WARNING("Drawing with no current shader program.");
    return false;
  }

  // Perform extra stencil validation on ANGLE/D3D, and for all WebGL contexts.
  if (!feature_info_->feature_flags().separate_stencil_ref_mask_writemask) {
    if (!ValidateStencilStateForDraw(function_name)) {
      return false;
    }
  }

  if (!state_.vertex_attrib_manager->ValidateBindings(
          function_name, this, feature_info_.get(), buffer_manager(),
          state_.current_program.get(), max_vertex_accessed, instanced,
          primcount, basevertex, baseinstance)) {
    return false;
  }

  if (workarounds().disallow_large_instanced_draw) {
    const GLsizei kMaxInstancedDrawPrimitiveCount = 0x4000000;
    if (primcount > kMaxInstancedDrawPrimitiveCount) {
      LOCAL_SET_GL_ERROR(
          GL_OUT_OF_MEMORY, function_name,
          "Instanced draw primcount too large for this platform");
      return false;
    }
  }

  return true;
}

void GLES2DecoderImpl::RestoreStateForAttrib(
    GLuint attrib_index, bool restore_array_binding) {
  const VertexAttrib* attrib =
      state_.vertex_attrib_manager->GetVertexAttrib(attrib_index);
  if (restore_array_binding) {
    const void* ptr = reinterpret_cast<const void*>(attrib->offset());
    Buffer* buffer = attrib->buffer();
    api()->glBindBufferFn(GL_ARRAY_BUFFER, buffer ? buffer->service_id() : 0);
    api()->glVertexAttribPointerFn(attrib_index, attrib->size(), attrib->type(),
                                   attrib->normalized(), attrib->gl_stride(),
                                   ptr);
  }

  // Attrib divisors should only be non-zero when the ANGLE_instanced_arrays
  // extension is available
  DCHECK(attrib->divisor() == 0 ||
      feature_info_->feature_flags().angle_instanced_arrays);

  if (feature_info_->feature_flags().angle_instanced_arrays)
    api()->glVertexAttribDivisorANGLEFn(attrib_index, attrib->divisor());
  api()->glBindBufferFn(GL_ARRAY_BUFFER,
                        state_.bound_array_buffer.get()
                            ? state_.bound_array_buffer->service_id()
                            : 0);

  // Restore the vertex attrib array enable-state according to
  // the VertexAttrib enabled_in_driver value (which really represents the
  // state of the virtual context - not the driver - notably, above the
  // vertex array object emulation layer).
  if (attrib->enabled_in_driver()) {
    api()->glEnableVertexAttribArrayFn(attrib_index);
  } else {
    api()->glDisableVertexAttribArrayFn(attrib_index);
  }
}

bool GLES2DecoderImpl::AttribsTypeMatch() {
  if (!state_.current_program.get())
    return true;
  const std::vector<uint32_t>& shader_attrib_active_mask =
      state_.current_program->vertex_input_active_mask();
  const std::vector<uint32_t>& shader_attrib_type_mask =
      state_.current_program->vertex_input_base_type_mask();
  const std::vector<uint32_t>& generic_vertex_attrib_type_mask =
      state_.generic_attrib_base_type_mask();
  const std::vector<uint32_t>& vertex_attrib_array_enabled_mask =
      state_.vertex_attrib_manager->attrib_enabled_mask();
  const std::vector<uint32_t>& vertex_attrib_array_type_mask =
      state_.vertex_attrib_manager->attrib_base_type_mask();
  DCHECK_EQ(shader_attrib_active_mask.size(),
            shader_attrib_type_mask.size());
  DCHECK_EQ(shader_attrib_active_mask.size(),
            generic_vertex_attrib_type_mask.size());
  DCHECK_EQ(shader_attrib_active_mask.size(),
            vertex_attrib_array_enabled_mask.size());
  DCHECK_EQ(shader_attrib_active_mask.size(),
            vertex_attrib_array_type_mask.size());
  for (size_t ii = 0; ii < shader_attrib_active_mask.size(); ++ii) {
    uint32_t vertex_attrib_source_type_mask =
        (~vertex_attrib_array_enabled_mask[ii] &
         generic_vertex_attrib_type_mask[ii]) |
        (vertex_attrib_array_enabled_mask[ii] &
         vertex_attrib_array_type_mask[ii]);
    if ((shader_attrib_type_mask[ii] & shader_attrib_active_mask[ii]) !=
        (vertex_attrib_source_type_mask & shader_attrib_active_mask[ii])) {
      return false;
    }
  }
  return true;
}

template <GLES2DecoderImpl::DrawArraysOption option>
ALWAYS_INLINE bool GLES2DecoderImpl::CheckMultiDrawArraysVertices(
    const char* function_name,
    bool instanced,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* primcounts,
    const GLuint* baseinstances,
    GLsizei drawcount,
    GLuint* total_max_vertex_accessed,
    GLsizei* total_max_primcount) {
  if (option == DrawArraysOption::Default) {
    DCHECK_EQ(baseinstances, nullptr);
  }
  DCHECK_GE(drawcount, 0);
  for (GLsizei draw_id = 0; draw_id < drawcount; ++draw_id) {
    GLint first = firsts[draw_id];
    GLsizei count = counts[draw_id];
    GLsizei primcount = instanced ? primcounts[draw_id] : 1;
    GLuint baseinstance = (option == DrawArraysOption::UseBaseInstance)
                              ? baseinstances[draw_id]
                              : 0;
    // We have to check this here because the prototype for glDrawArrays
    // is GLint not GLsizei.
    if (first < 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "first < 0");
      return false;
    }
    if (count < 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "count < 0");
      return false;
    }
    if (primcount < 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "primcount < 0");
      return false;
    }
    if (count == 0 || primcount == 0) {
      LOCAL_RENDER_WARNING("Render count or primcount is 0.");
      continue;
    }

    base::CheckedNumeric<GLuint> checked_max_vertex = first;
    checked_max_vertex += count - 1;
    // first and count-1 are both a non-negative int, so their sum fits an
    // unsigned int.
    GLuint max_vertex_accessed = 0;
    if (!checked_max_vertex.AssignIfValid(&max_vertex_accessed)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                         "first + count overflow");
      return false;
    }
    if (!IsDrawValid(function_name, max_vertex_accessed, instanced, primcount,
                     0, baseinstance)) {
      return false;
    }
    *total_max_vertex_accessed =
        std::max(*total_max_vertex_accessed, max_vertex_accessed);
    *total_max_primcount = std::max(*total_max_primcount, primcount);
  }
  return true;
}

ALWAYS_INLINE bool GLES2DecoderImpl::CheckTransformFeedback(
    const char* function_name,
    bool instanced,
    GLenum mode,
    const GLsizei* counts,
    const GLsizei* primcounts,
    GLsizei drawcount,
    GLsizei* vertices_drawn) {
  DCHECK(state_.bound_transform_feedback.get());
  if (state_.bound_transform_feedback->active() &&
      !state_.bound_transform_feedback->paused()) {
    if (mode != state_.bound_transform_feedback->primitive_mode()) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, function_name,
          "mode differs from active transformfeedback's primitiveMode");
      return false;
    }
    for (GLsizei draw_id = 0; draw_id < drawcount; ++draw_id) {
      GLsizei count = counts[draw_id];
      GLsizei primcount = instanced ? primcounts[draw_id] : 1;

      bool valid = state_.bound_transform_feedback->GetVerticesNeededForDraw(
          mode, count, primcount, *vertices_drawn, vertices_drawn);
      if (!valid) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                           "integer overflow calculating number of vertices "
                           "for transform feedback");
        return false;
      }
    }

    if (!buffer_manager()->RequestBuffersAccess(
            error_state_.get(), state_.bound_transform_feedback.get(),
            state_.current_program->GetTransformFeedbackVaryingSizes(),
            *vertices_drawn, function_name, "transformfeedback buffers")) {
      return false;
    }
  }
  return true;
}

template <GLES2DecoderImpl::DrawArraysOption option>
ALWAYS_INLINE error::Error GLES2DecoderImpl::DoMultiDrawArrays(
    const char* function_name,
    bool instanced,
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* primcounts,
    const GLuint* baseinstances,
    GLsizei drawcount) {
  if (option == DrawArraysOption::Default) {
    DCHECK_EQ(baseinstances, nullptr);
  }

  if (!validators_->draw_mode.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, mode, "mode");
    return error::kNoError;
  }

  if (drawcount < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "drawcount < 0");
    return error::kNoError;
  }

  if (!CheckBoundDrawFramebufferValid(function_name, true)) {
    return error::kNoError;
  }

  GLuint total_max_vertex_accessed = 0;
  GLsizei total_max_primcount = 0;
  if (!CheckMultiDrawArraysVertices<option>(
          function_name, instanced, firsts, counts, primcounts, baseinstances,
          drawcount, &total_max_vertex_accessed, &total_max_primcount)) {
    return error::kNoError;
  }

  if (total_max_primcount == 0) {
    return error::kNoError;
  }

  GLsizei transform_feedback_vertices = 0;
  if (feature_info_->IsWebGL2OrES3Context()) {
    if (!AttribsTypeMatch()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "vertexAttrib function must match shader attrib type");
      return error::kNoError;
    }

    if (!CheckTransformFeedback(function_name, instanced, mode, counts,
                                primcounts, drawcount,
                                &transform_feedback_vertices)) {
      return error::kNoError;
    }

    if (!ValidateUniformBlockBackings(function_name)) {
      return error::kNoError;
    }
  }

  if (!ClearUnclearedTextures()) {
    // TODO(enga): Can this be GL_OUT_OF_MEMORY?
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "out of memory");
    return error::kNoError;
  }

  // The branch with fixed attrib is not meant to be used
  // normally but just to pass OpenGL ES 2 conformance where there's no
  // basevertex and baseinstance support.
  {
    bool textures_set;
    if (!PrepareTexturesForRender(&textures_set, function_name)) {
      return error::kNoError;
    }
    ApplyDirtyState();
    if (!ValidateAndAdjustDrawBuffers(function_name)) {
      return error::kNoError;
    }

    GLint draw_id_location = state_.current_program->draw_id_uniform_location();
    GLint base_instance_location =
        state_.current_program->base_instance_uniform_location();
    for (GLsizei draw_id = 0; draw_id < drawcount; ++draw_id) {
      GLint first = firsts[draw_id];
      GLsizei count = counts[draw_id];
      GLsizei primcount = instanced ? primcounts[draw_id] : 1;
      if (count == 0 || primcount == 0) {
        continue;
      }
      if (draw_id_location >= 0) {
        api()->glUniform1iFn(draw_id_location, draw_id);
      }
      if (!instanced) {
        api()->glDrawArraysFn(mode, first, count);
      } else {
        if (option != DrawArraysOption::UseBaseInstance) {
          api()->glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
        } else {
          GLuint baseinstance = baseinstances[draw_id];
          if (base_instance_location >= 0) {
            api()->glUniform1iFn(base_instance_location, baseinstance);
          }
          api()->glDrawArraysInstancedBaseInstanceANGLEFn(
              mode, first, count, primcount, baseinstance);
        }
      }
    }
    if (state_.bound_transform_feedback.get()) {
      state_.bound_transform_feedback->OnVerticesDrawn(
          transform_feedback_vertices);
    }

    if (textures_set) {
      RestoreStateForTextures();
    }
    // only reset base vertex and base instance shader variable when it's
    // possibly non-zero
    if (option == DrawArraysOption::UseBaseInstance) {
      if (base_instance_location >= 0) {
        api()->glUniform1iFn(base_instance_location, 0);
      }
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDrawArrays(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile cmds::DrawArrays& c =
      *static_cast<const volatile cmds::DrawArrays*>(cmd_data);
  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);
  return DoMultiDrawArrays<DrawArraysOption::Default>(
      "glDrawArrays", false, static_cast<GLenum>(c.mode), &first, &count,
      nullptr, nullptr, 1);
}

error::Error GLES2DecoderImpl::HandleDrawArraysInstancedANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawArraysInstancedANGLE& c =
      *static_cast<const volatile gles2::cmds::DrawArraysInstancedANGLE*>(
          cmd_data);
  if (!features().angle_instanced_arrays)
    return error::kUnknownCommand;

  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLsizei primcount = static_cast<GLsizei>(c.primcount);
  return DoMultiDrawArrays<DrawArraysOption::Default>(
      "glDrawArraysInstancedANGLE", true, static_cast<GLenum>(c.mode), &first,
      &count, &primcount, nullptr, 1);
}

error::Error GLES2DecoderImpl::HandleDrawArraysInstancedBaseInstanceANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawArraysInstancedBaseInstanceANGLE& c =
      *static_cast<
          const volatile gles2::cmds::DrawArraysInstancedBaseInstanceANGLE*>(
          cmd_data);
  if (!features().angle_instanced_arrays)
    return error::kUnknownCommand;
  if (!features().webgl_draw_instanced_base_vertex_base_instance &&
      !features().webgl_multi_draw_instanced_base_vertex_base_instance)
    return error::kUnknownCommand;

  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLsizei primcount = static_cast<GLsizei>(c.primcount);
  GLuint baseInstances = static_cast<GLuint>(c.baseinstance);
  return DoMultiDrawArrays<DrawArraysOption::UseBaseInstance>(
      "glDrawArraysInstancedBaseInstanceANGLE", true,
      static_cast<GLenum>(c.mode), &first, &count, &primcount, &baseInstances,
      1);
}

template <GLES2DecoderImpl::DrawElementsOption option>
ALWAYS_INLINE bool GLES2DecoderImpl::CheckMultiDrawElementsVertices(
    const char* function_name,
    bool instanced,
    const GLsizei* counts,
    GLenum type,
    const int32_t* offsets,
    const GLsizei* primcounts,
    const GLint* basevertices,
    const GLuint* baseinstances,
    GLsizei drawcount,
    Buffer* element_array_buffer,
    GLuint* total_max_vertex_accessed,
    GLsizei* total_max_primcount) {
  if (option == DrawElementsOption::Default) {
    DCHECK_EQ(basevertices, nullptr);
    DCHECK_EQ(baseinstances, nullptr);
  }
  DCHECK_GE(drawcount, 0);
  for (GLsizei draw_id = 0; draw_id < drawcount; ++draw_id) {
    GLsizei count = counts[draw_id];
    GLsizei offset = offsets[draw_id];
    GLsizei primcount = instanced ? primcounts[draw_id] : 1;
    GLint basevertex = (option == DrawElementsOption::UseBaseVertexBaseInstance)
                           ? basevertices[draw_id]
                           : 0;
    GLint baseinstance =
        (option == DrawElementsOption::UseBaseVertexBaseInstance)
            ? baseinstances[draw_id]
            : 0;

    if (count < 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "count < 0");
      return false;
    }
    if (offset < 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "offset < 0");
      return false;
    }
    if (primcount < 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "primcount < 0");
      return false;
    }
    if (count == 0 || primcount == 0) {
      continue;
    }

    GLuint max_vertex_accessed;
    if (!element_array_buffer->GetMaxValueForRange(
            offset, count, type,
            state_.enable_flags.primitive_restart_fixed_index,
            &max_vertex_accessed)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "range out of bounds for buffer");
      return false;
    }

    if (!IsDrawValid(function_name, max_vertex_accessed, instanced, primcount,
                     basevertex, baseinstance)) {
      return false;
    }

    *total_max_vertex_accessed =
        std::max(*total_max_vertex_accessed, max_vertex_accessed + basevertex);
    *total_max_primcount = std::max(*total_max_primcount, primcount);
  }
  return true;
}

template <GLES2DecoderImpl::DrawElementsOption option>
ALWAYS_INLINE error::Error GLES2DecoderImpl::DoMultiDrawElements(
    const char* function_name,
    bool instanced,
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const int32_t* offsets,
    const GLsizei* primcounts,
    const GLint* basevertices,
    const GLuint* baseinstances,
    GLsizei drawcount) {
  if (option == DrawElementsOption::Default) {
    DCHECK_EQ(basevertices, nullptr);
    DCHECK_EQ(baseinstances, nullptr);
  }

  if (!validators_->draw_mode.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, mode, "mode");
    return error::kNoError;
  }

  if (!validators_->index_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, type, "type");
    return error::kNoError;
  }

  if (drawcount < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "drawcount < 0");
    return error::kNoError;
  }

  if (!CheckBoundDrawFramebufferValid(function_name, true)) {
    return error::kNoError;
  }

  Buffer* element_array_buffer = buffer_manager()->RequestBufferAccess(
      &state_, error_state_.get(), GL_ELEMENT_ARRAY_BUFFER, function_name);
  if (!element_array_buffer) {
    return error::kNoError;
  }

  if (state_.bound_transform_feedback.get() &&
      state_.bound_transform_feedback->active() &&
      !state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "transformfeedback is active and not paused");
    return error::kNoError;
  }

  GLuint total_max_vertex_accessed = 0;
  GLsizei total_max_primcount = 0;
  if (!CheckMultiDrawElementsVertices<option>(
          function_name, instanced, counts, type, offsets, primcounts,
          basevertices, baseinstances, drawcount, element_array_buffer,
          &total_max_vertex_accessed, &total_max_primcount)) {
    return error::kNoError;
  }

  if (total_max_primcount == 0) {
    return error::kNoError;
  }

  if (feature_info_->IsWebGL2OrES3Context()) {
    if (!AttribsTypeMatch()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "vertexAttrib function must match shader attrib type");
      return error::kNoError;
    }
    if (!ValidateUniformBlockBackings(function_name)) {
      return error::kNoError;
    }
  }

  if (!ClearUnclearedTextures()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "out of memory");
    return error::kNoError;
  }

  // The branch with fixed attrib is not meant to be used
  // normally But just to pass OpenGL ES 2 conformance where there's no
  // basevertex and baseinstance support.
  {
    bool textures_set;
    if (!PrepareTexturesForRender(&textures_set, function_name)) {
      return error::kNoError;
    }
    ApplyDirtyState();
    // TODO(gman): Refactor to hide these details in BufferManager or
    // VertexAttribManager.
    bool used_client_side_array = false;
    if (element_array_buffer->IsClientSideArray()) {
      used_client_side_array = true;
      api()->glBindBufferFn(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    if (!ValidateAndAdjustDrawBuffers(function_name)) {
      return error::kNoError;
    }
    if (state_.enable_flags.primitive_restart_fixed_index &&
        feature_info_->feature_flags().emulate_primitive_restart_fixed_index) {
      api()->glEnableFn(GL_PRIMITIVE_RESTART);
      buffer_manager()->SetPrimitiveRestartFixedIndexIfNecessary(type);
    }

    GLint draw_id_location = state_.current_program->draw_id_uniform_location();
    GLint base_vertex_location =
        state_.current_program->base_vertex_uniform_location();
    GLint base_instance_location =
        state_.current_program->base_instance_uniform_location();
    for (GLsizei draw_id = 0; draw_id < drawcount; ++draw_id) {
      GLsizei count = counts[draw_id];
      GLsizei offset = offsets[draw_id];
      GLsizei primcount = instanced ? primcounts[draw_id] : 1;
      if (count == 0 || primcount == 0) {
        continue;
      }
      const GLvoid* indices = reinterpret_cast<const GLvoid*>(offset);
      if (used_client_side_array) {
        indices = element_array_buffer->GetRange(offset, 0);
      }
      if (draw_id_location >= 0) {
        api()->glUniform1iFn(draw_id_location, draw_id);
      }
      if (!instanced) {
        api()->glDrawElementsFn(mode, count, type, indices);
      } else {
        if (option == DrawElementsOption::Default) {
          api()->glDrawElementsInstancedANGLEFn(mode, count, type, indices,
                                                primcount);
        } else {
          GLint basevertex = basevertices[draw_id];
          GLuint baseinstance = baseinstances[draw_id];
          if (base_vertex_location >= 0) {
            api()->glUniform1iFn(base_vertex_location, basevertex);
          }
          if (base_instance_location >= 0) {
            api()->glUniform1iFn(base_instance_location, baseinstance);
          }
          api()->glDrawElementsInstancedBaseVertexBaseInstanceANGLEFn(
              mode, count, type, indices, primcount, basevertex, baseinstance);
        }
      }
    }
    if (state_.enable_flags.primitive_restart_fixed_index &&
        feature_info_->feature_flags().emulate_primitive_restart_fixed_index) {
      api()->glDisableFn(GL_PRIMITIVE_RESTART);
    }
    if (used_client_side_array) {
      api()->glBindBufferFn(GL_ELEMENT_ARRAY_BUFFER,
                            element_array_buffer->service_id());
    }
    if (textures_set) {
      RestoreStateForTextures();
    }
    // only reset base vertex and base instance shader variable when it's
    // possibly non-zero
    if (option == DrawElementsOption::UseBaseVertexBaseInstance) {
      if (base_vertex_location >= 0) {
        api()->glUniform1iFn(base_vertex_location, 0);
      }
      if (base_instance_location >= 0) {
        api()->glUniform1iFn(base_instance_location, 0);
      }
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDrawElements(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawElements& c =
      *static_cast<const volatile gles2::cmds::DrawElements*>(cmd_data);
  GLsizei count = static_cast<GLsizei>(c.count);
  int32_t offset = static_cast<int32_t>(c.index_offset);
  return DoMultiDrawElements<DrawElementsOption::Default>(
      "glDrawElements", false, static_cast<GLenum>(c.mode), &count,
      static_cast<GLenum>(c.type), &offset, nullptr, nullptr, nullptr, 1);
}

error::Error GLES2DecoderImpl::HandleDrawElementsInstancedANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawElementsInstancedANGLE& c =
      *static_cast<const volatile gles2::cmds::DrawElementsInstancedANGLE*>(
          cmd_data);
  if (!features().angle_instanced_arrays)
    return error::kUnknownCommand;

  GLsizei count = static_cast<GLsizei>(c.count);
  int32_t offset = static_cast<int32_t>(c.index_offset);
  GLsizei primcount = static_cast<GLsizei>(c.primcount);

  return DoMultiDrawElements<DrawElementsOption::Default>(
      "glDrawElementsInstancedANGLE", true, static_cast<GLenum>(c.mode), &count,
      static_cast<GLenum>(c.type), &offset, &primcount, nullptr, nullptr, 1);
}

error::Error
GLES2DecoderImpl::HandleDrawElementsInstancedBaseVertexBaseInstanceANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawElementsInstancedBaseVertexBaseInstanceANGLE&
      c = *static_cast<const volatile gles2::cmds::
                           DrawElementsInstancedBaseVertexBaseInstanceANGLE*>(
          cmd_data);
  if (!features().angle_instanced_arrays)
    return error::kUnknownCommand;

  GLsizei count = static_cast<GLsizei>(c.count);
  int32_t offset = static_cast<int32_t>(c.index_offset);
  GLsizei primcount = static_cast<GLsizei>(c.primcount);
  GLint basevertex = static_cast<GLsizei>(c.basevertex);
  GLuint baseinstance = static_cast<GLsizei>(c.baseinstance);
  return DoMultiDrawElements<DrawElementsOption::UseBaseVertexBaseInstance>(
      "glDrawElementsInstancedBaseVertexBaseInstanceANGLE", true,
      static_cast<GLenum>(c.mode), &count, static_cast<GLenum>(c.type), &offset,
      &primcount, &basevertex, &baseinstance, 1);
}

void GLES2DecoderImpl::DoMultiDrawBeginCHROMIUM(GLsizei drawcount) {
  if (!multi_draw_manager_->Begin(drawcount)) {
    MarkContextLost(error::kGuilty);
    group_->LoseContexts(error::kInnocent);
  }
}

void GLES2DecoderImpl::DoMultiDrawEndCHROMIUM() {
  MultiDrawManager::ResultData result;
  if (!multi_draw_manager_->End(&result)) {
    MarkContextLost(error::kGuilty);
    group_->LoseContexts(error::kInnocent);
    return;
  }
  switch (result.draw_function) {
    case MultiDrawManager::DrawFunction::DrawArrays:
      DoMultiDrawArrays<DrawArraysOption::Default>(
          "glMultiDrawArraysWEBGL", false, result.mode, result.firsts.data(),
          result.counts.data(), nullptr, nullptr, result.drawcount);
      break;
    case MultiDrawManager::DrawFunction::DrawArraysInstanced:
      DoMultiDrawArrays<DrawArraysOption::Default>(
          "glMultiDrawArraysInstancedWEBGL", true, result.mode,
          result.firsts.data(), result.counts.data(),
          result.instance_counts.data(), nullptr, result.drawcount);
      break;
    case MultiDrawManager::DrawFunction::DrawArraysInstancedBaseInstance:
      DoMultiDrawArrays<DrawArraysOption::UseBaseInstance>(
          "glMultiDrawArraysInstancedBaseInstanceWEBGL", true, result.mode,
          result.firsts.data(), result.counts.data(),
          result.instance_counts.data(), result.baseinstances.data(),
          result.drawcount);
      break;
    case MultiDrawManager::DrawFunction::DrawElements:
      DoMultiDrawElements<DrawElementsOption::Default>(
          "glMultiDrawElementsWEBGL", false, result.mode, result.counts.data(),
          result.type, result.offsets.data(), nullptr, nullptr, nullptr,
          result.drawcount);
      break;
    case MultiDrawManager::DrawFunction::DrawElementsInstanced:
      DoMultiDrawElements<DrawElementsOption::Default>(
          "glMultiDrawElementsInstancedWEBGL", true, result.mode,
          result.counts.data(), result.type, result.offsets.data(),
          result.instance_counts.data(), nullptr, nullptr, result.drawcount);
      break;
    case MultiDrawManager::DrawFunction::
        DrawElementsInstancedBaseVertexBaseInstance:
      DoMultiDrawElements<DrawElementsOption::UseBaseVertexBaseInstance>(
          "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL", true,
          result.mode, result.counts.data(), result.type, result.offsets.data(),
          result.instance_counts.data(), result.basevertices.data(),
          result.baseinstances.data(), result.drawcount);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      MarkContextLost(error::kGuilty);
      group_->LoseContexts(error::kInnocent);
  }
}

error::Error GLES2DecoderImpl::HandleMultiDrawArraysCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawArraysCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::MultiDrawArraysCHROMIUM*>(
          cmd_data);
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t firsts_size, counts_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLint)).AssignIfValid(&firsts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  const GLint* firsts = GetSharedMemoryAs<const GLint*>(
      c.firsts_shm_id, c.firsts_shm_offset, firsts_size);
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  if (firsts == nullptr) {
    return error::kOutOfBounds;
  }
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawArrays(mode, firsts, counts, drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMultiDrawArraysInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawArraysInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::MultiDrawArraysInstancedCHROMIUM*>(
          cmd_data);
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t firsts_size, counts_size, instance_counts_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLint)).AssignIfValid(&firsts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&instance_counts_size)) {
    return error::kOutOfBounds;
  }
  const GLint* firsts = GetSharedMemoryAs<const GLint*>(
      c.firsts_shm_id, c.firsts_shm_offset, firsts_size);
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* instance_counts = GetSharedMemoryAs<const GLsizei*>(
      c.instance_counts_shm_id, c.instance_counts_shm_offset,
      instance_counts_size);
  if (firsts == nullptr) {
    return error::kOutOfBounds;
  }
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (instance_counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawArraysInstanced(
          mode, firsts, counts, instance_counts, drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderImpl::HandleMultiDrawArraysInstancedBaseInstanceCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawArraysInstancedBaseInstanceCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::
                       MultiDrawArraysInstancedBaseInstanceCHROMIUM*>(cmd_data);
  if (!features().webgl_multi_draw_instanced_base_vertex_base_instance) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t firsts_size, counts_size, instance_counts_size, baseinstances_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLint)).AssignIfValid(&firsts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&instance_counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLuint)).AssignIfValid(&baseinstances_size)) {
    return error::kOutOfBounds;
  }
  const GLint* firsts = GetSharedMemoryAs<const GLint*>(
      c.firsts_shm_id, c.firsts_shm_offset, firsts_size);
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* instance_counts = GetSharedMemoryAs<const GLsizei*>(
      c.instance_counts_shm_id, c.instance_counts_shm_offset,
      instance_counts_size);
  const GLuint* baseinstances_counts = GetSharedMemoryAs<const GLuint*>(
      c.baseinstances_shm_id, c.baseinstances_shm_offset, baseinstances_size);
  if (firsts == nullptr) {
    return error::kOutOfBounds;
  }
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (instance_counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (baseinstances_counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawArraysInstancedBaseInstance(
          mode, firsts, counts, instance_counts, baseinstances_counts,
          drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMultiDrawElementsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawElementsCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::MultiDrawElementsCHROMIUM*>(
          cmd_data);
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLenum type = static_cast<GLenum>(c.type);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t counts_size, offsets_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&offsets_size)) {
    return error::kOutOfBounds;
  }
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* offsets = GetSharedMemoryAs<const GLsizei*>(
      c.offsets_shm_id, c.offsets_shm_offset, offsets_size);
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (offsets == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawElements(mode, counts, type, offsets,
                                              drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMultiDrawElementsInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawElementsInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::MultiDrawElementsInstancedCHROMIUM*>(
          cmd_data);
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLenum type = static_cast<GLenum>(c.type);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t counts_size, offsets_size, instance_counts_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&offsets_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&instance_counts_size)) {
    return error::kOutOfBounds;
  }
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* offsets = GetSharedMemoryAs<const GLsizei*>(
      c.offsets_shm_id, c.offsets_shm_offset, offsets_size);
  const GLsizei* instance_counts = GetSharedMemoryAs<const GLsizei*>(
      c.instance_counts_shm_id, c.instance_counts_shm_offset,
      instance_counts_size);
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (offsets == nullptr) {
    return error::kOutOfBounds;
  }
  if (instance_counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawElementsInstanced(
          mode, counts, type, offsets, instance_counts, drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::
    HandleMultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM(
        uint32_t immediate_data_size,
        const volatile void* cmd_data) {
  const volatile gles2::cmds::
      MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM& c =
          *static_cast<
              const volatile gles2::cmds::
                  MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM*>(
              cmd_data);
  if (!features().webgl_multi_draw_instanced_base_vertex_base_instance) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLenum type = static_cast<GLenum>(c.type);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t counts_size, offsets_size, instance_counts_size, basevertices_size,
      baseinstances_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&offsets_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&instance_counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLint)).AssignIfValid(&basevertices_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLuint)).AssignIfValid(&baseinstances_size)) {
    return error::kOutOfBounds;
  }
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* offsets = GetSharedMemoryAs<const GLsizei*>(
      c.offsets_shm_id, c.offsets_shm_offset, offsets_size);
  const GLsizei* instance_counts = GetSharedMemoryAs<const GLsizei*>(
      c.instance_counts_shm_id, c.instance_counts_shm_offset,
      instance_counts_size);
  const GLint* basevertices = GetSharedMemoryAs<const GLint*>(
      c.basevertices_shm_id, c.basevertices_shm_offset, basevertices_size);
  const GLuint* baseinstances = GetSharedMemoryAs<const GLuint*>(
      c.baseinstances_shm_id, c.baseinstances_shm_offset, baseinstances_size);
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (offsets == nullptr) {
    return error::kOutOfBounds;
  }
  if (instance_counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (basevertices == nullptr) {
    return error::kOutOfBounds;
  }
  if (baseinstances == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawElementsInstancedBaseVertexBaseInstance(
          mode, counts, type, offsets, instance_counts, basevertices,
          baseinstances, drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

GLuint GLES2DecoderImpl::DoGetMaxValueInBufferCHROMIUM(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  GLuint max_vertex_accessed = 0;
  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    // TODO(gman): Should this be a GL error or a command buffer error?
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "GetMaxValueInBufferCHROMIUM", "unknown buffer");
  } else {
    // The max value is used here to emulate client-side vertex
    // arrays, by uploading enough vertices into buffer objects to
    // cover the DrawElements call. Baking the primitive restart bit
    // into this result isn't strictly correct in all cases; the
    // client side code should pass down the bit and decide how to use
    // the result. However, the only caller makes the draw call
    // immediately afterward, so the state won't change between this
    // query and the draw call.
    if (!buffer->GetMaxValueForRange(
            offset, count, type,
            state_.enable_flags.primitive_restart_fixed_index,
            &max_vertex_accessed)) {
      // TODO(gman): Should this be a GL error or a command buffer error?
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "GetMaxValueInBufferCHROMIUM", "range out of bounds for buffer");
    }
  }
  return max_vertex_accessed;
}

void GLES2DecoderImpl::DoShaderSource(
    GLuint client_id, GLsizei count, const char** data, const GLint* length) {
  std::string str;
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (length && length[ii] > 0)
      str.append(data[ii], length[ii]);
    else
      str.append(data[ii]);
  }
  size_t len = str.size();
  // Accommodate gles2_conform_tests
  // where there are '\0' at the end of the shaders
  while (len > 0 && str[len - 1] == '\0') {
    len -= 1;
  }
  // Delegate validation of the incoming shader source to ANGLE's
  // shader translator.
  Shader* shader = GetShaderInfoNotProgram(client_id, "glShaderSource");
  if (!shader) {
    return;
  }
  // Note: We don't actually call glShaderSource here. We wait until
  // we actually compile the shader.
  shader->set_source(str);
}

void GLES2DecoderImpl::DoTransformFeedbackVaryings(
    GLuint client_program_id, GLsizei count, const char* const* varyings,
    GLenum buffer_mode) {
  Program* program = GetProgramInfoNotShader(
      client_program_id, "glTransformFeedbackVaryings");
  if (!program) {
    return;
  }
  program->TransformFeedbackVaryings(count, varyings, buffer_mode);
}

scoped_refptr<ShaderTranslatorInterface> GLES2DecoderImpl::GetTranslator(
    GLenum type) {
  return type == GL_VERTEX_SHADER ? vertex_translator_ : fragment_translator_;
}

scoped_refptr<ShaderTranslatorInterface>
GLES2DecoderImpl::GetOrCreateTranslator(GLenum type) {
  if (!InitializeShaderTranslator()) {
    return nullptr;
  }
  return GetTranslator(type);
}

void GLES2DecoderImpl::DoCompileShader(GLuint client_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoCompileShader");
  Shader* shader = GetShaderInfoNotProgram(client_id, "glCompileShader");
  if (!shader) {
    return;
  }

  scoped_refptr<ShaderTranslatorInterface> translator;
  if (!feature_info_->disable_shader_translator())
    translator = GetOrCreateTranslator(shader->shader_type());

  const Shader::TranslatedShaderSourceType source_type =
      feature_info_->feature_flags().angle_translated_shader_source ?
      Shader::kANGLE : Shader::kGL;
  shader->RequestCompile(translator, source_type);
}

void GLES2DecoderImpl::DoGetShaderiv(GLuint shader_id,
                                     GLenum pname,
                                     GLint* params,
                                     GLsizei params_size) {
  Shader* shader = GetShaderInfoNotProgram(shader_id, "glGetShaderiv");
  if (!shader) {
    return;
  }

  if (pname == GL_COMPILE_STATUS) {
    if (shader->HasCompiled()) {
      *params = compile_shader_always_succeeds_ ? true : shader->valid();
      return;
    }
    // Lookup if there is compiled shader cache
    if (program_manager()->HasCachedCompileStatus(shader)) {
      // Only successful compile is cached
      // Fail-to-compile shader is not cached as needs compiling
      // to get log info
      *params = true;
      return;
    }
  }

  // Compile now for statuses that require it.
  switch (pname) {
    case GL_COMPILE_STATUS:
    case GL_INFO_LOG_LENGTH:
    case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE:
      CompileShaderAndExitCommandProcessingEarly(shader);
      break;

    default:
    break;
  }

  switch (pname) {
    case GL_SHADER_SOURCE_LENGTH:
      *params = shader->source().size();
      if (*params)
        ++(*params);
      return;
    case GL_COMPILE_STATUS:
      *params = compile_shader_always_succeeds_ ? true : shader->valid();
      return;
    case GL_INFO_LOG_LENGTH:
      *params = shader->log_info().size();
      if (*params)
        ++(*params);
      return;
    case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE:
      *params = shader->translated_source().size();
      if (*params)
        ++(*params);
      return;
    default:
      break;
  }
  api()->glGetShaderivFn(shader->service_id(), pname, params);
}

error::Error GLES2DecoderImpl::HandleGetShaderSource(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderSource& c =
      *static_cast<const volatile gles2::cmds::GetShaderSource*>(cmd_data);
  GLuint shader_id = c.shader;
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Shader* shader = GetShaderInfoNotProgram(shader_id, "glGetShaderSource");
  if (!shader || shader->source().empty()) {
    bucket->SetSize(0);
    return error::kNoError;
  }
  bucket->SetFromString(shader->source().c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTranslatedShaderSourceANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetTranslatedShaderSourceANGLE& c =
      *static_cast<const volatile gles2::cmds::GetTranslatedShaderSourceANGLE*>(
          cmd_data);
  GLuint shader_id = c.shader;
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Shader* shader = GetShaderInfoNotProgram(
      shader_id, "glGetTranslatedShaderSourceANGLE");
  if (!shader) {
    bucket->SetSize(0);
    return error::kNoError;
  }

  // Make sure translator has been utilized in compile.
  CompileShaderAndExitCommandProcessingEarly(shader);

  bucket->SetFromString(shader->translated_source().c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramInfoLog(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetProgramInfoLog& c =
      *static_cast<const volatile gles2::cmds::GetProgramInfoLog*>(cmd_data);
  GLuint program_id = c.program;
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetProgramInfoLog");
  if (!program || !program->log_info()) {
    bucket->SetFromString("");
    return error::kNoError;
  }
  bucket->SetFromString(program->log_info()->c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetShaderInfoLog(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderInfoLog& c =
      *static_cast<const volatile gles2::cmds::GetShaderInfoLog*>(cmd_data);
  GLuint shader_id = c.shader;
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Shader* shader = GetShaderInfoNotProgram(shader_id, "glGetShaderInfoLog");
  if (!shader) {
    bucket->SetFromString("");
    return error::kNoError;
  }

  // Shader must be compiled in order to get the info log.
  CompileShaderAndExitCommandProcessingEarly(shader);

  bucket->SetFromString(shader->log_info().c_str());
  return error::kNoError;
}

bool GLES2DecoderImpl::DoIsEnabled(GLenum cap) {
  return state_.GetEnabled(cap);
}

bool GLES2DecoderImpl::DoIsEnablediOES(GLenum target, GLuint index) {
  // Note (crbug.com/1058744): not implemented for validating command decoder
  return false;
}

bool GLES2DecoderImpl::DoIsBuffer(GLuint client_id) {
  const Buffer* buffer = GetBuffer(client_id);
  return buffer && buffer->IsValid() && !buffer->IsDeleted();
}

bool GLES2DecoderImpl::DoIsFramebuffer(GLuint client_id) {
  const Framebuffer* framebuffer =
      GetFramebuffer(client_id);
  return framebuffer && framebuffer->IsValid() && !framebuffer->IsDeleted();
}

bool GLES2DecoderImpl::DoIsProgram(GLuint client_id) {
  // IsProgram is true for programs as soon as they are created, until they are
  // deleted and no longer in use.
  const Program* program = GetProgram(client_id);
  return program != nullptr && !program->IsDeleted();
}

bool GLES2DecoderImpl::DoIsRenderbuffer(GLuint client_id) {
  const Renderbuffer* renderbuffer =
      GetRenderbuffer(client_id);
  return renderbuffer && renderbuffer->IsValid() && !renderbuffer->IsDeleted();
}

bool GLES2DecoderImpl::DoIsShader(GLuint client_id) {
  // IsShader is true for shaders as soon as they are created, until they
  // are deleted and not attached to any programs.
  const Shader* shader = GetShader(client_id);
  return shader != nullptr && !shader->IsDeleted();
}

bool GLES2DecoderImpl::DoIsTexture(GLuint client_id) {
  const TextureRef* texture_ref = GetTexture(client_id);
  return texture_ref && texture_ref->texture()->IsValid();
}

bool GLES2DecoderImpl::DoIsSampler(GLuint client_id) {
  const Sampler* sampler = GetSampler(client_id);
  return sampler && !sampler->IsDeleted();
}

bool GLES2DecoderImpl::DoIsTransformFeedback(GLuint client_id) {
  const TransformFeedback* transform_feedback =
      GetTransformFeedback(client_id);
  return transform_feedback && transform_feedback->has_been_bound();
}

void GLES2DecoderImpl::DoAttachShader(
    GLuint program_client_id, GLint shader_client_id) {
  Program* program = GetProgramInfoNotShader(
      program_client_id, "glAttachShader");
  if (!program) {
    return;
  }
  Shader* shader = GetShaderInfoNotProgram(shader_client_id, "glAttachShader");
  if (!shader) {
    return;
  }
  if (!program->AttachShader(shader_manager(), shader)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glAttachShader",
        "can not attach more than one shader of the same type.");
    return;
  }
  api()->glAttachShaderFn(program->service_id(), shader->service_id());
}

void GLES2DecoderImpl::DoDetachShader(
    GLuint program_client_id, GLint shader_client_id) {
  Program* program = GetProgramInfoNotShader(
      program_client_id, "glDetachShader");
  if (!program) {
    return;
  }
  Shader* shader = GetShaderInfoNotProgram(shader_client_id, "glDetachShader");
  if (!shader) {
    return;
  }
  if (!program->IsShaderAttached(shader)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glDetachShader", "shader not attached to program");
    return;
  }
  api()->glDetachShaderFn(program->service_id(), shader->service_id());
  program->DetachShader(shader_manager(), shader);
}

void GLES2DecoderImpl::DoValidateProgram(GLuint program_client_id) {
  Program* program = GetProgramInfoNotShader(
      program_client_id, "glValidateProgram");
  if (!program) {
    return;
  }
  program->Validate();
}

void GLES2DecoderImpl::GetVertexAttribHelper(
    const VertexAttrib* attrib, GLenum pname, GLint* params) {
  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      {
        Buffer* buffer = attrib->buffer();
        if (buffer && !buffer->IsDeleted()) {
          GLuint client_id;
          buffer_manager()->GetClientId(buffer->service_id(), &client_id);
          *params = client_id;
        }
        break;
      }
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      *params = attrib->enabled();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      *params = attrib->size();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      *params = attrib->gl_stride();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      *params = attrib->type();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      *params = attrib->normalized();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
      *params = attrib->divisor();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      *params = attrib->integer();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void GLES2DecoderImpl::DoGetSamplerParameterfv(GLuint client_id,
                                               GLenum pname,
                                               GLfloat* params,
                                               GLsizei params_size) {
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetSamplerParamterfv", "unknown sampler");
    return;
  }
  api()->glGetSamplerParameterfvFn(sampler->service_id(), pname, params);
}

void GLES2DecoderImpl::DoGetSamplerParameteriv(GLuint client_id,
                                               GLenum pname,
                                               GLint* params,
                                               GLsizei params_size) {
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetSamplerParamteriv", "unknown sampler");
    return;
  }
  api()->glGetSamplerParameterivFn(sampler->service_id(), pname, params);
}

void GLES2DecoderImpl::GetTexParameterImpl(
    GLenum target, GLenum pname, GLfloat* fparams, GLint* iparams,
    const char* function_name) {
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  switch (pname) {
    case GL_TEXTURE_BASE_LEVEL:
      // Use shadowed value in case it's clamped; also because older MacOSX
      // stores the value on int16_t (see https://crbug.com/610153).
      if (fparams) {
        fparams[0] = static_cast<GLfloat>(texture->unclamped_base_level());
      } else {
        iparams[0] = texture->unclamped_base_level();
      }
      return;
    case GL_TEXTURE_MAX_LEVEL:
      // Use shadowed value in case it's clamped; also because older MacOSX
      // stores the value on int16_t (see https://crbug.com/610153).
      if (fparams) {
        fparams[0] = static_cast<GLfloat>(texture->unclamped_max_level());
      } else {
        iparams[0] = texture->unclamped_max_level();
      }
      return;
    case GL_TEXTURE_SWIZZLE_R:
      if (fparams) {
        fparams[0] = static_cast<GLfloat>(texture->swizzle_r());
      } else {
        iparams[0] = texture->swizzle_r();
      }
      return;
    case GL_TEXTURE_SWIZZLE_G:
      if (fparams) {
        fparams[0] = static_cast<GLfloat>(texture->swizzle_g());
      } else {
        iparams[0] = texture->swizzle_g();
      }
      return;
    case GL_TEXTURE_SWIZZLE_B:
      if (fparams) {
        fparams[0] = static_cast<GLfloat>(texture->swizzle_b());
      } else {
        iparams[0] = texture->swizzle_b();
      }
      return;
    case GL_TEXTURE_SWIZZLE_A:
      if (fparams) {
        fparams[0] = static_cast<GLfloat>(texture->swizzle_a());
      } else {
        iparams[0] = texture->swizzle_a();
      }
      return;
    default:
      break;
  }
  if (fparams) {
    api()->glGetTexParameterfvFn(target, pname, fparams);
  } else {
    api()->glGetTexParameterivFn(target, pname, iparams);
  }
}

void GLES2DecoderImpl::DoGetTexParameterfv(GLenum target,
                                           GLenum pname,
                                           GLfloat* params,
                                           GLsizei params_size) {
  GetTexParameterImpl(target, pname, params, nullptr, "glGetTexParameterfv");
}

void GLES2DecoderImpl::DoGetTexParameteriv(GLenum target,
                                           GLenum pname,
                                           GLint* params,
                                           GLsizei params_size) {
  GetTexParameterImpl(target, pname, nullptr, params, "glGetTexParameteriv");
}

template <typename T>
void GLES2DecoderImpl::DoGetVertexAttribImpl(
    GLuint index, GLenum pname, T* params) {
  VertexAttrib* attrib = state_.vertex_attrib_manager->GetVertexAttrib(index);
  if (!attrib) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetVertexAttrib", "index out of range");
    return;
  }
  switch (pname) {
    case GL_CURRENT_VERTEX_ATTRIB:
      state_.attrib_values[index].GetValues(params);
      break;
    default: {
      GLint value = 0;
      GetVertexAttribHelper(attrib, pname, &value);
      *params = static_cast<T>(value);
      break;
    }
  }
}

void GLES2DecoderImpl::DoGetVertexAttribfv(GLuint index,
                                           GLenum pname,
                                           GLfloat* params,
                                           GLsizei params_size) {
  DoGetVertexAttribImpl<GLfloat>(index, pname, params);
}

void GLES2DecoderImpl::DoGetVertexAttribiv(GLuint index,
                                           GLenum pname,
                                           GLint* params,
                                           GLsizei params_size) {
  DoGetVertexAttribImpl<GLint>(index, pname, params);
}

void GLES2DecoderImpl::DoGetVertexAttribIiv(GLuint index,
                                            GLenum pname,
                                            GLint* params,
                                            GLsizei params_size) {
  DoGetVertexAttribImpl<GLint>(index, pname, params);
}

void GLES2DecoderImpl::DoGetVertexAttribIuiv(GLuint index,
                                             GLenum pname,
                                             GLuint* params,
                                             GLsizei params_size) {
  DoGetVertexAttribImpl<GLuint>(index, pname, params);
}

template <typename T>
bool GLES2DecoderImpl::SetVertexAttribValue(
    const char* function_name, GLuint index, const T* value) {
  if (index >= state_.attrib_values.size()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "index out of range");
    return false;
  }
  state_.attrib_values[index].SetValues(value);
  return true;
}

void GLES2DecoderImpl::DoVertexAttrib1f(GLuint index, GLfloat v0) {
  GLfloat v[4] = { v0, 0.0f, 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib1f", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    api()->glVertexAttrib1fFn(index, v0);
  }
}

void GLES2DecoderImpl::DoVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1) {
  GLfloat v[4] = { v0, v1, 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib2f", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    api()->glVertexAttrib2fFn(index, v0, v1);
  }
}

void GLES2DecoderImpl::DoVertexAttrib3f(
    GLuint index, GLfloat v0, GLfloat v1, GLfloat v2) {
  GLfloat v[4] = { v0, v1, v2, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib3f", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    api()->glVertexAttrib3fFn(index, v0, v1, v2);
  }
}

void GLES2DecoderImpl::DoVertexAttrib4f(
    GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
  GLfloat v[4] = { v0, v1, v2, v3, };
  if (SetVertexAttribValue("glVertexAttrib4f", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    api()->glVertexAttrib4fFn(index, v0, v1, v2, v3);
  }
}

void GLES2DecoderImpl::DoVertexAttrib1fv(GLuint index,
                                         const volatile GLfloat* v) {
  GLfloat t[4] = { v[0], 0.0f, 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib1fv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    api()->glVertexAttrib1fvFn(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttrib2fv(GLuint index,
                                         const volatile GLfloat* v) {
  GLfloat t[4] = { v[0], v[1], 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib2fv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    api()->glVertexAttrib2fvFn(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttrib3fv(GLuint index,
                                         const volatile GLfloat* v) {
  GLfloat t[4] = { v[0], v[1], v[2], 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib3fv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    api()->glVertexAttrib3fvFn(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttrib4fv(GLuint index,
                                         const volatile GLfloat* v) {
  GLfloat t[4] = {v[0], v[1], v[2], v[3]};
  if (SetVertexAttribValue("glVertexAttrib4fv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    api()->glVertexAttrib4fvFn(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttribI4i(
    GLuint index, GLint v0, GLint v1, GLint v2, GLint v3) {
  GLint v[4] = { v0, v1, v2, v3 };
  if (SetVertexAttribValue("glVertexAttribI4i", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_INT);
    api()->glVertexAttribI4iFn(index, v0, v1, v2, v3);
  }
}

void GLES2DecoderImpl::DoVertexAttribI4iv(GLuint index,
                                          const volatile GLint* v) {
  GLint t[4] = {v[0], v[1], v[2], v[3]};
  if (SetVertexAttribValue("glVertexAttribI4iv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_INT);
    api()->glVertexAttribI4ivFn(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttribI4ui(
    GLuint index, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
  GLuint v[4] = { v0, v1, v2, v3 };
  if (SetVertexAttribValue("glVertexAttribI4ui", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_UINT);
    api()->glVertexAttribI4uiFn(index, v0, v1, v2, v3);
  }
}

void GLES2DecoderImpl::DoVertexAttribI4uiv(GLuint index,
                                           const volatile GLuint* v) {
  GLuint t[4] = {v[0], v[1], v[2], v[3]};
  if (SetVertexAttribValue("glVertexAttribI4uiv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_UINT);
    api()->glVertexAttribI4uivFn(index, t);
  }
}

error::Error GLES2DecoderImpl::HandleVertexAttribIPointer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::VertexAttribIPointer& c =
      *static_cast<const volatile gles2::cmds::VertexAttribIPointer*>(cmd_data);
  GLuint indx = c.indx;
  GLint size = c.size;
  GLenum type = c.type;
  GLsizei stride = c.stride;
  GLsizei offset = c.offset;

  if (!state_.bound_array_buffer.get() ||
      state_.bound_array_buffer->IsDeleted()) {
    if (offset != 0) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, "glVertexAttribIPointer", "offset != 0");
      return error::kNoError;
    }
  }

  const void* ptr = reinterpret_cast<const void*>(offset);
  if (!validators_->vertex_attrib_i_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glVertexAttribIPointer", type, "type");
    return error::kNoError;
  }
  if (size < 1 || size > 4) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "size GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (indx >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "index out of range");
    return error::kNoError;
  }
  if (stride < 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "stride < 0");
    return error::kNoError;
  }
  if (stride > 255) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "stride > 255");
    return error::kNoError;
  }
  if (offset < 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "offset < 0");
    return error::kNoError;
  }
  uint32_t type_size = GLES2Util::GetGLTypeSizeForBuffers(type);
  // type_size must be a power of two to use & as optimized modulo.
  DCHECK(GLES2Util::IsPOT(type_size));
  if (offset & (type_size - 1)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glVertexAttribIPointer", "offset not valid for type");
    return error::kNoError;
  }
  if (stride & (type_size - 1)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glVertexAttribIPointer", "stride not valid for type");
    return error::kNoError;
  }

  GLenum base_type = (type == GL_BYTE || type == GL_SHORT || type == GL_INT) ?
                      SHADER_VARIABLE_INT : SHADER_VARIABLE_UINT;
  state_.vertex_attrib_manager->UpdateAttribBaseTypeAndMask(indx, base_type);

  uint32_t group_size = GLES2Util::GetGroupSizeForBufferType(size, type);
  DCHECK_LE(group_size, static_cast<uint32_t>(INT_MAX));
  state_.vertex_attrib_manager
      ->SetAttribInfo(indx,
                      state_.bound_array_buffer.get(),
                      size,
                      type,
                      GL_FALSE,
                      stride,
                      stride != 0 ? stride : group_size,
                      offset,
                      GL_TRUE);
  api()->glVertexAttribIPointerFn(indx, size, type, stride, ptr);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttribPointer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttribPointer& c =
      *static_cast<const volatile gles2::cmds::VertexAttribPointer*>(cmd_data);
  GLuint indx = c.indx;
  GLint size = c.size;
  GLenum type = c.type;
  GLboolean normalized = static_cast<GLboolean>(c.normalized);
  GLsizei stride = c.stride;
  GLsizei offset = c.offset;

  if (!state_.bound_array_buffer.get() ||
      state_.bound_array_buffer->IsDeleted()) {
    if (offset != 0) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, "glVertexAttribPointer", "offset != 0");
      return error::kNoError;
    }
  }
  const void* ptr = reinterpret_cast<const void*>(offset);
  if (!validators_->vertex_attrib_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glVertexAttribPointer", type, "type");
    return error::kNoError;
  }
  if (size < 1 || size > 4) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "size GL_INVALID_VALUE");
    return error::kNoError;
  }
  if ((type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV)
      && size != 4) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glVertexAttribPointer", "size != 4");
    return error::kNoError;
  }
  if (indx >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "index out of range");
    return error::kNoError;
  }
  if (stride < 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "stride < 0");
    return error::kNoError;
  }
  if (stride > 255) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "stride > 255");
    return error::kNoError;
  }
  if (offset < 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "offset < 0");
    return error::kNoError;
  }
  uint32_t type_size = GLES2Util::GetGLTypeSizeForBuffers(type);
  // type_size must be a power of two to use & as optimized modulo.
  DCHECK(GLES2Util::IsPOT(type_size));
  if (offset & (type_size - 1)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glVertexAttribPointer", "offset not valid for type");
    return error::kNoError;
  }
  if (stride & (type_size - 1)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glVertexAttribPointer", "stride not valid for type");
    return error::kNoError;
  }

  state_.vertex_attrib_manager->UpdateAttribBaseTypeAndMask(
      indx, SHADER_VARIABLE_FLOAT);

  uint32_t group_size = GLES2Util::GetGroupSizeForBufferType(size, type);
  DCHECK_LE(group_size, static_cast<uint32_t>(INT_MAX));
  state_.vertex_attrib_manager
      ->SetAttribInfo(indx,
                      state_.bound_array_buffer.get(),
                      size,
                      type,
                      normalized,
                      stride,
                      stride != 0 ? stride : group_size,
                      offset,
                      GL_FALSE);
  api()->glVertexAttribPointerFn(indx, size, type, normalized, stride, ptr);
  return error::kNoError;
}

void GLES2DecoderImpl::DoViewport(GLint x, GLint y, GLsizei width,
                                  GLsizei height) {
  state_.viewport_x = x;
  state_.viewport_y = y;
  state_.viewport_width = std::min(width, viewport_max_width_);
  state_.viewport_height = std::min(height, viewport_max_height_);
  api()->glViewportFn(x, y, width, height);
}

void GLES2DecoderImpl::DoScissor(GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height) {
  api()->glScissorFn(x, y, width, height);
}

error::Error GLES2DecoderImpl::HandleVertexAttribDivisorANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttribDivisorANGLE& c =
      *static_cast<const volatile gles2::cmds::VertexAttribDivisorANGLE*>(
          cmd_data);
  if (!features().angle_instanced_arrays)
    return error::kUnknownCommand;

  GLuint index = c.index;
  GLuint divisor = c.divisor;
  if (index >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glVertexAttribDivisorANGLE", "index out of range");
    return error::kNoError;
  }

  state_.vertex_attrib_manager->SetDivisor(
      index,
      divisor);
  api()->glVertexAttribDivisorANGLEFn(index, divisor);
  return error::kNoError;
}

template <typename pixel_data_type>
static void WriteAlphaData(void* pixels,
                           uint32_t row_count,
                           uint32_t channel_count,
                           uint32_t alpha_channel_index,
                           uint32_t unpadded_row_size,
                           uint32_t padded_row_size,
                           pixel_data_type alpha_value) {
  DCHECK_GT(channel_count, 0U);
  DCHECK_EQ(unpadded_row_size % sizeof(pixel_data_type), 0U);
  uint32_t unpadded_row_size_in_elements =
      unpadded_row_size / sizeof(pixel_data_type);
  DCHECK_EQ(padded_row_size % sizeof(pixel_data_type), 0U);
  uint32_t padded_row_size_in_elements =
      padded_row_size / sizeof(pixel_data_type);
  pixel_data_type* dst =
      static_cast<pixel_data_type*>(pixels) + alpha_channel_index;
  for (uint32_t yy = 0; yy < row_count; ++yy) {
    pixel_data_type* end = dst + unpadded_row_size_in_elements;
    for (pixel_data_type* d = dst; d < end; d += channel_count) {
      *d = alpha_value;
    }
    dst += padded_row_size_in_elements;
  }
}

void GLES2DecoderImpl::FinishReadPixels(GLsizei width,
                                        GLsizei height,
                                        GLsizei format,
                                        GLsizei type,
                                        uint32_t pixels_shm_id,
                                        uint32_t pixels_shm_offset,
                                        uint32_t result_shm_id,
                                        uint32_t result_shm_offset,
                                        GLint pack_alignment,
                                        GLenum read_format,
                                        GLuint buffer) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::FinishReadPixels");
  typedef cmds::ReadPixels::Result Result;
  uint32_t pixels_size;
  Result* result = nullptr;
  if (result_shm_id != 0) {
    result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                        sizeof(*result));
    if (!result) {
      if (buffer != 0) {
        api()->glDeleteBuffersARBFn(1, &buffer);
      }
      return;
    }
  }
  GLES2Util::ComputeImageDataSizes(width, height, 1, format, type,
                                   pack_alignment, &pixels_size, nullptr,
                                   nullptr);
  void* pixels =
      GetSharedMemoryAs<void*>(pixels_shm_id, pixels_shm_offset, pixels_size);
  if (!pixels) {
    if (buffer != 0) {
      api()->glDeleteBuffersARBFn(1, &buffer);
    }
    return;
  }

  if (buffer != 0) {
    api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER_ARB, buffer);
    void* data;
    if (features().map_buffer_range) {
      data = api()->glMapBufferRangeFn(GL_PIXEL_PACK_BUFFER_ARB, 0, pixels_size,
                                       GL_MAP_READ_BIT);
    } else {
      data = api()->glMapBufferFn(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
    }
    if (!data) {
      LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "glMapBuffer",
                         "Unable to map memory for readback.");
      return;
    }
    memcpy(pixels, data, pixels_size);
    api()->glUnmapBufferFn(GL_PIXEL_PACK_BUFFER_ARB);
    api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER_ARB,
                          GetServiceId(state_.bound_pixel_pack_buffer.get()));
    api()->glDeleteBuffersARBFn(1, &buffer);
  }

  if (result != nullptr) {
    result->success = 1;
  }
}

error::Error GLES2DecoderImpl::HandleReadbackARGBImagePixelsINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED_LOG_ONCE();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleWritePixelsYUVINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED_LOG_ONCE();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleReadPixels(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const char* func_name = "glReadPixels";
  const volatile gles2::cmds::ReadPixels& c =
      *static_cast<const volatile gles2::cmds::ReadPixels*>(cmd_data);
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::HandleReadPixels");
  GLint x = c.x;
  GLint y = c.y;
  GLsizei width = c.width;
  GLsizei height = c.height;
  GLenum format = c.format;
  GLenum type = c.type;
  uint32_t pixels_shm_id = c.pixels_shm_id;
  uint32_t pixels_shm_offset = c.pixels_shm_offset;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;
  GLboolean async = static_cast<GLboolean>(c.async);
  if (width < 0 || height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }
  typedef cmds::ReadPixels::Result Result;

  PixelStoreParams params;
  if (pixels_shm_id == 0) {
    params = state_.GetPackParams();
  } else {
    // When reading into client buffer, we actually set pack parameters to 0
    // (except for alignment) before calling glReadPixels. This makes sure we
    // only send back meaningful pixel data to the command buffer client side,
    // and the client side will take the responsibility to take the pixels and
    // write to the client buffer according to the full ES3 pack parameters.
    params.alignment = state_.pack_alignment;
  }
  uint32_t pixels_size = 0;
  uint32_t unpadded_row_size = 0;
  uint32_t padded_row_size = 0;
  uint32_t skip_size = 0;
  uint32_t padding = 0;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, 1,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           &unpadded_row_size,
                                           &padded_row_size,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }

  uint8_t* pixels = nullptr;
  Buffer* buffer = state_.bound_pixel_pack_buffer.get();
  if (pixels_shm_id == 0) {
    if (!buffer) {
      return error::kInvalidArguments;
    }
    if (!buffer_manager()->RequestBufferAccess(
            error_state_.get(), buffer, func_name, "pixel pack buffer")) {
      return error::kNoError;
    }
    uint32_t size = 0;
    if (!base::CheckAdd(pixels_size + skip_size, pixels_shm_offset)
             .AssignIfValid(&size)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "size + offset overflow");
      return error::kNoError;
    }
    if (static_cast<uint32_t>(buffer->size()) < size) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glReadPixels",
                         "pixel pack buffer is not large enough");
      return error::kNoError;
    }
    pixels = reinterpret_cast<uint8_t *>(pixels_shm_offset);
    pixels += skip_size;
  } else {
    if (buffer) {
      return error::kInvalidArguments;
    }
    DCHECK_EQ(0u, skip_size);
    pixels = GetSharedMemoryAs<uint8_t*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
  }

  Result* result = nullptr;
  if (result_shm_id != 0) {
    result = GetSharedMemoryAs<Result*>(
        result_shm_id, result_shm_offset, sizeof(*result));
    if (!result) {
      return error::kOutOfBounds;
    }
    if (result->success != 0) {
      return error::kInvalidArguments;
    }
  }

  if (!validators_->read_pixel_format.IsValid(format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, format, "format");
    return error::kNoError;
  }
  if (!validators_->read_pixel_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, type, "type");
    return error::kNoError;
  }

  if (!CheckBoundReadFramebufferValid(
          func_name, GL_INVALID_FRAMEBUFFER_OPERATION)) {
    return error::kNoError;
  }
  GLenum src_internal_format = GetBoundReadFramebufferInternalFormat();
  if (src_internal_format == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "no valid color image");
    return error::kNoError;
  }
  std::vector<GLenum> accepted_formats;
  std::vector<GLenum> accepted_types;
  switch (src_internal_format) {
    case GL_R8UI:
    case GL_R16UI:
    case GL_R32UI:
    case GL_RG8UI:
    case GL_RG16UI:
    case GL_RG32UI:
    // All the RGB_INTEGER formats are not renderable.
    case GL_RGBA8UI:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA32UI:
      accepted_formats.push_back(GL_RGBA_INTEGER);
      accepted_types.push_back(GL_UNSIGNED_INT);
      break;
    case GL_R8I:
    case GL_R16I:
    case GL_R32I:
    case GL_RG8I:
    case GL_RG16I:
    case GL_RG32I:
    case GL_RGBA8I:
    case GL_RGBA16I:
    case GL_RGBA32I:
      accepted_formats.push_back(GL_RGBA_INTEGER);
      accepted_types.push_back(GL_INT);
      break;
    case GL_RGB10_A2:
      accepted_formats.push_back(GL_RGBA);
      accepted_types.push_back(GL_UNSIGNED_BYTE);
      // Special case with an extra supported format/type.
      accepted_formats.push_back(GL_RGBA);
      accepted_types.push_back(GL_UNSIGNED_INT_2_10_10_10_REV);
      break;
    case GL_R16_EXT:
    case GL_RG16_EXT:
    case GL_RGBA16_EXT:
      accepted_formats.push_back(GL_RGBA);
      accepted_types.push_back(GL_UNSIGNED_SHORT);
      break;
    default:
      accepted_formats.push_back(GL_RGBA);
      {
        GLenum src_type = GetBoundReadFramebufferTextureType();
        switch (src_type) {
          case GL_HALF_FLOAT:
          case GL_HALF_FLOAT_OES:
          case GL_FLOAT:
          case GL_UNSIGNED_INT_10F_11F_11F_REV:
            accepted_types.push_back(GL_FLOAT);
            break;
          default:
            accepted_types.push_back(GL_UNSIGNED_BYTE);
            break;
        }
      }
      break;
  }
  if (!feature_info_->IsWebGLContext()) {
    accepted_formats.push_back(GL_BGRA_EXT);
    accepted_types.push_back(GL_UNSIGNED_BYTE);
  }
  DCHECK_EQ(accepted_formats.size(), accepted_types.size());
  bool format_type_acceptable = false;
  for (size_t ii = 0; ii < accepted_formats.size(); ++ii) {
    if (format == accepted_formats[ii] && type == accepted_types[ii]) {
      format_type_acceptable = true;
      break;
    }
  }
  if (!format_type_acceptable) {
    // format and type are acceptable enums but not guaranteed to be supported
    // for this framebuffer.  Have to ask gl if they are valid.
    GLint preferred_format = 0;
    DoGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &preferred_format, 1);
    GLint preferred_type = 0;
    DoGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &preferred_type, 1);
    if (format == static_cast<GLenum>(preferred_format) &&
        type == static_cast<GLenum>(preferred_type)) {
      format_type_acceptable = true;
    }
  }
  if (!format_type_acceptable) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "format and type incompatible with the current read framebuffer");
    return error::kNoError;
  }
  if (width == 0 || height == 0) {
    return error::kNoError;
  }

  // Get the size of the current fbo or backbuffer.
  gfx::Size max_size = GetBoundReadFramebufferSize();

  int32_t max_x;
  int32_t max_y;
  if (!base::CheckAdd(x, width).AssignIfValid(&max_x) ||
      !base::CheckAdd(y, height).AssignIfValid(&max_y)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions out of range");
    return error::kNoError;
  }

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(func_name);

  GLenum read_format = GetBoundReadFramebufferInternalFormat();

  gfx::Rect rect(x, y, width, height);  // Safe before we checked above.
  gfx::Rect max_rect(max_size);
  if (!max_rect.Contains(rect)) {
    rect.Intersect(max_rect);
    if (!rect.IsEmpty()) {
      std::unique_ptr<ScopedFramebufferCopyBinder> copy_binder;
      if (workarounds()
              .use_copyteximage2d_instead_of_readpixels_on_multisampled_textures &&
          framebuffer_state_.bound_read_framebuffer.get() &&
          framebuffer_state_.bound_read_framebuffer.get()
              ->GetReadBufferIsMultisampledTexture()) {
        copy_binder = std::make_unique<ScopedFramebufferCopyBinder>(this);
      }
      if (y < 0) {
        pixels += static_cast<uint32_t>(-y) * padded_row_size;;
      }
      if (x < 0) {
        uint32_t group_size = GLES2Util::ComputeImageGroupSize(format, type);
        uint32_t leading_bytes = static_cast<uint32_t>(-x) * group_size;
        pixels += leading_bytes;
      }
      for (GLint iy = rect.y(); iy < rect.bottom(); ++iy) {
        bool reset_row_length = false;
        if (iy + 1 == max_y && pixels_shm_id == 0 &&
            workarounds().pack_parameters_workaround_with_pack_buffer &&
            state_.pack_row_length > 0 && state_.pack_row_length < width) {
          // Some drivers (for example, Mac AMD) incorrecly limit the last
          // row to ROW_LENGTH in this case.
          api()->glPixelStoreiFn(GL_PACK_ROW_LENGTH, width);
          reset_row_length = true;
        }
        api()->glReadPixelsFn(rect.x(), iy, rect.width(), 1, format, type,
                              pixels);
        if (reset_row_length) {
          api()->glPixelStoreiFn(GL_PACK_ROW_LENGTH, state_.pack_row_length);
        }
        pixels += padded_row_size;
      }
    }
  } else {
    if (async && features().use_async_readpixels &&
        !state_.bound_pixel_pack_buffer.get()) {
      // This workaround isn't implemented in this code path yet. Hopefully it's
      // unnecessary.
      DCHECK(
          !workarounds()
               .use_copyteximage2d_instead_of_readpixels_on_multisampled_textures ||
          !framebuffer_state_.bound_read_framebuffer.get() ||
          !framebuffer_state_.bound_read_framebuffer.get()
               ->GetReadBufferIsMultisampledTexture());
      // To simply the state tracking, we don't go down the async path if
      // a PIXEL_PACK_BUFFER is bound (in which case the client can
      // implement something similar on their own - all necessary functions
      // should be exposed).
      GLuint buffer_handle = 0;
      api()->glGenBuffersARBFn(1, &buffer_handle);
      api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER_ARB, buffer_handle);
      // For ANGLE client version 2, GL_STREAM_READ is not available.
      const GLenum usage_hint =
          gl_version_info().is_angle ? GL_STATIC_DRAW : GL_STREAM_READ;
      api()->glBufferDataFn(GL_PIXEL_PACK_BUFFER_ARB, pixels_size, nullptr,
                            usage_hint);
      GLenum error = api()->glGetErrorFn();
      if (error == GL_NO_ERROR) {
        // No need to worry about ES3 pixel pack parameters, because no
        // PIXEL_PACK_BUFFER is bound, and all these settings haven't been
        // sent to GL.
        api()->glReadPixelsFn(x, y, width, height, format, type, 0);
        pending_readpixel_fences_.push(FenceCallback());
        WaitForReadPixels(base::BindOnce(
            &GLES2DecoderImpl::FinishReadPixels, weak_ptr_factory_.GetWeakPtr(),
            width, height, format, type, pixels_shm_id, pixels_shm_offset,
            result_shm_id, result_shm_offset, state_.pack_alignment,
            read_format, buffer_handle));
        api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER_ARB, 0);
        return error::kNoError;
      } else {
        // On error, unbind pack buffer and fall through to sync readpixels
        api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER_ARB, 0);
        api()->glDeleteBuffersARBFn(1, &buffer_handle);
      }
    }
    if (pixels_shm_id == 0 &&
        workarounds().pack_parameters_workaround_with_pack_buffer) {
      // If we ever have a device that needs both of these workarounds, we'll
      // need to do some extra work to implement that correctly here.
      DCHECK(
          !workarounds()
               .use_copyteximage2d_instead_of_readpixels_on_multisampled_textures);
      if (state_.pack_row_length > 0 && state_.pack_row_length < width) {
        // Some drivers (for example, NVidia Linux) reset in this case.
        // Some drivers (for example, Mac AMD) incorrecly limit the last
        // row to ROW_LENGTH in this case.
        api()->glPixelStoreiFn(GL_PACK_ROW_LENGTH, width);
        for (GLint iy = y; iy < max_y; ++iy) {
          // Need to set PACK_ALIGNMENT for last row. See comment below.
          if (iy + 1 == max_y && padding > 0)
            api()->glPixelStoreiFn(GL_PACK_ALIGNMENT, 1);
          api()->glReadPixelsFn(x, iy, width, 1, format, type, pixels);
          if (iy + 1 == max_y && padding > 0)
            api()->glPixelStoreiFn(GL_PACK_ALIGNMENT, state_.pack_alignment);
          pixels += padded_row_size;
        }
        api()->glPixelStoreiFn(GL_PACK_ROW_LENGTH, state_.pack_row_length);
      } else if (padding > 0) {
        // Some drivers (for example, NVidia Linux) incorrectly require the
        // pack buffer to have padding for the last row.
        if (height > 1)
          api()->glReadPixelsFn(x, y, width, height - 1, format, type, pixels);
        api()->glPixelStoreiFn(GL_PACK_ALIGNMENT, 1);
        pixels += padded_row_size * (height - 1);
        api()->glReadPixelsFn(x, max_y - 1, width, 1, format, type, pixels);
        api()->glPixelStoreiFn(GL_PACK_ALIGNMENT, state_.pack_alignment);
      } else {
        api()->glReadPixelsFn(x, y, width, height, format, type, pixels);
      }
    } else if (
        workarounds()
            .use_copyteximage2d_instead_of_readpixels_on_multisampled_textures &&
        framebuffer_state_.bound_read_framebuffer.get() &&
        framebuffer_state_.bound_read_framebuffer.get()
            ->GetReadBufferIsMultisampledTexture()) {
      ScopedFramebufferCopyBinder copy_binder(this, x, y, width, height);
      api()->glReadPixelsFn(0, 0, width, height, format, type, pixels);
    } else {
      api()->glReadPixelsFn(x, y, width, height, format, type, pixels);
    }
  }
  if (pixels_shm_id != 0) {
    GLenum error = LOCAL_PEEK_GL_ERROR(func_name);
    if (error == GL_NO_ERROR) {
      if (result) {
        result->success = 1;
        result->row_length = static_cast<uint32_t>(rect.width());
        result->num_rows = static_cast<uint32_t>(rect.height());
      }
      FinishReadPixels(width, height, format, type, pixels_shm_id,
                       pixels_shm_offset, result_shm_id, result_shm_offset,
                       state_.pack_alignment, read_format, 0);
    }
  }

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePixelStorei(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PixelStorei& c =
      *static_cast<const volatile gles2::cmds::PixelStorei*>(cmd_data);
  GLenum pname = c.pname;
  GLint param = c.param;
  if (!validators_->pixel_store.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glPixelStorei", pname, "pname");
    return error::kNoError;
  }
  switch (pname) {
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      if (!validators_->pixel_store_alignment.IsValid(param)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_VALUE, "glPixelStorei", "invalid param");
        return error::kNoError;
      }
      break;
    case GL_PACK_ROW_LENGTH:
    case GL_UNPACK_ROW_LENGTH:
    case GL_UNPACK_IMAGE_HEIGHT:
      if (param < 0) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_VALUE, "glPixelStorei", "invalid param");
        return error::kNoError;
      }
      break;
    case GL_PACK_SKIP_PIXELS:
    case GL_PACK_SKIP_ROWS:
    case GL_UNPACK_SKIP_PIXELS:
    case GL_UNPACK_SKIP_ROWS:
    case GL_UNPACK_SKIP_IMAGES:
      // All SKIP parameters are handled on the client side and should never
      // be passed to the service side.
      return error::kInvalidArguments;
    default:
      break;
  }
  // For alignment parameters, we always apply them.
  // For other parameters, we don't apply them if no buffer is bound at
  // PIXEL_PACK or PIXEL_UNPACK. We will handle pack and unpack according to
  // the user specified parameters on the client side.
  switch (pname) {
    case GL_PACK_ROW_LENGTH:
      if (state_.bound_pixel_pack_buffer.get())
        api()->glPixelStoreiFn(pname, param);
      break;
    case GL_UNPACK_ROW_LENGTH:
    case GL_UNPACK_IMAGE_HEIGHT:
      if (state_.bound_pixel_unpack_buffer.get())
        api()->glPixelStoreiFn(pname, param);
      break;
    default:
      api()->glPixelStoreiFn(pname, param);
      break;
  }
  switch (pname) {
    case GL_PACK_ALIGNMENT:
      state_.pack_alignment = param;
      break;
    case GL_PACK_ROW_LENGTH:
      state_.pack_row_length = param;
      break;
    case GL_UNPACK_ALIGNMENT:
      state_.unpack_alignment = param;
      break;
    case GL_UNPACK_ROW_LENGTH:
      state_.unpack_row_length = param;
      break;
    case GL_UNPACK_IMAGE_HEIGHT:
      state_.unpack_image_height = param;
      break;
    default:
      // Validation should have prevented us from getting here.
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::GetAttribLocationHelper(
    GLuint client_id,
    uint32_t location_shm_id,
    uint32_t location_shm_offset,
    const std::string& name_str) {
  if (!StringIsValidForGLES(name_str)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetAttribLocation", "Invalid character");
    return error::kNoError;
  }
  Program* program = GetProgramInfoNotShader(
      client_id, "glGetAttribLocation");
  if (!program) {
    return error::kNoError;
  }
  if (!program->IsValid()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetAttribLocation", "program not linked");
    return error::kNoError;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  *location = program->GetAttribLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetAttribLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetAttribLocation& c =
      *static_cast<const volatile gles2::cmds::GetAttribLocation*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetAttribLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::GetUniformLocationHelper(
    GLuint client_id,
    uint32_t location_shm_id,
    uint32_t location_shm_offset,
    const std::string& name_str) {
  if (!StringIsValidForGLES(name_str)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetUniformLocation", "Invalid character");
    return error::kNoError;
  }
  Program* program = GetProgramInfoNotShader(
      client_id, "glGetUniformLocation");
  if (!program) {
    return error::kNoError;
  }
  if (!program->IsValid()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetUniformLocation", "program not linked");
    return error::kNoError;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  *location = program->GetUniformFakeLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetUniformLocation& c =
      *static_cast<const volatile gles2::cmds::GetUniformLocation*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetUniformLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::HandleGetUniformIndices(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetUniformIndices& c =
      *static_cast<const volatile gles2::cmds::GetUniformIndices*>(cmd_data);
  Bucket* bucket = GetBucket(c.names_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  std::vector<char*> names;
  std::vector<GLint> len;
  if (!bucket->GetAsStrings(&count, &names, &len) || count <= 0) {
    return error::kInvalidArguments;
  }
  typedef cmds::GetUniformIndices::Result Result;
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(count).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.indices_shm_id, c.indices_shm_offset, checked_size);
  GLuint* indices = result ? result->GetData() : nullptr;
  if (indices == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(c.program, "glGetUniformIndices");
  if (!program) {
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  api()->glGetProgramivFn(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetUniformIndices", "program not linked");
    return error::kNoError;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetUniformIndices");
  api()->glGetUniformIndicesFn(service_id, count, &names[0], indices);
  GLenum error = api()->glGetErrorFn();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(count);
  } else {
    LOCAL_SET_GL_ERROR(error, "GetUniformIndices", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::GetFragDataLocationHelper(
    GLuint client_id,
    uint32_t location_shm_id,
    uint32_t location_shm_offset,
    const std::string& name_str) {
  const char kFunctionName[] = "glGetFragDataLocation";
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(client_id, kFunctionName);
  if (!program) {
    return error::kNoError;
  }
  if (!program->IsValid()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "program not linked");
    return error::kNoError;
  }

  *location = program->GetFragDataLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFragDataLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetFragDataLocation& c =
      *static_cast<const volatile gles2::cmds::GetFragDataLocation*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetFragDataLocationHelper(
      c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::GetFragDataIndexHelper(
    GLuint program_id,
    uint32_t index_shm_id,
    uint32_t index_shm_offset,
    const std::string& name_str) {
  const char kFunctionName[] = "glGetFragDataIndexEXT";
  GLint* index =
      GetSharedMemoryAs<GLint*>(index_shm_id, index_shm_offset, sizeof(GLint));
  if (!index) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*index != -1) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(program_id, kFunctionName);
  if (!program) {
    return error::kNoError;
  }
  if (!program->IsValid()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "program not linked");
    return error::kNoError;
  }

  *index = program->GetFragDataIndex(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFragDataIndexEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_blend_func_extended) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetFragDataIndexEXT& c =
      *static_cast<const volatile gles2::cmds::GetFragDataIndexEXT*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetFragDataIndexHelper(c.program, c.index_shm_id, c.index_shm_offset,
                                name_str);
}

error::Error GLES2DecoderImpl::HandleGetUniformBlockIndex(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetUniformBlockIndex& c =
      *static_cast<const volatile gles2::cmds::GetUniformBlockIndex*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLuint* index = GetSharedMemoryAs<GLuint*>(
      c.index_shm_id, c.index_shm_offset, sizeof(GLuint));
  if (!index) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*index != GL_INVALID_INDEX) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      c.program, "glGetUniformBlockIndex");
  if (!program) {
    return error::kNoError;
  }
  *index =
      api()->glGetUniformBlockIndexFn(program->service_id(), name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetString(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::GetString& c =
      *static_cast<const volatile gles2::cmds::GetString*>(cmd_data);
  GLenum name = static_cast<GLenum>(c.name);
  if (!validators_->string_type.IsValid(name)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetString", name, "name");
    return error::kNoError;
  }

  const char* str = nullptr;
  std::string extensions;
  switch (name) {
    case GL_VERSION:
      str = GetServiceVersionString(feature_info_.get());
      break;
    case GL_SHADING_LANGUAGE_VERSION:
      str = GetServiceShadingLanguageVersionString(feature_info_.get());
      break;
    case GL_EXTENSIONS: {
      gfx::ExtensionSet extension_set = feature_info_->extensions();
      // For WebGL contexts, strip out shader extensions if they have not
      // been enabled on WebGL1 or no longer exist (become core) in WebGL2.
      if (feature_info_->IsWebGLContext()) {
        if (!derivatives_explicitly_enabled_)
          extension_set.erase(kOESDerivativeExtension);
        if (!fbo_render_mipmap_explicitly_enabled_)
          extension_set.erase(kOESFboRenderMipmapExtension);
        if (!frag_depth_explicitly_enabled_)
          extension_set.erase(kEXTFragDepthExtension);
        if (!draw_buffers_explicitly_enabled_)
          extension_set.erase(kEXTDrawBuffersExtension);
        if (!shader_texture_lod_explicitly_enabled_)
          extension_set.erase(kEXTShaderTextureLodExtension);
        if (!multi_draw_explicitly_enabled_)
          extension_set.erase(kWEBGLMultiDrawExtension);
        if (!draw_instanced_base_vertex_base_instance_explicitly_enabled_)
          extension_set.erase(
              kWEBGLDrawInstancedBaseVertexBaseInstanceExtension);
        if (!multi_draw_instanced_base_vertex_base_instance_explicitly_enabled_)
          extension_set.erase(
              kWEBGLMultiDrawInstancedBaseVertexBaseInstanceExtension);
      }
      extensions = gfx::MakeExtensionString(extension_set);
      str = extensions.c_str();
      break;
    }
    default:
      str = reinterpret_cast<const char*>(api()->glGetStringFn(name));
      break;
  }
  Bucket* bucket = CreateBucket(c.bucket_id);
  bucket->SetFromString(str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBufferData(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile gles2::cmds::BufferData& c =
      *static_cast<const volatile gles2::cmds::BufferData*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t data_shm_id = static_cast<uint32_t>(c.data_shm_id);
  uint32_t data_shm_offset = static_cast<uint32_t>(c.data_shm_offset);
  GLenum usage = static_cast<GLenum>(c.usage);
  const void* data = nullptr;
  if (data_shm_id != 0 || data_shm_offset != 0) {
    data = GetSharedMemoryAs<const void*>(data_shm_id, data_shm_offset, size);
    if (!data) {
      return error::kOutOfBounds;
    }
  }
  buffer_manager()->ValidateAndDoBufferData(&state_, error_state_.get(), target,
                                            size, data, usage);
  return error::kNoError;
}

void GLES2DecoderImpl::DoBufferSubData(
  GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
  // Just delegate it. Some validation is actually done before this.
  buffer_manager()->ValidateAndDoBufferSubData(&state_, error_state_.get(),
                                               target, offset, size, data);
}

bool GLES2DecoderImpl::ClearLevel(Texture* texture,
                                  unsigned target,
                                  int level,
                                  unsigned format,
                                  unsigned type,
                                  int xoffset,
                                  int yoffset,
                                  int width,
                                  int height) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::ClearLevel");
  DCHECK(target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY &&
         target != GL_TEXTURE_EXTERNAL_OES);
  uint32_t channels = GLES2Util::GetChannelsForFormat(format);

  bool must_use_gl_clear = false;
  if ((channels & GLES2Util::kDepth) != 0 &&
      feature_info_->feature_flags().angle_depth_texture &&
      feature_info_->gl_version_info().is_es2) {
    // It's a depth format and ANGLE doesn't allow texImage2D or texSubImage2D
    // on depth formats in ES2.
    must_use_gl_clear = true;
  }

  uint32_t size;
  uint32_t padded_row_size;
  if (!GLES2Util::ComputeImageDataSizes(width, height, 1, format, type,
                                        state_.unpack_alignment, &size, nullptr,
                                        &padded_row_size)) {
    return false;
  }

  // Prefer to do the clear using a glClear call unless the clear is very small
  // (less than 64x64, or one page). Calls to TexSubImage2D on large textures
  // can take hundreds of milliseconds because of slow uploads on macOS. Do this
  // only on macOS because clears are buggy on other drivers.
  // https://crbug.com/848952 (slow uploads on macOS)
  // https://crbug.com/883276 (buggy clears on Android)
  bool prefer_use_gl_clear = false;
  if (must_use_gl_clear || prefer_use_gl_clear) {
    if (ClearLevelUsingGL(texture, channels, target, level, xoffset, yoffset,
                          width, height)) {
      return true;
    }
  }
  if (must_use_gl_clear)
    return false;

  TRACE_EVENT1("gpu", "Clear using TexSubImage2D", "size", size);

  int tile_height;

  const uint32_t kMaxZeroSize = 1024 * 1024 * 4;
  if (size > kMaxZeroSize) {
    if (kMaxZeroSize < padded_row_size) {
      // That'd be an awfully large texture.
      return false;
    }
    // We should never have a large total size with a zero row size.
    DCHECK_GT(padded_row_size, 0U);
    tile_height = kMaxZeroSize / padded_row_size;
    if (!GLES2Util::ComputeImageDataSizes(width, tile_height, 1, format, type,
                                          state_.unpack_alignment, &size,
                                          nullptr, nullptr)) {
      return false;
    }
  } else {
    tile_height = height;
  }

  api()->glBindTextureFn(texture->target(), texture->service_id());
  {
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    // Assumes the size has already been checked.
    auto zero = base::HeapArray<char>::WithSize(size);

    ScopedPixelUnpackState reset_restore(&state_);
    GLint y = 0;
    while (y < height) {
      GLint h = y + tile_height > height ? height - y : tile_height;
      api()->glTexSubImage2DFn(
          target, level, xoffset, yoffset + y, width, h,
          TextureManager::AdjustTexFormat(feature_info_.get(), format), type,
          zero.data());
      y += tile_height;
    }
  }

  TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  api()->glBindTextureFn(texture->target(),
                         bound_texture ? bound_texture->service_id() : 0);
  DCHECK(glGetError() == GL_NO_ERROR);
  return true;
}

bool GLES2DecoderImpl::ClearLevelUsingGL(Texture* texture,
                                         uint32_t channels,
                                         unsigned target,
                                         int level,
                                         int xoffset,
                                         int yoffset,
                                         int width,
                                         int height) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::ClearLevelUsingGL");
  GLenum fb_target = GetDrawFramebufferTarget();
  GLuint fb = 0;
  api()->glGenFramebuffersEXTFn(1, &fb);
  api()->glBindFramebufferEXTFn(fb_target, fb);

  bool have_color = (channels & GLES2Util::kRGBA) != 0;
  if (have_color) {
    api()->glFramebufferTexture2DEXTFn(fb_target, GL_COLOR_ATTACHMENT0, target,
                                       texture->service_id(), level);
  }
  bool have_depth = (channels & GLES2Util::kDepth) != 0;
  if (have_depth) {
    api()->glFramebufferTexture2DEXTFn(fb_target, GL_DEPTH_ATTACHMENT, target,
                                       texture->service_id(), level);
  }
  bool have_stencil = (channels & GLES2Util::kStencil) != 0;
  if (have_stencil) {
    api()->glFramebufferTexture2DEXTFn(fb_target, GL_STENCIL_ATTACHMENT, target,
                                       texture->service_id(), level);
  }
  // Attempt to do the clear only if the framebuffer is complete. ANGLE
  // promises a depth only attachment ok.
  bool result = false;
  if (api()->glCheckFramebufferStatusEXTFn(fb_target) ==
      GL_FRAMEBUFFER_COMPLETE) {
    state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    api()->glClearColorFn(0.0, 0.0, 0.0, 0.0);
    api()->glClearStencilFn(0);
    state_.SetDeviceStencilMaskSeparate(GL_FRONT, kDefaultStencilMask);
    state_.SetDeviceStencilMaskSeparate(GL_BACK, kDefaultStencilMask);
    api()->glClearDepthFn(1.0f);
    state_.SetDeviceDepthMask(GL_TRUE);
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, true);
    api()->glScissorFn(xoffset, yoffset, width, height);
    ClearDeviceWindowRectangles();

    api()->glClearFn((have_color ? GL_COLOR_BUFFER_BIT : 0) |
                     (have_depth ? GL_DEPTH_BUFFER_BIT : 0) |
                     (have_stencil ? GL_STENCIL_BUFFER_BIT : 0));
    result = true;
  }
  RestoreClearState();
  api()->glDeleteFramebuffersEXTFn(1, &fb);
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(fb_target);
  GLuint fb_service_id =
      framebuffer ? framebuffer->service_id() : GetBackbufferServiceId();
  api()->glBindFramebufferEXTFn(fb_target, fb_service_id);
  return result;
}

bool GLES2DecoderImpl::ClearCompressedTextureLevel(Texture* texture,
                                                   unsigned target,
                                                   int level,
                                                   unsigned format,
                                                   int width,
                                                   int height) {
  DCHECK(target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY);
  // This code path can only be called if the texture was originally
  // allocated via TexStorage2D. Note that TexStorage2D is exposed
  // internally for ES 2.0 contexts, but compressed texture support is
  // not part of that exposure.
  DCHECK(feature_info_->IsWebGL2OrES3Context());

  GLsizei bytes_required = 0;
  if (!GetCompressedTexSizeInBytes("ClearCompressedTextureLevel", width, height,
                                   1, format, &bytes_required,
                                   error_state_.get())) {
    return false;
  }

  TRACE_EVENT1("gpu", "GLES2DecoderImpl::ClearCompressedTextureLevel",
               "bytes_required", bytes_required);

  api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
  {
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    auto zero = base::HeapArray<char>::WithSize(bytes_required);
    api()->glBindTextureFn(texture->target(), texture->service_id());
    api()->glCompressedTexSubImage2DFn(target, level, 0, 0, width, height,
                                       format, zero.size(), zero.data());
  }
  TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  api()->glBindTextureFn(texture->target(),
                         bound_texture ? bound_texture->service_id() : 0);
  Buffer* bound_buffer = buffer_manager()->GetBufferInfoForTarget(
      &state_, GL_PIXEL_UNPACK_BUFFER);
  if (bound_buffer) {
    api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, bound_buffer->service_id());
  }
  return true;
}

bool GLES2DecoderImpl::ClearCompressedTextureLevel3D(Texture* texture,
                                                     unsigned target,
                                                     int level,
                                                     unsigned format,
                                                     int width,
                                                     int height,
                                                     int depth) {
  DCHECK(target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY);
  // This code path can only be called if the texture was originally
  // allocated via TexStorage3D. Note that TexStorage3D is exposed
  // internally for ES 2.0 contexts, but compressed texture support is
  // not part of that exposure.
  DCHECK(feature_info_->IsWebGL2OrES3Context());

  GLsizei bytes_required = 0;
  if (!GetCompressedTexSizeInBytes("ClearCompressedTextureLevel3D", width,
                                   height, 1, format, &bytes_required,
                                   error_state_.get())) {
    return false;
  }

  TRACE_EVENT1("gpu", "GLES2DecoderImpl::ClearCompressedTextureLevel3D",
               "bytes_required", bytes_required);

  api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
  {
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    auto zero = base::HeapArray<char>::WithSize(bytes_required);
    api()->glBindTextureFn(texture->target(), texture->service_id());
    api()->glCompressedTexSubImage3DFn(target, level, 0, 0, 0, width, height,
                                       depth, format, zero.size(), zero.data());
  }
  TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  api()->glBindTextureFn(texture->target(),
                         bound_texture ? bound_texture->service_id() : 0);
  Buffer* bound_buffer =
      buffer_manager()->GetBufferInfoForTarget(&state_, GL_PIXEL_UNPACK_BUFFER);
  if (bound_buffer) {
    api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, bound_buffer->service_id());
  }
  return true;
}

bool GLES2DecoderImpl::IsCompressedTextureFormat(unsigned format) {
  return feature_info_->validators()->compressed_texture_format.IsValid(
      format);
}

bool GLES2DecoderImpl::ClearLevel3D(Texture* texture,
                                    unsigned target,
                                    int level,
                                    unsigned format,
                                    unsigned type,
                                    int width,
                                    int height,
                                    int depth) {
  DCHECK(target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY);
  DCHECK(feature_info_->IsWebGL2OrES3Context());
  if (width == 0 || height == 0 || depth == 0)
    return true;

  uint32_t size;
  uint32_t padded_row_size;
  uint32_t padding;
  // Here we use unpack buffer to upload zeros into the texture, one layer
  // at a time.
  // We only take into consideration UNPACK_ALIGNMENT, and clear other unpack
  // parameters if necessary before TexSubImage3D calls.
  PixelStoreParams params;
  params.alignment = state_.unpack_alignment;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, depth,
                                           format, type,
                                           params,
                                           &size,
                                           nullptr,
                                           &padded_row_size,
                                           nullptr,
                                           &padding)) {
    return false;
  }
  const uint32_t kMaxZeroSize = 1024 * 1024 * 2;
  uint32_t buffer_size;
  std::vector<TexSubCoord3D> subs;
  if (size < kMaxZeroSize) {
    // Case 1: one TexSubImage3D call clears the entire 3D texture.
    buffer_size = size;
    subs.push_back(TexSubCoord3D(0, 0, 0, width, height, depth));
  } else {
    uint32_t size_per_layer;
    if (!base::CheckMul(padded_row_size, height)
             .AssignIfValid(&size_per_layer)) {
      return false;
    }
    if (size_per_layer < kMaxZeroSize) {
      // Case 2: Each TexSubImage3D call clears 1 or more layers.
      uint32_t depth_step = kMaxZeroSize / size_per_layer;
      uint32_t num_of_slices = depth / depth_step;
      if (num_of_slices * depth_step < static_cast<uint32_t>(depth))
        num_of_slices++;
      DCHECK_LT(0u, num_of_slices);
      buffer_size = size_per_layer * depth_step;
      int depth_of_last_slice = depth - (num_of_slices - 1) * depth_step;
      DCHECK_LT(0, depth_of_last_slice);
      for (uint32_t ii = 0; ii < num_of_slices; ++ii) {
        int depth_ii =
            (ii + 1 == num_of_slices ? depth_of_last_slice : depth_step);
        subs.push_back(
            TexSubCoord3D(0, 0, depth_step * ii, width, height, depth_ii));
      }
    } else {
      // Case 3: Multiple TexSubImage3D calls clear 1 layer.
      if (kMaxZeroSize < padded_row_size) {
        // That'd be an awfully large texture.
        return false;
      }
      uint32_t height_step = kMaxZeroSize / padded_row_size;
      uint32_t num_of_slices = height / height_step;
      if (num_of_slices * height_step < static_cast<uint32_t>(height))
        num_of_slices++;
      DCHECK_LT(0u, num_of_slices);
      buffer_size = padded_row_size * height_step;
      int height_of_last_slice = height - (num_of_slices - 1) * height_step;
      DCHECK_LT(0, height_of_last_slice);
      for (int zz = 0; zz < depth; ++zz) {
        for (uint32_t ii = 0; ii < num_of_slices; ++ii) {
          int height_ii =
              (ii + 1 == num_of_slices ? height_of_last_slice : height_step);
          subs.push_back(
              TexSubCoord3D(0, height_step * ii, zz, width, height_ii, 1));
        }
      }
    }
  }

  TRACE_EVENT1("gpu", "GLES2DecoderImpl::ClearLevel3D", "size", size);

  {
    ScopedPixelUnpackState reset_restore(&state_);
    GLuint buffer_id = 0;
    api()->glGenBuffersARBFn(1, &buffer_id);
    api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, buffer_id);
    {
      // Include padding as some drivers incorrectly requires padding for the
      // last row.
      buffer_size += padding;
      auto zero = base::HeapArray<char>::WithSize(buffer_size);
      // TODO(zmo): Consider glMapBufferRange instead.
      api()->glBufferDataFn(GL_PIXEL_UNPACK_BUFFER, zero.size(), zero.data(),
                            GL_STATIC_DRAW);
    }

    api()->glBindTextureFn(texture->target(), texture->service_id());
    for (size_t ii = 0; ii < subs.size(); ++ii) {
      api()->glTexSubImage3DFn(target, level, subs[ii].xoffset,
                               subs[ii].yoffset, subs[ii].zoffset,
                               subs[ii].width, subs[ii].height, subs[ii].depth,
                               format, type, nullptr);
    }
    api()->glDeleteBuffersARBFn(1, &buffer_id);
  }

  TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  api()->glBindTextureFn(texture->target(),
                         bound_texture ? bound_texture->service_id() : 0);
  return true;
}

namespace {

bool CheckETCFormatSupport(const FeatureInfo& feature_info) {
  const gl::GLVersionInfo& version_info = feature_info.gl_version_info();
  return version_info.IsAtLeastGLES(3, 0);
}

using CompressedFormatSupportCheck = bool (*)(const FeatureInfo&);
using CompressedFormatDecompressionFunction = void (*)(size_t width,
                                                       size_t height,
                                                       size_t depth,
                                                       const uint8_t* input,
                                                       size_t inputRowPitch,
                                                       size_t inputDepthPitch,
                                                       uint8_t* output,
                                                       size_t outputRowPitch,
                                                       size_t outputDepthPitch);

struct CompressedFormatInfo {
  GLenum format;
  uint32_t block_size;
  uint32_t bytes_per_block;
  CompressedFormatSupportCheck support_check;
  CompressedFormatDecompressionFunction decompression_function;
  GLenum decompressed_internal_format;
  GLenum decompressed_format;
  GLenum decompressed_type;
};

const CompressedFormatInfo kCompressedFormatInfoArray[] = {
    {
        GL_COMPRESSED_R11_EAC, 4, 8, CheckETCFormatSupport,
        angle::LoadEACR11ToR8, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SIGNED_R11_EAC, 4, 8, CheckETCFormatSupport,
        angle::LoadEACR11SToR8, GL_R8_SNORM, GL_RED, GL_BYTE,
    },
    {
        GL_COMPRESSED_RG11_EAC, 4, 16, CheckETCFormatSupport,
        angle::LoadEACRG11ToRG8, GL_RG8, GL_RG, GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SIGNED_RG11_EAC, 4, 16, CheckETCFormatSupport,
        angle::LoadEACRG11SToRG8, GL_RG8_SNORM, GL_RG, GL_BYTE,
    },
    {
        GL_COMPRESSED_RGB8_ETC2, 4, 8, CheckETCFormatSupport,
        angle::LoadETC2RGB8ToRGBA8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SRGB8_ETC2, 4, 8, CheckETCFormatSupport,
        angle::LoadETC2SRGB8ToRGBA8, GL_SRGB8_ALPHA8, GL_SRGB_ALPHA,
        GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_RGBA8_ETC2_EAC, 4, 16, CheckETCFormatSupport,
        angle::LoadETC2RGBA8ToRGBA8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, 4, 8,
        CheckETCFormatSupport, angle::LoadETC2RGB8A1ToRGBA8, GL_RGBA8, GL_RGBA,
        GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, 4, 16, CheckETCFormatSupport,
        angle::LoadETC2SRGBA8ToSRGBA8, GL_SRGB8_ALPHA8, GL_SRGB_ALPHA,
        GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 4, 8,
        CheckETCFormatSupport, angle::LoadETC2SRGB8A1ToRGBA8, GL_SRGB8_ALPHA8,
        GL_SRGB_ALPHA, GL_UNSIGNED_BYTE,
    },
};

const CompressedFormatInfo* GetCompressedFormatInfo(GLenum format) {
  for (size_t i = 0; i < std::size(kCompressedFormatInfoArray); i++) {
    if (kCompressedFormatInfoArray[i].format == format) {
      return &kCompressedFormatInfoArray[i];
    }
  }
  return nullptr;
}

uint32_t GetCompressedFormatRowPitch(const CompressedFormatInfo& info,
                                     uint32_t width) {
  uint32_t num_blocks_wide = (width + info.block_size - 1) / info.block_size;
  return num_blocks_wide * info.bytes_per_block;
}

uint32_t GetCompressedFormatDepthPitch(const CompressedFormatInfo& info,
                                       uint32_t width,
                                       uint32_t height) {
  uint32_t num_blocks_high = (height + info.block_size - 1) / info.block_size;
  return num_blocks_high * GetCompressedFormatRowPitch(info, width);
}

base::HeapArray<uint8_t> DecompressTextureData(const ContextState& state,
                                               const CompressedFormatInfo& info,
                                               uint32_t width,
                                               uint32_t height,
                                               uint32_t depth,
                                               GLsizei image_size,
                                               const void* data) {
  auto* api = state.api();
  uint32_t output_pixel_size = GLES2Util::ComputeImageGroupSize(
      info.decompressed_format, info.decompressed_type);
  auto decompressed_data =
      base::HeapArray<uint8_t>::Uninit(output_pixel_size * width * height);

  // If a PBO is bound, map it to decompress the data.
  const void* input_data = data;
  if (state.bound_pixel_unpack_buffer) {
    input_data = api->glMapBufferRangeFn(GL_PIXEL_UNPACK_BUFFER,
                                         reinterpret_cast<GLintptr>(data),
                                         image_size, GL_MAP_READ_BIT);
    if (input_data == nullptr) {
      LOG(ERROR) << "Failed to map pixel unpack buffer.";
      return base::HeapArray<uint8_t>();
    }
  }

  DCHECK_NE(input_data, nullptr);
  info.decompression_function(
      width, height, depth, static_cast<const uint8_t*>(input_data),
      GetCompressedFormatRowPitch(info, width),
      GetCompressedFormatDepthPitch(info, width, height),
      decompressed_data.data(), output_pixel_size * width,
      output_pixel_size * width * height);

  if (state.bound_pixel_unpack_buffer) {
    if (api->glUnmapBufferFn(GL_PIXEL_UNPACK_BUFFER) != GL_TRUE) {
      LOG(ERROR) << "glUnmapBuffer unexpectedly returned GL_FALSE";
      return base::HeapArray<uint8_t>();
    }
  }

  return decompressed_data;
}

}  // anonymous namespace.

bool GLES2DecoderImpl::ValidateCompressedTexFuncData(const char* function_name,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLsizei depth,
                                                     GLenum format,
                                                     GLsizei size,
                                                     const GLvoid* data) {
  GLsizei bytes_required = 0;
  if (!GetCompressedTexSizeInBytes(function_name, width, height, depth, format,
                                   &bytes_required, error_state_.get())) {
    return false;
  }

  if (size != bytes_required) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, function_name, "size is not correct for dimensions");
    return false;
  }

  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer &&
      !buffer_manager()->RequestBufferAccess(
          error_state_.get(), buffer, reinterpret_cast<GLintptr>(data),
          static_cast<GLsizeiptr>(bytes_required), function_name,
          "pixel unpack buffer")) {
    return false;
  }

  return true;
}

bool GLES2DecoderImpl::ValidateCompressedTexDimensions(
    const char* function_name, GLenum target, GLint level,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format) {
  const char* error_message = "";
  if (!::gpu::gles2::ValidateCompressedTexDimensions(
          target, level, width, height, depth, format, &error_message)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name, error_message);
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::ValidateCompressedTexSubDimensions(
    const char* function_name,
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    Texture* texture) {
  const char* error_message = "";
  if (!::gpu::gles2::ValidateCompressedTexSubDimensions(
          target, level, xoffset, yoffset, zoffset, width, height, depth,
          format, texture, &error_message)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name, error_message);
    return false;
  }
  return true;
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage2DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexImage2DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage2DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  GLint border = static_cast<GLint>(c.border);

  if (state_.bound_pixel_unpack_buffer.get()) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket)
    return error::kInvalidArguments;
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexImage(target, level, internal_format, width, height, 1,
                              border, image_size, data, ContextState::k2D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage2D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexImage2D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  const void* data;
  if (state_.bound_pixel_unpack_buffer.get()) {
    if (data_shm_id) {
      return error::kInvalidArguments;
    }
    data = reinterpret_cast<const void*>(data_shm_offset);
  } else {
    if (!data_shm_id && data_shm_offset) {
      return error::kInvalidArguments;
    }
    data = GetSharedMemoryAs<const void*>(
        data_shm_id, data_shm_offset, image_size);
  }
  return DoCompressedTexImage(target, level, internal_format, width, height, 1,
                              border, image_size, data, ContextState::k2D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage3DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CompressedTexImage3DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage3DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  GLint border = static_cast<GLint>(c.border);

  if (state_.bound_pixel_unpack_buffer.get()) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket)
    return error::kInvalidArguments;
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexImage(target, level, internal_format, width, height,
                              depth, border, image_size, data,
                              ContextState::k3D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage3D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CompressedTexImage3D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLint border = static_cast<GLint>(c.border);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  const void* data;
  if (state_.bound_pixel_unpack_buffer.get()) {
    if (data_shm_id) {
      return error::kInvalidArguments;
    }
    data = reinterpret_cast<const void*>(data_shm_offset);
  } else {
    if (!data_shm_id && data_shm_offset) {
      return error::kInvalidArguments;
    }
    data = GetSharedMemoryAs<const void*>(
        data_shm_id, data_shm_offset, image_size);
  }
  return DoCompressedTexImage(target, level, internal_format, width, height,
                              depth, border, image_size, data,
                              ContextState::k3D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage3DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CompressedTexSubImage3DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage3DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);

  if (state_.bound_pixel_unpack_buffer.get()) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket)
    return error::kInvalidArguments;
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexSubImage(target, level, xoffset, yoffset, zoffset,
                                 width, height, depth, format, image_size,
                                 data, ContextState::k3D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage3D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CompressedTexSubImage3D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage3D*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  const void* data;
  if (state_.bound_pixel_unpack_buffer.get()) {
    if (data_shm_id) {
      return error::kInvalidArguments;
    }
    data = reinterpret_cast<const void*>(data_shm_offset);
  } else {
    if (!data_shm_id && data_shm_offset) {
      return error::kInvalidArguments;
    }
    data = GetSharedMemoryAs<const void*>(
        data_shm_id, data_shm_offset, image_size);
  }
  return DoCompressedTexSubImage(target, level, xoffset, yoffset, zoffset,
                                 width, height, depth, format, image_size,
                                 data, ContextState::k3D);
}

error::Error GLES2DecoderImpl::DoCompressedTexImage(
    GLenum target, GLint level, GLenum internal_format, GLsizei width,
    GLsizei height, GLsizei depth, GLint border, GLsizei image_size,
    const void* data, ContextState::Dimension dimension) {
  const char* func_name;
  if (dimension == ContextState::k2D) {
    func_name = "glCompressedTexImage2D";
    if (!validators_->texture_target.IsValid(target)) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
    // TODO(ccameron): Add a separate texture from |texture_target| for
    // [Compressed]Tex[Sub]Image2D and related functions.
    // http://crbug.com/536854
    if (target == GL_TEXTURE_RECTANGLE_ARB) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
  } else {
    DCHECK_EQ(ContextState::k3D, dimension);
    func_name = "glCompressedTexImage3D";
    if (!validators_->texture_3_d_target.IsValid(target)) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
  }
  if (!validators_->compressed_texture_format.IsValid(internal_format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        func_name, internal_format, "internalformat");
    return error::kNoError;
  }
  if (image_size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "imageSize < 0");
    return error::kNoError;
  }
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, func_name, "no texture bound at target");
    return error::kNoError;
  }
  Texture* texture = texture_ref->texture();
  if (!texture_manager()->ValidForTextureTarget(texture, level, width, height,
                                                depth) ||
      border != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions out of range");
    return error::kNoError;
  }
  if (texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "texture is immutable");
    return error::kNoError;
  }

  if (!ValidateCompressedTexDimensions(func_name, target, level,
                                       width, height, depth, internal_format) ||
      !ValidateCompressedTexFuncData(func_name, width, height, depth,
                                     internal_format, image_size, data)) {
    return error::kNoError;
  }

  if (texture->IsAttachedToFramebuffer()) {
    framebuffer_state_.clear_state_dirty = true;
  }

  std::unique_ptr<int8_t[]> zero;
  if (!state_.bound_pixel_unpack_buffer && !data) {
    zero.reset(new int8_t[image_size]);
    memset(zero.get(), 0, image_size);
    data = zero.get();
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(func_name);
  const CompressedFormatInfo* format_info =
      GetCompressedFormatInfo(internal_format);
  if (format_info != nullptr && !format_info->support_check(*feature_info_)) {
    auto decompressed_data = DecompressTextureData(
        state_, *format_info, width, height, depth, image_size, data);
    if (decompressed_data.empty()) {
      MarkContextLost(error::kGuilty);
      group_->LoseContexts(error::kInnocent);
      return error::kLostContext;
    }
    ScopedPixelUnpackState reset_restore(&state_);
    if (dimension == ContextState::k2D) {
      api()->glTexImage2DFn(
          target, level, format_info->decompressed_internal_format, width,
          height, border, format_info->decompressed_format,
          format_info->decompressed_type, decompressed_data.data());
    } else {
      api()->glTexImage3DFn(
          target, level, format_info->decompressed_internal_format, width,
          height, depth, border, format_info->decompressed_format,
          format_info->decompressed_type, decompressed_data.data());
    }
  } else {
    if (dimension == ContextState::k2D) {
      api()->glCompressedTexImage2DFn(target, level, internal_format, width,
                                      height, border, image_size, data);
    } else {
      api()->glCompressedTexImage3DFn(target, level, internal_format, width,
                                      height, depth, border, image_size, data);
    }
  }
  GLenum error = LOCAL_PEEK_GL_ERROR(func_name);
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(texture_ref, target, level, internal_format,
                                    width, height, depth, border, 0, 0,
                                    gfx::Rect(width, height));
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexImage2D(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const char* func_name = "glTexImage2D";
  const volatile gles2::cmds::TexImage2D& c =
      *static_cast<const volatile gles2::cmds::TexImage2D*>(cmd_data);
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::HandleTexImage2D",
      "width", c.width, "height", c.height);
  // Set as failed for now, but if it successed, this will be set to not failed.
  texture_state_.tex_image_failed = true;
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = static_cast<uint32_t>(c.pixels_shm_id);
  uint32_t pixels_shm_offset = static_cast<uint32_t>(c.pixels_shm_offset);

  if (width < 0 || height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }

  PixelStoreParams params;
  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (pixels_shm_id)
      return error::kInvalidArguments;
    if (buffer->GetMappedRange()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "pixel unpack buffer should not be mapped to client memory");
      return error::kNoError;
    }
    params = state_.GetUnpackParams(ContextState::k2D);
  } else {
    if (!pixels_shm_id && pixels_shm_offset)
      return error::kInvalidArguments;
    // When reading from client buffer, the command buffer client side took
    // the responsibility to take the pixels from the client buffer and
    // unpack them according to the full ES3 pack parameters as source, all
    // parameters for 0 (except for alignment) as destination mem for the
    // service side.
    params.alignment = state_.unpack_alignment;
  }
  uint32_t pixels_size;
  uint32_t skip_size;
  uint32_t padding;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, 1,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           nullptr,
                                           nullptr,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }
  DCHECK_EQ(0u, skip_size);

  const void* pixels;
  if (pixels_shm_id) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels)
      return error::kOutOfBounds;
  } else {
    pixels = reinterpret_cast<const void*>(pixels_shm_offset);
  }

  TextureManager::DoTexImageArguments args = {
      target,
      level,
      internal_format,
      width,
      height,
      1,
      border,
      format,
      type,
      pixels,
      pixels_size,
      padding,
      TextureManager::DoTexImageArguments::CommandType::kTexImage2D};
  texture_manager()->ValidateAndDoTexImage(
      &texture_state_, &state_, error_state_.get(), &framebuffer_state_,
      func_name, args);

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexImage3D(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;

  const char* func_name = "glTexImage3D";
  const volatile gles2::cmds::TexImage3D& c =
      *static_cast<const volatile gles2::cmds::TexImage3D*>(cmd_data);
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::HandleTexImage3D",
      "widthXheight", c.width * c.height, "depth", c.depth);
  // Set as failed for now, but if it successed, this will be set to not failed.
  texture_state_.tex_image_failed = true;
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = static_cast<uint32_t>(c.pixels_shm_id);
  uint32_t pixels_shm_offset = static_cast<uint32_t>(c.pixels_shm_offset);

  if (width < 0 || height < 0 || depth < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }

  PixelStoreParams params;
  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (pixels_shm_id)
      return error::kInvalidArguments;
    if (buffer->GetMappedRange()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "pixel unpack buffer should not be mapped to client memory");
      return error::kNoError;
    }
    params = state_.GetUnpackParams(ContextState::k3D);
  } else {
    if (!pixels_shm_id && pixels_shm_offset)
      return error::kInvalidArguments;
    // When reading from client buffer, the command buffer client side took
    // the responsibility to take the pixels from the client buffer and
    // unpack them according to the full ES3 pack parameters as source, all
    // parameters for 0 (except for alignment) as destination mem for the
    // service side.
    params.alignment = state_.unpack_alignment;
  }
  uint32_t pixels_size;
  uint32_t skip_size;
  uint32_t padding;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, depth,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           nullptr,
                                           nullptr,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }
  DCHECK_EQ(0u, skip_size);

  const void* pixels;
  if (pixels_shm_id) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels)
      return error::kOutOfBounds;
  } else {
    pixels = reinterpret_cast<const void*>(pixels_shm_offset);
  }

  TextureManager::DoTexImageArguments args = {
      target,
      level,
      internal_format,
      width,
      height,
      depth,
      border,
      format,
      type,
      pixels,
      pixels_size,
      padding,
      TextureManager::DoTexImageArguments::CommandType::kTexImage3D};
  texture_manager()->ValidateAndDoTexImage(
      &texture_state_, &state_, error_state_.get(), &framebuffer_state_,
      func_name, args);

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage2DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexSubImage2DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage2DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);

  if (state_.bound_pixel_unpack_buffer.get()) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket)
    return error::kInvalidArguments;
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexSubImage(target, level, xoffset, yoffset, 0,
                                 width, height, 1, format, image_size, data,
                                 ContextState::k2D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexSubImage2D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage2D*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  const void* data;
  if (state_.bound_pixel_unpack_buffer.get()) {
    if (data_shm_id) {
      return error::kInvalidArguments;
    }
    data = reinterpret_cast<const void*>(data_shm_offset);
  } else {
    if (!data_shm_id) {
      return error::kInvalidArguments;
    }
    data = GetSharedMemoryAs<const void*>(
        data_shm_id, data_shm_offset, image_size);
  }
  return DoCompressedTexSubImage(target, level, xoffset, yoffset, 0,
                                 width, height, 1, format, image_size, data,
                                 ContextState::k2D);
}

error::Error GLES2DecoderImpl::DoCompressedTexSubImage(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    GLsizei image_size, const void * data, ContextState::Dimension dimension) {
  const char* func_name;
  if (dimension == ContextState::k2D) {
    func_name = "glCompressedTexSubImage2D";
    if (!validators_->texture_target.IsValid(target)) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
  } else {
    DCHECK_EQ(ContextState::k3D, dimension);
    func_name = "glCompressedTexSubImage3D";
    if (!validators_->texture_3_d_target.IsValid(target)) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
  }
  if (!validators_->compressed_texture_format.IsValid(format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, format, "format");
    return error::kNoError;
  }
  if (image_size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "imageSize < 0");
    return error::kNoError;
  }
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "no texture bound at target");
    return error::kNoError;
  }

  Texture* texture = texture_ref->texture();
  if (!texture_manager()->ValidForTextureTarget(texture, level, width, height,
                                                depth)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions out of range");
    return error::kNoError;
  }

  GLenum type = 0;
  GLenum internal_format = 0;
  if (!texture->GetLevelType(target, level, &type, &internal_format)) {
    std::string msg = base::StringPrintf("level %d does not exist", level);
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, msg.c_str());
    return error::kNoError;
  }
  if (internal_format != format) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                       "format does not match internalformat.");
    return error::kNoError;
  }
  if (!texture->ValidForTexture(target, level, xoffset, yoffset, zoffset,
                                width, height, depth)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "bad dimensions.");
    return error::kNoError;
  }

  if (!ValidateCompressedTexFuncData(
          func_name, width, height, depth, format, image_size, data) ||
      !ValidateCompressedTexSubDimensions(
          func_name, target, level, xoffset, yoffset, zoffset,
          width, height, depth, format, texture)) {
    return error::kNoError;
  }
  if (image_size > 0 && !state_.bound_pixel_unpack_buffer && !data)
    return error::kInvalidArguments;

  if (!texture->IsLevelCleared(target, level)) {
    // This can only happen if the compressed texture was allocated
    // using TexStorage{2|3}D.
    DCHECK(texture->IsImmutable());
    GLsizei level_width = 0, level_height = 0, level_depth = 0;
    bool success = texture->GetLevelSize(
        target, level, &level_width, &level_height, &level_depth);
    DCHECK(success);
    if (xoffset == 0 && width == level_width &&
        yoffset == 0 && height == level_height &&
        zoffset == 0 && depth == level_depth) {
      // We can skip the clear if we're uploading the entire level.
      texture_manager()->SetLevelCleared(texture_ref, target, level, true);
    } else {
      texture_manager()->ClearTextureLevel(this, texture_ref, target, level);
    }
    DCHECK(texture->IsLevelCleared(target, level));
  }

  const CompressedFormatInfo* format_info =
      GetCompressedFormatInfo(internal_format);
  if (format_info != nullptr && !format_info->support_check(*feature_info_)) {
    auto decompressed_data = DecompressTextureData(
        state_, *format_info, width, height, depth, image_size, data);
    if (decompressed_data.empty()) {
      MarkContextLost(error::kGuilty);
      group_->LoseContexts(error::kInnocent);
      return error::kLostContext;
    }
    ScopedPixelUnpackState reset_restore(&state_);
    if (dimension == ContextState::k2D) {
      api()->glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                               format_info->decompressed_format,
                               format_info->decompressed_type,
                               decompressed_data.data());
    } else {
      api()->glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                               height, depth, format_info->decompressed_format,
                               format_info->decompressed_type,
                               decompressed_data.data());
    }
  } else {
    if (dimension == ContextState::k2D) {
      api()->glCompressedTexSubImage2DFn(target, level, xoffset, yoffset, width,
                                         height, format, image_size, data);
    } else {
      api()->glCompressedTexSubImage3DFn(target, level, xoffset, yoffset,
                                         zoffset, width, height, depth, format,
                                         image_size, data);
    }
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

bool GLES2DecoderImpl::ValidateCopyTexFormat(const char* func_name,
                                             GLenum internal_format,
                                             GLenum read_format,
                                             GLenum read_type) {
  std::string output_error_msg;
  if (!ValidateCopyTexFormatHelper(GetFeatureInfo(), internal_format,
                                   read_format, read_type, &output_error_msg)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                       output_error_msg.c_str());
    return false;
  }
  return true;
}

void GLES2DecoderImpl::DoCopyTexImage2D(
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLint border) {
  const char* func_name = "glCopyTexImage2D";
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  if (texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "texture is immutable");
    return;
  }
  if (!texture_manager()->ValidForTextureTarget(texture, level, width, height,
                                                1) ||
      border != 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, func_name, "dimensions out of range");
    return;
  }

  if (!CheckBoundReadFramebufferValid(func_name,
                                      GL_INVALID_FRAMEBUFFER_OPERATION)) {
    return;
  }

  GLenum read_format = GetBoundReadFramebufferInternalFormat();
  GLenum read_type = GetBoundReadFramebufferTextureType();
  if (!ValidateCopyTexFormat(func_name, internal_format,
                             read_format, read_type)) {
    return;
  }

  uint32_t pixels_size = 0;
  // TODO(piman): OpenGL ES 3.0.4 Section 3.8.5 specifies how to pick an
  // effective internal format if internal_format is unsized, which is a fairly
  // involved logic. For now, just make sure we pick something valid.
  GLenum format =
      TextureManager::ExtractFormatFromStorageFormat(internal_format);
  GLenum type = TextureManager::ExtractTypeFromStorageFormat(internal_format);
  bool internal_format_unsized = internal_format == format;
  // The picks made by the temporary logic above may not be valid on ES3.
  // This if-block checks the temporary logic and should be removed with it.
  if (internal_format_unsized && feature_info_->IsWebGL2OrES3Context()) {
    // While there are other possible types for unsized formats (cf. OpenGL ES
    // 3.0.5, section 3.7, table 3.3, page 113), they won't appear here.
    // ExtractTypeFromStorageFormat will always return UNSIGNED_BYTE for
    // unsized formats.
    DCHECK(type == GL_UNSIGNED_BYTE);
    // Changing the internal format here is probably not completely
    // correct. This is the "effective" internal format, and the spec
    // says "effective internal format is used by the GL for purposes
    // such as texture completeness or type checks for CopyTex*
    // commands". But we don't have a separate concept of "effective"
    // vs. "actual" internal format, so this will have to do for now. See
    // Table 3.12 and associated explanatory text in the OpenGL ES 3.0.6
    // spec for more information.
    bool attempt_sized_upgrade =
        read_format == GL_RGB || read_format == GL_RGB8 ||
        read_format == GL_RGBA || read_format == GL_RGBA8;
    switch (internal_format) {
      case GL_RGB:
        if (attempt_sized_upgrade) {
          internal_format = GL_RGB8;
        }
        break;
      case GL_RGBA:
        if (attempt_sized_upgrade) {
          internal_format = GL_RGBA8;
        }
        break;
      case GL_LUMINANCE_ALPHA:
      case GL_LUMINANCE:
      case GL_ALPHA:
      case GL_BGRA_EXT:
        // There are no GL constants for sized versions of these internal
        // formats. We'll just go ahead with the unsized ones.
        break;
      default:
        // Other unsized internal_formats are invalid in ES3.
        format = GL_NONE;
        break;
    }
  }
  if (!format || !type) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        func_name, "Invalid unsized internal format.");
    return;
  }

  DCHECK(texture_manager()->ValidateTextureParameters(
      error_state_.get(), func_name, true, format, type, internal_format,
      level));

  // Only target image size is validated here.
  if (!GLES2Util::ComputeImageDataSizes(width, height, 1, format, type,
                                        state_.unpack_alignment, &pixels_size,
                                        nullptr, nullptr)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "dimensions too large");
    return;
  }

  if (FormsTextureCopyingFeedbackLoop(texture_ref, level, 0)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        func_name, "source and destination textures are the same");
    return;
  }

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(func_name);
  gfx::Size size = GetBoundReadFramebufferSize();

  if (texture->IsAttachedToFramebuffer()) {
    framebuffer_state_.clear_state_dirty = true;
  }

  bool requires_luma_blit =
      CopyTexImageResourceManager::CopyTexImageRequiresBlit(feature_info_.get(),
                                                            format);
  if (requires_luma_blit &&
      !InitializeCopyTexImageBlitter(func_name)) {
    return;
  }

  // If fbo's read buffer and the target texture are the same texture, but
  // different levels, and if the read buffer is non-base texture level,
  // then following internal glTexImage2D() calls may change the target texture
  // and make the originally mipmap complete texture mipmap incomplete, which
  // in turn make the fbo incomplete.
  // In order to avoid that, we clamp the BASE_LEVEL and MAX_LEVEL to the same
  // texture level that's attached to the fbo's read buffer.
  bool reset_source_texture_base_level_max_level = false;
  GLint attached_texture_level = -1;
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (framebuffer) {
    const Framebuffer::Attachment* attachment =
        framebuffer->GetReadBufferAttachment();
    if (attachment->IsTexture(texture_ref)) {
      DCHECK(attachment->IsTextureAttachment());
      attached_texture_level = attachment->level();
      DCHECK_GE(attached_texture_level, 0);
      if (attached_texture_level != texture->base_level())
        reset_source_texture_base_level_max_level = true;
    }
  }
  if (reset_source_texture_base_level_max_level) {
    api()->glTexParameteriFn(target, GL_TEXTURE_BASE_LEVEL,
                             attached_texture_level);
    api()->glTexParameteriFn(target, GL_TEXTURE_MAX_LEVEL,
                             attached_texture_level);
  }

  // Clip to size to source dimensions
  gfx::Rect src(x, y, width, height);
  const gfx::Rect dst(0, 0, size.width(), size.height());
  src.Intersect(dst);

  GLenum final_internal_format = TextureManager::AdjustTexInternalFormat(
      feature_info_.get(), internal_format, type);
  if (workarounds().force_int_or_srgb_cube_texture_complete &&
      texture->target() == GL_TEXTURE_CUBE_MAP &&
      (GLES2Util::IsIntegerFormat(final_internal_format) ||
       GLES2Util::GetColorEncodingFromInternalFormat(final_internal_format) ==
           GL_SRGB)) {
    TextureManager::DoTexImageArguments args = {
        target,
        level,
        final_internal_format,
        width,
        height,
        1,
        border,
        format,
        type,
        nullptr,
        pixels_size,
        0,
        TextureManager::DoTexImageArguments::CommandType::kTexImage2D};
    texture_manager()->WorkaroundCopyTexImageCubeMap(
        &texture_state_, &state_, error_state_.get(), &framebuffer_state_,
        texture_ref, func_name, args);
  }

  if (src.x() != x || src.y() != y ||
      src.width() != width || src.height() != height ||
      final_internal_format == GL_BGRA_EXT) {
    // GL_BGRA_EXT is not allowed as internalformat for glCopyTexImage2D,
    // which is a bit of a quirk in the spec, but this path works.
    {
      // Add extra scope to destroy zero and the object it owns right
      // after its usage.
      // some part was clipped so clear the rect.

      auto zero = base::HeapArray<char>::WithSize(pixels_size);
      ScopedPixelUnpackState reset_restore(&state_);
      api()->glTexImage2DFn(target, level, final_internal_format, width, height,
                            border, format, type, zero.data());
    }

    if (!src.IsEmpty()) {
      GLint destX = src.x() - x;
      GLint destY = src.y() - y;
      if (requires_luma_blit) {
        copy_tex_image_blit_->DoCopyTexSubImageToLUMACompatibilityTexture(
            this, texture->service_id(), texture->target(), target, format,
            type, level, destX, destY, 0,
            src.x(), src.y(), src.width(), src.height(),
            GetBoundReadFramebufferServiceId(),
            GetBoundReadFramebufferInternalFormat());
      } else {
        api()->glCopyTexSubImage2DFn(target, level, destX, destY, src.x(),
                                     src.y(), src.width(), src.height());
      }
    }
  } else {
    if (requires_luma_blit) {
      copy_tex_image_blit_->DoCopyTexImage2DToLUMACompatibilityTexture(
          this, texture->service_id(), texture->target(), target, format, type,
          level, internal_format, x, y, width, height,
          GetBoundReadFramebufferServiceId(),
          GetBoundReadFramebufferInternalFormat());
    } else {
      api()->glCopyTexImage2DFn(target, level, final_internal_format, x, y,
                                width, height, border);
    }
  }
  if (reset_source_texture_base_level_max_level) {
    api()->glTexParameteriFn(target, GL_TEXTURE_BASE_LEVEL,
                             texture->base_level());
    api()->glTexParameteriFn(target, GL_TEXTURE_MAX_LEVEL,
                             texture->max_level());
  }
  GLenum error = LOCAL_PEEK_GL_ERROR(func_name);
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(texture_ref, target, level, internal_format,
                                    width, height, 1, border, format,
                                    type, gfx::Rect(width, height));
    texture->ApplyFormatWorkarounds(feature_info_.get());
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

void GLES2DecoderImpl::DoCopyTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  const char* func_name = "glCopyTexSubImage2D";
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  GLenum type = 0;
  GLenum internal_format = 0;
  if (!texture->GetLevelType(target, level, &type, &internal_format) ||
      !texture->ValidForTexture(
          target, level, xoffset, yoffset, 0, width, height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "bad dimensions.");
    return;
  }

  if (!CheckBoundReadFramebufferValid(func_name,
                                      GL_INVALID_FRAMEBUFFER_OPERATION)) {
    return;
  }

  GLenum read_format = GetBoundReadFramebufferInternalFormat();
  GLenum read_type = GetBoundReadFramebufferTextureType();
  if (!ValidateCopyTexFormat(func_name, internal_format,
                             read_format, read_type)) {
    return;
  }

  if (FormsTextureCopyingFeedbackLoop(texture_ref, level, 0)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        func_name, "source and destination textures are the same");
    return;
  }

  gfx::Size size = GetBoundReadFramebufferSize();

  gfx::Rect src(x, y, width, height);
  const gfx::Rect dst(0, 0, size.width(), size.height());
  src.Intersect(dst);

  if (src.IsEmpty())
    return;

  GLint dx = src.x() - x;
  GLint dy = src.y() - y;
  GLint destX = xoffset + dx;
  GLint destY = yoffset + dy;
  // It's only legal to skip clearing the level of the target texture
  // if the entire level is being redefined.
  GLsizei level_width = 0;
  GLsizei level_height = 0;
  GLsizei level_depth = 0;
  bool have_level = texture->GetLevelSize(
      target, level, &level_width, &level_height, &level_depth);
  // Validated above.
  DCHECK(have_level);
  if (destX == 0 && destY == 0 &&
      src.width() == level_width && src.height() == level_height) {
    // Write all pixels in below.
    texture_manager()->SetLevelCleared(texture_ref, target, level, true);
  } else {
    gfx::Rect cleared_rect;
    if (TextureManager::CombineAdjacentRects(
            texture->GetLevelClearedRect(target, level),
            gfx::Rect(destX, destY, src.width(), src.height()),
            &cleared_rect)) {
      DCHECK_GE(cleared_rect.size().GetArea(),
                texture->GetLevelClearedRect(target, level).size().GetArea());
      texture_manager()->SetLevelClearedRect(texture_ref, target, level,
                                             cleared_rect);
    } else {
      // Otherwise clear part of texture level that is not already cleared.
      if (!texture_manager()->ClearTextureLevel(this, texture_ref, target,
                                                level)) {
        LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "dimensions too big");
        return;
      }
    }
  }

  if (CopyTexImageResourceManager::CopyTexImageRequiresBlit(
          feature_info_.get(), internal_format)) {
    if (!InitializeCopyTexImageBlitter("glCopyTexSubImage2D")) {
      return;
    }
    copy_tex_image_blit_->DoCopyTexSubImageToLUMACompatibilityTexture(
        this, texture->service_id(), texture->target(), target,
        internal_format, type, level, destX, destY, 0,
        src.x(), src.y(), src.width(), src.height(),
        GetBoundReadFramebufferServiceId(),
        GetBoundReadFramebufferInternalFormat());
  } else {
    api()->glCopyTexSubImage2DFn(target, level, destX, destY, src.x(), src.y(),
                                 src.width(), src.height());
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

void GLES2DecoderImpl::DoCopyTexSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  const char* func_name = "glCopyTexSubImage3D";
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  GLenum type = 0;
  GLenum internal_format = 0;
  if (!texture->GetLevelType(target, level, &type, &internal_format) ||
      !texture->ValidForTexture(
          target, level, xoffset, yoffset, zoffset, width, height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "bad dimensions.");
    return;
  }

  if (!CheckBoundReadFramebufferValid(func_name,
                                      GL_INVALID_FRAMEBUFFER_OPERATION)) {
    return;
  }

  GLenum read_format = GetBoundReadFramebufferInternalFormat();
  GLenum read_type = GetBoundReadFramebufferTextureType();
  if (!ValidateCopyTexFormat(func_name, internal_format,
                             read_format, read_type)) {
    return;
  }

  if (FormsTextureCopyingFeedbackLoop(texture_ref, level, zoffset)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        func_name, "source and destination textures are the same");
    return;
  }

  gfx::Size size = GetBoundReadFramebufferSize();

  gfx::Rect src(x, y, width, height);
  const gfx::Rect dst(0, 0, size.width(), size.height());
  src.Intersect(dst);
  if (src.IsEmpty())
    return;

  GLint dx = src.x() - x;
  GLint dy = src.y() - y;
  GLint destX = xoffset + dx;
  GLint destY = yoffset + dy;
  // For 3D textures, we always clear the entire texture to 0 if it is not
  // cleared. See the code in TextureManager::ValidateAndDoTexSubImage
  // for TexSubImage3D.
  if (!texture->IsLevelCleared(target, level)) {
    if (!texture_manager()->ClearTextureLevel(this, texture_ref, target,
                                              level)) {
      LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "dimensions too big");
      return;
    }
    DCHECK(texture->IsLevelCleared(target, level));
  }

  if (CopyTexImageResourceManager::CopyTexImageRequiresBlit(
          feature_info_.get(), internal_format)) {
    if (!InitializeCopyTexImageBlitter(func_name)) {
      return;
    }
    copy_tex_image_blit_->DoCopyTexSubImageToLUMACompatibilityTexture(
        this, texture->service_id(), texture->target(), target,
        internal_format, type, level, destX, destY, zoffset,
        src.x(), src.y(), src.width(), src.height(),
        GetBoundReadFramebufferServiceId(),
        GetBoundReadFramebufferInternalFormat());
  } else {
    api()->glCopyTexSubImage3DFn(target, level, destX, destY, zoffset, src.x(),
                                 src.y(), src.width(), src.height());
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

error::Error GLES2DecoderImpl::HandleTexSubImage2D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const char* func_name = "glTexSubImage2D";
  const volatile gles2::cmds::TexSubImage2D& c =
      *static_cast<const volatile gles2::cmds::TexSubImage2D*>(cmd_data);
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::HandleTexSubImage2D",
      "width", c.width, "height", c.height);
  GLboolean internal = static_cast<GLboolean>(c.internal);
  if (internal == GL_TRUE && texture_state_.tex_image_failed)
    return error::kNoError;

  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = static_cast<uint32_t>(c.pixels_shm_id);
  uint32_t pixels_shm_offset = static_cast<uint32_t>(c.pixels_shm_offset);

  if (width < 0 || height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }

  PixelStoreParams params;
  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (pixels_shm_id)
      return error::kInvalidArguments;
    if (buffer->GetMappedRange()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "pixel unpack buffer should not be mapped to client memory");
      return error::kNoError;
    }
    params = state_.GetUnpackParams(ContextState::k2D);
  } else {
    if (!pixels_shm_id && pixels_shm_offset)
      return error::kInvalidArguments;
    // When reading from client buffer, the command buffer client side took
    // the responsibility to take the pixels from the client buffer and
    // unpack them according to the full ES3 pack parameters as source, all
    // parameters for 0 (except for alignment) as destination mem for the
    // service side.
    params.alignment = state_.unpack_alignment;
  }
  uint32_t pixels_size;
  uint32_t skip_size;
  uint32_t padding;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, 1,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           nullptr,
                                           nullptr,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }
  DCHECK_EQ(0u, skip_size);

  const void* pixels;
  if (pixels_shm_id) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels)
      return error::kOutOfBounds;
  } else {
    DCHECK(buffer || !pixels_shm_offset);
    pixels = reinterpret_cast<const void*>(pixels_shm_offset);
  }

  TextureManager::DoTexSubImageArguments args = {
      target,
      level,
      xoffset,
      yoffset,
      0,
      width,
      height,
      1,
      format,
      type,
      pixels,
      pixels_size,
      padding,
      TextureManager::DoTexSubImageArguments::CommandType::kTexSubImage2D};
  texture_manager()->ValidateAndDoTexSubImage(
      this, &texture_state_, &state_, error_state_.get(), &framebuffer_state_,
      func_name, args);

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexSubImage3D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;

  const char* func_name = "glTexSubImage3D";
  const volatile gles2::cmds::TexSubImage3D& c =
      *static_cast<const volatile gles2::cmds::TexSubImage3D*>(cmd_data);
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::HandleTexSubImage3D",
      "widthXheight", c.width * c.height, "depth", c.depth);
  GLboolean internal = static_cast<GLboolean>(c.internal);
  if (internal == GL_TRUE && texture_state_.tex_image_failed)
    return error::kNoError;

  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = static_cast<uint32_t>(c.pixels_shm_id);
  uint32_t pixels_shm_offset = static_cast<uint32_t>(c.pixels_shm_offset);

  if (width < 0 || height < 0 || depth < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }

  PixelStoreParams params;
  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (pixels_shm_id)
      return error::kInvalidArguments;
    if (buffer->GetMappedRange()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "pixel unpack buffer should not be mapped to client memory");
      return error::kNoError;
    }
    params = state_.GetUnpackParams(ContextState::k3D);
  } else {
    if (!pixels_shm_id && pixels_shm_offset)
      return error::kInvalidArguments;
    // When reading from client buffer, the command buffer client side took
    // the responsibility to take the pixels from the client buffer and
    // unpack them according to the full ES3 pack parameters as source, all
    // parameters for 0 (except for alignment) as destination mem for the
    // service side.
    params.alignment = state_.unpack_alignment;
  }
  uint32_t pixels_size;
  uint32_t skip_size;
  uint32_t padding;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, depth,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           nullptr,
                                           nullptr,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }
  DCHECK_EQ(0u, skip_size);

  const void* pixels;
  if (pixels_shm_id) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels)
      return error::kOutOfBounds;
  } else {
    DCHECK(buffer || !pixels_shm_offset);
    pixels = reinterpret_cast<const void*>(pixels_shm_offset);
  }

  TextureManager::DoTexSubImageArguments args = {
      target,
      level,
      xoffset,
      yoffset,
      zoffset,
      width,
      height,
      depth,
      format,
      type,
      pixels,
      pixels_size,
      padding,
      TextureManager::DoTexSubImageArguments::CommandType::kTexSubImage3D};
  texture_manager()->ValidateAndDoTexSubImage(
      this, &texture_state_, &state_, error_state_.get(), &framebuffer_state_,
      func_name, args);

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribPointerv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetVertexAttribPointerv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribPointerv*>(
          cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetVertexAttribPointerv::Result Result;
  Result* result =
      GetSharedMemoryAs<Result*>(c.pointer_shm_id, c.pointer_shm_offset,
                                 Result::ComputeSize(1).ValueOrDie());
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  if (!validators_->vertex_pointer.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glGetVertexAttribPointerv", pname, "pname");
    return error::kNoError;
  }
  if (index >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetVertexAttribPointerv", "index out of range.");
    return error::kNoError;
  }
  result->SetNumResults(1);
  *result->GetData() =
      state_.vertex_attrib_manager->GetVertexAttrib(index)->offset();
  return error::kNoError;
}

template <class T>
bool GLES2DecoderImpl::GetUniformSetup(GLuint program_id,
                                       GLint fake_location,
                                       uint32_t shm_id,
                                       uint32_t shm_offset,
                                       error::Error* error,
                                       GLint* real_location,
                                       GLuint* service_id,
                                       SizedResult<T>** result_pointer,
                                       GLenum* result_type,
                                       GLsizei* result_size) {
  DCHECK(error);
  DCHECK(service_id);
  DCHECK(result_pointer);
  DCHECK(result_type);
  DCHECK(result_size);
  DCHECK(real_location);
  *error = error::kNoError;
  // Make sure we have enough room for the result on failure.
  SizedResult<T>* result;
  result = GetSharedMemoryAs<SizedResult<T>*>(
      shm_id, shm_offset, SizedResult<T>::ComputeSize(0).ValueOrDie());
  if (!result) {
    *error = error::kOutOfBounds;
    return false;
  }
  *result_pointer = result;
  // Set the result size to 0 so the client does not have to check for success.
  result->SetNumResults(0);
  Program* program = GetProgramInfoNotShader(program_id, "glGetUniform");
  if (!program) {
    return false;
  }
  if (!program->IsValid()) {
    // Program was not linked successfully. (ie, glLinkProgram)
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetUniform", "program not linked");
    return false;
  }
  *service_id = program->service_id();
  GLint array_index = -1;
  const Program::UniformInfo* uniform_info =
      program->GetUniformInfoByFakeLocation(
          fake_location, real_location, &array_index);
  if (!uniform_info) {
    // No such location.
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetUniform", "unknown location");
    return false;
  }
  GLenum type = uniform_info->type;
  uint32_t num_elements = GLES2Util::GetElementCountForUniformType(type);
  if (num_elements == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glGetUniform", "unknown type");
    return false;
  }
  uint32_t checked_size = 0;
  if (!SizedResult<T>::ComputeSize(num_elements).AssignIfValid(&checked_size)) {
    *error = error::kOutOfBounds;
    return false;
  }
  result = GetSharedMemoryAs<SizedResult<T>*>(shm_id, shm_offset, checked_size);
  if (!result) {
    *error = error::kOutOfBounds;
    return false;
  }
  result->SetNumResults(num_elements);
  *result_size = num_elements * sizeof(T);
  *result_type = type;
  return true;
}

error::Error GLES2DecoderImpl::HandleGetUniformiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetUniformiv& c =
      *static_cast<const volatile gles2::cmds::GetUniformiv*>(cmd_data);
  GLuint program = c.program;
  GLint fake_location = c.location;
  GLuint service_id;
  GLenum result_type;
  GLsizei result_size;
  GLint real_location = -1;
  Error error;
  cmds::GetUniformiv::Result* result;
  if (GetUniformSetup<GLint>(program, fake_location, c.params_shm_id,
                             c.params_shm_offset, &error, &real_location,
                             &service_id, &result, &result_type,
                             &result_size)) {
    api()->glGetUniformivFn(service_id, real_location, result->GetData());
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetUniformuiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;

  const volatile gles2::cmds::GetUniformuiv& c =
      *static_cast<const volatile gles2::cmds::GetUniformuiv*>(cmd_data);
  GLuint program = c.program;
  GLint fake_location = c.location;
  GLuint service_id;
  GLenum result_type;
  GLsizei result_size;
  GLint real_location = -1;
  Error error;
  cmds::GetUniformuiv::Result* result;
  if (GetUniformSetup<GLuint>(program, fake_location, c.params_shm_id,
                              c.params_shm_offset, &error, &real_location,
                              &service_id, &result, &result_type,
                              &result_size)) {
    api()->glGetUniformuivFn(service_id, real_location, result->GetData());
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetUniformfv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetUniformfv& c =
      *static_cast<const volatile gles2::cmds::GetUniformfv*>(cmd_data);
  GLuint program = c.program;
  GLint fake_location = c.location;
  GLuint service_id;
  GLint real_location = -1;
  Error error;
  cmds::GetUniformfv::Result* result;
  GLenum result_type;
  GLsizei result_size;
  if (GetUniformSetup<GLfloat>(program, fake_location, c.params_shm_id,
                               c.params_shm_offset, &error, &real_location,
                               &service_id, &result, &result_type,
                               &result_size)) {
    if (result_type == GL_BOOL || result_type == GL_BOOL_VEC2 ||
        result_type == GL_BOOL_VEC3 || result_type == GL_BOOL_VEC4) {
      GLsizei num_values = result_size / sizeof(GLfloat);
      auto temp = base::HeapArray<GLint>::Uninit(num_values);
      api()->glGetUniformivFn(service_id, real_location, temp.data());
      GLfloat* dst = result->GetData();
      for (GLsizei ii = 0; ii < num_values; ++ii) {
        dst[ii] = (temp[ii] != 0);
      }
    } else {
      api()->glGetUniformfvFn(service_id, real_location, result->GetData());
    }
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetShaderPrecisionFormat(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderPrecisionFormat& c =
      *static_cast<const volatile gles2::cmds::GetShaderPrecisionFormat*>(
          cmd_data);
  GLenum shader_type = static_cast<GLenum>(c.shadertype);
  GLenum precision_type = static_cast<GLenum>(c.precisiontype);
  typedef cmds::GetShaderPrecisionFormat::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  if (!validators_->shader_type.IsValid(shader_type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glGetShaderPrecisionFormat", shader_type, "shader_type");
    return error::kNoError;
  }
  if (!validators_->shader_precision.IsValid(precision_type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glGetShaderPrecisionFormat", precision_type, "precision_type");
    return error::kNoError;
  }

  result->success = 1;  // true

  GLint range[2] = { 0, 0 };
  GLint precision = 0;
  QueryShaderPrecisionFormat(shader_type, precision_type, range, &precision);

  result->min_range = range[0];
  result->max_range = range[1];
  result->precision = precision;

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetAttachedShaders(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetAttachedShaders& c =
      *static_cast<const volatile gles2::cmds::GetAttachedShaders*>(cmd_data);
  uint32_t result_size = c.result_size;
  GLuint program_id = static_cast<GLuint>(c.program);
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetAttachedShaders");
  if (!program) {
    return error::kNoError;
  }
  typedef cmds::GetAttachedShaders::Result Result;
  uint32_t max_count = Result::ComputeMaxResults(result_size);
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(max_count).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, checked_size);
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  api()->glGetAttachedShadersFn(program->service_id(), max_count, &count,
                                result->GetData());
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (!shader_manager()->GetClientId(result->GetData()[ii],
                                       &result->GetData()[ii])) {
      NOTREACHED_IN_MIGRATION();
      return error::kGenericError;
    }
  }
  result->SetNumResults(count);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveUniform(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetActiveUniform& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniform*>(cmd_data);
  GLuint program_id = c.program;
  GLuint index = c.index;
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveUniform::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveUniform");
  if (!program) {
    return error::kNoError;
  }
  const Program::UniformInfo* uniform_info =
      program->GetUniformInfo(index);
  if (!uniform_info) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetActiveUniform", "index out of range");
    return error::kNoError;
  }
  result->success = 1;  // true.
  result->size = uniform_info->size;
  result->type = uniform_info->type;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(uniform_info->name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveUniformBlockiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetActiveUniformBlockiv& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniformBlockiv*>(
          cmd_data);
  GLuint program_id = c.program;
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveUniformBlockiv");
  if (!program) {
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  api()->glGetProgramivFn(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetActiveActiveUniformBlockiv", "program not linked");
    return error::kNoError;
  }
  if (index >= program->uniform_block_size_info().size()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glGetActiveUniformBlockiv",
                       "uniformBlockIndex >= active uniform blocks");
    return error::kNoError;
  }
  GLsizei num_values = 1;
  if (pname == GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES) {
    GLint num = 0;
    api()->glGetActiveUniformBlockivFn(service_id, index,
                                       GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &num);
    GLenum error = api()->glGetErrorFn();
    if (error != GL_NO_ERROR) {
      // Assume this will the same error if calling with pname.
      LOCAL_SET_GL_ERROR(error, "GetActiveUniformBlockiv", "");
      return error::kNoError;
    }
    num_values = static_cast<GLsizei>(num);
  }
  typedef cmds::GetActiveUniformBlockiv::Result Result;
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  api()->glGetActiveUniformBlockivFn(service_id, index, pname, params);
  result->SetNumResults(num_values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveUniformBlockName(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetActiveUniformBlockName& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniformBlockName*>(
          cmd_data);
  GLuint program_id = c.program;
  GLuint index = c.index;
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveUniformBlockName::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveUniformBlockName");
  if (!program) {
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  api()->glGetProgramivFn(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetActiveActiveUniformBlockName", "program not linked");
    return error::kNoError;
  }
  if (index >= program->uniform_block_size_info().size()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glGetActiveUniformBlockName",
                       "uniformBlockIndex >= active uniform blocks");
    return error::kNoError;
  }
  GLint max_length = 0;
  api()->glGetProgramivFn(service_id, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
                          &max_length);
  // Increase one so &buffer[0] is always valid.
  GLsizei buf_size = static_cast<GLsizei>(max_length) + 1;
  std::vector<char> buffer(buf_size);
  GLsizei length = 0;
  api()->glGetActiveUniformBlockNameFn(service_id, index, buf_size, &length,
                                       &buffer[0]);
  if (length == 0) {
    *result = 0;
    return error::kNoError;
  }
  *result = 1;
  Bucket* bucket = CreateBucket(name_bucket_id);
  DCHECK_GT(buf_size, length);
  DCHECK_EQ(0, buffer[length]);
  bucket->SetFromString(&buffer[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveUniformsiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetActiveUniformsiv& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniformsiv*>(cmd_data);
  GLuint program_id = c.program;
  GLenum pname = static_cast<GLenum>(c.pname);
  Bucket* bucket = GetBucket(c.indices_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  if (!validators_->uniform_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetActiveUniformsiv", pname, "pname");
    return error::kNoError;
  }
  GLsizei count = static_cast<GLsizei>(bucket->size() / sizeof(GLuint));
  const GLuint* indices = bucket->GetDataAs<const GLuint*>(0, bucket->size());
  typedef cmds::GetActiveUniformsiv::Result Result;
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(count).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveUniformsiv");
  if (!program) {
    return error::kNoError;
  }
  GLint activeUniforms = 0;
  program->GetProgramiv(GL_ACTIVE_UNIFORMS, &activeUniforms);
  for (int i = 0; i < count; i++) {
    if (indices[i] >= static_cast<GLuint>(activeUniforms)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
          "glGetActiveUniformsiv", "index >= active uniforms");
      return error::kNoError;
    }
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  api()->glGetProgramivFn(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetActiveUniformsiv", "program not linked");
    return error::kNoError;
  }
  api()->glGetActiveUniformsivFn(service_id, count, indices, pname, params);
  result->SetNumResults(count);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveAttrib(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetActiveAttrib& c =
      *static_cast<const volatile gles2::cmds::GetActiveAttrib*>(cmd_data);
  GLuint program_id = c.program;
  GLuint index = c.index;
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveAttrib::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveAttrib");
  if (!program) {
    return error::kNoError;
  }
  const Program::VertexAttrib* attrib_info =
      program->GetAttribInfo(index);
  if (!attrib_info) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetActiveAttrib", "index out of range");
    return error::kNoError;
  }
  result->success = 1;  // true.
  result->size = attrib_info->size;
  result->type = attrib_info->type;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(attrib_info->name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleShaderBinary(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
#if 1  // No binary shader support.
  LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glShaderBinary", "not supported");
  return error::kNoError;
#else
  GLsizei n = static_cast<GLsizei>(c.n);
  if (n < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glShaderBinary", "n < 0");
    return error::kNoError;
  }
  GLsizei length = static_cast<GLsizei>(c.length);
  if (length < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glShaderBinary", "length < 0");
    return error::kNoError;
  }
  uint32_t data_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* shaders = GetSharedMemoryAs<const GLuint*>(
      c.shaders_shm_id, c.shaders_shm_offset, data_size);
  GLenum binaryformat = static_cast<GLenum>(c.binaryformat);
  const void* binary = GetSharedMemoryAs<const void*>(
      c.binary_shm_id, c.binary_shm_offset, length);
  if (shaders == nullptr || binary == nullptr) {
    return error::kOutOfBounds;
  }
  auto service_ids = base::HeapArray<GLuint>::Uninit(n);
  for (GLsizei ii = 0; ii < n; ++ii) {
    Shader* shader = GetShader(shaders[ii]);
    if (!shader) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glShaderBinary", "unknown shader");
      return error::kNoError;
    }
    service_ids[ii] = shader->service_id();
  }
  // TODO(gman): call glShaderBinary
  return error::kNoError;
#endif
}

void GLES2DecoderImpl::DoSwapBuffers(uint64_t swap_id, GLbitfield flags) {
  bool is_offscreen = !!offscreen_target_frame_buffer_.get();

  int this_frame_number = frame_number_++;
  // TRACE_EVENT for gpu tests:
  TRACE_EVENT_INSTANT2(
      "test_gpu", "SwapBuffersLatency", TRACE_EVENT_SCOPE_THREAD, "GLImpl",
      static_cast<int>(gl::GetGLImplementation()), "width",
      (is_offscreen ? offscreen_size_.width() : surface_->GetSize().width()));
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoSwapBuffers",
               "offscreen", is_offscreen,
               "frame", this_frame_number);

  ScopedGPUTrace scoped_gpu_trace(gpu_tracer_.get(), kTraceDecoder,
                                  "GLES2Decoder", "SwapBuffer");

  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("gpu.debug"),
                                     &is_tracing);
  if (is_tracing) {
    ScopedFramebufferBinder binder(this, GetBoundDrawFramebufferServiceId());
    gpu_state_tracer_->TakeSnapshotWithCurrentFramebuffer(
        is_offscreen ? offscreen_size_ : surface_->GetSize());
  }

  if (is_offscreen) {
    // We don't support SwapBuffers on the offscreen contexts.
    LOG(ERROR) << "SwapBuffers called for the offscreen context";
    return;
  } else if (supports_async_swap_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "gpu", "AsyncSwapBuffers",
        TRACE_ID_WITH_SCOPE("AsyncSwapBuffers", swap_id));

    client()->OnSwapBuffers(swap_id, flags);
    surface_->SwapBuffersAsync(
        base::BindOnce(&GLES2DecoderImpl::FinishAsyncSwapBuffers,
                       weak_ptr_factory_.GetWeakPtr(), swap_id),
        base::DoNothing(), gfx::FrameData());
  } else {
    client()->OnSwapBuffers(swap_id, flags);
    FinishSwapBuffers(
        surface_->SwapBuffers(base::DoNothing(), gfx::FrameData()));
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

void GLES2DecoderImpl::FinishAsyncSwapBuffers(
    uint64_t swap_id,
    gfx::SwapCompletionResult result) {
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "gpu", "AsyncSwapBuffers",
      TRACE_ID_WITH_SCOPE("AsyncSwapBuffers", swap_id));
  FinishSwapBuffers(result.swap_result);
}

void GLES2DecoderImpl::FinishSwapBuffers(gfx::SwapResult result) {
  if (result == gfx::SwapResult::SWAP_FAILED) {
    // If SwapBuffers failed, we may not have a current context any more.
    LOG(ERROR) << "Context lost because SwapBuffers failed.";
    if (!context_->IsCurrent(surface_.get()) || !CheckResetStatus()) {
      MarkContextLost(error::kUnknown);
      group_->LoseContexts(error::kUnknown);
    }
  }
  ++swaps_since_resize_;
  if (swaps_since_resize_ == 1 && surface_->BuffersFlipped()) {
    // The second buffer after a resize is new and needs to be cleared to
    // known values.
    backbuffer_needs_clear_bits_ |= GL_COLOR_BUFFER_BIT;
  }
}

error::Error GLES2DecoderImpl::HandleEnableFeatureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::EnableFeatureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::EnableFeatureCHROMIUM*>(
          cmd_data);
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  typedef cmds::EnableFeatureCHROMIUM::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }

  // TODO(gman): make this some kind of table to function pointer thingy.
  if (feature_str.compare("pepper3d_allow_buffers_on_multiple_targets") == 0) {
    buffer_manager()->set_allow_buffers_on_multiple_targets(true);
  } else if (feature_str.compare("pepper3d_support_fixed_attribs") == 0) {
    buffer_manager()->set_allow_fixed_attribs(true);
    // TODO(gman): decide how to remove the need for this const_cast.
    // I could make validators_ non const but that seems bad as this is the only
    // place it is needed. I could make some special friend class of validators
    // just to allow this to set them. That seems silly. I could refactor this
    // code to use the extension mechanism or the initialization attributes to
    // turn this feature on. Given that the only real point of this is to make
    // the conformance tests pass and given that there is lots of real work that
    // needs to be done it seems like refactoring for one to one of those
    // methods is a very low priority.
    const_cast<Validators*>(validators_.get())
        ->vertex_attrib_type.AddValue(GL_FIXED);
  } else {
    return error::kNoError;
  }

  *result = 1;  // true.
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetRequestableExtensionsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetRequestableExtensionsCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::GetRequestableExtensionsCHROMIUM*>(
          cmd_data);
  Bucket* bucket = CreateBucket(c.bucket_id);
  scoped_refptr<FeatureInfo> info(
      new FeatureInfo(workarounds(), group_->gpu_feature_info()));
  DisallowedFeatures disallowed_features = feature_info_->disallowed_features();
  disallowed_features.AllowExtensions();
  info->Initialize(feature_info_->context_type(),
                   false /* is_passthrough_cmd_decoder */, disallowed_features);
  bucket->SetFromString(gfx::MakeExtensionString(info->extensions()).c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRequestExtensionCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::RequestExtensionCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::RequestExtensionCHROMIUM*>(
          cmd_data);
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }
  feature_str = feature_str + " ";

  bool desire_standard_derivatives = false;
  bool desire_fbo_render_mipmap = false;
  bool desire_frag_depth = false;
  bool desire_draw_buffers = false;
  bool desire_shader_texture_lod = false;
  bool desire_multi_draw = false;
  bool desire_draw_instanced_base_vertex_base_instance = false;
  bool desire_multi_draw_instanced_base_vertex_base_instance = false;
  bool desire_arb_texture_rectangle = false;
  bool desire_oes_egl_image_external = false;
  bool desire_nv_egl_stream_consumer_external = false;
  if (feature_info_->context_type() == CONTEXT_TYPE_WEBGL1) {
    desire_standard_derivatives =
        base::Contains(feature_str, "GL_OES_standard_derivatives ");
    desire_fbo_render_mipmap =
        base::Contains(feature_str, "GL_OES_fbo_render_mipmap ");
    desire_frag_depth = base::Contains(feature_str, "GL_EXT_frag_depth ");
    desire_draw_buffers = base::Contains(feature_str, "GL_EXT_draw_buffers ");
    desire_shader_texture_lod =
        base::Contains(feature_str, "GL_EXT_shader_texture_lod ");
  } else if (feature_info_->context_type() == CONTEXT_TYPE_WEBGL2) {
    desire_draw_instanced_base_vertex_base_instance = base::Contains(
        feature_str, "GL_WEBGL_draw_instanced_base_vertex_base_instance ");
    desire_multi_draw_instanced_base_vertex_base_instance = base::Contains(
        feature_str,
        "GL_WEBGL_multi_draw_instanced_base_vertex_base_instance ");
  }
  if (feature_info_->IsWebGLContext()) {
    desire_multi_draw = base::Contains(feature_str, "GL_WEBGL_multi_draw ");
    desire_arb_texture_rectangle =
        base::Contains(feature_str, "GL_ANGLE_texture_rectangle ");
    desire_oes_egl_image_external =
        base::Contains(feature_str, "GL_OES_EGL_image_external ");
    desire_nv_egl_stream_consumer_external =
        base::Contains(feature_str, "GL_NV_EGL_stream_consumer_external ");
  }
  if (desire_standard_derivatives != derivatives_explicitly_enabled_ ||
      desire_fbo_render_mipmap != fbo_render_mipmap_explicitly_enabled_ ||
      desire_frag_depth != frag_depth_explicitly_enabled_ ||
      desire_draw_buffers != draw_buffers_explicitly_enabled_ ||
      desire_shader_texture_lod != shader_texture_lod_explicitly_enabled_ ||
      desire_multi_draw != multi_draw_explicitly_enabled_ ||
      desire_draw_instanced_base_vertex_base_instance !=
          draw_instanced_base_vertex_base_instance_explicitly_enabled_ ||
      desire_multi_draw_instanced_base_vertex_base_instance !=
          multi_draw_instanced_base_vertex_base_instance_explicitly_enabled_ ||
      desire_arb_texture_rectangle != arb_texture_rectangle_enabled_ ||
      desire_oes_egl_image_external != oes_egl_image_external_enabled_ ||
      desire_nv_egl_stream_consumer_external !=
          nv_egl_stream_consumer_external_enabled_) {
    derivatives_explicitly_enabled_ |= desire_standard_derivatives;
    fbo_render_mipmap_explicitly_enabled_ |= desire_fbo_render_mipmap;
    frag_depth_explicitly_enabled_ |= desire_frag_depth;
    draw_buffers_explicitly_enabled_ |= desire_draw_buffers;
    shader_texture_lod_explicitly_enabled_ |= desire_shader_texture_lod;
    multi_draw_explicitly_enabled_ |= desire_multi_draw;
    draw_instanced_base_vertex_base_instance_explicitly_enabled_ |=
        desire_draw_instanced_base_vertex_base_instance;
    multi_draw_instanced_base_vertex_base_instance_explicitly_enabled_ |=
        desire_multi_draw_instanced_base_vertex_base_instance;
    arb_texture_rectangle_enabled_ |= desire_arb_texture_rectangle;
    oes_egl_image_external_enabled_ |= desire_oes_egl_image_external;
    nv_egl_stream_consumer_external_enabled_ |=
        desire_nv_egl_stream_consumer_external;
    DestroyShaderTranslator();
  }

  if (base::Contains(feature_str, "GL_CHROMIUM_color_buffer_float_rgba ")) {
    feature_info_->EnableCHROMIUMColorBufferFloatRGBA();
  }
  if (base::Contains(feature_str, "GL_CHROMIUM_color_buffer_float_rgb ")) {
    feature_info_->EnableCHROMIUMColorBufferFloatRGB();
  }
  if (base::Contains(feature_str, "GL_EXT_color_buffer_float ")) {
    feature_info_->EnableEXTColorBufferFloat();
  }
  if (base::Contains(feature_str, "GL_EXT_color_buffer_half_float ")) {
    feature_info_->EnableEXTColorBufferHalfFloat();
  }
  if (base::Contains(feature_str, "GL_EXT_texture_filter_anisotropic ")) {
    feature_info_->EnableEXTTextureFilterAnisotropic();
  }
  if (base::Contains(feature_str, "GL_OES_texture_float_linear ")) {
    feature_info_->EnableOESTextureFloatLinear();
  }
  if (base::Contains(feature_str, "GL_OES_texture_half_float_linear ")) {
    feature_info_->EnableOESTextureHalfFloatLinear();
  }
  if (base::Contains(feature_str, "GL_EXT_float_blend ")) {
    feature_info_->EnableEXTFloatBlend();
  }
  if (base::Contains(feature_str, "GL_OES_fbo_render_mipmap ")) {
    feature_info_->EnableOESFboRenderMipmap();
  }

  UpdateCapabilities();

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramInfoCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetProgramInfoCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetProgramInfoCHROMIUM*>(
          cmd_data);
  GLuint program_id = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(ProgramInfoHeader));  // in case we fail.
  Program* program = nullptr;
  program = GetProgram(program_id);
  if (!program || !program->IsValid()) {
    return error::kNoError;
  }
  program->GetProgramInfo(program_manager(), bucket);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformBlocksCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetUniformBlocksCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetUniformBlocksCHROMIUM*>(
          cmd_data);
  GLuint program_id = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(UniformBlocksHeader));  // in case we fail.
  Program* program = nullptr;
  program = GetProgram(program_id);
  if (!program || !program->IsValid()) {
    return error::kNoError;
  }
  program->GetUniformBlocks(bucket);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformsES3CHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetUniformsES3CHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetUniformsES3CHROMIUM*>(
          cmd_data);
  GLuint program_id = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(UniformsES3Header));  // in case we fail.
  Program* program = nullptr;
  program = GetProgram(program_id);
  if (!program || !program->IsValid()) {
    return error::kNoError;
  }
  program->GetUniformsES3(bucket);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTransformFeedbackVarying(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetTransformFeedbackVarying& c =
      *static_cast<const volatile gles2::cmds::GetTransformFeedbackVarying*>(
          cmd_data);
  GLuint program_id = c.program;
  GLuint index = c.index;
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetTransformFeedbackVarying::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetTransformFeedbackVarying");
  if (!program) {
    // An error is already set.
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  api()->glGetProgramivFn(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetTransformFeedbackVarying", "program not linked");
    return error::kNoError;
  }
  GLint num_varyings = 0;
  api()->glGetProgramivFn(service_id, GL_TRANSFORM_FEEDBACK_VARYINGS,
                          &num_varyings);
  if (index >= static_cast<GLuint>(num_varyings)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
        "glGetTransformFeedbackVarying", "index out of bounds");
    return error::kNoError;
  }
  GLint max_length = 0;
  api()->glGetProgramivFn(service_id, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH,
                          &max_length);
  max_length = std::max(1, max_length);
  std::vector<char> buffer(max_length);
  GLsizei length = 0;
  GLsizei size = 0;
  GLenum type = 0;
  api()->glGetTransformFeedbackVaryingFn(service_id, index, max_length, &length,
                                         &size, &type, &buffer[0]);
  result->success = 1;  // true.
  result->size = static_cast<int32_t>(size);
  result->type = static_cast<uint32_t>(type);
  Bucket* bucket = CreateBucket(name_bucket_id);
  DCHECK(length >= 0 && length < max_length);
  buffer[length] = '\0';  // Just to be safe.
  bucket->SetFromString(&buffer[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTransformFeedbackVaryingsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM*>(
          cmd_data);
  GLuint program_id = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(TransformFeedbackVaryingsHeader));  // in case we fail.
  Program* program = nullptr;
  program = GetProgram(program_id);
  if (!program || !program->IsValid()) {
    return error::kNoError;
  }
  program->GetTransformFeedbackVaryings(bucket);
  return error::kNoError;
}

bool GLES2DecoderImpl::WasContextLost() const {
  return context_was_lost_;
}

bool GLES2DecoderImpl::WasContextLostByRobustnessExtension() const {
  return WasContextLost() && reset_by_robustness_extension_;
}

void GLES2DecoderImpl::MarkContextLost(error::ContextLostReason reason) {
  // Only lose the context once.
  if (WasContextLost())
    return;

  // Don't make GL calls in here, the context might not be current.
  command_buffer_service()->SetContextLostReason(reason);
  current_decoder_error_ = error::kLostContext;
  context_was_lost_ = true;

  if (transform_feedback_manager_.get()) {
    transform_feedback_manager_->MarkContextLost();
  }
  if (vertex_array_manager_.get()) {
    vertex_array_manager_->MarkContextLost();
  }
  state_.MarkContextLost();
}

bool GLES2DecoderImpl::CheckResetStatus() {
  DCHECK(!WasContextLost());
  DCHECK(context_->IsCurrent(nullptr));

  // If the reason for the call was a GL error, we can try to determine the
  // reset status more accurately.
  GLenum driver_status = context_->CheckStickyGraphicsResetStatus();
  if (driver_status == GL_NO_ERROR)
    return false;

  LOG(ERROR) << (surface_->IsOffscreen() ? "Offscreen" : "Onscreen")
             << " context lost via ARB/EXT_robustness. Reset status = "
             << GLES2Util::GetStringEnum(driver_status);

  switch (driver_status) {
    case GL_GUILTY_CONTEXT_RESET_ARB:
      MarkContextLost(error::kGuilty);
      break;
    case GL_INNOCENT_CONTEXT_RESET_ARB:
      MarkContextLost(error::kInnocent);
      break;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      MarkContextLost(error::kUnknown);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
  reset_by_robustness_extension_ = true;
  return true;
}

error::Error GLES2DecoderImpl::HandleDescheduleUntilFinishedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!gl::GLFence::IsSupported())
    return error::kNoError;
  std::unique_ptr<gl::GLFence> fence = gl::GLFence::Create();
  if (fence)
    deschedule_until_finished_fences_.push_back(std::move(fence));

  if (deschedule_until_finished_fences_.size() <= 1)
    return error::kNoError;

  DCHECK_EQ(2u, deschedule_until_finished_fences_.size());
  if (deschedule_until_finished_fences_[0]->HasCompleted()) {
    deschedule_until_finished_fences_.erase(
        deschedule_until_finished_fences_.begin());
    return error::kNoError;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "cc", "GLES2DecoderImpl::DescheduleUntilFinished", TRACE_ID_LOCAL(this));
  client()->OnDescheduleUntilFinished();
  return error::kDeferLaterCommands;
}

bool GLES2DecoderImpl::GenQueriesEXTHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (query_manager_->IsValidQuery(client_ids[ii])) {
      return false;
    }
  }
  query_manager_->GenQueries(n, client_ids);
  return true;
}

void GLES2DecoderImpl::DeleteQueriesEXTHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    query_manager_->RemoveQuery(client_id);
  }
}

void GLES2DecoderImpl::SetQueryCallback(unsigned int query_client_id,
                                        base::OnceClosure callback) {
  QueryManager::Query* query = query_manager_->GetQuery(query_client_id);
  if (query) {
    query->AddCallback(std::move(callback));
  } else {
    VLOG(1) << "GLES2DecoderImpl::SetQueryCallback: No query with ID "
            << query_client_id << ". Running the callback immediately.";
    std::move(callback).Run();
  }
}

void GLES2DecoderImpl::CancelAllQueries() {
  query_manager_->RemoveAllQueries();
}

bool GLES2DecoderImpl::HasPendingQueries() const {
  return query_manager_.get() && query_manager_->HavePendingQueries();
}

void GLES2DecoderImpl::ProcessPendingQueries(bool did_finish) {
  if (!query_manager_.get())
    return;
  query_manager_->ProcessPendingQueries(did_finish);
}

// Note that if there are no pending readpixels right now,
// this function will call the callback immediately.
void GLES2DecoderImpl::WaitForReadPixels(base::OnceClosure callback) {
  if (features().use_async_readpixels && !pending_readpixel_fences_.empty()) {
    pending_readpixel_fences_.back().callbacks.push_back(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void GLES2DecoderImpl::ProcessPendingReadPixels(bool did_finish) {
  // Note: |did_finish| guarantees that the GPU has passed the fence but
  // we cannot assume that GLFence::HasCompleted() will return true yet as
  // that's not guaranteed by all GLFence implementations.
  while (!pending_readpixel_fences_.empty() &&
         (did_finish ||
          pending_readpixel_fences_.front().fence->HasCompleted())) {
    std::vector<base::OnceClosure> callbacks =
        std::move(pending_readpixel_fences_.front().callbacks);
    pending_readpixel_fences_.pop();
    for (size_t i = 0; i < callbacks.size(); i++) {
      std::move(callbacks[i]).Run();
    }
  }
}

void GLES2DecoderImpl::ProcessDescheduleUntilFinished() {
  if (deschedule_until_finished_fences_.size() < 2)
    return;
  DCHECK_EQ(2u, deschedule_until_finished_fences_.size());

  if (!deschedule_until_finished_fences_[0]->HasCompleted())
    return;

  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "cc", "GLES2DecoderImpl::DescheduleUntilFinished", TRACE_ID_LOCAL(this));
  deschedule_until_finished_fences_.erase(
      deschedule_until_finished_fences_.begin());
  client()->OnRescheduleAfterFinished();
}

bool GLES2DecoderImpl::HasMoreIdleWork() const {
  bool has_more_idle_work =
      !pending_readpixel_fences_.empty() || gpu_tracer_->HasTracesToProcess();
  return has_more_idle_work;
}

void GLES2DecoderImpl::PerformIdleWork() {
  gpu_tracer_->ProcessTraces();
  ProcessPendingReadPixels(false);
}

bool GLES2DecoderImpl::HasPollingWork() const {
  return deschedule_until_finished_fences_.size() >= 2;
}

void GLES2DecoderImpl::PerformPollingWork() {
  ProcessDescheduleUntilFinished();
}

error::Error GLES2DecoderImpl::HandleBeginQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BeginQueryEXT& c =
      *static_cast<const volatile gles2::cmds::BeginQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint client_id = static_cast<GLuint>(c.id);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);

  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
    case GL_GET_ERROR_QUERY_CHROMIUM:
      break;
    case GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM:
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      if (!features().chromium_sync_query) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, "glBeginQueryEXT",
            "not enabled for commands completed queries");
        return error::kNoError;
      }
      break;
    case GL_PROGRAM_COMPLETION_QUERY_CHROMIUM:
      if (!features().chromium_completion_query) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                           "not enabled for program completion queries");
        return error::kNoError;
      }
      break;
    case GL_SAMPLES_PASSED_ARB:
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                         "not enabled for boolean occlusion queries");
      return error::kNoError;
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
      if (!features().occlusion_query_boolean) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                           "not enabled for boolean occlusion queries");
        return error::kNoError;
      }
      break;
    case GL_TIME_ELAPSED:
      if (!query_manager_->GPUTimingAvailable()) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, "glBeginQueryEXT",
            "not enabled for timing queries");
        return error::kNoError;
      }
      break;
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
      if (feature_info_->IsWebGL2OrES3Context()) {
        break;
      }
      [[fallthrough]];
    default:
      LOCAL_SET_GL_ERROR(
          GL_INVALID_ENUM, "glBeginQueryEXT",
          "unknown query target");
      return error::kNoError;
  }

  if (query_manager_->GetActiveQuery(target)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glBeginQueryEXT", "query already in progress");
    return error::kNoError;
  }

  if (client_id == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT", "id is 0");
    return error::kNoError;
  }

  scoped_refptr<gpu::Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  QueryManager::Query* query = query_manager_->GetQuery(client_id);
  if (!query) {
    if (!query_manager_->IsValidQuery(client_id)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                         "glBeginQueryEXT",
                         "id not made by glGenQueriesEXT");
      return error::kNoError;
    }

    query =
        query_manager_->CreateQuery(target, client_id, std::move(buffer), sync);
  } else {
    if (query->target() != target) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                         "target does not match");
      return error::kNoError;
    } else if (query->sync() != sync) {
      DLOG(ERROR) << "Shared memory used by query not the same as before";
      return error::kInvalidArguments;
    }
  }

  query_manager_->BeginQuery(query);
  return error::kNoError;
}

void GLES2DecoderImpl::ReadBackBuffersIntoShadowCopies(
    base::flat_set<scoped_refptr<Buffer>> buffers_to_shadow_copy) {
  GLuint old_binding =
      state_.bound_array_buffer ? state_.bound_array_buffer->service_id() : 0;

  for (scoped_refptr<Buffer>& buffer : buffers_to_shadow_copy) {
    if (buffer->IsDeleted()) {
      continue;
    }
    void* shadow = nullptr;
    scoped_refptr<gpu::Buffer> gpu_buffer =
        buffer->TakeReadbackShadowAllocation(&shadow);
    if (!shadow) {
      continue;
    }

    if (buffer->GetMappedRange()) {
      // The buffer is already mapped by the client. It's okay that the shadow
      // copy will be out-of-date, because the client will never read it:
      // * Client issues READBACK_SHADOW_COPIES_UPDATED_CHROMIUM query
      // * Client maps buffer
      // * Client receives signal that the query completed
      // * Client unmaps buffer - invalidating the shadow copy
      // * Client maps buffer to read back - hits the round-trip path
      continue;
    }

    api()->glBindBufferFn(GL_ARRAY_BUFFER, buffer->service_id());
    void* mapped = api()->glMapBufferRangeFn(GL_ARRAY_BUFFER, 0, buffer->size(),
                                             GL_MAP_READ_BIT);
    if (!mapped) {
      DLOG(ERROR) << "glMapBufferRange unexpectedly returned nullptr";
      MarkContextLost(error::kOutOfMemory);
      group_->LoseContexts(error::kUnknown);
      return;
    }
    memcpy(shadow, mapped, buffer->size());
    bool unmap_ok = api()->glUnmapBufferFn(GL_ARRAY_BUFFER);
    if (unmap_ok == GL_FALSE) {
      DLOG(ERROR) << "glUnmapBuffer unexpectedly returned GL_FALSE";
      MarkContextLost(error::kUnknown);
      group_->LoseContexts(error::kUnknown);
      return;
    }
  }

  // Restore original GL_ARRAY_BUFFER binding
  api()->glBindBufferFn(GL_ARRAY_BUFFER, old_binding);
}

error::Error GLES2DecoderImpl::HandleEndQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::EndQueryEXT& c =
      *static_cast<const volatile gles2::cmds::EndQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  QueryManager::Query* query = query_manager_->GetActiveQuery(target);
  if (!query) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glEndQueryEXT", "No active query");
    return error::kNoError;
  }

  if (target == GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM &&
      !writes_submitted_but_not_completed_.empty()) {
    // - This callback will be called immediately by MarkAsCompleted, so it's
    //   okay that it's called after UnmarkAsPending. (It's guaranteed to not be
    //   called immediately here, because the query has not even ended yet.)
    //
    // - It's okay to capture Unretained(this) because the callback is called by
    //   the query, which is owned by query_manager_, which is owned by `this`.
    query->AddCallback(
        base::BindOnce(&GLES2DecoderImpl::ReadBackBuffersIntoShadowCopies,
                       base::Unretained(this),
                       std::move(writes_submitted_but_not_completed_)));
    writes_submitted_but_not_completed_.clear();
  }

  query_manager_->EndQuery(query, submit_count);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleQueryCounterEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::QueryCounterEXT& c =
      *static_cast<const volatile gles2::cmds::QueryCounterEXT*>(cmd_data);
  GLuint client_id = static_cast<GLuint>(c.id);
  GLenum target = static_cast<GLenum>(c.target);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  switch (target) {
    case GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM:
      break;
    case GL_TIMESTAMP:
      if (!query_manager_->GPUTimingAvailable()) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, "glQueryCounterEXT",
            "not enabled for timing queries");
        return error::kNoError;
      }
      break;
    default:
      LOCAL_SET_GL_ERROR(
          GL_INVALID_ENUM, "glQueryCounterEXT",
          "unknown query target");
      return error::kNoError;
  }

  scoped_refptr<gpu::Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  QueryManager::Query* query = query_manager_->GetQuery(client_id);
  if (!query) {
    if (!query_manager_->IsValidQuery(client_id)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                         "glQueryCounterEXT",
                         "id not made by glGenQueriesEXT");
      return error::kNoError;
    }
    query =
        query_manager_->CreateQuery(target, client_id, std::move(buffer), sync);
  } else {
    if (query->target() != target) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glQueryCounterEXT",
                         "target does not match");
      return error::kNoError;
    } else if (query->sync() != sync) {
      DLOG(ERROR) << "Shared memory used by query not the same as before";
      return error::kInvalidArguments;
    }
  }
  query_manager_->QueryCounter(query, submit_count);

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSetDisjointValueSyncCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SetDisjointValueSyncCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::SetDisjointValueSyncCHROMIUM*>(
          cmd_data);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);

  return query_manager_->SetDisjointSync(sync_shm_id, sync_shm_offset);
}

bool GLES2DecoderImpl::GenVertexArraysOESHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetVertexAttribManager(client_ids[ii])) {
      return false;
    }
  }

  if (!features().native_vertex_array_object) {
    // Emulated VAO
    for (GLsizei ii = 0; ii < n; ++ii) {
      CreateVertexAttribManager(client_ids[ii], 0, true);
    }
  } else {
    auto service_ids = base::HeapArray<GLuint>::Uninit(n);

    api()->glGenVertexArraysOESFn(n, service_ids.data());
    for (GLsizei ii = 0; ii < n; ++ii) {
      CreateVertexAttribManager(client_ids[ii], service_ids[ii], true);
    }
  }

  return true;
}

void GLES2DecoderImpl::DeleteVertexArraysOESHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    VertexAttribManager* vao = GetVertexAttribManager(client_id);
    if (vao && !vao->IsDeleted()) {
      if (state_.vertex_attrib_manager.get() == vao) {
        DoBindVertexArrayOES(0);
      }
      RemoveVertexAttribManager(client_id);
    }
  }
}

void GLES2DecoderImpl::DoBindVertexArrayOES(GLuint client_id) {
  VertexAttribManager* vao = nullptr;
  if (client_id != 0) {
    vao = GetVertexAttribManager(client_id);
    if (!vao) {
      // Unlike most Bind* methods, the spec explicitly states that VertexArray
      // only allows names that have been previously generated. As such, we do
      // not generate new names here.
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "glBindVertexArrayOES", "bad vertex array id.");
      current_decoder_error_ = error::kNoError;
      return;
    }
  } else {
    vao = state_.default_vertex_attrib_manager.get();
  }

  // Only set the VAO state if it's changed
  if (state_.vertex_attrib_manager.get() != vao) {
    if (state_.vertex_attrib_manager)
      state_.vertex_attrib_manager->SetIsBound(false);
    state_.vertex_attrib_manager = vao;
    if (vao)
      vao->SetIsBound(true);
    if (!features().native_vertex_array_object) {
      EmulateVertexArrayState();
    } else {
      GLuint service_id = vao->service_id();
      api()->glBindVertexArrayOESFn(service_id);
    }
  }
}

// Used when OES_vertex_array_object isn't natively supported
void GLES2DecoderImpl::EmulateVertexArrayState() {
  // Setup the Vertex attribute state
  for (uint32_t vv = 0; vv < group_->max_vertex_attribs(); ++vv) {
    RestoreStateForAttrib(vv, true);
  }

  // Setup the element buffer
  Buffer* element_array_buffer =
      state_.vertex_attrib_manager->element_array_buffer();
  api()->glBindBufferFn(
      GL_ELEMENT_ARRAY_BUFFER,
      element_array_buffer ? element_array_buffer->service_id() : 0);
}

bool GLES2DecoderImpl::DoIsVertexArrayOES(GLuint client_id) {
  const VertexAttribManager* vao =
      GetVertexAttribManager(client_id);
  return vao && vao->IsValid() && !vao->IsDeleted();
}

bool GLES2DecoderImpl::DoIsSync(GLuint client_id) {
  GLsync service_sync = 0;
  return group_->GetSyncServiceId(client_id, &service_sync);
}

bool GLES2DecoderImpl::ValidateCopyTextureCHROMIUMTextures(
    const char* function_name,
    GLenum dest_target,
    TextureRef* source_texture_ref,
    TextureRef* dest_texture_ref) {
  if (!source_texture_ref || !dest_texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "unknown texture id");
    return false;
  }

  Texture* source_texture = source_texture_ref->texture();
  Texture* dest_texture = dest_texture_ref->texture();
  if (source_texture == dest_texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "source and destination textures are the same");
    return false;
  }

  if (dest_texture->target() !=
      GLES2Util::GLFaceTargetToTextureTarget(dest_target)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "target should be aligned with dest target");
    return false;
  }
  switch (dest_texture->target()) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_RECTANGLE_ARB:
      break;
    default:
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "invalid dest texture target binding");
    return false;
  }

  switch (source_texture->target()) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_RECTANGLE_ARB:
    case GL_TEXTURE_EXTERNAL_OES:
      break;
    default:
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "invalid source texture target binding");
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::CanUseCopyTextureCHROMIUMInternalFormat(
    GLenum dest_internal_format) {
  switch (dest_internal_format) {
    case GL_RGB:
    case GL_RGBA:
    case GL_RGB8:
    case GL_RGBA8:
    case GL_BGRA_EXT:
    case GL_BGRA8_EXT:
    case GL_SRGB_EXT:
    case GL_SRGB_ALPHA_EXT:
    case GL_R8:
    case GL_R8UI:
    case GL_RG8:
    case GL_RG8UI:
    case GL_SRGB8:
    case GL_RGB565:
    case GL_RGB8UI:
    case GL_SRGB8_ALPHA8:
    case GL_RGB5_A1:
    case GL_RGBA4:
    case GL_RGBA8UI:
    case GL_RGB9_E5:
    case GL_R16F:
    case GL_R32F:
    case GL_RG16F:
    case GL_RG32F:
    case GL_RGB16F:
    case GL_RGB32F:
    case GL_RGBA16F:
    case GL_RGBA32F:
    case GL_R11F_G11F_B10F:
      return true;
    default:
      return false;
  }
}

void GLES2DecoderImpl::DoCopyTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum internal_format,
    GLenum dest_type,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoCopyTextureCHROMIUM");
  static const char kFunctionName[] = "glCopyTextureCHROMIUM";

  TextureRef* source_texture_ref = GetTexture(source_id);
  TextureRef* dest_texture_ref = GetTexture(dest_id);

  if (!ValidateCopyTextureCHROMIUMTextures(
          kFunctionName, dest_target, source_texture_ref, dest_texture_ref)) {
    return;
  }

  if (source_level < 0 || dest_level < 0 ||
      (feature_info_->IsWebGL1OrES2Context() && source_level > 0)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "source_level or dest_level out of range");
    return;
  }

  Texture* source_texture = source_texture_ref->texture();
  Texture* dest_texture = dest_texture_ref->texture();
  GLenum source_target = source_texture->target();
  GLenum dest_binding_target = dest_texture->target();

  GLenum source_type = 0;
  GLenum source_internal_format = 0;
  source_texture->GetLevelType(source_target, source_level, &source_type,
                               &source_internal_format);
  GLenum format =
      TextureManager::ExtractFormatFromStorageFormat(internal_format);
  if (!texture_manager()->ValidateTextureParameters(
          error_state_.get(), kFunctionName, true, format, dest_type,
          internal_format, dest_level)) {
    return;
  }

  std::string output_error_msg;
  if (!ValidateCopyTextureCHROMIUMInternalFormats(
          GetFeatureInfo(), source_internal_format, internal_format,
          &output_error_msg)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       output_error_msg.c_str());
    return;
  }

  if (source_target == GL_TEXTURE_EXTERNAL_OES &&
      CopyTextureCHROMIUMNeedsESSL3(internal_format) &&
      !feature_info_->feature_flags().oes_egl_image_external_essl3) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "Copy*TextureCHROMIUM from EXTERNAL_OES to integer "
                       "format requires OES_EGL_image_external_essl3");
    return;
  }

  int source_width = 0;
  int source_height = 0;
  if (!source_texture->GetLevelSize(source_target, source_level, &source_width,
                                    &source_height, nullptr)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "source texture has no data for level");
    return;
  }

  // Check that this type of texture is allowed.
  if (!texture_manager()->ValidForTextureTarget(
          source_texture, source_level, source_width, source_height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "Bad dimensions");
    return;
  }

  if (dest_texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "texture is immutable");
    return;
  }

  // Clear the source texture if necessary.
  if (!texture_manager()->ClearTextureLevel(this, source_texture_ref,
                                            source_target, source_level)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, kFunctionName, "dimensions too big");
    return;
  }

  if (!InitializeCopyTextureCHROMIUM(kFunctionName))
    return;

  GLenum dest_type_previous = dest_type;
  GLenum dest_internal_format = internal_format;
  int dest_width = 0;
  int dest_height = 0;
  bool dest_level_defined = dest_texture->GetLevelSize(
      dest_target, dest_level, &dest_width, &dest_height, nullptr);

  if (dest_level_defined) {
    dest_texture->GetLevelType(dest_target, dest_level, &dest_type_previous,
                               &dest_internal_format);
  }

  // Resize the destination texture to the dimensions of the source texture.
  if (!dest_level_defined || dest_width != source_width ||
      dest_height != source_height ||
      dest_internal_format != internal_format ||
      dest_type_previous != dest_type) {
    // Ensure that the glTexImage2D succeeds.
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(kFunctionName);
    api()->glBindTextureFn(dest_binding_target, dest_texture->service_id());
    ScopedPixelUnpackState reset_restore(&state_);
    api()->glTexImage2DFn(
        dest_target, dest_level,
        TextureManager::AdjustTexInternalFormat(feature_info_.get(),
                                                internal_format, dest_type),
        source_width, source_height, 0,
        TextureManager::AdjustTexFormat(feature_info_.get(), format), dest_type,
        nullptr);
    GLenum error = LOCAL_PEEK_GL_ERROR(kFunctionName);
    if (error != GL_NO_ERROR) {
      RestoreCurrentTextureBindings(&state_, dest_binding_target,
                                    state_.active_texture_unit);
      return;
    }

    texture_manager()->SetLevelInfo(dest_texture_ref, dest_target, dest_level,
                                    internal_format, source_width,
                                    source_height, 1, 0, format, dest_type,
                                    gfx::Rect(source_width, source_height));
    dest_texture->ApplyFormatWorkarounds(feature_info_.get());
  } else {
    texture_manager()->SetLevelCleared(dest_texture_ref, dest_target,
                                       dest_level, true);
  }

  CopyTextureMethod method = GetCopyTextureCHROMIUMMethod(
      GetFeatureInfo(), source_target, source_level, source_internal_format,
      source_type, dest_binding_target, dest_level, internal_format,
      unpack_flip_y == GL_TRUE, unpack_premultiply_alpha == GL_TRUE,
      unpack_unmultiply_alpha == GL_TRUE);

  copy_texture_chromium_->DoCopyTexture(
      this, source_target, source_texture->service_id(), source_level,
      source_internal_format, dest_target, dest_texture->service_id(),
      dest_level, internal_format, source_width, source_height,
      unpack_flip_y == GL_TRUE, unpack_premultiply_alpha == GL_TRUE,
      unpack_unmultiply_alpha == GL_TRUE, method, copy_tex_image_blit_.get());
}

void GLES2DecoderImpl::CopySubTextureHelper(const char* function_name,
                                            GLuint source_id,
                                            GLint source_level,
                                            GLenum dest_target,
                                            GLuint dest_id,
                                            GLint dest_level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLboolean unpack_flip_y,
                                            GLboolean unpack_premultiply_alpha,
                                            GLboolean unpack_unmultiply_alpha) {
  TextureRef* source_texture_ref = GetTexture(source_id);
  TextureRef* dest_texture_ref = GetTexture(dest_id);

  if (!ValidateCopyTextureCHROMIUMTextures(
          function_name, dest_target, source_texture_ref, dest_texture_ref)) {
    return;
  }

  if (source_level < 0 || dest_level < 0 ||
      (feature_info_->IsWebGL1OrES2Context() && source_level > 0)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "source_level or dest_level out of range");
    return;
  }

  Texture* source_texture = source_texture_ref->texture();
  Texture* dest_texture = dest_texture_ref->texture();
  GLenum source_target = source_texture->target();
  GLenum dest_binding_target = dest_texture->target();
  int source_width = 0;
  int source_height = 0;
  if (!source_texture->GetLevelSize(source_target, source_level, &source_width,
                                    &source_height, nullptr)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "source texture has no data for level");
    return;
  }

  // Check that this type of texture is allowed.
  if (!texture_manager()->ValidForTextureTarget(
          source_texture, source_level, source_width, source_height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "source texture bad dimensions");
    return;
  }

  if (!source_texture->ValidForTexture(source_target, source_level, x, y, 0,
                                       width, height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "source texture bad dimensions.");
    return;
  }

  GLenum source_type = 0;
  GLenum source_internal_format = 0;
  source_texture->GetLevelType(source_target, source_level, &source_type,
                               &source_internal_format);

  GLenum dest_type = 0;
  GLenum dest_internal_format = 0;
  bool dest_level_defined = dest_texture->GetLevelType(
      dest_target, dest_level, &dest_type, &dest_internal_format);
  if (!dest_level_defined) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "destination texture is not defined");
    return;
  }
  if (!dest_texture->ValidForTexture(dest_target, dest_level, xoffset, yoffset,
                                     0, width, height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "destination texture bad dimensions.");
    return;
  }

  std::string output_error_msg;
  if (!ValidateCopyTextureCHROMIUMInternalFormats(
          GetFeatureInfo(), source_internal_format, dest_internal_format,
          &output_error_msg)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       output_error_msg.c_str());
    return;
  }

  if (source_target == GL_TEXTURE_EXTERNAL_OES &&
      CopyTextureCHROMIUMNeedsESSL3(dest_internal_format) &&
      !feature_info_->feature_flags().oes_egl_image_external_essl3) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "Copy*TextureCHROMIUM from EXTERNAL_OES to integer "
                       "format requires OES_EGL_image_external_essl3");
    return;
  }

  // Clear the source texture if necessary.
  if (!texture_manager()->ClearTextureLevel(this, source_texture_ref,
                                            source_target, source_level)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, function_name,
                       "source texture dimensions too big");
    return;
  }

  if (!InitializeCopyTextureCHROMIUM(function_name))
    return;

  int dest_width = 0;
  int dest_height = 0;
  bool ok = dest_texture->GetLevelSize(dest_target, dest_level, &dest_width,
                                       &dest_height, nullptr);
  DCHECK(ok);
  if (xoffset != 0 || yoffset != 0 || width != dest_width ||
      height != dest_height) {
    gfx::Rect cleared_rect;
    if (TextureManager::CombineAdjacentRects(
            dest_texture->GetLevelClearedRect(dest_target, dest_level),
            gfx::Rect(xoffset, yoffset, width, height), &cleared_rect)) {
      DCHECK_GE(cleared_rect.size().GetArea(),
                dest_texture->GetLevelClearedRect(dest_target, dest_level)
                    .size()
                    .GetArea());
      texture_manager()->SetLevelClearedRect(dest_texture_ref, dest_target,
                                             dest_level, cleared_rect);
    } else {
      // Otherwise clear part of texture level that is not already cleared.
      if (!texture_manager()->ClearTextureLevel(this, dest_texture_ref,
                                                dest_target, dest_level)) {
        LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, function_name,
                           "destination texture dimensions too big");
        return;
      }
    }
  } else {
    texture_manager()->SetLevelCleared(dest_texture_ref, dest_target,
                                       dest_level, true);
  }

  CopyTextureMethod method = GetCopyTextureCHROMIUMMethod(
      GetFeatureInfo(), source_target, source_level, source_internal_format,
      source_type, dest_binding_target, dest_level, dest_internal_format,
      unpack_flip_y == GL_TRUE, unpack_premultiply_alpha == GL_TRUE,
      unpack_unmultiply_alpha == GL_TRUE);

  // Use DRAW instead of COPY if the workaround is enabled.
  if (method == CopyTextureMethod::DIRECT_COPY &&
      workarounds().prefer_draw_to_copy) {
    method = CopyTextureMethod::DIRECT_DRAW;
  }

  copy_texture_chromium_->DoCopySubTexture(
      this, source_target, source_texture->service_id(), source_level,
      source_internal_format, dest_target, dest_texture->service_id(),
      dest_level, dest_internal_format, xoffset, yoffset, x, y, width, height,
      dest_width, dest_height, source_width, source_height,
      unpack_flip_y == GL_TRUE, unpack_premultiply_alpha == GL_TRUE,
      unpack_unmultiply_alpha == GL_TRUE, method, copy_tex_image_blit_.get());
}

void GLES2DecoderImpl::DoCopySubTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoCopySubTextureCHROMIUM");
  static const char kFunctionName[] = "glCopySubTextureCHROMIUM";
  CopySubTextureHelper(kFunctionName, source_id, source_level, dest_target,
                       dest_id, dest_level, xoffset, yoffset, x, y, width,
                       height, unpack_flip_y, unpack_premultiply_alpha,
                       unpack_unmultiply_alpha);
}

bool GLES2DecoderImpl::InitializeCopyTexImageBlitter(
    const char* function_name) {
  if (!copy_tex_image_blit_.get()) {
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name);
    copy_tex_image_blit_ =
        std::make_unique<CopyTexImageResourceManager>(feature_info_.get());
    copy_tex_image_blit_->Initialize(this);
    if (LOCAL_PEEK_GL_ERROR(function_name) != GL_NO_ERROR)
      return false;
  }
  return true;
}

bool GLES2DecoderImpl::InitializeCopyTextureCHROMIUM(
    const char* function_name) {
  // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
  // needed because it takes 10s of milliseconds to initialize.
  if (!copy_texture_chromium_.get()) {
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name);
    copy_texture_chromium_.reset(CopyTextureCHROMIUMResourceManager::Create());
    copy_texture_chromium_->Initialize(this, features());
    if (LOCAL_PEEK_GL_ERROR(function_name) != GL_NO_ERROR)
      return false;

    // On the desktop core profile this also needs emulation of
    // CopyTex{Sub}Image2D for luminance, alpha, and luminance_alpha
    // textures.
    if (CopyTexImageResourceManager::CopyTexImageRequiresBlit(
            feature_info_.get(), GL_LUMINANCE)) {
      if (!InitializeCopyTexImageBlitter(function_name))
        return false;
    }
  }
  return true;
}

void GLES2DecoderImpl::TexStorageImpl(GLenum target,
                                      GLsizei levels,
                                      GLenum internal_format,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      ContextState::Dimension dimension,
                                      const char* function_name) {
  if (levels == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "levels == 0");
    return;
  }
  bool is_compressed_format = IsCompressedTextureFormat(internal_format);
  if (is_compressed_format) {
    if (target == GL_TEXTURE_3D &&
        !feature_info_->feature_flags().ext_texture_format_astc_hdr &&
        ::gpu::gles2::IsASTCFormat(internal_format)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "target invalid for format");
      return;
    }
    if (!::gpu::gles2::ValidateCompressedFormatTarget(target,
                                                      internal_format)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "target invalid for format");
      return;
    }
  }
  TextureRef* texture_ref =
      texture_manager()->GetTextureInfoForTarget(&state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  // The glTexStorage entry points require width, height, and depth to be
  // at least 1, but the other texture entry points (those which use
  // ValidForTextureTarget) do not. So we have to add an extra check here.
  bool is_invalid_texstorage_size = width < 1 || height < 1 || depth < 1;
  if (!texture_manager()->ValidForTextureTarget(texture, 0, width, height,
                                                depth) ||
      is_invalid_texstorage_size) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, function_name, "dimensions out of range");
    return;
  }
  // glTexStorage generates GL_INVALID_OPERATION for out of bounds level
  // which is a bit different from other GL calls generating GL_INVALID_VALUE
  if (TextureManager::ComputeMipMapCount(target, width, height, depth) <
      levels) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name, "too many levels");
    return;
  }
  if (texture->IsAttachedToFramebuffer()) {
    framebuffer_state_.clear_state_dirty = true;
  }
  if (texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "texture is immutable");
    return;
  }

  GLenum format = TextureManager::ExtractFormatFromStorageFormat(
      internal_format);
  GLenum type = TextureManager::ExtractTypeFromStorageFormat(internal_format);

  {
    GLsizei level_width = width;
    GLsizei level_height = height;
    GLsizei level_depth = depth;
    base::CheckedNumeric<uint32_t> estimated_size(0);
    PixelStoreParams params;
    params.alignment = 1;
    for (int ii = 0; ii < levels; ++ii) {
      uint32_t size;
      if (is_compressed_format) {
        GLsizei level_size;
        if (!GetCompressedTexSizeInBytes(
                function_name, level_width, level_height, level_depth,
                internal_format, &level_size, error_state_.get())) {
          // GetCompressedTexSizeInBytes() already generates a GL error.
          return;
        }
        size = static_cast<uint32_t>(level_size);
      } else {
        if (!GLES2Util::ComputeImageDataSizesES3(level_width,
                                                 level_height,
                                                 level_depth,
                                                 format, type,
                                                 params,
                                                 &size,
                                                 nullptr, nullptr,
                                                 nullptr, nullptr)) {
          LOCAL_SET_GL_ERROR(
              GL_OUT_OF_MEMORY, function_name, "dimensions too large");
          return;
        }
      }
      estimated_size += size;
      level_width = std::max(1, level_width >> 1);
      level_height = std::max(1, level_height >> 1);
      if (target == GL_TEXTURE_3D)
        level_depth = std::max(1, level_depth >> 1);
    }
    if (!estimated_size.IsValid()) {
      LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, function_name, "out of memory");
      return;
    }
  }

  // First lookup compatibility format via texture manager for swizzling legacy
  // LUMINANCE/ALPHA formats.
  GLenum compatibility_internal_format =
      texture_manager()->AdjustTexStorageFormat(feature_info_.get(),
                                                internal_format);

  // Then lookup compatibility format for compressed formats.
  const CompressedFormatInfo* format_info =
      GetCompressedFormatInfo(internal_format);
  if (format_info != nullptr && !format_info->support_check(*feature_info_)) {
    compatibility_internal_format = format_info->decompressed_internal_format;
  }

  {
    GLsizei level_width = width;
    GLsizei level_height = height;
    GLsizei level_depth = depth;

    GLenum adjusted_internal_format =
        feature_info_->IsWebGL1OrES2Context() ? format : internal_format;
    for (int ii = 0; ii < levels; ++ii) {
      if (target == GL_TEXTURE_CUBE_MAP) {
        for (int jj = 0; jj < 6; ++jj) {
          GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + jj;
          texture_manager()->SetLevelInfo(
              texture_ref, face, ii, adjusted_internal_format, level_width,
              level_height, 1, 0, format, type, gfx::Rect());
        }
      } else {
        texture_manager()->SetLevelInfo(
            texture_ref, target, ii, adjusted_internal_format, level_width,
            level_height, level_depth, 0, format, type, gfx::Rect());
      }
      level_width = std::max(1, level_width >> 1);
      level_height = std::max(1, level_height >> 1);
      if (target == GL_TEXTURE_3D)
        level_depth = std::max(1, level_depth >> 1);
    }
    texture->ApplyFormatWorkarounds(feature_info_.get());
    texture->SetImmutable(true, true);
  }

  // TODO(zmo): We might need to emulate TexStorage using TexImage or
  // CompressedTexImage on Mac OSX where we expose ES3 APIs when the underlying
  // driver is lower than 4.2 and ARB_texture_storage extension doesn't exist.
  if (dimension == ContextState::k2D) {
    api()->glTexStorage2DEXTFn(target, levels, compatibility_internal_format,
                               width, height);
  } else {
    api()->glTexStorage3DFn(target, levels, compatibility_internal_format,
                            width, height, depth);
  }
}

void GLES2DecoderImpl::DoTexStorage2DEXT(GLenum target,
                                         GLsizei levels,
                                         GLenum internal_format,
                                         GLsizei width,
                                         GLsizei height) {
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoTexStorage2D",
      "width", width, "height", height);
  TexStorageImpl(target, levels, internal_format, width, height, 1,
                 ContextState::k2D, "glTexStorage2D");
}

void GLES2DecoderImpl::DoTexStorage3D(GLenum target,
                                      GLsizei levels,
                                      GLenum internal_format,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth) {
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoTexStorage3D",
      "widthXheight", width * height, "depth", depth);
  TexStorageImpl(target, levels, internal_format, width, height, depth,
                 ContextState::k3D, "glTexStorage3D");
}

void GLES2DecoderImpl::DoCreateAndTexStorage2DSharedImageINTERNAL(
    GLuint client_id,
    const volatile GLbyte* mailbox_data) {
  TRACE_EVENT2("gpu",
               "GLES2DecoderImpl::DoCreateAndTexStorage2DSharedImageCHROMIUM",
               "context", logger_.GetLogPrefix(), "mailbox[0]",
               static_cast<unsigned char>(mailbox_data[0]));
  Mailbox mailbox = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox_data));
  DLOG_IF(ERROR, !mailbox.Verify())
      << "DoCreateAndTexStorage2DSharedImageCHROMIUM was passed an invalid "
         "mailbox.";
  if (!client_id) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "DoCreateAndTexStorage2DSharedImageINTERNAL",
                       "invalid client id");
    return;
  }

  TextureRef* texture_ref = GetTexture(client_id);
  if (texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "DoCreateAndTexStorage2DSharedImageINTERNAL",
                       "client id already in use");
    return;
  }

  std::unique_ptr<GLTextureImageRepresentation> shared_image =
      group_->shared_image_representation_factory()->ProduceGLTexture(mailbox);

  if (!shared_image) {
    // Mailbox missing, generate a texture.
    bool result = GenTexturesHelper(1, &client_id);
    DCHECK(result);
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "DoCreateAndTexStorage2DSharedImageINTERNAL",
                       "invalid mailbox name");
    return;
  }

  texture_ref =
      texture_manager()->ConsumeSharedImage(client_id, std::move(shared_image));
}

void GLES2DecoderImpl::DoBeginSharedImageAccessDirectCHROMIUM(GLuint client_id,
                                                              GLenum mode) {
  TextureRef* texture_ref = GetTexture(client_id);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "DoBeginSharedImageAccessCHROMIUM",
                       "invalid texture id");
    return;
  }

  GLTextureImageRepresentation* shared_image = texture_ref->shared_image();
  if (!shared_image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "DoBeginSharedImageAccessCHROMIUM",
                       "bound texture is not a shared image");
    return;
  }

  if (texture_ref->shared_image_scoped_access()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "DoBeginSharedImageAccessCHROMIUM",
                       "shared image is being accessed");
    return;
  }

  if (!texture_ref->BeginAccessSharedImage(mode)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "DoBeginSharedImageAccessCHROMIUM",
                       "Unable to begin access");
    return;
  }
}

void GLES2DecoderImpl::DoEndSharedImageAccessDirectCHROMIUM(GLuint client_id) {
  TextureRef* texture_ref = GetTexture(client_id);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "DoBeginSharedImageAccessCHROMIUM",
                       "invalid texture id");
    return;
  }

  GLTextureImageRepresentation* shared_image = texture_ref->shared_image();
  if (!shared_image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "DoEndSharedImageAccessCHROMIUM",
                       "bound texture is not a shared image");
    return;
  }

  if (!texture_ref->shared_image_scoped_access()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "DoEndSharedImageAccessCHROMIUM",
                       "shared image is not being accessed");
    return;
  }

  texture_ref->EndAccessSharedImage();
}

void GLES2DecoderImpl::DoCopySharedImageINTERNAL(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    const volatile GLbyte* mailboxes) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void GLES2DecoderImpl::DoCopySharedImageToTextureINTERNAL(
    GLuint texture,
    GLenum target,
    GLuint internal_format,
    GLenum type,
    GLint src_x,
    GLint src_y,
    GLsizei width,
    GLsizei height,
    GLboolean flip_y,
    const volatile GLbyte* src_mailbox) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void GLES2DecoderImpl::DoInsertEventMarkerEXT(
    GLsizei length, const GLchar* marker) {
  if (!marker) {
    marker = "";
  }
  debug_marker_manager_.SetMarker(
      length ? std::string(marker, length) : std::string(marker));
}

void GLES2DecoderImpl::DoPushGroupMarkerEXT(
    GLsizei /*length*/, const GLchar* /*marker*/) {
}

void GLES2DecoderImpl::DoPopGroupMarkerEXT(void) {
}

error::Error GLES2DecoderImpl::HandleTraceBeginCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TraceBeginCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::TraceBeginCHROMIUM*>(cmd_data);
  Bucket* category_bucket = GetBucket(c.category_bucket_id);
  Bucket* name_bucket = GetBucket(c.name_bucket_id);
  static constexpr size_t kMaxStrLen = 256;
  if (!category_bucket || category_bucket->size() == 0 ||
      category_bucket->size() > kMaxStrLen || !name_bucket ||
      name_bucket->size() == 0 || name_bucket->size() > kMaxStrLen) {
    return error::kInvalidArguments;
  }

  std::string category_name;
  std::string trace_name;
  if (!category_bucket->GetAsString(&category_name) ||
      !name_bucket->GetAsString(&trace_name)) {
    return error::kInvalidArguments;
  }

  debug_marker_manager_.PushGroup(trace_name);
  if (!gpu_tracer_->Begin(category_name, trace_name, kTraceCHROMIUM)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glTraceBeginCHROMIUM", "unable to create begin trace");
    return error::kNoError;
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoTraceEndCHROMIUM() {
  debug_marker_manager_.PopGroup();
  if (!gpu_tracer_->End(kTraceCHROMIUM)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glTraceEndCHROMIUM", "no trace begin found");
    return;
  }
}

void GLES2DecoderImpl::DoDrawBuffersEXT(GLsizei count,
                                        const volatile GLenum* bufs) {
  DCHECK_LE(group_->max_draw_buffers(), 16u);
  if (count > static_cast<GLsizei>(group_->max_draw_buffers())) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glDrawBuffersEXT", "greater than GL_MAX_DRAW_BUFFERS_EXT");
    return;
  }

  Framebuffer* framebuffer = GetFramebufferInfoForTarget(GL_FRAMEBUFFER);
  if (framebuffer) {
    GLenum safe_bufs[16];
    for (GLsizei i = 0; i < count; ++i) {
      GLenum buf = bufs[i];
      if (buf != static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i) &&
          buf != GL_NONE) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION,
            "glDrawBuffersEXT",
            "bufs[i] not GL_NONE or GL_COLOR_ATTACHMENTi_EXT");
        return;
      }
      safe_bufs[i] = buf;
    }
    api()->glDrawBuffersARBFn(count, safe_bufs);
    framebuffer->SetDrawBuffers(count, safe_bufs);
  } else {  // backbuffer
    if (count != 1) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glDrawBuffersEXT",
                         "invalid number of buffers");
      return;
    }
    GLenum buf = bufs[0];
    if (buf != GL_BACK && buf != GL_NONE) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glDrawBuffersEXT",
                         "buffer is not GL_NONE or GL_BACK");
      return;
    }
    back_buffer_draw_buffer_ = buf;
    if (buf == GL_BACK && GetBackbufferServiceId() != 0)  // emulated backbuffer
      buf = GL_COLOR_ATTACHMENT0;
    api()->glDrawBuffersARBFn(count, &buf);
  }
}

void GLES2DecoderImpl::DoLoseContextCHROMIUM(GLenum current, GLenum other) {
  MarkContextLost(GetContextLostReasonFromResetStatus(current));
  group_->LoseContexts(GetContextLostReasonFromResetStatus(other));
  reset_by_robustness_extension_ = true;
}

void GLES2DecoderImpl::DoFlushDriverCachesCHROMIUM(void) {
  // On Adreno Android devices we need to use a workaround to force caches to
  // clear.
  if (workarounds().unbind_egl_context_to_flush_driver_caches) {
    context_->ReleaseCurrent(nullptr);
    context_->MakeCurrent(surface_.get());
  }
}

error::Error GLES2DecoderImpl::HandleUniformBlockBinding(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const char* func_name = "glUniformBlockBinding";
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::UniformBlockBinding& c =
      *static_cast<const volatile gles2::cmds::UniformBlockBinding*>(cmd_data);
  GLuint client_id = c.program;
  GLuint index = static_cast<GLuint>(c.index);
  GLuint binding = static_cast<GLuint>(c.binding);
  Program* program = GetProgramInfoNotShader(client_id, func_name);
  if (!program) {
    return error::kNoError;
  }
  if (index >= program->uniform_block_size_info().size()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name,
        "uniformBlockIndex is not an active uniform block index");
    return error::kNoError;
  }
  if (binding >= group_->max_uniform_buffer_bindings()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name,
        "uniformBlockBinding >= MAX_UNIFORM_BUFFER_BINDINGS");
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  api()->glUniformBlockBindingFn(service_id, index, binding);
  program->SetUniformBlockBinding(index, binding);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClientWaitSync(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const char* function_name = "glClientWaitSync";
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::ClientWaitSync& c =
      *static_cast<const volatile gles2::cmds::ClientWaitSync*>(cmd_data);
  const GLuint sync = static_cast<GLuint>(c.sync);
  GLbitfield flags = static_cast<GLbitfield>(c.flags);
  const GLuint64 timeout = c.timeout();
  typedef cmds::ClientWaitSync::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (*result_dst != GL_WAIT_FAILED) {
    return error::kInvalidArguments;
  }
  GLsync service_sync = 0;
  if (!group_->GetSyncServiceId(sync, &service_sync)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid sync");
    return error::kNoError;
  }
  if ((flags & ~GL_SYNC_FLUSH_COMMANDS_BIT) != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid flags");
    return error::kNoError;
  }
  // Force GL_SYNC_FLUSH_COMMANDS_BIT to avoid infinite wait.
  flags |= GL_SYNC_FLUSH_COMMANDS_BIT;

  GLenum status = api()->glClientWaitSyncFn(service_sync, flags, timeout);
  switch (status) {
    case GL_ALREADY_SIGNALED:
    case GL_TIMEOUT_EXPIRED:
    case GL_CONDITION_SATISFIED:
      break;
    case GL_WAIT_FAILED:
      // Avoid leaking GL errors when using virtual contexts.
      LOCAL_PEEK_GL_ERROR(function_name);
      *result_dst = status;
      // If validation is complete, this only happens if the context is lost.
      return error::kLostContext;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  *result_dst = status;
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleWaitSync(uint32_t immediate_data_size,
                                              const volatile void* cmd_data) {
  const char* function_name = "glWaitSync";
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::WaitSync& c =
      *static_cast<const volatile gles2::cmds::WaitSync*>(cmd_data);
  const GLuint sync = static_cast<GLuint>(c.sync);
  const GLbitfield flags = static_cast<GLbitfield>(c.flags);
  const GLuint64 timeout = c.timeout();
  GLsync service_sync = 0;
  if (!group_->GetSyncServiceId(sync, &service_sync)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid sync");
    return error::kNoError;
  }
  if (flags != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid flags");
    return error::kNoError;
  }
  if (timeout != GL_TIMEOUT_IGNORED) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid timeout");
    return error::kNoError;
  }
  api()->glWaitSyncFn(service_sync, flags, timeout);
  return error::kNoError;
}

GLsync GLES2DecoderImpl::DoFenceSync(GLenum condition, GLbitfield flags) {
  const char* function_name = "glFenceSync";
  if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, function_name, "invalid condition");
    return 0;
  }
  if (flags != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid flags");
    return 0;
  }
  return api()->glFenceSyncFn(condition, flags);
}

GLsizei GLES2DecoderImpl::InternalFormatSampleCountsHelper(
    GLenum target,
    GLenum internalformat,
    std::vector<GLint>* out_sample_counts) {
  DCHECK(out_sample_counts == nullptr || out_sample_counts->size() == 0);

  GLint num_sample_counts = 0;
  api()->glGetInternalformativFn(target, internalformat, GL_NUM_SAMPLE_COUNTS,
                                 1, &num_sample_counts);

  bool remove_nonconformant_sample_counts =
      feature_info_->IsWebGLContext() &&
      feature_info_->feature_flags().nv_internalformat_sample_query;

  if (out_sample_counts != nullptr || remove_nonconformant_sample_counts) {
    std::vector<GLint> sample_counts(num_sample_counts);
    api()->glGetInternalformativFn(target, internalformat, GL_SAMPLES,
                                   num_sample_counts, sample_counts.data());

    if (remove_nonconformant_sample_counts) {
      ScopedGLErrorSuppressor suppressor(
          "GLES2DecoderImpl::InternalFormatSampleCountsHelper",
          error_state_.get());

      auto is_nonconformant = [this, target,
                               internalformat](GLint sample_count) {
        GLint conformant = GL_FALSE;
        api()->glGetInternalformatSampleivNVFn(target, internalformat,
                                               sample_count, GL_CONFORMANT_NV,
                                               1, &conformant);

        // getInternalformatSampleivNV does not work for all formats on NVIDIA
        // Shield TV drivers. Assume that formats with large sample counts are
        // non-conformant in case the query generates an error.
        if (api()->glGetErrorFn() != GL_NO_ERROR) {
          return sample_count > 8;
        }
        return conformant == GL_FALSE;
      };

      sample_counts.erase(std::remove_if(sample_counts.begin(),
                                         sample_counts.end(), is_nonconformant),
                          sample_counts.end());
      num_sample_counts = sample_counts.size();
    }

    if (out_sample_counts != nullptr) {
      *out_sample_counts = std::move(sample_counts);
    }
  }

  DCHECK(out_sample_counts == nullptr ||
         out_sample_counts->size() == static_cast<size_t>(num_sample_counts));
  return num_sample_counts;
}

error::Error GLES2DecoderImpl::HandleGetInternalformativ(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetInternalformativ& c =
      *static_cast<const volatile gles2::cmds::GetInternalformativ*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum pname = static_cast<GLenum>(c.pname);
  if (!validators_->render_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetInternalformativ", target, "target");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetInternalformativ", format,
                                    "internalformat");
    return error::kNoError;
  }
  if (!validators_->internal_format_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetInternalformativ", pname, "pname");
    return error::kNoError;
  }

  typedef cmds::GetInternalformativ::Result Result;

  GLsizei num_sample_counts = 0;
  std::vector<GLint> sample_counts;

  GLsizei num_values = 0;
  GLint* values = nullptr;
  switch (pname) {
    case GL_NUM_SAMPLE_COUNTS:
      num_sample_counts =
          InternalFormatSampleCountsHelper(target, format, nullptr);
      num_values = 1;
      values = &num_sample_counts;
      break;
    case GL_SAMPLES:
      num_sample_counts =
          InternalFormatSampleCountsHelper(target, format, &sample_counts);
      num_values = num_sample_counts;
      values = sample_counts.data();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);

  GLint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }

  std::copy(values, &values[num_values], params);
  result->SetNumResults(num_values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMapBufferRange(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context()) {
    return error::kUnknownCommand;
  }

  const char* func_name = "glMapBufferRange";
  const volatile gles2::cmds::MapBufferRange& c =
      *static_cast<const volatile gles2::cmds::MapBufferRange*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLbitfield access = static_cast<GLbitfield>(c.access);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t data_shm_id = static_cast<uint32_t>(c.data_shm_id);
  uint32_t data_shm_offset = static_cast<uint32_t>(c.data_shm_offset);

  typedef cmds::MapBufferRange::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  if (*result != 0) {
    *result = 0;
    return error::kInvalidArguments;
  }
  if (!validators_->buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
    return error::kNoError;
  }
  if (size == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "length is zero");
    return error::kNoError;
  }
  Buffer* buffer = buffer_manager()->RequestBufferAccess(
      &state_, error_state_.get(), target, offset, size, func_name);
  if (!buffer) {
    // An error is already set.
    return error::kNoError;
  }
  if (state_.bound_transform_feedback->active() &&
      !state_.bound_transform_feedback->paused()) {
    size_t used_binding_count =
        state_.current_program->effective_transform_feedback_varyings().size();
    if (state_.bound_transform_feedback->UsesBuffer(
            used_binding_count, buffer)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "active transform feedback is using this buffer");
      return error::kNoError;
    }
  }

  int8_t* mem =
      GetSharedMemoryAs<int8_t*>(data_shm_id, data_shm_offset, size);
  if (!mem) {
    return error::kOutOfBounds;
  }
  if (AnyOtherBitsSet(access, (GL_MAP_READ_BIT |
                               GL_MAP_WRITE_BIT |
                               GL_MAP_INVALIDATE_RANGE_BIT |
                               GL_MAP_INVALIDATE_BUFFER_BIT |
                               GL_MAP_FLUSH_EXPLICIT_BIT |
                               GL_MAP_UNSYNCHRONIZED_BIT))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "invalid access bits");
    return error::kNoError;
  }
  if (!AnyBitsSet(access, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "neither MAP_READ_BIT nor MAP_WRITE_BIT is set");
    return error::kNoError;
  }
  if (AllBitsSet(access, GL_MAP_READ_BIT) &&
      AnyBitsSet(access, (GL_MAP_INVALIDATE_RANGE_BIT |
                          GL_MAP_INVALIDATE_BUFFER_BIT |
                          GL_MAP_UNSYNCHRONIZED_BIT))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "incompatible access bits with MAP_READ_BIT");
    return error::kNoError;
  }
  if (AllBitsSet(access, GL_MAP_FLUSH_EXPLICIT_BIT) &&
      !AllBitsSet(access, GL_MAP_WRITE_BIT)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "MAP_FLUSH_EXPLICIT_BIT set without MAP_WRITE_BIT");
    return error::kNoError;
  }
  GLbitfield filtered_access = access;
  if (AllBitsSet(filtered_access, GL_MAP_INVALIDATE_BUFFER_BIT)) {
    // To be on the safe side, always map GL_MAP_INVALIDATE_BUFFER_BIT to
    // GL_MAP_INVALIDATE_RANGE_BIT.
    filtered_access = (filtered_access & ~GL_MAP_INVALIDATE_BUFFER_BIT);
    filtered_access = (filtered_access | GL_MAP_INVALIDATE_RANGE_BIT);
  }
  // Always filter out GL_MAP_UNSYNCHRONIZED_BIT to get rid of undefined
  // behaviors.
  filtered_access = (filtered_access & ~GL_MAP_UNSYNCHRONIZED_BIT);
  if (AllBitsSet(filtered_access, GL_MAP_WRITE_BIT) &&
      !AllBitsSet(filtered_access, GL_MAP_INVALIDATE_RANGE_BIT)) {
    filtered_access = (filtered_access | GL_MAP_READ_BIT);
  }
  void* ptr = api()->glMapBufferRangeFn(target, offset, size, filtered_access);
  if (ptr == nullptr) {
    // This should mean GL_OUT_OF_MEMORY (or context loss).
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(func_name);
    return error::kNoError;
  }
  // The un-filtered bits in |access| are deliberately used here.
  buffer->SetMappedRange(offset, size, access, ptr,
                         GetSharedMemoryBuffer(data_shm_id),
                         static_cast<unsigned int>(data_shm_offset));
  if ((filtered_access & GL_MAP_INVALIDATE_RANGE_BIT) == 0) {
    memcpy(mem, ptr, size);
  }
  *result = 1;
  return error::kNoError;
}

bool GLES2DecoderImpl::UnmapBufferHelper(Buffer* buffer, GLenum target) {
  DCHECK(buffer);
  const Buffer::MappedRange* mapped_range = buffer->GetMappedRange();
  if (!mapped_range)
    return true;
  if (!AllBitsSet(mapped_range->access, GL_MAP_WRITE_BIT) ||
      AllBitsSet(mapped_range->access, GL_MAP_FLUSH_EXPLICIT_BIT)) {
    // If we don't need to write back, or explict flush is required, no copying
    // back is needed.
  } else if (!WasContextLost()) {
    void* mem = mapped_range->GetShmPointer();
    DCHECK(mem);
    DCHECK(mapped_range->pointer);
    memcpy(mapped_range->pointer, mem, mapped_range->size);
    if (buffer->shadowed()) {
      buffer->SetRange(mapped_range->offset, mapped_range->size, mem);
    }
  }
  buffer->RemoveMappedRange();
  if (WasContextLost())
    return true;
  GLboolean rt = api()->glUnmapBufferFn(target);
  if (rt == GL_FALSE) {
    // At this point, we have already done the necessary validation, so
    // GL_FALSE indicates data corruption.
    // TODO(zmo): We could redo the map / copy data / unmap to recover, but
    // the second unmap could still return GL_FALSE. For now, we simply lose
    // the contexts in the share group.
    LOG(ERROR) << "glUnmapBuffer unexpectedly returned GL_FALSE";
    // Need to lose current context before broadcasting!
    MarkContextLost(error::kGuilty);
    group_->LoseContexts(error::kInnocent);
    return false;
  }
  return true;
}

error::Error GLES2DecoderImpl::HandleUnmapBuffer(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context()) {
    return error::kUnknownCommand;
  }
  const char* func_name = "glUnmapBuffer";

  const volatile gles2::cmds::UnmapBuffer& c =
      *static_cast<const volatile gles2::cmds::UnmapBuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);

  if (!validators_->buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
    return error::kNoError;
  }

  Buffer* buffer = buffer_manager()->GetBufferInfoForTarget(&state_, target);
  if (!buffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "no buffer bound");
    return error::kNoError;
  }
  const Buffer::MappedRange* mapped_range = buffer->GetMappedRange();
  if (!mapped_range) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "buffer is unmapped");
    return error::kNoError;
  }
  if (!UnmapBufferHelper(buffer, target))
    return error::kLostContext;
  return error::kNoError;
}

void GLES2DecoderImpl::DoFlushMappedBufferRange(
    GLenum target, GLintptr offset, GLsizeiptr size) {
  const char* func_name = "glFlushMappedBufferRange";
  // |size| is validated in HandleFlushMappedBufferRange().
  if (offset < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "offset < 0");
    return;
  }
  Buffer* buffer = buffer_manager()->GetBufferInfoForTarget(&state_, target);
  if (!buffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "no buffer bound");
    return;
  }
  const Buffer::MappedRange* mapped_range = buffer->GetMappedRange();
  if (!mapped_range) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "buffer is unmapped");
    return;
  }
  if (!AllBitsSet(mapped_range->access, GL_MAP_FLUSH_EXPLICIT_BIT)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "buffer is mapped without MAP_FLUSH_EXPLICIT_BIT flag");
    return;
  }
  base::CheckedNumeric<int32_t> range_size = size;
  range_size += offset;
  if (!range_size.IsValid() ||
      range_size.ValueOrDefault(0) > mapped_range->size) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name,
        "offset + size out of bounds");
    return;
  }
  char* client_data = reinterpret_cast<char*>(mapped_range->GetShmPointer());
  DCHECK(client_data);
  char* gpu_data = reinterpret_cast<char*>(mapped_range->pointer.get());
  DCHECK(gpu_data);
  memcpy(gpu_data + offset, client_data + offset, size);
  if (buffer->shadowed()) {
    buffer->SetRange(mapped_range->offset + offset, size, client_data + offset);
  }
  api()->glFlushMappedBufferRangeFn(target, offset, size);
}

void GLES2DecoderImpl::DoFramebufferMemorylessPixelLocalStorageANGLE(
    GLint plane,
    GLenum internalformat) {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoFramebufferTexturePixelLocalStorageANGLE(
    GLint plane,
    GLuint client_texture_id,
    GLint level,
    GLint layer) {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoFramebufferPixelLocalClearValuefvANGLE(
    GLint plane,
    const volatile GLfloat* value) {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoFramebufferPixelLocalClearValueivANGLE(
    GLint plane,
    const volatile GLint* value) {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoFramebufferPixelLocalClearValueuivANGLE(
    GLint plane,
    const volatile GLuint* value) {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoBeginPixelLocalStorageANGLE(
    GLsizei n,
    const volatile GLenum* loadops) {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoEndPixelLocalStorageANGLE(
    GLsizei n,
    const volatile GLenum* storeops) {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoPixelLocalStorageBarrierANGLE() {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoFramebufferPixelLocalStorageInterruptANGLE() {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoFramebufferPixelLocalStorageRestoreANGLE() {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoGetFramebufferPixelLocalStorageParameterfvANGLE(
    GLint plane,
    GLenum pname,
    GLfloat* params,
    GLsizei params_size) {
  NOTIMPLEMENTED();
}

void GLES2DecoderImpl::DoGetFramebufferPixelLocalStorageParameterivANGLE(
    GLint plane,
    GLenum pname,
    GLint* params,
    GLsizei params_size) {
  NOTIMPLEMENTED();
}

// Note that GL_LOST_CONTEXT is specific to GLES.
// For desktop GL we have to query the reset status proactively.
void GLES2DecoderImpl::OnContextLostError() {
  if (!WasContextLost()) {
    // Need to lose current context before broadcasting!
    CheckResetStatus();
    group_->LoseContexts(error::kUnknown);
    reset_by_robustness_extension_ = true;
  }
}

void GLES2DecoderImpl::OnOutOfMemoryError() {
  if (lose_context_when_out_of_memory_ && !WasContextLost()) {
    error::ContextLostReason other = error::kOutOfMemory;
    if (CheckResetStatus()) {
      other = error::kUnknown;
    } else {
      // Need to lose current context before broadcasting!
      MarkContextLost(error::kOutOfMemory);
    }
    group_->LoseContexts(other);
  }
}

const SamplerState& GLES2DecoderImpl::GetSamplerStateForTextureUnit(
    GLenum target, GLuint unit) {
  if (features().enable_samplers) {
    Sampler* sampler = state_.sampler_units[unit].get();
    if (sampler)
      return sampler->sampler_state();
  }
  TextureUnit& texture_unit = state_.texture_units[unit];
  TextureRef* texture_ref = texture_unit.GetInfoForSamplerType(target);
  if (texture_ref)
    return texture_ref->texture()->sampler_state();

  return default_sampler_state_;
}

void GLES2DecoderImpl::ClearFramebufferForWorkaround(GLbitfield mask) {
  ScopedGLErrorSuppressor suppressor("GLES2DecoderImpl::ClearWorkaround",
                                     error_state_.get());
  clear_framebuffer_blit_->ClearFramebuffer(
      this, gfx::Size(viewport_max_width_, viewport_max_height_), mask,
      state_.color_clear_red, state_.color_clear_green, state_.color_clear_blue,
      state_.color_clear_alpha, state_.depth_clear, state_.stencil_clear);
}

void GLES2DecoderImpl::RestoreAllExternalTextureBindingsIfNeeded() {
  if (texture_manager()->GetServiceIdGeneration() ==
      texture_manager_service_id_generation_)
    return;

  // Texture manager's version has changed, so rebind all external textures
  // in case their service ids have changed.
  for (unsigned texture_unit_index = 0;
       texture_unit_index < state_.texture_units.size(); texture_unit_index++) {
    TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
    if (texture_unit.bind_target != GL_TEXTURE_EXTERNAL_OES)
      continue;

    if (TextureRef* texture_ref =
            texture_unit.bound_texture_external_oes.get()) {
      api()->glActiveTextureFn(GL_TEXTURE0 + texture_unit_index);
      api()->glBindTextureFn(GL_TEXTURE_EXTERNAL_OES,
                             texture_ref->service_id());
    }
  }

  api()->glActiveTextureFn(GL_TEXTURE0 + state_.active_texture_unit);

  texture_manager_service_id_generation_ =
      texture_manager()->GetServiceIdGeneration();
}

void GLES2DecoderImpl::DoContextVisibilityHintCHROMIUM(GLboolean visibility) {
  if (feature_info_->IsWebGLContext())
    context_->SetVisibility(visibility == GL_TRUE);
}

error::Error GLES2DecoderImpl::HandleInitializeDiscardableTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::InitializeDiscardableTextureCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::InitializeDiscardableTextureCHROMIUM*>(
          cmd_data);
  GLuint texture_id = c.texture_id;
  uint32_t shm_id = c.shm_id;
  uint32_t shm_offset = c.shm_offset;

  TextureRef* texture = texture_manager()->GetTexture(texture_id);
  if (!texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glInitializeDiscardableTextureCHROMIUM",
                       "Invalid texture ID");
    return error::kNoError;
  }
  scoped_refptr<gpu::Buffer> buffer = GetSharedMemoryBuffer(shm_id);
  if (!DiscardableHandleBase::ValidateParameters(buffer.get(), shm_offset))
    return error::kInvalidArguments;

  size_t size = texture->texture()->estimated_size();
  ServiceDiscardableHandle handle(std::move(buffer), shm_offset, shm_id);
  GetContextGroup()->discardable_manager()->InsertLockedTexture(
      texture_id, size, group_->texture_manager(), std::move(handle));
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUnlockDiscardableTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UnlockDiscardableTextureCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::UnlockDiscardableTextureCHROMIUM*>(
          cmd_data);
  GLuint texture_id = c.texture_id;
  ServiceDiscardableManager* discardable_manager =
      GetContextGroup()->discardable_manager();
  TextureRef* texture_to_unbind;
  if (!discardable_manager->UnlockTexture(texture_id, group_->texture_manager(),
                                          &texture_to_unbind)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUnlockDiscardableTextureCHROMIUM",
                       "Texture ID not initialized");
  }
  if (texture_to_unbind)
    UnbindTexture(texture_to_unbind, SupportsSeparateFramebufferBinds());

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleLockDiscardableTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::LockDiscardableTextureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::LockDiscardableTextureCHROMIUM*>(
          cmd_data);
  GLuint texture_id = c.texture_id;
  if (!GetContextGroup()->discardable_manager()->LockTexture(
          texture_id, group_->texture_manager())) {
    // Temporarily log a crash dump for debugging crbug.com/870317.
    base::debug::DumpWithoutCrashing();
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glLockDiscardableTextureCHROMIUM",
                       "Texture ID not initialized");
  }
  return error::kNoError;
}


scoped_refptr<gpu::Buffer> GLES2DecoderImpl::GetShmBuffer(uint32_t shm_id) {
  return GetSharedMemoryBuffer(shm_id);
}

void GLES2DecoderImpl::DoWindowRectanglesEXT(GLenum mode,
                                             GLsizei n,
                                             const volatile GLint* box) {
  std::vector<GLint> box_copy(box, box + (n * 4));
  if (static_cast<size_t>(n) > state_.GetMaxWindowRectangles()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glWindowRectanglesEXT",
                       "count > GL_MAX_WINDOW_RECTANGLES_EXT");
    return;
  }
  for (int i = 0; i < n; ++i) {
    int boxindex = i * 4;
    if (box_copy[boxindex + 2] < 0 || box_copy[boxindex + 3] < 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glWindowRectanglesEXT",
                         "negative box width or height");
      return;
    }
  }
  state_.SetWindowRectangles(mode, n, box_copy.data());
  state_.UpdateWindowRectangles();
}

void GLES2DecoderImpl::DoSetReadbackBufferShadowAllocationINTERNAL(
    GLuint buffer_id,
    GLuint shm_id,
    GLuint shm_offset,
    GLuint size) {
  static const char kFunctionName[] = "glSetBufferShadowAllocationINTERNAL";
  scoped_refptr<Buffer> buffer = buffer_manager()->GetBuffer(buffer_id);
  if (!buffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName, "unknown buffer");
    return;
  }
  if (static_cast<GLsizeiptr>(size) != buffer->size()) {
    MarkContextLost(error::kGuilty);
    group_->LoseContexts(error::kUnknown);
    return;
  }

  scoped_refptr<gpu::Buffer> shm = GetSharedMemoryBuffer(shm_id);
  buffer->SetReadbackShadowAllocation(shm, shm_offset);
  // All buffers in writes_submitted_but_not_completed_ should have shm
  // allocations.
  writes_submitted_but_not_completed_.insert(buffer);
}

void GLES2DecoderImpl::CompileShaderAndExitCommandProcessingEarly(
    Shader* shader) {
  // No need to call DoCompile or exit command processing early if the call
  // to DoCompile will be a no-op.
  if (!shader->CanCompile())
    return;

  shader->DoCompile();

  // Shader compilation can be very slow (see https://crbug.com/844780), Exit
  // command processing to allow for context preemption and GPU watchdog
  // checks.
  ExitCommandProcessingEarly();
}

void GLES2DecoderImpl::ReportProgress() {
  if (group_)
    group_->ReportProgress();
}

error::Error GLES2DecoderImpl::HandleSetActiveURLCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile cmds::SetActiveURLCHROMIUM& c =
      *static_cast<const volatile cmds::SetActiveURLCHROMIUM*>(cmd_data);
  Bucket* url_bucket = GetBucket(c.url_bucket_id);
  static constexpr size_t kMaxStrLen = 1024;
  if (!url_bucket || url_bucket->size() == 0 ||
      url_bucket->size() > kMaxStrLen) {
    return error::kInvalidArguments;
  }

  size_t size = url_bucket->size();
  const char* url_str = url_bucket->GetDataAs<const char*>(0, size);
  if (!url_str)
    return error::kInvalidArguments;

  GURL url(std::string_view(url_str, size));
  client()->SetActiveURL(std::move(url));
  return error::kNoError;
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "build/chromeos_buildflags.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h"

}  // namespace gles2
}  // namespace gpu
