// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_PEAK_GPU_MEMORY_TRACKER_IMPL_H_
#define CONTENT_BROWSER_GPU_PEAK_GPU_MEMORY_TRACKER_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "components/input/peak_gpu_memory_tracker.h"

namespace content {

// Tracks the peak memory of the GPU service for its lifetime. Upon its
// destruction a report will be requested from the GPU service. The peak will be
// reported to UMA Histograms.
//
// If the GPU is lost during this objects lifetime, upon destruction there will
// be no report to UMA Histograms. The same for if there is never a successful
// GPU connection.
//
// This is instaniated via `PeakGpuMemoryTrackerFactory::Create`.
class PeakGpuMemoryTrackerImpl : public input::PeakGpuMemoryTracker {
 public:
  // Requests the GPU service to begin peak memory tracking.
  PeakGpuMemoryTrackerImpl(input::PeakGpuMemoryTracker::Usage usage);
  // Requests the GPU service provides the peak memory, the result is presented
  // to UMA Histograms.
  ~PeakGpuMemoryTrackerImpl() override;

  PeakGpuMemoryTrackerImpl(const PeakGpuMemoryTrackerImpl*) = delete;
  PeakGpuMemoryTrackerImpl& operator=(const PeakGpuMemoryTrackerImpl&) = delete;

  void Cancel() override;

 private:
  friend class PeakGpuMemoryTrackerImplTest;

  // A callback which will be run after receiving a callback from the
  // GpuService. For use by tests to synchronize work done on the UI thread.
  base::OnceClosure post_gpu_service_callback_for_testing_ = base::DoNothing();

  // Provides the unique identifier for each PeakGpuMemoryTrackerImpl.
  static uint32_t next_sequence_number_;

  bool canceled_ = false;
  input::PeakGpuMemoryTracker::Usage usage_;
  uint32_t sequence_num_ = next_sequence_number_++;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_PEAK_GPU_MEMORY_TRACKER_IMPL_H_
