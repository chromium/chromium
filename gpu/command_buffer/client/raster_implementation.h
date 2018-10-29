// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/client/client_font_manager.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/implementation_base.h"
#include "gpu/command_buffer/client/logging.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/raster_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class TransferCacheSerializeHelper;
}  // namespace cc

namespace gpu {

class GpuControl;
struct SharedMemoryLimits;

namespace raster {

class RasterCmdHelper;

// This class emulates Raster over command buffers. It can be used by a client
// program so that the program does not need deal with shared memory and command
// buffer management.
class RASTER_EXPORT RasterImplementation : public RasterInterface,
                                           public ImplementationBase,
                                           public ClientTransferCache::Client,
                                           public gles2::QueryTrackerClient,
                                           public ClientFontManager::Client {
 public:
  RasterImplementation(RasterCmdHelper* helper,
                       TransferBufferInterface* transfer_buffer,
                       bool bind_generates_resource,
                       bool lose_context_when_out_of_memory,
                       GpuControl* gpu_control);

  ~RasterImplementation() override;

  gpu::ContextResult Initialize(const SharedMemoryLimits& limits);

  // The RasterCmdHelper being used by this RasterImplementation. You can use
  // this to issue cmds at a lower level for certain kinds of optimization.
  RasterCmdHelper* helper() const;

  // QueryTrackerClient implementation.
  void IssueBeginQuery(GLenum target,
                       GLuint id,
                       uint32_t sync_data_shm_id,
                       uint32_t sync_data_shm_offset) override;
  void IssueEndQuery(GLenum target, GLuint submit_count) override;
  void IssueQueryCounter(GLuint id,
                         GLenum target,
                         uint32_t sync_data_shm_id,
                         uint32_t sync_data_shm_offset,
                         GLuint submit_count) override;
  void IssueSetDisjointValueSync(uint32_t sync_data_shm_id,
                                 uint32_t sync_data_shm_offset) override;
  GLenum GetClientSideGLError() override;
  CommandBufferHelper* cmd_buffer_helper() override;
  void SetGLError(GLenum error,
                  const char* function_name,
                  const char* msg) override;

  // ClientTransferCache::Client implementation.
  void IssueCreateTransferCacheEntry(GLuint entry_type,
                                     GLuint entry_id,
                                     GLuint handle_shm_id,
                                     GLuint handle_shm_offset,
                                     GLuint data_shm_id,
                                     GLuint data_shm_offset,
                                     GLuint data_size) override;
  void IssueDeleteTransferCacheEntry(GLuint entry_type,
                                     GLuint entry_id) override;
  void IssueUnlockTransferCacheEntry(GLuint entry_type,
                                     GLuint entry_id) override;
  CommandBuffer* command_buffer() const override;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/raster_implementation_autogen.h"

  // RasterInterface implementation.
  void BeginRasterCHROMIUM(GLuint sk_color,
                           GLuint msaa_sample_count,
                           GLboolean can_use_lcd_text,
                           GLint color_type,
                           const cc::RasterColorSpace& raster_color_space,
                           const GLbyte* mailbox) override;
  void RasterCHROMIUM(const cc::DisplayItemList* list,
                      cc::ImageProvider* provider,
                      const gfx::Size& content_size,
                      const gfx::Rect& full_raster_rect,
                      const gfx::Rect& playback_rect,
                      const gfx::Vector2dF& post_translate,
                      GLfloat post_scale,
                      bool requires_clear) override;
  void BeginGpuRaster() override;
  void EndGpuRaster() override;

  // ContextSupport implementation.
  void SetAggressivelyFreeResources(bool aggressively_free_resources) override;
  void Swap(uint32_t flags,
            SwapCompletedCallback swap_completed,
            PresentationCallback present_callback) override;
  void SwapWithBounds(const std::vector<gfx::Rect>& rects,
                      uint32_t flags,
                      SwapCompletedCallback swap_completed,
                      PresentationCallback present_callback) override;
  void PartialSwapBuffers(const gfx::Rect& sub_buffer,
                          uint32_t flags,
                          SwapCompletedCallback swap_completed,
                          PresentationCallback present_callback) override;
  void CommitOverlayPlanes(uint32_t flags,
                           SwapCompletedCallback swap_completed,
                           PresentationCallback present_callback) override;
  void ScheduleOverlayPlane(int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            unsigned overlay_texture_id,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& uv_rect,
                            bool enable_blend,
                            unsigned gpu_fence_id) override;
  uint64_t ShareGroupTracingGUID() const override;
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) override;
  bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) override;
  void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) override;
  bool ThreadsafeDiscardableTextureIsDeletedForTracing(
      uint32_t texture_id) override;
  void* MapTransferCacheEntry(size_t serialized_size) override;
  void UnmapAndCreateTransferCacheEntry(uint32_t type, uint32_t id) override;
  bool ThreadsafeLockTransferCacheEntry(uint32_t type, uint32_t id) override;
  void UnlockTransferCacheEntries(
      const std::vector<std::pair<uint32_t, uint32_t>>& entries) override;
  void DeleteTransferCacheEntry(uint32_t type, uint32_t id) override;
  unsigned int GetTransferBufferFreeSize() const override;

  bool GetQueryObjectValueHelper(const char* function_name,
                                 GLuint id,
                                 GLenum pname,
                                 GLuint64* params);

  // ClientFontManager::Client implementation.
  void* MapFontBuffer(size_t size) override;

  void set_max_inlined_entry_size_for_testing(size_t max_size) {
    max_inlined_entry_size_ = max_size;
  }

  std::unique_ptr<cc::TransferCacheSerializeHelper>
  CreateTransferCacheHelperForTesting();
  void SetRasterMappedBufferForTesting(ScopedTransferBufferPtr buffer);

 private:
  class TransferCacheSerializeHelperImpl;
  class PaintOpSerializer;
  friend class RasterImplementationTest;

  using IdNamespaces = raster::id_namespaces::IdNamespaces;

  struct TextureUnit {
    TextureUnit() : bound_texture_2d(0) {}
    // texture currently bound to this unit's GL_TEXTURE_2D with glBindTexture
    GLuint bound_texture_2d;
  };

  // Checks for single threaded access.
  class SingleThreadChecker {
   public:
    explicit SingleThreadChecker(RasterImplementation* raster_implementation);
    ~SingleThreadChecker();

   private:
    RasterImplementation* raster_implementation_;
  };

  // ImplementationBase implementation.
  void IssueShallowFlush() override;

  // GpuControlClient implementation.
  void OnGpuControlLostContext() final;
  void OnGpuControlLostContextMaybeReentrant() final;
  void OnGpuControlErrorMessage(const char* message, int32_t id) final;
  void OnGpuControlSwapBuffersCompleted(
      const SwapBuffersCompleteParams& params) final;
  void OnSwapBufferPresented(uint64_t swap_id,
                             const gfx::PresentationFeedback& feedback) final;

  // Gets the GLError through our wrapper.
  GLenum GetGLError();

  // Sets our wrapper for the GLError.
  void SetGLErrorInvalidEnum(const char* function_name,
                             GLenum value,
                             const char* label);

  void* MapRasterCHROMIUM(GLsizeiptr size);

  // |raster_written_size| is the size of buffer used by raster commands.
  // |total_written_size| is the total size of the buffer written to, including
  // any transfer cache entries inlined into the buffer.
  void UnmapRasterCHROMIUM(GLsizeiptr raster_written_size,
                           GLsizeiptr total_written_size);

  // Returns the last error and clears it. Useful for debugging.
  const std::string& GetLastError() { return last_error_; }

  void GenQueriesEXTHelper(GLsizei n, const GLuint* queries);

  void DeleteTexturesHelper(GLsizei n, const GLuint* textures);
  void UnbindTexturesHelper(GLsizei n, const GLuint* textures);
  void DeleteQueriesEXTHelper(GLsizei n, const GLuint* queries);

  GLuint CreateImageCHROMIUMHelper(ClientBuffer buffer,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum internalformat);
  void DestroyImageCHROMIUMHelper(GLuint image_id);

  // Helpers for query functions.
  bool GetIntegervHelper(GLenum pname, GLint* params);
  bool GetTexParameterivHelper(GLenum target, GLenum pname, GLint* params);

  // IdAllocators for objects that can't be shared among contexts.
  IdAllocator* GetIdAllocator(IdNamespaces id_namespace);

  void FinishHelper();
  void FlushHelper();

  void RunIfContextNotLost(base::OnceClosure callback);

  const std::string& GetLogPrefix() const;

// Set to 1 to have the client fail when a GL error is generated.
// This helps find bugs in the renderer since the debugger stops on the error.
#if DCHECK_IS_ON()
#if 0
#define RASTER_CLIENT_FAIL_GL_ERRORS
#endif
#endif

#if defined(RASTER_CLIENT_FAIL_GL_ERRORS)
  void CheckGLError();
  void FailGLError(GLenum error);
#else
  void CheckGLError() {}
  void FailGLError(GLenum /* error */) {}
#endif

  RasterCmdHelper* helper_;
  std::string last_error_;
  gles2::DebugMarkerManager debug_marker_manager_;
  std::string this_in_hex_;

  std::unique_ptr<TextureUnit[]> texture_units_;

  // 0 to capabilities_.max_combined_texture_image_units.
  GLuint active_texture_unit_;

  // Current GL error bits.
  uint32_t error_bits_;

  LogSettings log_settings_;

  // When true, the context is lost when a GL_OUT_OF_MEMORY error occurs.
  const bool lose_context_when_out_of_memory_;

  // Used to check for single threaded access.
  int use_count_;

  base::Optional<ScopedMappedMemoryPtr> font_mapped_buffer_;
  base::Optional<ScopedTransferBufferPtr> raster_mapped_buffer_;

  base::RepeatingCallback<void(const char*, int32_t)> error_message_callback_;

  int current_trace_stack_;

  // Flag to indicate whether the implementation can retain resources, or
  // whether it should aggressively free them.
  bool aggressively_free_resources_;

  IdAllocator texture_id_allocator_;
  IdAllocator query_id_allocator_;

  ClientFontManager font_manager_;

  mutable base::Lock lost_lock_;
  bool lost_;

  // To avoid repeated allocations when searching the rtrees, hold onto this
  // vector between RasterCHROMIUM calls.  It is not valid outside of that
  // function.
  std::vector<size_t> temp_raster_offsets_;

  struct RasterProperties {
    RasterProperties(SkColor background_color,
                     bool can_use_lcd_text,
                     sk_sp<SkColorSpace> color_space);
    ~RasterProperties();
    SkColor background_color = SK_ColorWHITE;
    bool can_use_lcd_text = false;
    sk_sp<SkColorSpace> color_space;
  };
  base::Optional<RasterProperties> raster_properties_;

  size_t max_inlined_entry_size_;
  ClientTransferCache transfer_cache_;
  std::string last_active_url_;

  // Tracing helpers.
  int raster_chromium_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(RasterImplementation);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_H_
