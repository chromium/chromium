// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/peak_gpu_memory_tracker_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/gpu_data_manager.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace content {

// static
std::unique_ptr<PeakGpuMemoryTracker> PeakGpuMemoryTracker::Create(
    PeakMemoryCallback callback) {
  return std::make_unique<PeakGpuMemoryTrackerImpl>(std::move(callback));
}

// static
uint32_t PeakGpuMemoryTrackerImpl::next_sequence_number_ = 0;

PeakGpuMemoryTrackerImpl::PeakGpuMemoryTrackerImpl(PeakMemoryCallback callback)
    : callback_(std::move(callback)),
      callback_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  // Actually performs request to GPU service to begin memory tracking for
  // |sequence_number_|. This will normally be created from the UI thread, so
  // repost to the IO thread.
  GpuProcessHost::CallOnIO(
      GPU_PROCESS_KIND_SANDBOXED, /* force_create=*/false,
      base::BindOnce(
          [](uint32_t sequence_num, GpuProcessHost* host) {
            // There may be no host nor service available. This may occur during
            // shutdown, when the service is fully disabled, and in some tests.
            // In those cases do nothing.
            if (!host)
              return;
            if (auto* gpu_service = host->gpu_service()) {
              gpu_service->StartPeakMemoryMonitor(sequence_num);
            }
          },
          sequence_num_));
}

PeakGpuMemoryTrackerImpl::~PeakGpuMemoryTrackerImpl() {
  // The reply arrives on the IO Thread, repost to the callback's thread.
  auto wrap_callback = base::BindOnce(
      [](base::SingleThreadTaskRunner* task_runner, PeakMemoryCallback callback,
         const uint64_t peak_memory) {
        if (callback.is_null())
          return;
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), peak_memory));
      },
      base::RetainedRef(std::move(callback_task_runner_)),
      std::move(callback_));

  GpuProcessHost::CallOnIO(
      GPU_PROCESS_KIND_SANDBOXED, /* force_create=*/false,
      base::BindOnce(
          [](uint32_t sequence_num, PeakMemoryCallback callback,
             GpuProcessHost* host) {
            // There may be no host nor service available. This may occur during
            // shutdown, when the service is fully disabled, and in some tests.
            // In those cases run the callback, reporting 0 memory usage. This
            // will signify a failure state, and allow for the callback to
            // perform any needed cleanup.
            if (!host) {
              std::move(callback).Run(0u);
              return;
            }
            if (auto* gpu_service = host->gpu_service()) {
              gpu_service->GetPeakMemoryUsage(sequence_num,
                                              std::move(callback));
            }
          },
          sequence_num_, std::move(wrap_callback)));
}

void PeakGpuMemoryTrackerImpl::Cancel() {
  callback_ = PeakMemoryCallback();
}

}  // namespace content
