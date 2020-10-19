// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bits.h"
#include "base/containers/flat_map.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/transfer_cache_entry.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/common/raster_cmd_ids.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_tex_image.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_cmd_validation.h"
#include "gpu/command_buffer/service/service_font_manager.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/wrapped_sk_image.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDeferredDisplayListRecorder.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#endif

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
#define LOCAL_RENDER_WARNING(msg) RenderWarning(__FILE__, __LINE__, msg)

namespace gpu {
namespace raster {

namespace {

base::AtomicSequenceNumber g_raster_decoder_id;

// This class prevents any GL errors that occur when it is in scope from
// being reported to the client.
class ScopedGLErrorSuppressor {
 public:
  ScopedGLErrorSuppressor(const char* function_name,
                          gles2::ErrorState* error_state)
      : function_name_(function_name), error_state_(error_state) {
    ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(error_state_, function_name_);
  }
  ~ScopedGLErrorSuppressor() {
    ERRORSTATE_CLEAR_REAL_GL_ERRORS(error_state_, function_name_);
  }

 private:
  const char* function_name_;
  gles2::ErrorState* error_state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGLErrorSuppressor);
};

// Temporarily changes a decoder's bound texture and restore it when this
// object goes out of scope. Also temporarily switches to using active texture
// unit zero in case the client has changed that to something invalid.
class ScopedTextureBinder {
 public:
  ScopedTextureBinder(gles2::ContextState* state,
                      GLenum target,
                      GLuint texture,
                      GrDirectContext* gr_context)
      : state_(state), target_(target) {
    auto* api = state->api();
    api->glActiveTextureFn(GL_TEXTURE0);
    api->glBindTextureFn(target_, texture);
    if (gr_context)
      gr_context->resetContext(kTextureBinding_GrGLBackendState);
  }

  ~ScopedTextureBinder() { state_->api()->glBindTextureFn(target_, 0); }

 private:
  gles2::ContextState* state_;
  GLenum target_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTextureBinder);
};

// Temporarily changes a decoder's PIXEL_UNPACK_BUFFER to 0 and set pixel
// unpack params to default, and restore them when this object goes out of
// scope.
class ScopedPixelUnpackState {
 public:
  explicit ScopedPixelUnpackState(gles2::ContextState* state,
                                  GrDirectContext* gr_context,
                                  const gles2::FeatureInfo* feature_info) {
    DCHECK(state);
    auto* api = state->api();
    api->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, 4);
    if (feature_info->gl_version_info().is_es3 ||
        feature_info->gl_version_info().is_desktop_core_profile ||
        feature_info->feature_flags().ext_pixel_buffer_object)
      api->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);

    if (feature_info->gl_version_info().is_es3 ||
        feature_info->gl_version_info().is_desktop_core_profile ||
        feature_info->feature_flags().ext_unpack_subimage)
      api->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, 0);
    if (gr_context) {
      gr_context->resetContext(kMisc_GrGLBackendState |
                               kPixelStore_GrGLBackendState);
    }
  }
  ~ScopedPixelUnpackState() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedPixelUnpackState);
};

// Commands that are explicitly listed as OK to occur between
// BeginRasterCHROMIUM and EndRasterCHROMIUM. They do not invalidate
// GrDirectContext state tracking.
bool AllowedBetweenBeginEndRaster(CommandId command) {
  switch (command) {
    case kCreateTransferCacheEntryINTERNAL:
    case kDeleteTransferCacheEntryINTERNAL:
    case kEndRasterCHROMIUM:
    case kFinish:
    case kFlush:
    case kGetError:
    case kRasterCHROMIUM:
    case kUnlockTransferCacheEntryINTERNAL:
      return true;
    default:
      return false;
  }
}

// This class is sent to cc::PaintOpReader during paint op deserialization. When
// a cc:PaintOp refers to a mailbox-backed cc:PaintImage, this class opens the
// shared image for read access and returns an SkImage reference.
// SharedImageProviderImpl maintains read access until it is destroyed
// which should occur after |end_semaphores| have been flushed to Skia.
class SharedImageProviderImpl final : public cc::SharedImageProvider {
 public:
  SharedImageProviderImpl(
      SharedImageRepresentationFactory* shared_image_factory,
      scoped_refptr<SharedContextState> shared_context_state,
      SkSurface* output_surface,
      std::vector<GrBackendSemaphore>* end_semaphores,
      gles2::ErrorState* error_state)
      : shared_image_factory_(shared_image_factory),
        shared_context_state_(std::move(shared_context_state)),
        output_surface_(output_surface),
        end_semaphores_(end_semaphores),
        error_state_(error_state) {
    DCHECK(shared_image_factory_);
    DCHECK(shared_context_state_);
    DCHECK(output_surface_);
    DCHECK(end_semaphores_);
    DCHECK(error_state_);
  }
  SharedImageProviderImpl(const SharedImageProviderImpl&) = delete;
  SharedImageProviderImpl& operator=(const SharedImageProviderImpl&) = delete;

  ~SharedImageProviderImpl() override { read_accessors_.clear(); }

  sk_sp<SkImage> OpenSharedImageForRead(const gpu::Mailbox& mailbox) override {
    auto it = read_accessors_.find(mailbox);
    if (it != read_accessors_.end())
      return it->second.read_access_sk_image;

    auto shared_image_skia =
        shared_image_factory_->ProduceSkia(mailbox, shared_context_state_);
    if (!shared_image_skia) {
      ERRORSTATE_SET_GL_ERROR(error_state_, GL_INVALID_OPERATION,
                              "SharedImageProviderImpl::OpenSharedImageForRead",
                              ("Attempting to operate on unknown mailbox:" +
                               mailbox.ToDebugString())
                                  .c_str());
      return nullptr;
    }

    std::vector<GrBackendSemaphore> begin_semaphores;
    // |end_semaphores_| is owned by RasterDecoderImpl which will handle sending
    // them to SkCanvas
    auto scoped_read_access = shared_image_skia->BeginScopedReadAccess(
        &begin_semaphores, end_semaphores_);
    if (!scoped_read_access) {
      ERRORSTATE_SET_GL_ERROR(error_state_, GL_INVALID_OPERATION,
                              "SharedImageProviderImpl::OpenSharedImageForRead",
                              ("Couldn't access shared image for mailbox:" +
                               mailbox.ToDebugString())
                                  .c_str());
      return nullptr;
    }

    if (!begin_semaphores.empty()) {
      bool result = output_surface_->wait(begin_semaphores.size(),
                                          begin_semaphores.data(),
                                          /*deleteSemaphoresAfterWait=*/false);
      DCHECK(result);
    }

    auto sk_image =
        scoped_read_access->CreateSkImage(shared_context_state_->gr_context());
    if (!sk_image) {
      ERRORSTATE_SET_GL_ERROR(error_state_, GL_INVALID_OPERATION,
                              "SharedImageProviderImpl::OpenSharedImageForRead",
                              "Couldn't create output SkImage.");
      return nullptr;
    }

    read_accessors_[mailbox] = {std::move(shared_image_skia),
                                std::move(scoped_read_access), sk_image};
    return sk_image;
  }

 private:
  SharedImageRepresentationFactory* shared_image_factory_;
  scoped_refptr<SharedContextState> shared_context_state_;
  SkSurface* output_surface_;
  std::vector<GrBackendSemaphore>* end_semaphores_;
  gles2::ErrorState* error_state_;

  struct SharedImageReadAccess {
    std::unique_ptr<SharedImageRepresentationSkia> shared_image_skia;
    std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
        scoped_read_access;
    sk_sp<SkImage> read_access_sk_image;
  };
  base::flat_map<gpu::Mailbox, SharedImageReadAccess> read_accessors_;
};

}  // namespace

// RasterDecoderImpl uses two separate state trackers (gpu::gles2::ContextState
// and GrDirectContext) that cache the current GL driver state. Each class sees
// a fraction of the GL calls issued and can easily become inconsistent with GL
// state. We guard against that by resetting. But resetting is expensive, so we
// avoid it as much as possible.
class RasterDecoderImpl final : public RasterDecoder,
                                public gles2::ErrorStateClient,
                                public ServiceFontManager::Client,
                                public SharedContextState::ContextLostObserver {
 public:
  RasterDecoderImpl(DecoderClient* client,
                    CommandBufferServiceBase* command_buffer_service,
                    gles2::Outputter* outputter,
                    const GpuFeatureInfo& gpu_feature_info,
                    const GpuPreferences& gpu_preferences,
                    MemoryTracker* memory_tracker,
                    SharedImageManager* shared_image_manager,
                    scoped_refptr<SharedContextState> shared_context_state,
                    bool is_privileged);
  ~RasterDecoderImpl() override;

  gles2::GLES2Util* GetGLES2Util() override { return &util_; }

  // DecoderContext implementation.
  base::WeakPtr<DecoderContext> AsWeakPtr() override;
  ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const gles2::DisallowedFeatures& disallowed_features,
      const ContextCreationAttribs& attrib_helper) override;
  void Destroy(bool have_context) override;
  bool MakeCurrent() override;
  gl::GLContext* GetGLContext() override;
  gl::GLSurface* GetGLSurface() override;
  const gles2::FeatureInfo* GetFeatureInfo() const override {
    return feature_info();
  }
  Capabilities GetCapabilities() override;
  const gles2::ContextState* GetContextState() override;

  // TODO(penghuang): Remove unused context state related methods.
  void RestoreGlobalState() const override;
  void ClearAllAttributes() const override;
  void RestoreAllAttributes() const override;
  void RestoreState(const gles2::ContextState* prev_state) override;
  void RestoreActiveTexture() const override;
  void RestoreAllTextureUnitAndSamplerBindings(
      const gles2::ContextState* prev_state) const override;
  void RestoreActiveTextureUnitBinding(unsigned int target) const override;
  void RestoreBufferBinding(unsigned int target) override;
  void RestoreBufferBindings() const override;
  void RestoreFramebufferBindings() const override;
  void RestoreRenderbufferBindings() override;
  void RestoreProgramBindings() const override;
  void RestoreTextureState(unsigned service_id) override;
  void RestoreTextureUnitBindings(unsigned unit) const override;
  void RestoreVertexAttribArray(unsigned index) override;
  void RestoreAllExternalTextureBindingsIfNeeded() override;
  QueryManager* GetQueryManager() override;

  void SetQueryCallback(unsigned int query_client_id,
                        base::OnceClosure callback) override;
  gles2::GpuFenceManager* GetGpuFenceManager() override;
  bool HasPendingQueries() const override;
  void ProcessPendingQueries(bool did_finish) override;
  bool HasMoreIdleWork() const override;
  void PerformIdleWork() override;
  bool HasPollingWork() const override;
  void PerformPollingWork() override;
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
  bool WasContextLost() const override;
  bool WasContextLostByRobustnessExtension() const override;
  void MarkContextLost(error::ContextLostReason reason) override;
  bool CheckResetStatus() override;
  void BeginDecoding() override;
  void EndDecoding() override;
  const char* GetCommandName(unsigned int command_id) const;
  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) override;
  base::StringPiece GetLogPrefix() override;
  void BindImage(uint32_t client_texture_id,
                 uint32_t texture_target,
                 gl::GLImage* image,
                 bool can_bind_to_sampler) override;
  gles2::ContextGroup* GetContextGroup() override;
  gles2::ErrorState* GetErrorState() override;
  std::unique_ptr<gles2::AbstractTexture> CreateAbstractTexture(
      GLenum target,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLint border,
      GLenum format,
      GLenum type) override;
  bool IsCompressedTextureFormat(unsigned format) override;
  bool ClearLevel(gles2::Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override;
  bool ClearCompressedTextureLevel(gles2::Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override;
  bool ClearCompressedTextureLevel3D(gles2::Texture* texture,
                                     unsigned target,
                                     int level,
                                     unsigned format,
                                     int width,
                                     int height,
                                     int depth) override;
  bool ClearLevel3D(gles2::Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override {
    NOTIMPLEMENTED();
    return false;
  }
  int GetRasterDecoderId() const override;
  int DecoderIdForTest() override;
  ServiceTransferCache* GetTransferCacheForTest() override;
  void SetUpForRasterCHROMIUMForTest() override;
  void SetOOMErrorForTest() override;
  void DisableFlushWorkaroundForTest() override;

  // ErrorClientState implementation.
  void OnContextLostError() override;
  void OnOutOfMemoryError() override;

  gles2::Logger* GetLogger() override;

  void SetIgnoreCachedStateForTest(bool ignore) override;
  gles2::ImageManager* GetImageManagerForTest() override;

  void SetCopyTextureResourceManagerForTest(
      gles2::CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager)
      override;

  // ServiceFontManager::Client implementation.
  scoped_refptr<Buffer> GetShmBuffer(uint32_t shm_id) override;
  void ReportProgress() override;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

 private:
  gles2::ContextState* state() const {
    if (use_passthrough_) {
      NOTREACHED();
      return nullptr;
    }
    return shared_context_state_->context_state();
  }
  gl::GLApi* api() const { return api_; }
  GrDirectContext* gr_context() const {
    return shared_context_state_->gr_context();
  }
  ServiceTransferCache* transfer_cache() {
    return shared_context_state_->transfer_cache();
  }

  const gles2::FeatureInfo* feature_info() const {
    return shared_context_state_->feature_info();
  }

  const gles2::FeatureInfo::FeatureFlags& features() const {
    return feature_info()->feature_flags();
  }

  const GpuDriverBugWorkarounds& workarounds() const {
    return feature_info()->workarounds();
  }

  void FlushToWorkAroundMacCrashes() {
#if defined(OS_MAC)
    if (!shared_context_state_->GrContextIsGL())
      return;
    // This function does aggressive flushes to work around crashes in the
    // macOS OpenGL driver.
    // https://crbug.com/906453
    if (!flush_workaround_disabled_for_test_) {
      TRACE_EVENT0("gpu", "RasterDecoderImpl::FlushToWorkAroundMacCrashes");
      if (gr_context())
        gr_context()->flushAndSubmit();
      api()->glFlushFn();

      // Flushes can be expensive, yield to allow interruption after each flush.
      ExitCommandProcessingEarly();
    }
#endif
  }

  const gl::GLVersionInfo& gl_version_info() {
    return feature_info()->gl_version_info();
  }

  // Set remaining commands to process to 0 to force DoCommands to return
  // and allow context preemption and GPU watchdog checks in
  // CommandExecutor().
  void ExitCommandProcessingEarly() override;

  template <bool DebugImpl>
  error::Error DoCommandsImpl(unsigned int num_commands,
                              const volatile void* buffer,
                              int num_entries,
                              int* entries_processed);

  bool GenQueriesEXTHelper(GLsizei n, const GLuint* client_ids);
  void DeleteQueriesEXTHelper(GLsizei n, const volatile GLuint* client_ids);
  void DoFinish();
  void DoFlush();
  void DoGetIntegerv(GLenum pname, GLint* params, GLsizei params_size);
  void DoTraceEndCHROMIUM();
  bool InitializeCopyTexImageBlitter();
  bool InitializeCopyTextureCHROMIUM();
  void DoCopySubTextureINTERNAL(GLint xoffset,
                                GLint yoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLboolean unpack_flip_y,
                                const volatile GLbyte* mailboxes);
  void DoCopySubTextureINTERNALGLPassthrough(GLint xoffset,
                                             GLint yoffset,
                                             GLint x,
                                             GLint y,
                                             GLsizei width,
                                             GLsizei height,
                                             GLboolean unpack_flip_y,
                                             const Mailbox& source_mailbox,
                                             const Mailbox& dest_mailbox);
  void DoCopySubTextureINTERNALGL(GLint xoffset,
                                  GLint yoffset,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height,
                                  GLboolean unpack_flip_y,
                                  const Mailbox& source_mailbox,
                                  const Mailbox& dest_mailbox);
  void DoCopySubTextureINTERNALSkia(GLint xoffset,
                                    GLint yoffset,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLboolean unpack_flip_y,
                                    const Mailbox& source_mailbox,
                                    const Mailbox& dest_mailbox);
  void DoWritePixelsINTERNAL(GLint x_offset,
                             GLint y_offset,
                             GLuint src_width,
                             GLuint src_height,
                             GLuint row_bytes,
                             GLuint src_sk_color_type,
                             GLuint src_sk_alpha_type,
                             GLint shm_id,
                             GLuint shm_offset,
                             GLuint shm_size,
                             const volatile GLbyte* mailbox);
  void DoReadbackImagePixelsINTERNAL(GLint src_x,
                                     GLint src_y,
                                     GLuint dst_width,
                                     GLuint dst_height,
                                     GLuint row_bytes,
                                     GLuint dst_sk_color_type,
                                     GLuint dst_sk_alpha_type,
                                     GLint shm_id,
                                     GLuint shm_offset,
                                     GLuint pixels_offset,
                                     const volatile GLbyte* mailbox);
  void DoConvertYUVMailboxesToRGBINTERNAL(GLenum yuv_color_space,
                                          GLboolean is_nv12,
                                          const volatile GLbyte* mailboxes);

  void DoLoseContextCHROMIUM(GLenum current, GLenum other) { NOTIMPLEMENTED(); }
  void DoBeginRasterCHROMIUM(GLuint sk_color,
                             GLuint msaa_sample_count,
                             GLboolean can_use_lcd_text,
                             const volatile GLbyte* key);
  void DoRasterCHROMIUM(GLuint raster_shm_id,
                        GLuint raster_shm_offset,
                        GLuint raster_shm_size,
                        GLuint font_shm_id,
                        GLuint font_shm_offset,
                        GLuint font_shm_size);
  void DoEndRasterCHROMIUM();
  void DoCreateTransferCacheEntryINTERNAL(GLuint entry_type,
                                          GLuint entry_id,
                                          GLuint handle_shm_id,
                                          GLuint handle_shm_offset,
                                          GLuint data_shm_id,
                                          GLuint data_shm_offset,
                                          GLuint data_size);
  void DoUnlockTransferCacheEntryINTERNAL(GLuint entry_type, GLuint entry_id);
  void DoDeleteTransferCacheEntryINTERNAL(GLuint entry_type, GLuint entry_id);
  void RestoreStateForAttrib(GLuint attrib, bool restore_array_binding);
  void DeletePaintCacheTextBlobsINTERNALHelper(
      GLsizei n,
      const volatile GLuint* paint_cache_ids);
  void DeletePaintCachePathsINTERNALHelper(
      GLsizei n,
      const volatile GLuint* paint_cache_ids);
  void DoClearPaintCacheINTERNAL();

  // Generates a DDL, if necessary, and compiles shaders requires to raster it.
  // Returns false each time a shader needed to be compiled and the decoder
  // should yield. Returns true once all shaders in the DDL have been compiled.
  bool EnsureDDLReadyForRaster();

  void FlushAndSubmitIfNecessary(
      SkSurface* surface,
      std::vector<GrBackendSemaphore> signal_semaphores) {
    if (signal_semaphores.empty()) {
      surface->flush();
      return;
    }

    // Always flush the surface even if source_scoped_access.success() is
    // false, so the begin_semaphores can be released, and end_semaphores can
    // be signalled.
    GrFlushInfo flush_info = {
        .fNumSemaphores = signal_semaphores.size(),
        .fSignalSemaphores = signal_semaphores.data(),
    };
    gpu::AddVulkanCleanupTaskForSkiaFlush(
        shared_context_state_->vk_context_provider(), &flush_info);
    auto result = surface->flush(flush_info);
    // If the |signal_semaphores| is empty, we can deferred the queue
    // submission.
    DCHECK_EQ(result, GrSemaphoresSubmitted::kYes);
    gr_context()->submit();
  }

#if defined(NDEBUG)
  void LogClientServiceMapping(const char* /* function_name */,
                               GLuint /* client_id */,
                               GLuint /* service_id */) {}
  template <typename T>
  void LogClientServiceForInfo(T* /* info */,
                               GLuint /* client_id */,
                               const char* /* function_name */) {}
#else
  void LogClientServiceMapping(const char* function_name,
                               GLuint client_id,
                               GLuint service_id) {
    if (gpu_preferences_.enable_gpu_service_logging_gpu) {
      VLOG(1) << "[" << logger_.GetLogPrefix() << "] " << function_name
              << ": client_id = " << client_id
              << ", service_id = " << service_id;
    }
  }
  template <typename T>
  void LogClientServiceForInfo(T* info,
                               GLuint client_id,
                               const char* function_name) {
    if (info) {
      LogClientServiceMapping(function_name, client_id, info->service_id());
    }
  }
#endif

// Generate a member function prototype for each command in an automated and
// typesafe way.
#define RASTER_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);

  RASTER_COMMAND_LIST(RASTER_CMD_OP)
#undef RASTER_CMD_OP

  typedef error::Error (RasterDecoderImpl::*CmdHandler)(
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
  static const CommandInfo command_info[kNumCommands - kFirstRasterCommand];

  const int raster_decoder_id_;

  // Number of commands remaining to be processed in DoCommands().
  int commands_to_process_ = 0;

  bool supports_gpu_raster_ = false;
  bool supports_oop_raster_ = false;
  bool use_passthrough_ = false;
  bool use_ddl_ = false;

  // The current decoder error communicates the decoder error through command
  // processing functions that do not return the error value. Should be set
  // only if not returning an error.
  error::Error current_decoder_error_ = error::kNoError;

  GpuPreferences gpu_preferences_;

  gles2::DebugMarkerManager debug_marker_manager_;
  gles2::Logger logger_;
  std::unique_ptr<gles2::ErrorState> error_state_;
  bool context_lost_ = false;

  scoped_refptr<SharedContextState> shared_context_state_;
  std::unique_ptr<Validators> validators_;

  SharedImageRepresentationFactory shared_image_representation_factory_;
  std::unique_ptr<QueryManager> query_manager_;

  gles2::GLES2Util util_;

  // An optional behaviour to lose the context when OOM.
  bool lose_context_when_out_of_memory_ = false;

  std::unique_ptr<gles2::CopyTexImageResourceManager> copy_tex_image_blit_;
  std::unique_ptr<gles2::CopyTextureCHROMIUMResourceManager>
      copy_texture_chromium_;

  std::unique_ptr<gles2::GPUTracer> gpu_tracer_;
  const unsigned char* gpu_decoder_category_;
  static constexpr int gpu_trace_level_ = 2;
  bool gpu_trace_commands_ = false;
  bool gpu_debug_commands_ = false;

  // Raster helpers.
  scoped_refptr<ServiceFontManager> font_manager_;
  std::unique_ptr<SharedImageRepresentationSkia> shared_image_;
  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_shared_image_write_;
  SkSurface* sk_surface_ = nullptr;
  std::unique_ptr<SharedImageProviderImpl> paint_op_shared_image_provider_;

  sk_sp<SkSurface> sk_surface_for_testing_;
  std::vector<GrBackendSemaphore> end_semaphores_;
  std::unique_ptr<cc::ServicePaintCache> paint_cache_;

  std::unique_ptr<SkDeferredDisplayListRecorder> recorder_;
  sk_sp<SkDeferredDisplayList> ddl_;
  base::Optional<SkDeferredDisplayList::ProgramIterator> program_iterator_;
  SkCanvas* raster_canvas_ = nullptr;  // ptr into recorder_ or sk_surface_
  std::vector<SkDiscardableHandleId> locked_handles_;

  // Tracing helpers.
  int raster_chromium_id_ = 0;

  // Workaround for https://crbug.com/906453
  bool flush_workaround_disabled_for_test_ = false;

  bool in_copy_sub_texture_ = false;
  bool reset_texture_state_ = false;

  bool is_privileged_ = false;

  gl::GLApi* api_ = nullptr;

  base::WeakPtrFactory<DecoderContext> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RasterDecoderImpl);
};

constexpr RasterDecoderImpl::CommandInfo RasterDecoderImpl::command_info[] = {
#define RASTER_CMD_OP(name)                                \
  {                                                        \
      &RasterDecoderImpl::Handle##name,                    \
      cmds::name::kArgFlags,                               \
      cmds::name::cmd_flags,                               \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1, \
  }, /* NOLINT */
    RASTER_COMMAND_LIST(RASTER_CMD_OP)
#undef RASTER_CMD_OP
};

// static
RasterDecoder* RasterDecoder::Create(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    gles2::Outputter* outputter,
    const GpuFeatureInfo& gpu_feature_info,
    const GpuPreferences& gpu_preferences,
    MemoryTracker* memory_tracker,
    SharedImageManager* shared_image_manager,
    scoped_refptr<SharedContextState> shared_context_state,
    bool is_privileged) {
  return new RasterDecoderImpl(client, command_buffer_service, outputter,
                               gpu_feature_info, gpu_preferences,
                               memory_tracker, shared_image_manager,
                               std::move(shared_context_state), is_privileged);
}

RasterDecoder::RasterDecoder(DecoderClient* client,
                             CommandBufferServiceBase* command_buffer_service,
                             gles2::Outputter* outputter)
    : CommonDecoder(client, command_buffer_service), outputter_(outputter) {}

RasterDecoder::~RasterDecoder() {}

bool RasterDecoder::initialized() const {
  return initialized_;
}

TextureBase* RasterDecoder::GetTextureBase(uint32_t client_id) {
  return nullptr;
}

void RasterDecoder::SetLevelInfo(uint32_t client_id,
                                 int level,
                                 unsigned internal_format,
                                 unsigned width,
                                 unsigned height,
                                 unsigned depth,
                                 unsigned format,
                                 unsigned type,
                                 const gfx::Rect& cleared_rect) {}

void RasterDecoder::BeginDecoding() {}

void RasterDecoder::EndDecoding() {}

void RasterDecoder::SetLogCommands(bool log_commands) {
  log_commands_ = log_commands;
}

gles2::Outputter* RasterDecoder::outputter() const {
  return outputter_;
}

base::StringPiece RasterDecoder::GetLogPrefix() {
  return GetLogger()->GetLogPrefix();
}

RasterDecoderImpl::RasterDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    gles2::Outputter* outputter,
    const GpuFeatureInfo& gpu_feature_info,
    const GpuPreferences& gpu_preferences,
    MemoryTracker* memory_tracker,
    SharedImageManager* shared_image_manager,
    scoped_refptr<SharedContextState> shared_context_state,
    bool is_privileged)
    : RasterDecoder(client, command_buffer_service, outputter),
      raster_decoder_id_(g_raster_decoder_id.GetNext() + 1),
      supports_gpu_raster_(
          gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_RASTERIZATION] ==
          kGpuFeatureStatusEnabled),
      use_passthrough_(gles2::PassthroughCommandDecoderSupported() &&
                       gpu_preferences.use_passthrough_cmd_decoder),
      gpu_preferences_(gpu_preferences),
      logger_(&debug_marker_manager_,
              base::BindRepeating(&DecoderClient::OnConsoleMessage,
                                  base::Unretained(client),
                                  0),
              gpu_preferences_.disable_gl_error_limit),
      error_state_(gles2::ErrorState::Create(this, &logger_)),
      shared_context_state_(std::move(shared_context_state)),
      validators_(new Validators),
      shared_image_representation_factory_(shared_image_manager,
                                           memory_tracker),
      gpu_decoder_category_(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.decoder"))),
      font_manager_(base::MakeRefCounted<ServiceFontManager>(
          this,
          gpu_preferences_.disable_oopr_debug_crash_dump)),
      is_privileged_(is_privileged) {
  DCHECK(shared_context_state_);
  shared_context_state_->AddContextLostObserver(this);
}

RasterDecoderImpl::~RasterDecoderImpl() {
  shared_context_state_->RemoveContextLostObserver(this);
}

base::WeakPtr<DecoderContext> RasterDecoderImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ContextResult RasterDecoderImpl::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const gles2::DisallowedFeatures& disallowed_features,
    const ContextCreationAttribs& attrib_helper) {
  TRACE_EVENT0("gpu", "RasterDecoderImpl::Initialize");
  DCHECK(shared_context_state_->IsCurrent(nullptr));

  api_ = gl::g_current_gl_context;

  set_initialized();

  if (!offscreen) {
    return ContextResult::kFatalFailure;
  }

  if (gpu_preferences_.enable_gpu_debugging)
    set_debug(true);

  if (gpu_preferences_.enable_gpu_command_logging)
    SetLogCommands(true);

  DCHECK_EQ(surface.get(), shared_context_state_->surface());
  DCHECK_EQ(context.get(), shared_context_state_->context());

  // Create GPU Tracer for timing values.
  gpu_tracer_.reset(
      new gles2::GPUTracer(this, shared_context_state_->GrContextIsGL()));

  // Save the loseContextWhenOutOfMemory context creation attribute.
  lose_context_when_out_of_memory_ =
      attrib_helper.lose_context_when_out_of_memory;

  CHECK_GL_ERROR();

  query_manager_ = std::make_unique<QueryManager>();

  if (attrib_helper.enable_oop_rasterization) {
    if (!features().chromium_raster_transport) {
      LOG(ERROR) << "ContextResult::kFatalFailure: "
                    "chromium_raster_transport not present";
      Destroy(true);
      return ContextResult::kFatalFailure;
    }

    supports_oop_raster_ = !!shared_context_state_->gr_context();
    if (supports_oop_raster_)
      paint_cache_ = std::make_unique<cc::ServicePaintCache>();
    use_ddl_ = gpu_preferences_.enable_oop_rasterization_ddl;
  }

  return ContextResult::kSuccess;
}

void RasterDecoderImpl::Destroy(bool have_context) {
  if (!initialized())
    return;

  DCHECK(!have_context || shared_context_state_->context()->IsCurrent(nullptr));

  if (have_context) {
    if (supports_oop_raster_) {
      transfer_cache()->DeleteAllEntriesForDecoder(raster_decoder_id_);
    }

    if (copy_tex_image_blit_) {
      copy_tex_image_blit_->Destroy();
      copy_tex_image_blit_.reset();
    }

    if (copy_texture_chromium_) {
      copy_texture_chromium_->Destroy();
      copy_texture_chromium_.reset();
    }

    // Make sure we flush any pending skia work on this context.
    if (sk_surface_) {
      GrFlushInfo flush_info = {
          .fNumSemaphores = end_semaphores_.size(),
          .fSignalSemaphores = end_semaphores_.data(),
      };
      AddVulkanCleanupTaskForSkiaFlush(
          shared_context_state_->vk_context_provider(), &flush_info);
      auto result = sk_surface_->flush(flush_info);
      DCHECK(result == GrSemaphoresSubmitted::kYes || end_semaphores_.empty());
      end_semaphores_.clear();
      sk_surface_ = nullptr;
    }

    if (gr_context())
      gr_context()->flushAndSubmit();

    scoped_shared_image_write_.reset();
    shared_image_.reset();
    sk_surface_for_testing_.reset();
    paint_op_shared_image_provider_.reset();
  }

  copy_tex_image_blit_.reset();
  copy_texture_chromium_.reset();

  if (query_manager_) {
    query_manager_->Destroy(have_context);
    query_manager_.reset();
  }

  font_manager_->Destroy();
  font_manager_.reset();
}

// Make this decoder's GL context current.
bool RasterDecoderImpl::MakeCurrent() {
  if (!shared_context_state_->GrContextIsGL())
    return true;

  if (context_lost_) {
    LOG(ERROR) << "  RasterDecoderImpl: Trying to make lost context current.";
    return false;
  }

  if (shared_context_state_->context_lost() ||
      !shared_context_state_->MakeCurrent(nullptr)) {
    LOG(ERROR) << "  RasterDecoderImpl: Context lost during MakeCurrent.";
    return false;
  }

  DCHECK_EQ(api(), gl::g_current_gl_context);

  // Rebind textures if the service ids may have changed.
  RestoreAllExternalTextureBindingsIfNeeded();

  return true;
}

gl::GLContext* RasterDecoderImpl::GetGLContext() {
  return shared_context_state_->GrContextIsGL()
             ? shared_context_state_->context()
             : nullptr;
}

gl::GLSurface* RasterDecoderImpl::GetGLSurface() {
  return shared_context_state_->GrContextIsGL()
             ? shared_context_state_->surface()
             : nullptr;
}

Capabilities RasterDecoderImpl::GetCapabilities() {
  // TODO(enne): reconcile this with gles2_cmd_decoder's capability settings.
  Capabilities caps;
  caps.gpu_rasterization = supports_gpu_raster_;
  caps.supports_oop_raster = supports_oop_raster_;
  caps.gpu_memory_buffer_formats =
      feature_info()->feature_flags().gpu_memory_buffer_formats;
  caps.texture_target_exception_list =
      gpu_preferences_.texture_target_exception_list;
  caps.texture_format_bgra8888 =
      feature_info()->feature_flags().ext_texture_format_bgra8888;
  caps.texture_storage_image =
      feature_info()->feature_flags().chromium_texture_storage_image;
  caps.texture_storage = feature_info()->feature_flags().ext_texture_storage;
  // TODO(piman): have a consistent limit in shared image backings.
  // https://crbug.com/960588
  if (shared_context_state_->GrContextIsGL()) {
    api()->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &caps.max_texture_size);
  } else if (shared_context_state_->GrContextIsVulkan()) {
#if BUILDFLAG(ENABLE_VULKAN)
    caps.max_texture_size = shared_context_state_->vk_context_provider()
                                ->GetDeviceQueue()
                                ->vk_physical_device_properties()
                                .limits.maxImageDimension2D;
#else
    NOTREACHED();
#endif
  } else if (shared_context_state_->GrContextIsDawn()) {
    // TODO(crbug.com/1090476): Query Dawn for this value once an API exists for
    // capabilities.
    caps.max_texture_size = 8192;
  } else {
    NOTIMPLEMENTED();
  }
  if (feature_info()->workarounds().max_texture_size) {
    caps.max_texture_size = std::min(
        caps.max_texture_size, feature_info()->workarounds().max_texture_size);
    caps.max_cube_map_texture_size =
        std::min(caps.max_cube_map_texture_size,
                 feature_info()->workarounds().max_texture_size);
  }
  if (feature_info()->workarounds().max_3d_array_texture_size) {
    caps.max_3d_texture_size =
        std::min(caps.max_3d_texture_size,
                 feature_info()->workarounds().max_3d_array_texture_size);
    caps.max_array_texture_layers =
        std::min(caps.max_array_texture_layers,
                 feature_info()->workarounds().max_3d_array_texture_size);
  }
  caps.sync_query = feature_info()->feature_flags().chromium_sync_query;
  caps.msaa_is_slow = feature_info()->workarounds().msaa_is_slow;
  caps.avoid_stencil_buffers =
      feature_info()->workarounds().avoid_stencil_buffers;

  if (gr_context()) {
    caps.context_supports_distance_field_text =
        gr_context()->supportsDistanceFieldText();
    caps.texture_norm16 =
        gr_context()->colorTypeSupportedAsImage(kA16_unorm_SkColorType);
    caps.texture_half_float_linear =
        gr_context()->colorTypeSupportedAsImage(kA16_float_SkColorType);
  } else {
    caps.texture_norm16 = feature_info()->feature_flags().ext_texture_norm16;
    caps.texture_half_float_linear =
        feature_info()->feature_flags().enable_texture_half_float_linear;
  }
  return caps;
}

const gles2::ContextState* RasterDecoderImpl::GetContextState() {
  NOTREACHED();
  return nullptr;
}

void RasterDecoderImpl::RestoreGlobalState() const {
  // We mark the context state is dirty instead of restoring global
  // state, and the global state will be restored by the next context.
  shared_context_state_->set_need_context_state_reset(true);
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::ClearAllAttributes() const {}

void RasterDecoderImpl::RestoreAllAttributes() const {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreState(const gles2::ContextState* prev_state) {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreActiveTexture() const {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreAllTextureUnitAndSamplerBindings(
    const gles2::ContextState* prev_state) const {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreActiveTextureUnitBinding(
    unsigned int target) const {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreBufferBinding(unsigned int target) {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreBufferBindings() const {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreFramebufferBindings() const {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreRenderbufferBindings() {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreProgramBindings() const {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreTextureState(unsigned service_id) {
  DCHECK(in_copy_sub_texture_);
  reset_texture_state_ = true;
}

void RasterDecoderImpl::RestoreTextureUnitBindings(unsigned unit) const {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreVertexAttribArray(unsigned index) {
  shared_context_state_->PessimisticallyResetGrContext();
}

void RasterDecoderImpl::RestoreAllExternalTextureBindingsIfNeeded() {
  shared_context_state_->PessimisticallyResetGrContext();
}

QueryManager* RasterDecoderImpl::GetQueryManager() {
  return query_manager_.get();
}

void RasterDecoderImpl::SetQueryCallback(unsigned int query_client_id,
                                         base::OnceClosure callback) {
  QueryManager::Query* query = query_manager_->GetQuery(query_client_id);
  if (query) {
    query->AddCallback(std::move(callback));
  } else {
    VLOG(1) << "RasterDecoderImpl::SetQueryCallback: No query with ID "
            << query_client_id << ". Running the callback immediately.";
    std::move(callback).Run();
  }
}

gles2::GpuFenceManager* RasterDecoderImpl::GetGpuFenceManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool RasterDecoderImpl::HasPendingQueries() const {
  return query_manager_ && query_manager_->HavePendingQueries();
}

void RasterDecoderImpl::ProcessPendingQueries(bool did_finish) {
  if (query_manager_)
    query_manager_->ProcessPendingQueries(did_finish);
}

bool RasterDecoderImpl::HasMoreIdleWork() const {
  return gpu_tracer_->HasTracesToProcess();
}

void RasterDecoderImpl::PerformIdleWork() {
  gpu_tracer_->ProcessTraces();
}

bool RasterDecoderImpl::HasPollingWork() const {
  return false;
}

void RasterDecoderImpl::PerformPollingWork() {}

TextureBase* RasterDecoderImpl::GetTextureBase(uint32_t client_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

void RasterDecoderImpl::SetLevelInfo(uint32_t client_id,
                                     int level,
                                     unsigned internal_format,
                                     unsigned width,
                                     unsigned height,
                                     unsigned depth,
                                     unsigned format,
                                     unsigned type,
                                     const gfx::Rect& cleared_rect) {
  NOTIMPLEMENTED();
}

bool RasterDecoderImpl::WasContextLost() const {
  return shared_context_state_->context_lost();
}

bool RasterDecoderImpl::WasContextLostByRobustnessExtension() const {
  return shared_context_state_->device_needs_reset();
}

void RasterDecoderImpl::MarkContextLost(error::ContextLostReason reason) {
  shared_context_state_->MarkContextLost(reason);
}

void RasterDecoderImpl::OnContextLost() {
  DCHECK(shared_context_state_->context_lost());
  command_buffer_service()->SetContextLostReason(
      *shared_context_state_->context_lost_reason());
  current_decoder_error_ = error::kLostContext;
}

bool RasterDecoderImpl::CheckResetStatus() {
  DCHECK(!WasContextLost());
  return shared_context_state_->CheckResetStatus(/*needs_gl=*/false);
}

gles2::Logger* RasterDecoderImpl::GetLogger() {
  return &logger_;
}

void RasterDecoderImpl::SetIgnoreCachedStateForTest(bool ignore) {
  if (use_passthrough_)
    return;
  state()->SetIgnoreCachedStateForTest(ignore);
}

gles2::ImageManager* RasterDecoderImpl::GetImageManagerForTest() {
  NOTREACHED();
  return nullptr;
}

void RasterDecoderImpl::SetCopyTextureResourceManagerForTest(
    gles2::CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager) {
  copy_texture_chromium_.reset(copy_texture_resource_manager);
}

void RasterDecoderImpl::BeginDecoding() {
  gpu_tracer_->BeginDecoding();
  gpu_trace_commands_ = gpu_tracer_->IsTracing() && *gpu_decoder_category_;
  gpu_debug_commands_ = log_commands() || debug() || gpu_trace_commands_;
  query_manager_->BeginProcessingCommands();
}

void RasterDecoderImpl::EndDecoding() {
  gpu_tracer_->EndDecoding();
  query_manager_->EndProcessingCommands();
}

const char* RasterDecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstRasterCommand && command_id < kNumCommands) {
    return raster::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

template <bool DebugImpl>
error::Error RasterDecoderImpl::DoCommandsImpl(unsigned int num_commands,
                                               const volatile void* buffer,
                                               int num_entries,
                                               int* entries_processed) {
  DCHECK(entries_processed);
  commands_to_process_ = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;
  CommandId command = static_cast<CommandId>(0);

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process_--) {
    const unsigned int size = cmd_data->value_header.size;
    command = static_cast<CommandId>(cmd_data->value_header.command);

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
    unsigned int command_index = command - kFirstRasterCommand;
    if (command_index < base::size(command_info)) {
      const CommandInfo& info = command_info[command_index];
      if (sk_surface_) {
        if (!AllowedBetweenBeginEndRaster(command)) {
          LOCAL_SET_GL_ERROR(
              GL_INVALID_OPERATION, GetCommandName(command),
              "Unexpected command between BeginRasterCHROMIUM and "
              "EndRasterCHROMIUM");
          process_pos += size;
          cmd_data += size;
          continue;
        }
      }
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        bool doing_gpu_trace = false;
        if (DebugImpl && gpu_trace_commands_) {
          if (CMD_FLAG_GET_TRACE_LEVEL(info.cmd_flags) <= gpu_trace_level_) {
            doing_gpu_trace = true;
            gpu_tracer_->Begin(TRACE_DISABLED_BY_DEFAULT("gpu.decoder"),
                               GetCommandName(command), gles2::kTraceDecoder);
          }
        }

        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);

        if (DebugImpl && doing_gpu_trace)
          gpu_tracer_->End(gles2::kTraceDecoder);

        if (DebugImpl && shared_context_state_->GrContextIsGL() && debug() &&
            !WasContextLost()) {
          GLenum error;
          while ((error = api()->glGetErrorFn()) != GL_NO_ERROR) {
            LOG(ERROR) << "[" << logger_.GetLogPrefix() << "] "
                       << "GL ERROR: " << gles2::GLES2Util::GetStringEnum(error)
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

    // Workaround for https://crbug.com/906453: Flush after every command that
    // is not between a BeginRaster and EndRaster.
    if (!sk_surface_)
      FlushToWorkAroundMacCrashes();
  }

  *entries_processed = process_pos;

  if (error::IsError(result)) {
    LOG(ERROR) << "Error: " << result << " for Command "
               << GetCommandName(command);
  }

  if (supports_oop_raster_)
    client()->ScheduleGrContextCleanup();

  return result;
}

error::Error RasterDecoderImpl::DoCommands(unsigned int num_commands,
                                           const volatile void* buffer,
                                           int num_entries,
                                           int* entries_processed) {
  if (gpu_debug_commands_) {
    return DoCommandsImpl<true>(num_commands, buffer, num_entries,
                                entries_processed);
  } else {
    return DoCommandsImpl<false>(num_commands, buffer, num_entries,
                                 entries_processed);
  }
}

void RasterDecoderImpl::ExitCommandProcessingEarly() {
  commands_to_process_ = 0;
}

base::StringPiece RasterDecoderImpl::GetLogPrefix() {
  return logger_.GetLogPrefix();
}

void RasterDecoderImpl::BindImage(uint32_t client_texture_id,
                                  uint32_t texture_target,
                                  gl::GLImage* image,
                                  bool can_bind_to_sampler) {
  NOTIMPLEMENTED();
}

gles2::ContextGroup* RasterDecoderImpl::GetContextGroup() {
  return nullptr;
}

gles2::ErrorState* RasterDecoderImpl::GetErrorState() {
  return error_state_.get();
}

std::unique_ptr<gles2::AbstractTexture>
RasterDecoderImpl::CreateAbstractTexture(GLenum target,
                                         GLenum internal_format,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLenum format,
                                         GLenum type) {
  return nullptr;
}

bool RasterDecoderImpl::IsCompressedTextureFormat(unsigned format) {
  return feature_info()->validators()->compressed_texture_format.IsValid(
      format);
}

bool RasterDecoderImpl::ClearLevel(gles2::Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   unsigned type,
                                   int xoffset,
                                   int yoffset,
                                   int width,
                                   int height) {
  DCHECK(target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY &&
         target != GL_TEXTURE_EXTERNAL_OES);
  uint32_t channels = gles2::GLES2Util::GetChannelsForFormat(format);
  if (channels & gles2::GLES2Util::kDepth) {
    DCHECK(false) << "depth not supported";
    return false;
  }

  static constexpr uint32_t kMaxZeroSize = 1024 * 1024 * 4;

  uint32_t size;
  uint32_t padded_row_size;
  constexpr GLint unpack_alignment = 4;
  if (!gles2::GLES2Util::ComputeImageDataSizes(width, height, 1, format, type,
                                               unpack_alignment, &size, nullptr,
                                               &padded_row_size)) {
    return false;
  }

  TRACE_EVENT1("gpu", "RasterDecoderImpl::ClearLevel", "size", size);

  int tile_height;

  if (size > kMaxZeroSize) {
    if (kMaxZeroSize < padded_row_size) {
      // That'd be an awfully large texture.
      return false;
    }
    // We should never have a large total size with a zero row size.
    DCHECK_GT(padded_row_size, 0U);
    tile_height = kMaxZeroSize / padded_row_size;
    if (!gles2::GLES2Util::ComputeImageDataSizes(width, tile_height, 1, format,
                                                 type, unpack_alignment, &size,
                                                 nullptr, nullptr)) {
      return false;
    }
  } else {
    tile_height = height;
  }

  {
    ScopedTextureBinder binder(state(), texture->target(),
                               texture->service_id(), gr_context());
    base::Optional<ScopedPixelUnpackState> pixel_unpack_state;
    if (shared_context_state_->need_context_state_reset()) {
      pixel_unpack_state.emplace(state(), gr_context(), feature_info());
    }
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    // Assumes the size has already been checked.
    std::unique_ptr<char[]> zero(new char[size]);
    memset(zero.get(), 0, size);
    GLint y = 0;
    while (y < height) {
      GLint h = y + tile_height > height ? height - y : tile_height;
      api()->glTexSubImage2DFn(
          target, level, xoffset, yoffset + y, width, h,
          gles2::TextureManager::AdjustTexFormat(feature_info(), format), type,
          zero.get());
      y += tile_height;
    }
  }
  DCHECK(glGetError() == GL_NO_ERROR);
  return true;
}

bool RasterDecoderImpl::ClearCompressedTextureLevel(gles2::Texture* texture,
                                                    unsigned target,
                                                    int level,
                                                    unsigned format,
                                                    int width,
                                                    int height) {
  NOTREACHED();
  return false;
}

bool RasterDecoderImpl::ClearCompressedTextureLevel3D(gles2::Texture* texture,
                                                      unsigned target,
                                                      int level,
                                                      unsigned format,
                                                      int width,
                                                      int height,
                                                      int depth) {
  NOTREACHED();
  return false;
}

int RasterDecoderImpl::GetRasterDecoderId() const {
  return raster_decoder_id_;
}

int RasterDecoderImpl::DecoderIdForTest() {
  return raster_decoder_id_;
}

ServiceTransferCache* RasterDecoderImpl::GetTransferCacheForTest() {
  return shared_context_state_->transfer_cache();
}

void RasterDecoderImpl::SetUpForRasterCHROMIUMForTest() {
  // Some tests use mock GL which doesn't work with skia. Just use a bitmap
  // backed surface for OOP raster commands.
  auto info = SkImageInfo::MakeN32(10, 10, kPremul_SkAlphaType,
                                   SkColorSpace::MakeSRGB());
  sk_surface_for_testing_ = SkSurface::MakeRaster(info);
  sk_surface_ = sk_surface_for_testing_.get();
  raster_canvas_ = sk_surface_->getCanvas();
}

void RasterDecoderImpl::SetOOMErrorForTest() {
  LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "SetOOMErrorForTest",
                     "synthetic out of memory");
}

void RasterDecoderImpl::DisableFlushWorkaroundForTest() {
  flush_workaround_disabled_for_test_ = true;
}

void RasterDecoderImpl::OnContextLostError() {
  if (!WasContextLost()) {
    // Need to lose current context before broadcasting!
    shared_context_state_->CheckResetStatus(/*needs_gl=*/false);
  }
}

void RasterDecoderImpl::OnOutOfMemoryError() {
  if (lose_context_when_out_of_memory_ && !WasContextLost()) {
    if (!shared_context_state_->CheckResetStatus(/*needs_gl=*/false)) {
      MarkContextLost(error::kOutOfMemory);
    }
  }
}

error::Error RasterDecoderImpl::HandleBeginQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::BeginQueryEXT& c =
      *static_cast<const volatile raster::cmds::BeginQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint client_id = static_cast<GLuint>(c.id);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);

  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
      break;
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      if (!features().chromium_sync_query) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                           "not enabled for commands completed queries");
        return error::kNoError;
      }
      break;
    default:
      LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, "glBeginQueryEXT",
                         "unknown query target");
      return error::kNoError;
  }

  if (query_manager_->GetActiveQuery(target)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                       "query already in progress");
    return error::kNoError;
  }

  if (client_id == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT", "id is 0");
    return error::kNoError;
  }

  scoped_refptr<Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  QueryManager::Query* query = query_manager_->GetQuery(client_id);
  if (!query) {
    if (!query_manager_->IsValidQuery(client_id)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
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

error::Error RasterDecoderImpl::HandleEndQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::EndQueryEXT& c =
      *static_cast<const volatile raster::cmds::EndQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  QueryManager::Query* query = query_manager_->GetActiveQuery(target);
  if (!query) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glEndQueryEXT",
                       "No active query");
    return error::kNoError;
  }

  query_manager_->EndQuery(query, submit_count);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleQueryCounterEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::QueryCounterEXT& c =
      *static_cast<const volatile raster::cmds::QueryCounterEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint client_id = static_cast<GLuint>(c.id);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  if (target != GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM) {
    LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, "glQueryCounterEXT",
                       "unknown query target");
    return error::kNoError;
  }

  scoped_refptr<Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  QueryManager::Query* query = query_manager_->GetQuery(client_id);
  if (!query) {
    if (!query_manager_->IsValidQuery(client_id)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glQueryCounterEXT",
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

void RasterDecoderImpl::DoFinish() {
  if (shared_context_state_->GrContextIsGL())
    api()->glFinishFn();
  ProcessPendingQueries(true);
}

void RasterDecoderImpl::DoFlush() {
  if (shared_context_state_->GrContextIsGL())
    api()->glFlushFn();
  ProcessPendingQueries(false);
}

bool RasterDecoderImpl::GenQueriesEXTHelper(GLsizei n,
                                            const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (query_manager_->IsValidQuery(client_ids[ii])) {
      return false;
    }
  }
  query_manager_->GenQueries(n, client_ids);
  return true;
}

void RasterDecoderImpl::DeleteQueriesEXTHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    query_manager_->RemoveQuery(client_id);
  }
}

error::Error RasterDecoderImpl::HandleTraceBeginCHROMIUM(
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
  if (!gpu_tracer_->Begin(category_name, trace_name, gles2::kTraceCHROMIUM)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glTraceBeginCHROMIUM",
                       "unable to create begin trace");
    return error::kNoError;
  }
  return error::kNoError;
}

void RasterDecoderImpl::DoTraceEndCHROMIUM() {
  debug_marker_manager_.PopGroup();
  if (!gpu_tracer_->End(gles2::kTraceCHROMIUM)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glTraceEndCHROMIUM",
                       "no trace begin found");
    return;
  }
}

error::Error RasterDecoderImpl::HandleSetActiveURLCHROMIUM(
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

  GURL url(base::StringPiece(url_str, size));
  client()->SetActiveURL(std::move(url));
  return error::kNoError;
}

bool RasterDecoderImpl::InitializeCopyTexImageBlitter() {
  if (!copy_tex_image_blit_.get()) {
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glCopySubTexture");
    copy_tex_image_blit_.reset(
        new gles2::CopyTexImageResourceManager(feature_info()));
    copy_tex_image_blit_->Initialize(this);
    if (LOCAL_PEEK_GL_ERROR("glCopySubTexture") != GL_NO_ERROR)
      return false;
  }
  return true;
}

bool RasterDecoderImpl::InitializeCopyTextureCHROMIUM() {
  // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
  // needed because it takes 10s of milliseconds to initialize.
  if (!copy_texture_chromium_.get()) {
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glCopySubTexture");
    copy_texture_chromium_.reset(
        gles2::CopyTextureCHROMIUMResourceManager::Create());
    copy_texture_chromium_->Initialize(this, features());
    if (LOCAL_PEEK_GL_ERROR("glCopySubTexture") != GL_NO_ERROR)
      return false;

    // On the desktop core profile this also needs emulation of
    // CopyTex{Sub}Image2D for luminance, alpha, and luminance_alpha
    // textures.
    if (gles2::CopyTexImageResourceManager::CopyTexImageRequiresBlit(
            feature_info(), GL_LUMINANCE)) {
      if (!InitializeCopyTexImageBlitter())
        return false;
    }
  }
  return true;
}

void RasterDecoderImpl::DoCopySubTextureINTERNAL(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    const volatile GLbyte* mailboxes) {
  Mailbox source_mailbox = Mailbox::FromVolatile(
      reinterpret_cast<const volatile Mailbox*>(mailboxes)[0]);
  DLOG_IF(ERROR, !source_mailbox.Verify())
      << "CopySubTexture was passed an invalid mailbox";
  Mailbox dest_mailbox = Mailbox::FromVolatile(
      reinterpret_cast<const volatile Mailbox*>(mailboxes)[1]);
  DLOG_IF(ERROR, !dest_mailbox.Verify())
      << "CopySubTexture was passed an invalid mailbox";

  if (source_mailbox == dest_mailbox) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glCopySubTexture",
                       "source and destination mailboxes are the same");
    return;
  }

  if (!shared_context_state_->GrContextIsGL()) {
    // Use Skia to copy texture if raster's gr_context() is not using GL.
    DoCopySubTextureINTERNALSkia(xoffset, yoffset, x, y, width, height,
                                 unpack_flip_y, source_mailbox, dest_mailbox);
  } else if (use_passthrough_) {
    DoCopySubTextureINTERNALGLPassthrough(xoffset, yoffset, x, y, width, height,
                                          unpack_flip_y, source_mailbox,
                                          dest_mailbox);
  } else {
    DoCopySubTextureINTERNALGL(xoffset, yoffset, x, y, width, height,
                               unpack_flip_y, source_mailbox, dest_mailbox);
  }
}

namespace {

GLboolean NeedsUnpackPremultiplyAlpha(
    const SharedImageRepresentation& representation) {
  return representation.alpha_type() == kUnpremul_SkAlphaType;
}

}  // namespace

void RasterDecoderImpl::DoCopySubTextureINTERNALGLPassthrough(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    const Mailbox& source_mailbox,
    const Mailbox& dest_mailbox) {
  DCHECK(source_mailbox != dest_mailbox);
  DCHECK(use_passthrough_);

  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
      source_shared_image =
          shared_image_representation_factory_.ProduceGLTexturePassthrough(
              source_mailbox);
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
      dest_shared_image =
          shared_image_representation_factory_.ProduceGLTexturePassthrough(
              dest_mailbox);
  if (!source_shared_image || !dest_shared_image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture", "unknown mailbox");
    return;
  }

  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough::ScopedAccess>
      source_access = source_shared_image->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kNo);
  if (!source_access) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "unable to access source for read");
    return;
  }

  // Allow uncleared access, as we manually handle clear tracking.
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough::ScopedAccess>
      dest_access = dest_shared_image->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_access) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "unable to access destination for write");
    return;
  }

  gfx::Rect new_cleared_rect;
  gfx::Rect old_cleared_rect = dest_shared_image->ClearedRect();
  gfx::Rect dest_rect(xoffset, yoffset, width, height);
  if (gles2::TextureManager::CombineAdjacentRects(old_cleared_rect, dest_rect,
                                                  &new_cleared_rect)) {
    DCHECK(old_cleared_rect.IsEmpty() ||
           new_cleared_rect.Contains(old_cleared_rect));
  } else {
    // No users of RasterDecoder leverage this functionality. Clearing uncleared
    // regions could be added here if needed.
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "Cannot clear non-combineable rects.");
    return;
  }

  gles2::TexturePassthrough* source_texture =
      source_shared_image->GetTexturePassthrough().get();
  gles2::TexturePassthrough* dest_texture =
      dest_shared_image->GetTexturePassthrough().get();
  DCHECK(!source_texture->is_bind_pending());
  DCHECK_NE(source_texture->service_id(), dest_texture->service_id());

  api()->glCopySubTextureCHROMIUMFn(
      source_texture->service_id(), /*source_level=*/0, dest_texture->target(),
      dest_texture->service_id(),
      /*dest_level=*/0, xoffset, yoffset, x, y, width, height, unpack_flip_y,
      NeedsUnpackPremultiplyAlpha(*source_shared_image),
      /*unpack_unmultiply_alpha=*/false);
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glCopySubTexture");

  if (!dest_shared_image->IsCleared()) {
    dest_shared_image->SetClearedRect(new_cleared_rect);
  }
}

void RasterDecoderImpl::DoCopySubTextureINTERNALGL(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    const Mailbox& source_mailbox,
    const Mailbox& dest_mailbox) {
  DCHECK(source_mailbox != dest_mailbox);
  DCHECK(shared_context_state_->GrContextIsGL());

  std::unique_ptr<SharedImageRepresentationGLTexture> source_shared_image =
      shared_image_representation_factory_.ProduceGLTexture(source_mailbox);
  std::unique_ptr<SharedImageRepresentationGLTexture> dest_shared_image =
      shared_image_representation_factory_.ProduceGLTexture(dest_mailbox);
  if (!source_shared_image || !dest_shared_image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture", "unknown mailbox");
    return;
  }

  std::unique_ptr<SharedImageRepresentationGLTexture::ScopedAccess>
      source_access = source_shared_image->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kNo);
  if (!source_access) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "unable to access source for read");
    return;
  }

  gles2::Texture* source_texture = source_shared_image->GetTexture();
  GLenum source_target = source_texture->target();
  DCHECK(source_target);
  GLint source_level = 0;
  gfx::Size source_size = source_shared_image->size();
  gfx::Rect source_rect(x, y, width, height);
  if (!gfx::Rect(source_size).Contains(source_rect)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "source texture bad dimensions.");
    return;
  }

  // Allow uncleared access, as we manually handle clear tracking.
  std::unique_ptr<SharedImageRepresentationGLTexture::ScopedAccess>
      dest_access = dest_shared_image->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_access) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "unable to access destination for write");
    return;
  }

  gles2::Texture* dest_texture = dest_shared_image->GetTexture();
  GLenum dest_target = dest_texture->target();
  DCHECK(dest_target);
  GLint dest_level = 0;
  gfx::Size dest_size = dest_shared_image->size();
  gfx::Rect dest_rect(xoffset, yoffset, width, height);
  if (!gfx::Rect(dest_size).Contains(dest_rect)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "destination texture bad dimensions.");
    return;
  }

  DCHECK_NE(source_texture->service_id(), dest_texture->service_id());

  GLenum source_type = 0;
  GLenum source_internal_format = 0;
  source_texture->GetLevelType(source_target, source_level, &source_type,
                               &source_internal_format);

  GLenum dest_type = 0;
  GLenum dest_internal_format = 0;
  bool dest_level_defined = dest_texture->GetLevelType(
      dest_target, dest_level, &dest_type, &dest_internal_format);
  DCHECK(dest_level_defined);

  // TODO(piman): Do we need this check? It might always be true by
  // construction.
  std::string output_error_msg;
  if (!ValidateCopyTextureCHROMIUMInternalFormats(
          GetFeatureInfo(), source_internal_format, dest_internal_format,
          &output_error_msg)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glCopySubTexture",
                       output_error_msg.c_str());
    return;
  }

  // Clear the source texture if necessary.
  if (!gles2::TextureManager::ClearTextureLevel(this, source_texture,
                                                source_target, 0 /* level */)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "glCopySubTexture",
                       "source texture dimensions too big");
    return;
  }

  gfx::Rect new_cleared_rect;
  gfx::Rect old_cleared_rect =
      dest_texture->GetLevelClearedRect(dest_target, dest_level);
  if (gles2::TextureManager::CombineAdjacentRects(
          dest_texture->GetLevelClearedRect(dest_target, dest_level), dest_rect,
          &new_cleared_rect)) {
    DCHECK(old_cleared_rect.IsEmpty() ||
           new_cleared_rect.Contains(old_cleared_rect));
  } else {
    // No users of RasterDecoder leverage this functionality. Clearing uncleared
    // regions could be added here if needed.
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "Cannot clear non-combineable rects.");
    return;
  }

  ScopedTextureBinder binder(state(), dest_target, dest_texture->service_id(),
                             gr_context());

  gles2::Texture::ImageState image_state;
  if (gl::GLImage* image =
          source_texture->GetLevelImage(source_target, 0, &image_state)) {
    base::Optional<ScopedPixelUnpackState> pixel_unpack_state;
    if (image->GetType() == gl::GLImage::Type::MEMORY &&
        shared_context_state_->need_context_state_reset()) {
      // If the image is in shared memory, we may need upload the pixel data
      // with SubTexImage2D, so we need reset pixel unpack state if gl context
      // state has been touched by skia.
      pixel_unpack_state.emplace(state(), gr_context(), feature_info());
    }

    // Try to copy by uploading to the destination texture.
    if (dest_internal_format == source_internal_format) {
      if (image->CopyTexSubImage(dest_target, gfx::Point(xoffset, yoffset),
                                 gfx::Rect(x, y, width, height))) {
        dest_texture->SetLevelClearedRect(dest_target, dest_level,
                                          new_cleared_rect);
        return;
      }
    }

    // Otherwise, update the source if needed.
    if (image_state == gles2::Texture::UNBOUND) {
      ScopedGLErrorSuppressor suppressor(
          "RasterDecoderImpl::DoCopySubTextureINTERNAL", error_state_.get());
      api()->glBindTextureFn(source_target, source_texture->service_id());
      if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
        bool rv = image->BindTexImage(source_target);
        DCHECK(rv) << "BindTexImage() failed";
        image_state = gles2::Texture::BOUND;
      } else {
        bool rv = image->CopyTexImage(source_target);
        DCHECK(rv) << "CopyTexImage() failed";
        image_state = gles2::Texture::COPIED;
      }
      source_texture->SetLevelImageState(source_target, 0, image_state);
    }
  }

  if (!InitializeCopyTextureCHROMIUM())
    return;

  gles2::CopyTextureMethod method = GetCopyTextureCHROMIUMMethod(
      GetFeatureInfo(), source_target, source_level, source_internal_format,
      source_type, dest_target, dest_level, dest_internal_format, unpack_flip_y,
      NeedsUnpackPremultiplyAlpha(*source_shared_image),
      false /* unpack_unmultiply_alpha */, false /* dither */);
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  // glDrawArrays is faster than glCopyTexSubImage2D on IA Mesa driver,
  // although opposite in Android.
  // TODO(dshwang): After Mesa fixes this issue, remove this hack.
  // https://bugs.freedesktop.org/show_bug.cgi?id=98478,
  // https://crbug.com/535198.
  if (gles2::Texture::ColorRenderable(GetFeatureInfo(), dest_internal_format,
                                      dest_texture->IsImmutable()) &&
      method == gles2::CopyTextureMethod::DIRECT_COPY) {
    method = gles2::CopyTextureMethod::DIRECT_DRAW;
  }
#endif

  in_copy_sub_texture_ = true;
  copy_texture_chromium_->DoCopySubTexture(
      this, source_target, source_texture->service_id(), source_level,
      source_internal_format, dest_target, dest_texture->service_id(),
      dest_level, dest_internal_format, xoffset, yoffset, x, y, width, height,
      dest_size.width(), dest_size.height(), source_size.width(),
      source_size.height(), unpack_flip_y,
      NeedsUnpackPremultiplyAlpha(*source_shared_image),
      false /* unpack_unmultiply_alpha */, false /* dither */, method,
      copy_tex_image_blit_.get());
  dest_texture->SetLevelClearedRect(dest_target, dest_level, new_cleared_rect);
  in_copy_sub_texture_ = false;
  if (reset_texture_state_) {
    reset_texture_state_ = false;
    for (auto* texture : {source_texture, dest_texture}) {
      GLenum target = texture->target();
      api()->glBindTextureFn(target, texture->service_id());
      api()->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, texture->wrap_s());
      api()->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, texture->wrap_t());
      api()->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER,
                               texture->min_filter());
      api()->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER,
                               texture->mag_filter());
    }
    shared_context_state_->PessimisticallyResetGrContext();
  }
}

void RasterDecoderImpl::DoCopySubTextureINTERNALSkia(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    const Mailbox& source_mailbox,
    const Mailbox& dest_mailbox) {
  DCHECK(source_mailbox != dest_mailbox);

  // Use Skia to copy texture if raster's gr_context() is not using GL.
  auto source_shared_image = shared_image_representation_factory_.ProduceSkia(
      source_mailbox, shared_context_state_);
  auto dest_shared_image = shared_image_representation_factory_.ProduceSkia(
      dest_mailbox, shared_context_state_);
  if (!source_shared_image || !dest_shared_image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture", "unknown mailbox");
    return;
  }

  gfx::Size source_size = source_shared_image->size();
  gfx::Rect source_rect(x, y, width, height);
  if (!gfx::Rect(source_size).Contains(source_rect)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "source texture bad dimensions.");
    return;
  }

  gfx::Size dest_size = dest_shared_image->size();
  gfx::Rect dest_rect(xoffset, yoffset, width, height);
  if (!gfx::Rect(dest_size).Contains(dest_rect)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "destination texture bad dimensions.");
    return;
  }

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  // Allow uncleared access, as we manually handle clear tracking.
  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      dest_scoped_access = dest_shared_image->BeginScopedWriteAccess(
          &begin_semaphores, &end_semaphores,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_scoped_access) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "Dest shared image is not writable");
    return;
  }

  gfx::Rect new_cleared_rect;
  gfx::Rect old_cleared_rect = dest_shared_image->ClearedRect();
  if (gles2::TextureManager::CombineAdjacentRects(old_cleared_rect, dest_rect,
                                                  &new_cleared_rect)) {
    DCHECK(old_cleared_rect.IsEmpty() ||
           new_cleared_rect.Contains(old_cleared_rect));
  } else {
    // No users of RasterDecoder leverage this functionality. Clearing uncleared
    // regions could be added here if needed.
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "Cannot clear non-combineable rects.");
    return;
  }

  // With OneCopyRasterBufferProvider, source_shared_image->BeginReadAccess()
  // will copy pixels from SHM GMB to the texture in |source_shared_image|,
  // and then use drawImageRect() to draw that texure to the target
  // |dest_shared_image|. We can save one copy by drawing the SHM GMB to the
  // target |dest_shared_image| directly.
  // TODO(penghuang): get rid of the one extra copy. https://crbug.com/984045
  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      source_scoped_access = source_shared_image->BeginScopedReadAccess(
          &begin_semaphores, &end_semaphores);

  if (!begin_semaphores.empty()) {
    bool result = dest_scoped_access->surface()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }

  if (!source_scoped_access) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                       "Source shared image is not accessable");
  } else {
    auto source_image = source_scoped_access->CreateSkImage(
        shared_context_state_->gr_context());
    if (!source_image) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture",
                         "Couldn't create SkImage from source shared image.");
    }

    auto* canvas = dest_scoped_access->surface()->getCanvas();
    SkPaint paint;
    if (unpack_flip_y) {
      canvas->scale(1, -1);
      canvas->translate(0, -height);
    }
    paint.setBlendMode(SkBlendMode::kSrc);
    canvas->drawImageRect(source_image, gfx::RectToSkRect(source_rect),
                          gfx::RectToSkRect(dest_rect), &paint);
  }

  FlushAndSubmitIfNecessary(dest_scoped_access->surface(),
                            std::move(end_semaphores));
  if (!dest_shared_image->IsCleared()) {
    dest_shared_image->SetClearedRect(new_cleared_rect);
  }
}

void RasterDecoderImpl::DoWritePixelsINTERNAL(GLint x_offset,
                                              GLint y_offset,
                                              GLuint src_width,
                                              GLuint src_height,
                                              GLuint row_bytes,
                                              GLuint src_sk_color_type,
                                              GLuint src_sk_alpha_type,
                                              GLint shm_id,
                                              GLuint shm_offset,
                                              GLuint pixels_offset,
                                              const volatile GLbyte* mailbox) {
  if (src_sk_color_type > kLastEnum_SkColorType) {
    LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, "WritePixels",
                       "src_sk_color_type must be a valid SkColorType");
    return;
  }
  if (src_sk_alpha_type > kLastEnum_SkAlphaType) {
    LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, "WritePixels",
                       "src_sk_alpha_type must be a valid SkAlphaType");
    return;
  }

  Mailbox dest_mailbox = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  DLOG_IF(ERROR, !dest_mailbox.Verify())
      << "WritePixels was passed an invalid mailbox";
  auto dest_shared_image = shared_image_representation_factory_.ProduceSkia(
      dest_mailbox, shared_context_state_);
  if (!dest_shared_image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glWritePixels",
                       "Attempting to write to unknown mailbox.");
    return;
  }

  if (SkColorTypeBytesPerPixel(viz::ResourceFormatToClosestSkColorType(
          true, dest_shared_image->format())) !=
      SkColorTypeBytesPerPixel(static_cast<SkColorType>(src_sk_color_type))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glWritePixels",
                       "Bytes per pixel for src SkColorType and dst "
                       "SkColorType must be the same.");
    return;
  }

  // If present, the color space is serialized into shared memory before the
  // pixel data.
  sk_sp<SkColorSpace> color_space;
  if (pixels_offset > 0) {
    void* color_space_bytes =
        GetSharedMemoryAs<void*>(shm_id, shm_offset, pixels_offset);
    if (!color_space_bytes) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glWritePixels",
                         "Failed to retrieve serialized SkColorSpace.");
      return;
    }

    color_space = SkColorSpace::Deserialize(color_space_bytes, pixels_offset);
    if (!color_space) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glWritePixels",
                         "Failed to deserialize expected SkColorSpace");
      return;
    }
  }

  SkImageInfo src_info = SkImageInfo::Make(
      src_width, src_height, static_cast<SkColorType>(src_sk_color_type),
      static_cast<SkAlphaType>(src_sk_alpha_type), std::move(color_space));

  if (row_bytes < src_info.minRowBytes()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glWritePixels",
                       "row_bytes be >= "
                       "SkImageInfo::minRowBytes() for source image.");
    return;
  }

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  // Allow uncleared access, as we manually handle clear tracking.
  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      dest_scoped_access = dest_shared_image->BeginScopedWriteAccess(
          &begin_semaphores, &end_semaphores,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_scoped_access) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glWritePixels",
                       "Dest shared image is not writable");
    return;
  }

  if (!begin_semaphores.empty()) {
    bool result = dest_scoped_access->surface()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    if (!result) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glWritePixels",
                         "Unable to obtain write access to dest shared image.");
      return;
    }
  }

  size_t byte_size = src_info.computeByteSize(row_bytes);
  if (byte_size > UINT32_MAX) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glWritePixels",
        "Cannot request a memory chunk larger than UINT32_MAX bytes");
    return;
  }

  // The pixels are stored after the serialized SkColorSpace + padding
  void* pixel_data =
      GetSharedMemoryAs<void*>(shm_id, shm_offset + pixels_offset, byte_size);
  if (!pixel_data) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glWritePixels",
                       "Couldn't retrieve pixel data.");
    return;
  }
  auto* canvas = dest_scoped_access->surface()->getCanvas();
  bool written =
      canvas->writePixels(src_info, pixel_data, row_bytes, x_offset, y_offset);
  if (!written) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glWritePixels",
                       "Failed to write pixels to SkCanvas");
  }

  FlushAndSubmitIfNecessary(dest_scoped_access->surface(),
                            std::move(end_semaphores));
  if (!dest_shared_image->IsCleared()) {
    dest_shared_image->SetClearedRect(
        gfx::Rect(x_offset, y_offset, src_width, src_height));
  }
}

void RasterDecoderImpl::DoReadbackImagePixelsINTERNAL(
    GLint src_x,
    GLint src_y,
    GLuint dst_width,
    GLuint dst_height,
    GLuint row_bytes,
    GLuint dst_sk_color_type,
    GLuint dst_sk_alpha_type,
    GLint shm_id,
    GLuint shm_offset,
    GLuint pixels_offset,
    const volatile GLbyte* mailbox) {
  if (dst_sk_color_type > kLastEnum_SkColorType) {
    LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, "ReadbackImagePixels",
                       "dst_sk_color_type must be a valid SkColorType");
    return;
  }
  if (dst_sk_alpha_type > kLastEnum_SkAlphaType) {
    LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, "ReadbackImagePixels",
                       "dst_sk_alpha_type must be a valid SkAlphaType");
    return;
  }

  Mailbox source_mailbox = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  DLOG_IF(ERROR, !source_mailbox.Verify())
      << "ReadbackImagePixels was passed an invalid mailbox";
  auto source_shared_image = shared_image_representation_factory_.ProduceSkia(
      source_mailbox, shared_context_state_);
  if (!source_shared_image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glReadbackImagePixels",
                       "Unknown mailbox");
    return;
  }

  // If present, the color space is serialized into shared memory before the
  // pixel data.
  sk_sp<SkColorSpace> dst_color_space;
  if (pixels_offset > 0) {
    void* color_space_bytes =
        GetSharedMemoryAs<void*>(shm_id, shm_offset, pixels_offset);
    if (!color_space_bytes) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glReadbackImagePixels",
                         "Failed to retrieve serialized SkColorSpace.");
      return;
    }
    dst_color_space =
        SkColorSpace::Deserialize(color_space_bytes, pixels_offset);
    if (!dst_color_space) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glReadbackImagePixels",
                         "Failed to deserialize expected SkColorSpace");
      return;
    }
  }

  SkImageInfo dst_info = SkImageInfo::Make(
      dst_width, dst_height, static_cast<SkColorType>(dst_sk_color_type),
      static_cast<SkAlphaType>(dst_sk_alpha_type), std::move(dst_color_space));

  if (row_bytes < dst_info.minRowBytes()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glReadbackImagePixels",
                       "row_bytes be >= "
                       "SkImageInfo::minRowBytes() for dest image.");
    return;
  }

  std::vector<GrBackendSemaphore> begin_semaphores;

  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      source_scoped_access = source_shared_image->BeginScopedReadAccess(
          &begin_semaphores, nullptr);

  if (!begin_semaphores.empty()) {
    bool result = shared_context_state_->gr_context()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }

  if (!source_scoped_access) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glReadbackImagePixels",
                       "Source shared image is not accessible");
    return;
  }

  auto sk_image =
      source_scoped_access->CreateSkImage(shared_context_state_->gr_context());
  if (!sk_image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glReadbackImagePixels",
                       "Couldn't create SkImage for reading.");
    return;
  }

  size_t byte_size = dst_info.computeByteSize(row_bytes);
  if (byte_size > UINT32_MAX) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glReadbackImagePixels",
        "Cannot request a memory chunk larger than UINT32_MAX bytes");
    return;
  }

  void* shm_address =
      GetSharedMemoryAs<void*>(shm_id, shm_offset + pixels_offset, byte_size);
  if (!shm_address) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glReadbackImagePixels",
                       "Failed to retrieve memory for readPixels");
    return;
  }

  bool success =
      sk_image->readPixels(dst_info, shm_address, row_bytes, src_x, src_y);
  if (!success) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glReadbackImagePixels",
                       "Failed to read pixels from SkImage");
  }
}

namespace {
// Helper class for mailbox index iteration that handles NV12 images which have
// no separate V plane mailbox.
class YUVConversionMailboxIndex {
 public:
  explicit YUVConversionMailboxIndex(bool is_nv12)
      : is_nv12_(is_nv12), cur_index_(kYIndex) {}
  ~YUVConversionMailboxIndex() = default;

  YUVConversionMailboxIndex& operator++() {
    cur_index_++;
    if (cur_index_ == kVIndex && is_nv12_)
      cur_index_++;
    return *this;
  }

  size_t operator()() { return cur_index_; }

  void reset() { cur_index_ = kYIndex; }

  enum Index : size_t {
    kYIndex = 0,
    kUIndex = 1,
    kVIndex = 2,
    kDestIndex = 3,
  };

  std::string ToString() {
    switch (cur_index_) {
      case YUVConversionMailboxIndex::kYIndex:
        return "Y Plane";
      case YUVConversionMailboxIndex::kUIndex:
        return is_nv12_ ? "UV Plane" : "U Plane";
      case YUVConversionMailboxIndex::kVIndex:
        DCHECK(!is_nv12_);
        return "V Plane";
      case YUVConversionMailboxIndex::kDestIndex:
        return "Destination";
      default:
        return "Invalid mailbox index";
    }
  }

  static constexpr size_t kNumInputMailboxes =
      YUVConversionMailboxIndex::kVIndex + 1;
  static constexpr size_t kTotalMailboxes =
      YUVConversionMailboxIndex::kDestIndex + 1;

 private:
  bool is_nv12_;
  size_t cur_index_;
};

}  // namespace

void RasterDecoderImpl::DoConvertYUVMailboxesToRGBINTERNAL(
    GLenum planes_yuv_color_space,
    GLboolean is_nv12,
    const volatile GLbyte* mailboxes_in) {
  if (planes_yuv_color_space > kLastEnum_SkYUVColorSpace) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_ENUM, "glConvertYUVMailboxesToRGB",
        "planes_yuv_color_space must be a valid SkYUVColorSpace");
    return;
  }
  SkYUVColorSpace src_color_space =
      static_cast<SkYUVColorSpace>(planes_yuv_color_space);

  YUVConversionMailboxIndex idx(is_nv12);

  // Mailboxes are sent over in the order y_plane, u_plane, v_plane, destination
  std::array<gpu::Mailbox, YUVConversionMailboxIndex::kTotalMailboxes>
      mailboxes;
  for (idx.reset(); idx() < mailboxes.size(); ++idx) {
    mailboxes[idx()] = Mailbox::FromVolatile(
        reinterpret_cast<const volatile Mailbox*>(mailboxes_in)[idx()]);
    DLOG_IF(ERROR, !mailboxes[idx()].Verify())
        << "ConvertYUVMailboxesToRGB was "
           "passed an invalid mailbox: "
        << idx.ToString();
  }

  std::array<std::unique_ptr<SharedImageRepresentationSkia>,
             YUVConversionMailboxIndex::kTotalMailboxes>
      images;
  for (idx.reset(); idx() < images.size(); ++idx) {
    images[idx()] = shared_image_representation_factory_.ProduceSkia(
        mailboxes[idx()], shared_context_state_);
    if (!images[idx()]) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, "glConvertYUVMailboxesToRGB",
          ("Attempting to operate on unknown mailbox:" + idx.ToString())
              .c_str());
      return;
    }
  }

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  auto dest_scoped_access =
      images[YUVConversionMailboxIndex::kDestIndex]->BeginScopedWriteAccess(
          &begin_semaphores, &end_semaphores,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_scoped_access) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glConvertYUVMailboxesToRGB",
                       "Destination shared image is not writable");
    DCHECK(begin_semaphores.empty());
    return;
  }

  bool source_access_valid = true;
  std::array<std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>,
             YUVConversionMailboxIndex::kNumInputMailboxes>
      source_scoped_access;
  for (idx.reset(); idx() < source_scoped_access.size(); ++idx) {
    source_scoped_access[idx()] = images[idx()]->BeginScopedReadAccess(
        &begin_semaphores, &end_semaphores);

    if (!source_scoped_access[idx()]) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, "glConvertYUVMailboxesToRGB",
          ("Couldn't access shared image for mailbox:" + idx.ToString())
              .c_str());
      source_access_valid = false;
      break;
    }
  }

  auto* dest_surface = dest_scoped_access->surface();
  if (!begin_semaphores.empty()) {
    bool result =
        dest_surface->wait(begin_semaphores.size(), begin_semaphores.data(),
                           /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }

  bool drew_image = false;
  if (source_access_valid) {
    std::array<GrBackendTexture, YUVConversionMailboxIndex::kNumInputMailboxes>
        yuva_textures;
    for (idx.reset(); idx() < yuva_textures.size(); ++idx) {
      yuva_textures[idx()] = source_scoped_access[idx()]
                                 ->promise_image_texture()
                                 ->backendTexture();
    }

    SkISize dest_size =
        SkISize::Make(dest_surface->width(), dest_surface->height());

    std::array<SkYUVAIndex, SkYUVAIndex::kIndexCount> yuva_indices;
    yuva_indices[SkYUVAIndex::kY_Index] = {0, SkColorChannel::kR};
    yuva_indices[SkYUVAIndex::kU_Index] = {1, SkColorChannel::kR};
    if (is_nv12)
      yuva_indices[SkYUVAIndex::kV_Index] = {1, SkColorChannel::kG};
    else
      yuva_indices[SkYUVAIndex::kV_Index] = {2, SkColorChannel::kR};
    yuva_indices[SkYUVAIndex::kA_Index] = {-1, SkColorChannel::kA};

    auto result_image = SkImage::MakeFromYUVATextures(
        gr_context(), src_color_space, yuva_textures.data(),
        yuva_indices.data(), dest_size, kTopLeft_GrSurfaceOrigin, nullptr);
    if (!result_image) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, "glConvertYUVMailboxesToRGB",
          "Couldn't create destination images from provided sources");
    } else {
      dest_surface->getCanvas()->drawImage(result_image, 0, 0);
      drew_image = true;
    }
  }

  FlushAndSubmitIfNecessary(dest_scoped_access->surface(),
                            std::move(end_semaphores));
  if (!images[YUVConversionMailboxIndex::kDestIndex]->IsCleared() &&
      drew_image) {
    images[YUVConversionMailboxIndex::kDestIndex]->SetCleared();
  }
}

namespace {

// Helper to read client data from transfer cache.
class TransferCacheDeserializeHelperImpl final
    : public cc::TransferCacheDeserializeHelper {
 public:
  explicit TransferCacheDeserializeHelperImpl(
      int raster_decoder_id,
      ServiceTransferCache* transfer_cache)
      : raster_decoder_id_(raster_decoder_id), transfer_cache_(transfer_cache) {
    DCHECK(transfer_cache_);
  }
  ~TransferCacheDeserializeHelperImpl() override = default;

  void CreateLocalEntry(
      uint32_t id,
      std::unique_ptr<cc::ServiceTransferCacheEntry> entry) override {
    auto type = entry->Type();
    transfer_cache_->CreateLocalEntry(
        ServiceTransferCache::EntryKey(raster_decoder_id_, type, id),
        std::move(entry));
  }

 private:
  cc::ServiceTransferCacheEntry* GetEntryInternal(
      cc::TransferCacheEntryType entry_type,
      uint32_t entry_id) override {
    return transfer_cache_->GetEntry(ServiceTransferCache::EntryKey(
        raster_decoder_id_, entry_type, entry_id));
  }

  const int raster_decoder_id_;
  ServiceTransferCache* const transfer_cache_;

  DISALLOW_COPY_AND_ASSIGN(TransferCacheDeserializeHelperImpl);
};

}  // namespace

void RasterDecoderImpl::DeletePaintCacheTextBlobsINTERNALHelper(
    GLsizei n,
    const volatile GLuint* paint_cache_ids) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glDeletePaintCacheEntriesINTERNAL",
                       "No chromium raster support");
    return;
  }

  paint_cache_->Purge(cc::PaintCacheDataType::kTextBlob, n, paint_cache_ids);
}

void RasterDecoderImpl::DeletePaintCachePathsINTERNALHelper(
    GLsizei n,
    const volatile GLuint* paint_cache_ids) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glDeletePaintCacheEntriesINTERNAL",
                       "No chromium raster support");
    return;
  }

  paint_cache_->Purge(cc::PaintCacheDataType::kPath, n, paint_cache_ids);
}

void RasterDecoderImpl::DoClearPaintCacheINTERNAL() {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glClearPaintCacheINTERNAL",
                       "No chromium raster support");
    return;
  }

  paint_cache_->PurgeAll();
}

void RasterDecoderImpl::DoBeginRasterCHROMIUM(GLuint sk_color,
                                              GLuint msaa_sample_count,
                                              GLboolean can_use_lcd_text,
                                              const volatile GLbyte* key) {
  // Workaround for https://crbug.com/906453: Flush before BeginRaster (the
  // commands between BeginRaster and EndRaster will not flush).
  FlushToWorkAroundMacCrashes();

  if (!gr_context() || !supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginRasterCHROMIUM",
                       "chromium_raster_transport not enabled via attribs");
    return;
  }
  if (sk_surface_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginRasterCHROMIUM",
                       "BeginRasterCHROMIUM without EndRasterCHROMIUM");
    return;
  }

  Mailbox mailbox =
      Mailbox::FromVolatile(*reinterpret_cast<const volatile Mailbox*>(key));
  DLOG_IF(ERROR, !mailbox.Verify()) << "BeginRasterCHROMIUM was "
                                       "passed a mailbox that was not "
                                       "generated by ProduceTextureCHROMIUM.";

  DCHECK(!shared_image_);
  shared_image_ = shared_image_representation_factory_.ProduceSkia(
      mailbox, shared_context_state_.get());
  if (!shared_image_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glBeginRasterCHROMIUM",
                       "passed invalid mailbox.");
    return;
  }

  DCHECK(locked_handles_.empty());
  DCHECK(!raster_canvas_);
  shared_context_state_->set_need_context_state_reset(true);

  // Use unknown pixel geometry to disable LCD text.
  uint32_t flags = 0;
  SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
  if (can_use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props =
        SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  }

  SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
      /*gpu_compositing=*/true, shared_image_->format());
  // If we can't match requested MSAA samples, don't use MSAA.
  int final_msaa_count = std::max(static_cast<int>(msaa_sample_count), 0);
  if (final_msaa_count >
      gr_context()->maxSurfaceSampleCountForColorType(sk_color_type))
    final_msaa_count = 0;

  std::vector<GrBackendSemaphore> begin_semaphores;
  DCHECK(end_semaphores_.empty());
  DCHECK(!scoped_shared_image_write_);
  // Allow uncleared access, as raster specifically handles uncleared images
  // by clearing them before writing.
  scoped_shared_image_write_ = shared_image_->BeginScopedWriteAccess(
      final_msaa_count, surface_props, &begin_semaphores, &end_semaphores_,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!scoped_shared_image_write_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginRasterCHROMIUM",
                       "failed to create surface");
    shared_image_.reset();
    return;
  }

  sk_surface_ = scoped_shared_image_write_->surface();

  if (!begin_semaphores.empty()) {
    bool result =
        sk_surface_->wait(begin_semaphores.size(), begin_semaphores.data(),
                          /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }

  if (use_ddl_) {
    SkSurfaceCharacterization characterization;
    bool result = sk_surface_->characterize(&characterization);
    DCHECK(result) << "Failed to characterize raster SkSurface.";
    recorder_ =
        std::make_unique<SkDeferredDisplayListRecorder>(characterization);
    raster_canvas_ = recorder_->getCanvas();
  } else {
    raster_canvas_ = sk_surface_->getCanvas();
  }

  paint_op_shared_image_provider_ = std::make_unique<SharedImageProviderImpl>(
      &shared_image_representation_factory_, shared_context_state_, sk_surface_,
      &end_semaphores_, error_state_.get());

  // All or nothing clearing, as no way to validate the client's input on what
  // is the "used" part of the texture.
  // TODO(enne): This doesn't handle the case where the background color
  // changes and so any extra pixels outside the raster area that get
  // sampled may be incorrect.
  if (shared_image_->IsCleared())
    return;

  raster_canvas_->drawColor(sk_color);
  shared_image_->SetCleared();
}

scoped_refptr<Buffer> RasterDecoderImpl::GetShmBuffer(uint32_t shm_id) {
  return GetSharedMemoryBuffer(shm_id);
}

void RasterDecoderImpl::ReportProgress() {
  if (shared_context_state_->progress_reporter())
    shared_context_state_->progress_reporter()->ReportProgress();
}

void RasterDecoderImpl::DoRasterCHROMIUM(GLuint raster_shm_id,
                                         GLuint raster_shm_offset,
                                         GLuint raster_shm_size,
                                         GLuint font_shm_id,
                                         GLuint font_shm_offset,
                                         GLuint font_shm_size) {
  TRACE_EVENT1("gpu", "RasterDecoderImpl::DoRasterCHROMIUM", "raster_id",
               ++raster_chromium_id_);

  if (!sk_surface_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glRasterCHROMIUM",
                       "RasterCHROMIUM without BeginRasterCHROMIUM");
    return;
  }
  DCHECK(transfer_cache());
  shared_context_state_->set_need_context_state_reset(true);

  if (font_shm_size > 0) {
    // Deserialize fonts before raster.
    volatile char* font_buffer_memory =
        GetSharedMemoryAs<char*>(font_shm_id, font_shm_offset, font_shm_size);
    if (!font_buffer_memory) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                         "Can not read font buffer.");
      return;
    }

    std::vector<SkDiscardableHandleId> new_locked_handles;
    if (!font_manager_->Deserialize(font_buffer_memory, font_shm_size,
                                    &new_locked_handles)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                         "Invalid font buffer.");
      return;
    }
    locked_handles_.insert(locked_handles_.end(), new_locked_handles.begin(),
                           new_locked_handles.end());
  }

  char* paint_buffer_memory = GetSharedMemoryAs<char*>(
      raster_shm_id, raster_shm_offset, raster_shm_size);
  if (!paint_buffer_memory) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                       "Can not read paint buffer.");
    return;
  }

  alignas(
      cc::PaintOpBuffer::PaintOpAlign) char data[sizeof(cc::LargestPaintOp)];

  cc::PlaybackParams playback_params(nullptr, SkMatrix::I());
  TransferCacheDeserializeHelperImpl impl(raster_decoder_id_, transfer_cache());
  cc::PaintOp::DeserializeOptions options(
      &impl, paint_cache_.get(), font_manager_->strike_client(),
      shared_context_state_->scratch_deserialization_buffer(), is_privileged_,
      paint_op_shared_image_provider_.get());
  options.crash_dump_on_failure =
      !gpu_preferences_.disable_oopr_debug_crash_dump;

  size_t paint_buffer_size = raster_shm_size;
  gl::ScopedProgressReporter report_progress(
      shared_context_state_->progress_reporter());
  while (paint_buffer_size > 0) {
    size_t skip = 0;
    cc::PaintOp* deserialized_op = cc::PaintOp::Deserialize(
        paint_buffer_memory, paint_buffer_size, &data[0],
        sizeof(cc::LargestPaintOp), &skip, options);
    if (!deserialized_op) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glRasterCHROMIUM",
                         "RasterCHROMIUM: serialization failure");
      return;
    }

    deserialized_op->Raster(raster_canvas_, playback_params);
    deserialized_op->DestroyThis();

    paint_buffer_size -= skip;
    paint_buffer_memory += skip;
  }
}

bool RasterDecoderImpl::EnsureDDLReadyForRaster() {
  DCHECK(use_ddl_);
  DCHECK_EQ(current_decoder_error_, error::kNoError);

  if (!ddl_) {
    DCHECK(recorder_);
    DCHECK(!program_iterator_);

    TRACE_EVENT0("gpu",
                 "RasterDecoderImpl::EnsureDDLReadyForRaster::DetachDDL");
    ddl_ = recorder_->detach();
    program_iterator_.emplace(shared_context_state_->gr_context(), ddl_.get());
  }

  while (!program_iterator_->done()) {
    TRACE_EVENT0("gpu",
                 "RasterDecoderImpl::EnsureDDLReadyForRaster::MaybeCompile");
    bool did_compile = program_iterator_->compile();
    program_iterator_->next();
    if (did_compile)
      return false;
  }

  program_iterator_.reset();
  return true;
}

void RasterDecoderImpl::DoEndRasterCHROMIUM() {
  TRACE_EVENT0("gpu", "RasterDecoderImpl::DoEndRasterCHROMIUM");
  if (!sk_surface_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glEndRasterCHROMIUM",
                       "EndRasterCHROMIUM without BeginRasterCHROMIUM");
    return;
  }

  shared_context_state_->set_need_context_state_reset(true);
  raster_canvas_ = nullptr;

  if (use_ddl_) {
    if (!EnsureDDLReadyForRaster()) {
      // This decoder error indicates that this command has not finished
      // executing. The decoder will yield and re-execute this command when it
      // resumes decoding.
      current_decoder_error_ = error::kDeferCommandUntilLater;
      return;
    }
    TRACE_EVENT0("gpu", "RasterDecoderImpl::DoEndRasterCHROMIUM::DrawDDL");
    sk_surface_->draw(std::move(ddl_));
  }

  {
    TRACE_EVENT0("gpu", "RasterDecoderImpl::DoEndRasterCHROMIUM::Flush");
    // This is a slow operation since skia will execute the GPU work for the
    // complete tile. Make sure the progress reporter is notified to avoid
    // hangs.
    gl::ScopedProgressReporter report_progress(
        shared_context_state_->progress_reporter());
    FlushAndSubmitIfNecessary(sk_surface_, std::move(end_semaphores_));
    end_semaphores_.clear();
  }

  shared_context_state_->UpdateSkiaOwnedMemorySize();
  sk_surface_ = nullptr;
  scoped_shared_image_write_.reset();
  shared_image_.reset();
  paint_op_shared_image_provider_.reset();

  // Test only path for SetUpForRasterCHROMIUMForTest.
  sk_surface_for_testing_.reset();

  // Unlock all font handles. This needs to be deferred until
  // SkSurface::flush since that flushes batched Gr operations
  // in skia that access the glyph data.
  // TODO(khushalsagar): We just unlocked a bunch of handles, do we need to
  // give a call to skia to attempt to purge any unlocked handles?
  if (!font_manager_->Unlock(locked_handles_)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                       "Invalid font discardable handle.");
  }
  locked_handles_.clear();

  // We just flushed a tile's worth of GPU work from the SkSurface in
  // flush above. Yield to the Scheduler to allow pre-emption before
  // processing more commands.
  ExitCommandProcessingEarly();
}

void RasterDecoderImpl::DoCreateTransferCacheEntryINTERNAL(
    GLuint raw_entry_type,
    GLuint entry_id,
    GLuint handle_shm_id,
    GLuint handle_shm_offset,
    GLuint data_shm_id,
    GLuint data_shm_offset,
    GLuint data_size) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
        "Attempt to use OOP transfer cache on a context without OOP raster.");
    return;
  }
  DCHECK(gr_context());
  DCHECK(transfer_cache());

  // Validate the type we are about to create.
  cc::TransferCacheEntryType entry_type;
  if (!cc::ServiceTransferCacheEntry::SafeConvertToType(raw_entry_type,
                                                        &entry_type)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
                       "Attempt to use OOP transfer cache with an invalid "
                       "cache entry type.");
    return;
  }

  if (entry_type == cc::TransferCacheEntryType::kSkottie && !is_privileged_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
                       "Attempt to use skottie on a non privileged channel");
    return;
  }

  uint8_t* data_memory =
      GetSharedMemoryAs<uint8_t*>(data_shm_id, data_shm_offset, data_size);
  if (!data_memory) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
                       "Can not read transfer cache entry data.");
    return;
  }

  scoped_refptr<Buffer> handle_buffer = GetSharedMemoryBuffer(handle_shm_id);
  if (!DiscardableHandleBase::ValidateParameters(handle_buffer.get(),
                                                 handle_shm_offset)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
                       "Invalid shm for discardable handle.");
    return;
  }
  ServiceDiscardableHandle handle(std::move(handle_buffer), handle_shm_offset,
                                  handle_shm_id);

  // If the entry is going to use skia during deserialization, make sure we
  // mark the context state dirty.
  GrDirectContext* context_for_entry =
      cc::ServiceTransferCacheEntry::UsesGrContext(entry_type) ? gr_context()
                                                               : nullptr;
  if (context_for_entry)
    shared_context_state_->set_need_context_state_reset(true);

  if (!transfer_cache()->CreateLockedEntry(
          ServiceTransferCache::EntryKey(raster_decoder_id_, entry_type,
                                         entry_id),
          handle, context_for_entry, base::make_span(data_memory, data_size))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCreateTransferCacheEntryINTERNAL",
                       "Failure to deserialize transfer cache entry.");
    return;
  }

  // The only entry using the GrContext are image transfer cache entries for
  // image uploads. Since this tends to a slow operation, yield to allow the
  // decoder to be pre-empted.
  if (context_for_entry)
    ExitCommandProcessingEarly();
}

void RasterDecoderImpl::DoUnlockTransferCacheEntryINTERNAL(
    GLuint raw_entry_type,
    GLuint entry_id) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUnlockTransferCacheEntryINTERNAL",
        "Attempt to use OOP transfer cache on a context without OOP raster.");
    return;
  }
  DCHECK(transfer_cache());
  cc::TransferCacheEntryType entry_type;
  if (!cc::ServiceTransferCacheEntry::SafeConvertToType(raw_entry_type,
                                                        &entry_type)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUnlockTransferCacheEntryINTERNAL",
                       "Attempt to use OOP transfer cache with an invalid "
                       "cache entry type.");
    return;
  }

  if (!transfer_cache()->UnlockEntry(ServiceTransferCache::EntryKey(
          raster_decoder_id_, entry_type, entry_id))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUnlockTransferCacheEntryINTERNAL",
                       "Attempt to unlock an invalid ID");
  }
}

void RasterDecoderImpl::DoDeleteTransferCacheEntryINTERNAL(
    GLuint raw_entry_type,
    GLuint entry_id) {
  if (!supports_oop_raster_) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glDeleteTransferCacheEntryINTERNAL",
        "Attempt to use OOP transfer cache on a context without OOP raster.");
    return;
  }
  DCHECK(transfer_cache());
  cc::TransferCacheEntryType entry_type;
  if (!cc::ServiceTransferCacheEntry::SafeConvertToType(raw_entry_type,
                                                        &entry_type)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDeleteTransferCacheEntryINTERNAL",
                       "Attempt to use OOP transfer cache with an invalid "
                       "cache entry type.");
    return;
  }

  if (!transfer_cache()->DeleteEntry(ServiceTransferCache::EntryKey(
          raster_decoder_id_, entry_type, entry_id))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDeleteTransferCacheEntryINTERNAL",
                       "Attempt to delete an invalid ID");
  }
}

void RasterDecoderImpl::RestoreStateForAttrib(GLuint attrib_index,
                                              bool restore_array_binding) {
  shared_context_state_->PessimisticallyResetGrContext();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "base/macros.h"
#include "gpu/command_buffer/service/raster_decoder_autogen.h"

}  // namespace raster
}  // namespace gpu
