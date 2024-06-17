/// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PEAK_GPU_MEMORY_TRACKER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_PEAK_GPU_MEMORY_TRACKER_FACTORY_H_

#include <memory>

#include "components/input/peak_gpu_memory_tracker.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT PeakGpuMemoryTrackerFactory {
 public:
  PeakGpuMemoryTrackerFactory(const PeakGpuMemoryTrackerFactory&) = delete;
  PeakGpuMemoryTrackerFactory& operator=(const PeakGpuMemoryTrackerFactory&) =
      delete;

  PeakGpuMemoryTrackerFactory() = delete;
  ~PeakGpuMemoryTrackerFactory() = delete;

  // Creates the PeakGpuMemoryTracker, which performs the registration with the
  // GPU service. Destroy the PeakGpuMemoryTracker to request a report from the
  // GPU service. The report will be recorded in UMA Histograms for the given
  // |usage| type.
  static std::unique_ptr<input::PeakGpuMemoryTracker> Create(
      input::PeakGpuMemoryTracker::Usage usage);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PEAK_GPU_MEMORY_TRACKER_FACTORY_H_
