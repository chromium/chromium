// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DECODER_CLIENT_H_
#define GPU_COMMAND_BUFFER_SERVICE_DECODER_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace gpu {

class GPU_EXPORT DecoderClient {
 public:
  virtual ~DecoderClient() = default;

  // Prints a message (error/warning) to the console.
  virtual void OnConsoleMessage(int32_t id, const std::string& message) = 0;

  // Notifies the renderer process that the active GPU changed.
  virtual void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) {}

  // Cache a blob (i.e. shader intermediates, shader bytecodes, pipelines, etc)
  // to persistent storage.
  virtual void CacheBlob(gpu::GpuDiskCacheType type,
                         const std::string& key,
                         const std::string& blob) = 0;

  // Called when the decoder releases a fence sync. Allows the client to
  // reschedule waiting decoders.
  virtual void OnFenceSyncRelease(uint64_t release) = 0;

  // Called when the decoder needs to be descheduled while waiting for a fence
  // completion. The client is responsible for descheduling the command buffer
  // before returning, and then calling PerformPollingWork periodically to test
  // for the fence completion and possibly reschedule.
  virtual void OnDescheduleUntilFinished() = 0;

  // Called from PerformPollingWork when the decoder needs to be rescheduled
  // because the fence completed.
  virtual void OnRescheduleAfterFinished() = 0;

  // Called when SwapBuffers is called.
  virtual void OnSwapBuffers(uint64_t swap_id, uint32_t flags) = 0;

  // Notifies the client that the shared GrContext may have been used by this
  // decoder and its GPU memory should be cleaned up.
  virtual void ScheduleGrContextCleanup() = 0;

  virtual void SetActiveURL(GURL url) {}

  // Called by the decoder to pass a variable-size block of data to the client.
  virtual void HandleReturnData(base::span<const uint8_t> data) = 0;

  // Returns true if rasterization should yield.
  virtual bool ShouldYield() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DECODER_CLIENT_H_
