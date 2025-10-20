// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/skottie_serialization_history.h"
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
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/raster_export.h"

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
                       bool lose_context_when_out_of_memory,
                       GpuControl* gpu_control);

  RasterImplementation(const RasterImplementation&) = delete;
  RasterImplementation& operator=(const RasterImplementation&) = delete;

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
  void CopySharedImage(const gpu::Mailbox& source_mailbox,
                       const gpu::Mailbox& dest_mailbox,
                       GLint xoffset,
                       GLint yoffset,
                       GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height) override;

  void CopySharedImage(const gpu::Mailbox& source_mailbox,
                       const gpu::Mailbox& dest_mailbox,
                       const gfx::Rect& source_rect,
                       const gfx::Rect& dest_rect) override;

  void WritePixels(const gpu::Mailbox& dest_mailbox,
                   int dst_x_offset,
                   int dst_y_offset,
                   GLenum texture_target,
                   const SkPixmap& src_sk_pixmap) override;

  void WritePixelsYUV(const gpu::Mailbox& dest_mailbox,
                      const SkYUVAPixmaps& src_yuv_pixmap) override;

  void BeginRasterCHROMIUM(SkColor4f sk_color_4f,
                           GLboolean needs_clear,
                           GLuint msaa_sample_count,
                           MsaaMode msaa_mode,
                           GLboolean can_use_lcd_text,
                           GLboolean visible,
                           const gfx::ColorSpace& color_space,
                           float hdr_headroom,
                           const GLbyte* mailbox) override;
  void RasterCHROMIUM(const cc::DisplayItemList* list,
                      cc::ImageProvider* provider,
                      const gfx::Size& content_size,
                      const gfx::Rect& full_raster_rect,
                      const gfx::Rect& playback_rect,
                      const gfx::Vector2dF& post_translate,
                      const gfx::Vector2dF& post_scale,
                      bool requires_clear,
                      const ScrollOffsetMap* raster_inducing_scroll_offsets,
                      size_t* max_op_size_hint) override;
  void ReadbackARGBPixelsAsync(
      const gpu::Mailbox& source_mailbox,
      GLenum source_target,
      GrSurfaceOrigin source_origin,
      const gfx::Size& source_size,
      const gfx::Point& source_starting_point,
      const SkImageInfo& dst_info,
      GLuint dst_row_bytes,
      base::span<uint8_t> out,
      base::OnceCallback<void(bool)> readback_done) override;

  void ReadbackYUVPixelsAsync(
      const gpu::Mailbox& source_mailbox,
      GLenum source_target,
      const gfx::Size& source_size,
      const gfx::Rect& output_rect,
      bool vertically_flip_texture,
      int y_plane_row_stride_bytes,
      base::span<uint8_t> y_plane_data,
      int u_plane_row_stride_bytes,
      base::span<uint8_t> u_plane_data,
      int v_plane_row_stride_bytes,
      base::span<uint8_t> v_plane_data,
      const gfx::Point& paste_location,
      base::OnceCallback<void()> release_mailbox,
      base::OnceCallback<void(bool)> readback_done) override;
  bool ReadbackImagePixels(const gpu::Mailbox& source_mailbox,
                           const SkImageInfo& dst_info,
                           GLuint dst_row_bytes,
                           int src_x,
                           int src_y,
                           int plane_index,
                           void* dst_pixels) override;

  // ContextSupport implementation.
  void SetAggressivelyFreeResources(bool aggressively_free_resources) override;
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) override;
  base::span<uint8_t> MapTransferCacheEntry(uint32_t serialized_size) override;
  void UnmapAndCreateTransferCacheEntry(uint32_t type, uint32_t id) override;
  bool ThreadsafeLockTransferCacheEntry(uint32_t type, uint32_t id) override;
  void UnlockTransferCacheEntries(
      const std::vector<std::pair<uint32_t, uint32_t>>& entries) override;
  void DeleteTransferCacheEntry(uint32_t type, uint32_t id) override;
  unsigned int GetTransferBufferFreeSize() const override;

  // InterfaceBase implementation.
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;
  void ShallowFlushCHROMIUM() override;

  bool GetQueryObjectValueHelper(const char* function_name,
                                 GLuint id,
                                 GLenum pname,
                                 GLuint64* params);

  // ClientFontManager::Client implementation.
  base::span<uint8_t> MapFontBuffer(uint32_t size) override;

  void set_max_inlined_entry_size_for_testing(uint32_t max_size) {
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
    raw_ptr<RasterImplementation> raster_implementation_;
  };

  // ImplementationBase implementation.
  void IssueShallowFlush() override;

  // GpuControlClient implementation.
  void OnGpuControlLostContext() final;
  void OnGpuControlLostContextMaybeReentrant() final;
  void OnGpuControlErrorMessage(const char* message, int32_t id) final;
  void OnGpuControlReturnData(base::span<const uint8_t> data) final;

  // Gets the GLError through our wrapper.
  GLenum GetGLError();

  // Sets our wrapper for the GLError.
  void SetGLErrorInvalidEnum(const char* function_name,
                             GLenum value,
                             const char* label);

  // Try to map a transfer buffer of |size|.  Will return a pointer to a
  // buffer of |size_allocated|, which will be equal to or lesser than |size|.
  void* MapRasterCHROMIUM(uint32_t size, uint32_t* size_allocated);

  // |raster_written_size| is the size of buffer used by raster commands.
  // |total_written_size| is the total size of the buffer written to, including
  // any transfer cache entries inlined into the buffer.
  void UnmapRasterCHROMIUM(uint32_t raster_written_size,
                           uint32_t total_written_size);

  // Returns the last error and clears it. Useful for debugging.
  const std::string& GetLastError() { return last_error_; }

  void GenQueriesEXTHelper(GLsizei n, const GLuint* queries);

  void DeleteTexturesHelper(GLsizei n, const GLuint* textures);
  void DeleteQueriesEXTHelper(GLsizei n, const GLuint* queries);

  // IdAllocators for objects that can't be shared among contexts.
  IdAllocator* GetIdAllocator(IdNamespaces id_namespace);

  void FinishHelper();
  void FlushHelper();

  void RunIfContextNotLost(base::OnceClosure callback);

  cc::ClientPaintCache* GetOrCreatePaintCache();
  void FlushPaintCachePurgedEntries();
  void ClearPaintCache();

  const std::string& GetLogPrefix() const;

  bool ReadbackImagePixelsINTERNAL(const gpu::Mailbox& source_mailbox,
                                   const SkImageInfo& dst_info,
                                   GLuint dst_row_bytes,
                                   int src_x,
                                   int src_y,
                                   int plane_index,
                                   base::OnceCallback<void(bool)> readback_done,
                                   void* dst_pixels);

  struct AsyncARGBReadbackRequest;
  void OnAsyncARGBReadbackDone(AsyncARGBReadbackRequest* request);
  base::queue<std::unique_ptr<AsyncARGBReadbackRequest>> argb_request_queue_;

  struct AsyncYUVReadbackRequest;
  void OnAsyncYUVReadbackDone(AsyncYUVReadbackRequest* request);
  base::queue<std::unique_ptr<AsyncYUVReadbackRequest>> yuv_request_queue_;

  void CancelRequests();

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

  raw_ptr<RasterCmdHelper> helper_;
  std::string last_error_;
  gles2::DebugMarkerManager debug_marker_manager_;
  std::string this_in_hex_;

  // Current GL error bits.
  uint32_t error_bits_;

  LogSettings log_settings_;

  // When true, the context is lost when a GL_OUT_OF_MEMORY error occurs.
  const bool lose_context_when_out_of_memory_;

  // Used to check for single threaded access.
  int use_count_;

  std::optional<ScopedMappedMemoryPtr> font_mapped_buffer_;
  std::optional<ScopedTransferBufferPtr> raster_mapped_buffer_;

  base::RepeatingCallback<void(const char*, int32_t)> error_message_callback_;

  int current_trace_stack_;

  // Flag to indicate whether the implementation can retain resources, or
  // whether it should aggressively free them.
  bool aggressively_free_resources_;

  IdAllocator query_id_allocator_;

  ClientFontManager font_manager_;

  mutable base::Lock lost_lock_;
  bool lost_ GUARDED_BY(lost_lock_);

  // To avoid repeated allocations when searching the rtrees, hold onto this
  // vector between RasterCHROMIUM calls.  It is not valid outside of that
  // function.
  std::vector<size_t> temp_raster_offsets_;

  struct RasterProperties {
    RasterProperties(SkColor4f background_color,
                     bool can_use_lcd_text,
                     sk_sp<SkColorSpace> color_space);
    ~RasterProperties();
    SkColor4f background_color = SkColors::kWhite;
    bool can_use_lcd_text = false;
    sk_sp<SkColorSpace> color_space;
  };
  std::optional<RasterProperties> raster_properties_;

  uint32_t max_inlined_entry_size_;
  ClientTransferCache transfer_cache_;
  std::string last_active_url_;

  cc::ClientPaintCache::PurgedData temp_paint_cache_purged_data_;
  std::unique_ptr<cc::ClientPaintCache> paint_cache_;

  cc::SkottieSerializationHistory skottie_serialization_history_;

  // Tracing helpers.
  int raster_chromium_id_ = 0;
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_H_
