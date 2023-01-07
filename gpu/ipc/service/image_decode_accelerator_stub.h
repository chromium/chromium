// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_STUB_H_
#define GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_STUB_H_

#include <stdint.h>

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {
class GpuChannel;
class SyncPointClientState;

// Processes incoming image decode requests from renderers: it schedules the
// decode with the appropriate hardware decode accelerator and releases sync
// tokens as decodes complete. These sync tokens must be generated on the client
// side (in ImageDecodeAcceleratorProxy) using the following information:
//
// - The command buffer namespace is GPU_IO.
// - The command buffer ID is created using the
//   CommandBufferIdFromChannelAndRoute() function using
//   GpuChannelReservedRoutes::kImageDecodeAccelerator as the route ID.
// - The release count should be incremented for each decode request.
//
// An object of this class is meant to be used in
// both the IO thread (for receiving decode requests) and the main thread (for
// processing completed decodes).
class GPU_IPC_SERVICE_EXPORT ImageDecodeAcceleratorStub
    : public base::RefCountedThreadSafe<ImageDecodeAcceleratorStub> {
 public:
  // TODO(andrescj): right now, we only accept one worker to be used for JPEG
  // decoding. If we want to use multiple workers, we need to ensure that sync
  // tokens are released in order.
  ImageDecodeAcceleratorStub(ImageDecodeAcceleratorWorker* worker,
                             GpuChannel* channel,
                             int32_t route_id);

  ImageDecodeAcceleratorStub(const ImageDecodeAcceleratorStub&) = delete;
  ImageDecodeAcceleratorStub& operator=(const ImageDecodeAcceleratorStub&) =
      delete;

  // Processes a decode request. Must be called on the IO thread.
  void ScheduleImageDecode(mojom::ScheduleImageDecodeParamsPtr params,
                           uint64_t release_count);

  // Called on the main thread to indicate that |channel_| should no longer be
  // used.
  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<ImageDecodeAcceleratorStub>;
  ~ImageDecodeAcceleratorStub();

  // Creates the service-side cache entry for a completed decode and releases
  // the decode sync token. If the decode was unsuccessful, no cache entry is
  // created but the decode sync token is still released.
  void ProcessCompletedDecode(mojom::ScheduleImageDecodeParamsPtr params_ptr,
                              uint64_t decode_release_count);

  // Releases the decode sync token corresponding to |decode_release_count| and
  // disables |sequence_| if there are no more decodes to process for now.
  void FinishCompletedDecode(uint64_t decode_release_count)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // The |worker_| calls this when a decode is completed. |result| is enqueued
  // and |sequence_| is enabled so that ProcessCompletedDecode() picks it up.
  void OnDecodeCompleted(
      gfx::Size expected_output_size,
      std::unique_ptr<ImageDecodeAcceleratorWorker::DecodeResult> result);

  // The object to which the actual decoding can be delegated.
  raw_ptr<ImageDecodeAcceleratorWorker> worker_ = nullptr;

  base::Lock lock_;
  raw_ptr<GpuChannel> channel_ GUARDED_BY(lock_) = nullptr;
  SequenceId sequence_ GUARDED_BY(lock_);
  scoped_refptr<SyncPointClientState> sync_point_client_state_
      GUARDED_BY(lock_);
  base::queue<std::unique_ptr<ImageDecodeAcceleratorWorker::DecodeResult>>
      pending_completed_decodes_ GUARDED_BY(lock_);

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_STUB_H_
