// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_PEAK_GPU_MEMORY_TRACKER_IMPL_H_
#define CONTENT_BROWSER_GPU_PEAK_GPU_MEMORY_TRACKER_IMPL_H_

#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/browser/peak_gpu_memory_tracker.h"

namespace content {

// Tracks the peak memory of the GPU service for its lifetime. Upon its
// destruction a report will be requested from the GPU service. The peak will be
// reported to the provided PeakMemoryCallback. This will occur on the thread
// that this was created on.
//
// If the GPU is lost during this objects lifetime, upon destruction the
// PeakMemoryCallback will be ran with "0" as the reported peak usage. The same
// for if there is never a successful GPU connection.
//
// This is instaniated via PeakGpuMemoryTracker::Create.
class CONTENT_EXPORT PeakGpuMemoryTrackerImpl : public PeakGpuMemoryTracker {
 public:
  // Requests the GPU service to begin peak memory tracking.
  PeakGpuMemoryTrackerImpl(PeakMemoryCallback callback);
  // Requests the GPU service provides the peak memory, the result is passed to
  // |callback_|.
  ~PeakGpuMemoryTrackerImpl() override;

  PeakGpuMemoryTrackerImpl(const PeakGpuMemoryTrackerImpl*) = delete;
  PeakGpuMemoryTrackerImpl& operator=(const PeakGpuMemoryTrackerImpl&) = delete;

  void Cancel() override;

 private:
  // Provides the unique identifier for each PeakGpuMemoryTrackerImpl.
  static uint32_t next_sequence_number_;

  PeakMemoryCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
  uint32_t sequence_num_ = next_sequence_number_++;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_PEAK_GPU_MEMORY_TRACKER_IMPL_H_
