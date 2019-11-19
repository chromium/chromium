// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_
#define GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/presentation_feedback.h"

class GrContext;

namespace gfx {
class GpuFence;
class Rect;
class RectF;
}

namespace cc {
struct ImageHeaderMetadata;
}

namespace gpu {

struct SwapBuffersCompleteParams;
struct SyncToken;

class ContextSupport {
 public:
  // Flush any outstanding ordering barriers for all contexts.
  virtual void FlushPendingWork() = 0;

  // Runs |callback| when the given sync token is signalled. The sync token may
  // belong to any context.
  virtual void SignalSyncToken(const SyncToken& sync_token,
                               base::OnceClosure callback) = 0;

  // Returns true if the given sync token has been signaled. The sync token must
  // belong to this context. This may be called from any thread.
  virtual bool IsSyncTokenSignaled(const SyncToken& sync_token) = 0;

  // Runs |callback| when a query created via glCreateQueryEXT() has cleared
  // passed the glEndQueryEXT() point.
  virtual void SignalQuery(uint32_t query, base::OnceClosure callback) = 0;

  // Fetches a GpuFenceHandle for a GpuFence that was previously created by
  // glInsertGpuFenceCHROMIUM on this context.
  virtual void GetGpuFence(
      uint32_t gpu_fence_id,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) = 0;

  // Indicates whether the context should aggressively free allocated resources.
  // If set to true, the context will purge all temporary resources when
  // flushed.
  virtual void SetAggressivelyFreeResources(
      bool aggressively_free_resources) = 0;

  using SwapCompletedCallback =
      base::OnceCallback<void(const SwapBuffersCompleteParams&)>;
  using PresentationCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  virtual void Swap(uint32_t flags,
                    SwapCompletedCallback complete_callback,
                    PresentationCallback presentation_callback) = 0;
  virtual void SwapWithBounds(const std::vector<gfx::Rect>& rects,
                              uint32_t flags,
                              SwapCompletedCallback swap_completed,
                              PresentationCallback presentation_callback) = 0;
  virtual void PartialSwapBuffers(
      const gfx::Rect& sub_buffer,
      uint32_t flags,
      SwapCompletedCallback swap_completed,
      PresentationCallback presentation_callback) = 0;
  virtual void CommitOverlayPlanes(
      uint32_t flags,
      SwapCompletedCallback swap_completed,
      PresentationCallback presentation_callback) = 0;

  // Schedule a texture to be presented as an overlay synchronously with the
  // primary surface during the next buffer swap or CommitOverlayPlanes.
  // This method is not stateful and needs to be re-scheduled every frame.
  virtual void ScheduleOverlayPlane(int plane_z_order,
                                    gfx::OverlayTransform plane_transform,
                                    unsigned overlay_texture_id,
                                    const gfx::Rect& display_bounds,
                                    const gfx::RectF& uv_rect,
                                    bool enable_blend,
                                    unsigned gpu_fence_id) = 0;

  // Returns an ID that can be used to globally identify the share group that
  // this context's resources belong to.
  virtual uint64_t ShareGroupTracingGUID() const = 0;

  // Sets a callback to be run when an error occurs.
  virtual void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) = 0;

  // Allows locking a GPU discardable texture from any thread. Any successful
  // call to ThreadSafeShallowLockDiscardableTexture must be paired with a
  // later call to CompleteLockDiscardableTexureOnContextThread.
  virtual bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) = 0;

  // Must be called on the context's thread, only following a successful call
  // to ThreadSafeShallowLockDiscardableTexture.
  virtual void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) = 0;

  // Checks if a discardable handle is deleted. For use in tracing code.
  virtual bool ThreadsafeDiscardableTextureIsDeletedForTracing(
      uint32_t texture_id) = 0;

  // Access to transfer cache functionality for OOP raster. Only
  // ThreadsafeLockTransferCacheEntry can be accessed without holding the
  // context lock.

  // Maps a buffer that will receive serialized data for an entry to be created.
  // Returns nullptr on failure. If success, must be paired with a call to
  // UnmapAndCreateTransferCacheEntry.
  virtual void* MapTransferCacheEntry(uint32_t serialized_size) = 0;

  // Unmaps the buffer and creates a transfer cache entry with the serialized
  // data.
  virtual void UnmapAndCreateTransferCacheEntry(uint32_t type, uint32_t id) = 0;

  // Locks a transfer cache entry. May be called on any thread.
  virtual bool ThreadsafeLockTransferCacheEntry(uint32_t type, uint32_t id) = 0;

  // Unlocks transfer cache entries.
  virtual void UnlockTransferCacheEntries(
      const std::vector<std::pair<uint32_t, uint32_t>>& entries) = 0;

  // Delete a transfer cache entry.
  virtual void DeleteTransferCacheEntry(uint32_t type, uint32_t id) = 0;

  virtual unsigned int GetTransferBufferFreeSize() const = 0;

  // Determines if hardware decode acceleration is supported for JPEG images.
  virtual bool IsJpegDecodeAccelerationSupported() const = 0;

  // Determines if hardware decode acceleration is supported for WebP images.
  virtual bool IsWebPDecodeAccelerationSupported() const = 0;

  // Determines if |image_metadata| corresponds to an image that can be decoded
  // using hardware decode acceleration. If this method returns true, then the
  // client can be confident that a call to
  // RasterInterface::ScheduleImageDecode() will succeed.
  virtual bool CanDecodeWithHardwareAcceleration(
      const cc::ImageHeaderMetadata* image_metadata) const = 0;

  // Returns true if the context provider automatically manages calls to
  // GrContext::resetContext under the hood to prevent GL state synchronization
  // problems between the GLES2 interface and skia.
  virtual bool HasGrContextSupport() const = 0;

  // Sets the GrContext that is to receive resetContext signals when the GL
  // state is modified via direct calls to the GLES2 interface.
  virtual void SetGrContext(GrContext* gr) = 0;

  virtual void WillCallGLFromSkia() = 0;

  virtual void DidCallGLFromSkia() = 0;

  // Notifies the onscreen surface of the display transform applied to the swaps
  // from the client.
  virtual void SetDisplayTransform(gfx::OverlayTransform transform) = 0;

 protected:
  ContextSupport() = default;
  virtual ~ContextSupport() = default;
};

}

#endif  // GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_
