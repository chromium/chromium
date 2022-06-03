// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PEAK_GPU_MEMORY_TRACKER_H_
#define CONTENT_PUBLIC_BROWSER_PEAK_GPU_MEMORY_TRACKER_H_

#include <memory>

#include "content/common/content_export.h"

namespace content {

// Tracks the peak memory of the GPU service for its lifetime. Upon its
// destruction a report will be requested from the GPU service. The peak will be
// reported to UMA Histograms.
//
// If the GPU is lost during this objects lifetime, there will be no
// corresponding report of usage. The same for if there is never a successful
// GPU connection.
//
// See PeakGpuMemoryTracker::Create.
class CONTENT_EXPORT PeakGpuMemoryTracker {
 public:
  // The type of user interaction, for which the GPU Peak Memory Usage is being
  // observed.
  enum class Usage {
    CHANGE_TAB,
    PAGE_LOAD,
    SCROLL,
    USAGE_MAX = SCROLL,
  };

  // Creates the PeakGpuMemoryTracker, which performs the registration with the
  // GPU service. Destroy the PeakGpuMemoryTracker to request a report from the
  // GPU service. The report will be recorded in UMA Histograms for the given
  // |usage| type.
  static std::unique_ptr<PeakGpuMemoryTracker> Create(Usage usage);

  virtual ~PeakGpuMemoryTracker() = default;

  PeakGpuMemoryTracker(const PeakGpuMemoryTracker*) = delete;
  PeakGpuMemoryTracker& operator=(const PeakGpuMemoryTracker&) = delete;

  // Invalidates this tracker, no UMA Histogram report is generated.
  virtual void Cancel() = 0;

 protected:
  PeakGpuMemoryTracker() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PEAK_GPU_MEMORY_TRACKER_H_
