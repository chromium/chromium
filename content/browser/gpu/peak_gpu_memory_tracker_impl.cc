// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/peak_gpu_memory_tracker_impl.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/peak_gpu_memory_callback.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/peak_gpu_memory_tracker_factory.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace content {

// static
std::unique_ptr<input::PeakGpuMemoryTracker>
PeakGpuMemoryTrackerFactory::Create(input::PeakGpuMemoryTracker::Usage usage) {
  return std::make_unique<PeakGpuMemoryTrackerImpl>(usage);
}

// static
uint32_t PeakGpuMemoryTrackerImpl::next_sequence_number_ = 0;

PeakGpuMemoryTrackerImpl::PeakGpuMemoryTrackerImpl(
    input::PeakGpuMemoryTracker::Usage usage)
    : usage_(usage) {
  // Actually performs request to GPU service to begin memory tracking for
  // |sequence_number_|.
  auto* host =
      GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, /*force_create*/ false);
  // There may be no host nor service available. This may occur during
  // shutdown, when the service is fully disabled, and in some tests.
  // In those cases do nothing.
  if (!host) {
    return;
  }
  if (auto* gpu_service = host->gpu_service()) {
    gpu_service->StartPeakMemoryMonitor(sequence_num_);
  }
}

PeakGpuMemoryTrackerImpl::~PeakGpuMemoryTrackerImpl() {
  if (canceled_)
    return;

  auto* host =
      GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, /*force_create*/ false);
  // There may be no host nor service available. This may occur during
  // shutdown, when the service is fully disabled, and in some tests.
  // In those cases there is nothing to report to UMA. However we
  // still run the optional testing callback.
  if (!host) {
    std::move(post_gpu_service_callback_for_testing_).Run();
    return;
  }
  if (auto* gpu_service = host->gpu_service()) {
    gpu_service->GetPeakMemoryUsage(
        sequence_num_,
        base::BindOnce(&PeakGpuMemoryCallback, usage_,
                       std::move(post_gpu_service_callback_for_testing_)));
  }
}

void PeakGpuMemoryTrackerImpl::Cancel() {
  canceled_ = true;

  auto* host =
      GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, /*force_create*/ false);
  if (!host) {
    return;
  }
  // Notify the GpuProcessHost that we are done observing this sequence.
  if (auto* gpu_service = host->gpu_service()) {
    gpu_service->GetPeakMemoryUsage(sequence_num_, base::DoNothing());
  }
}

}  // namespace content
