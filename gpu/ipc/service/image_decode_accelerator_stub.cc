// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_decode_accelerator_stub.h"

#include <stddef.h>

#include <algorithm>
#include <new>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/task_graph.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#endif

namespace gpu {
class Buffer;

#if BUILDFLAG(IS_CHROMEOS)
namespace {

struct CleanUpContext {
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  raw_ptr<SharedContextState> shared_context_state_ = nullptr;
  std::unique_ptr<SkiaImageRepresentation> skia_representation_;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      skia_scoped_access_;
  size_t num_callbacks_pending_;
  CleanUpContext(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                 raw_ptr<SharedContextState> shared_context_state,
                 std::unique_ptr<SkiaImageRepresentation> skia_representation,
                 std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
                     skia_scoped_access)
      : main_task_runner_(main_task_runner),
        shared_context_state_(shared_context_state),
        skia_representation_(std::move(skia_representation)),
        skia_scoped_access_(std::move(skia_scoped_access)),
        num_callbacks_pending_(skia_representation_->NumPlanesExpected()) {}
};

}  // namespace
#endif

// NOTE: `worker_`, `scheduler_`, and `channel_` must not be dereferenced
// within the constructor as doing so requires that `lock_` be held, which it's
// not here.
ImageDecodeAcceleratorStub::ImageDecodeAcceleratorStub(
    ImageDecodeAcceleratorWorker* worker,
    GpuChannel* channel,
    int32_t route_id)
    : worker_(worker),
      scheduler_(channel->scheduler()),
      command_buffer_id_(
          CommandBufferIdFromChannelAndRoute(channel->client_id(), route_id)),
      sequence_(
          channel->scheduler()->CreateSequence(SchedulingPriority::kLow,
                                               channel->task_runner(),
                                               CommandBufferNamespace::GPU_IO,
                                               command_buffer_id_)),
      channel_(channel),
      main_task_runner_(channel->task_runner()),
      io_task_runner_(channel->io_task_runner()) {
  // We need the sequence to be initially disabled so that when we schedule a
  // task to release the decode sync token, it doesn't run immediately (we want
  // it to run when the decode is done).
  channel->scheduler()->DisableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::Shutdown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);

  scheduler_->DestroySequence(sequence_);

  // Clear out raw_ptr references to objects that may be destroyed on the main
  // thread before this object is destroyed on the IO thread.
  channel_ = nullptr;
  worker_ = nullptr;
  scheduler_ = nullptr;
}

ImageDecodeAcceleratorStub::~ImageDecodeAcceleratorStub() {
  DCHECK(!channel_);
}

void ImageDecodeAcceleratorStub::FinishCompletedDecode() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  if (pending_completed_decodes_.empty())
    scheduler_->DisableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::ScheduleSyncTokenRelease(
    const SyncToken& release) {
  lock_.AssertAcquired();
  scheduler_->ScheduleTask(Scheduler::Task(sequence_,
                                           base::OnceClosure(base::DoNothing()),
                                           /*sync_token_fences=*/{}, release));
}

}  // namespace gpu
