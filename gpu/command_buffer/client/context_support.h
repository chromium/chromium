// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_
#define GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {
class GpuFence;
}

namespace gpu {

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

  // Sets a callback to be run when an error occurs.
  virtual void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) = 0;

  // Access to transfer cache functionality for OOP raster. Only
  // ThreadsafeLockTransferCacheEntry can be accessed without holding the
  // context lock.

  // Maps a buffer that will receive serialized data for an entry to be created.
  // Returns an empty span on failure. If success, must be paired with a call to
  // UnmapAndCreateTransferCacheEntry.
  virtual base::span<uint8_t> MapTransferCacheEntry(
      uint32_t serialized_size) = 0;

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

 protected:
  ContextSupport() = default;
  virtual ~ContextSupport() = default;
};

}

#endif  // GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_
