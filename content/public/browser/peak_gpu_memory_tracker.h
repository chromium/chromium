// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PEAK_GPU_MEMORY_TRACKER_H_
#define CONTENT_PUBLIC_BROWSER_PEAK_GPU_MEMORY_TRACKER_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "content/common/content_export.h"

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
// See PeakGpuMemoryTracker::Create.
class CONTENT_EXPORT PeakGpuMemoryTracker {
 public:
  // The callback will be provided with the peak memory usage in Bytes. Or 0 if
  // getting a report from the GPU service was not possible.
  using PeakMemoryCallback = base::OnceCallback<void(uint64_t)>;

  // Creates the PeakGpuMemoryTracker, which performs the registration with the
  // GPU service. Destroy the PeakGpuMemoryTracker to request a report from the
  // GPU service. The report will be delivered to |callback| on the thread which
  // calls Create.
  static std::unique_ptr<PeakGpuMemoryTracker> Create(
      PeakMemoryCallback callback);

  virtual ~PeakGpuMemoryTracker() = default;

  PeakGpuMemoryTracker(const PeakGpuMemoryTracker*) = delete;
  PeakGpuMemoryTracker& operator=(const PeakGpuMemoryTracker&) = delete;

  // Invalidates this tracker, i.e. the callback is never called when tracking
  // stops.
  virtual void Cancel() = 0;

 protected:
  PeakGpuMemoryTracker() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PEAK_GPU_MEMORY_TRACKER_H_
