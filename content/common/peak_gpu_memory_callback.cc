// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/common/peak_gpu_memory_callback.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"

namespace content {

namespace {

// These count values should be recalculated in case of changes to the number
// of values in their respective enums.
constexpr int kUsageTypeCount =
    static_cast<int>(input::PeakGpuMemoryTracker::Usage::USAGE_MAX) + 1;
constexpr int kAllocationSourceTypeCount =
    static_cast<int>(gpu::GpuPeakMemoryAllocationSource::
                         GPU_PEAK_MEMORY_ALLOCATION_SOURCE_MAX) +
    1;
constexpr int kAllocationSourceHistogramIndex =
    kUsageTypeCount * kAllocationSourceTypeCount;

// Histogram values based on MEMORY_METRICS_HISTOGRAM_MB, allowing this to match
// Memory.Gpu.PrivateMemoryFootprint. Previously this was reported in KB, with a
// maximum of 500 MB. However that maximum is too low for Mac.
constexpr int kMemoryHistogramMin = 1;
constexpr int kMemoryHistogramMax = 64000;
constexpr int kMemoryHistogramBucketCount = 100;

constexpr const char* GetUsageName(input::PeakGpuMemoryTracker::Usage usage) {
  switch (usage) {
    case input::PeakGpuMemoryTracker::Usage::CHANGE_TAB:
      return "ChangeTab2";
    case input::PeakGpuMemoryTracker::Usage::PAGE_LOAD:
      return "PageLoad";
    case input::PeakGpuMemoryTracker::Usage::SCROLL:
      return "Scroll";
  }
}

constexpr const char* GetAllocationSourceName(
    gpu::GpuPeakMemoryAllocationSource source) {
  switch (source) {
    case gpu::GpuPeakMemoryAllocationSource::UNKNOWN:
      return "Unknown";
    case gpu::GpuPeakMemoryAllocationSource::COMMAND_BUFFER:
      return "CommandBuffer";
    case gpu::GpuPeakMemoryAllocationSource::SHARED_CONTEXT_STATE:
      return "SharedContextState";
    case gpu::GpuPeakMemoryAllocationSource::SHARED_IMAGE_STUB:
      return "SharedImageStub";
    case gpu::GpuPeakMemoryAllocationSource::SKIA:
      return "Skia";
  }
}

std::string GetPeakMemoryUsageUMAName(
    input::PeakGpuMemoryTracker::Usage usage) {
  return base::StrCat({"Memory.GPU.PeakMemoryUsage2.", GetUsageName(usage)});
}

std::string GetPeakMemoryAllocationSourceUMAName(
    input::PeakGpuMemoryTracker::Usage usage,
    gpu::GpuPeakMemoryAllocationSource source) {
  return base::StrCat({"Memory.GPU.PeakMemoryAllocationSource2.",
                       GetUsageName(usage), ".",
                       GetAllocationSourceName(source)});
}

}  // namespace

// Callback provided to the GpuService, which will be notified of the
// |peak_memory| used. This will then report that to UMA Histograms, for the
// requested |usage|. Some tests may provide an optional |testing_callback| in
// order to sync tests with the work done here on the UI thread.
void PeakGpuMemoryCallback(
    input::PeakGpuMemoryTracker::Usage usage,
    base::OnceClosure testing_callback,
    const uint64_t peak_memory,
    const base::flat_map<gpu::GpuPeakMemoryAllocationSource, uint64_t>&
        allocation_per_source) {
  uint64_t memory_in_mb = peak_memory / 1048576u;
  STATIC_HISTOGRAM_POINTER_GROUP(
      GetPeakMemoryUsageUMAName(usage), static_cast<int>(usage),
      kUsageTypeCount, Add(memory_in_mb),
      base::Histogram::FactoryGet(
          GetPeakMemoryUsageUMAName(usage), kMemoryHistogramMin,
          kMemoryHistogramMax, kMemoryHistogramBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));

  for (auto& source : allocation_per_source) {
    uint64_t source_memory_in_mb = source.second / 1048576u;
    STATIC_HISTOGRAM_POINTER_GROUP(
        GetPeakMemoryAllocationSourceUMAName(usage, source.first),
        static_cast<int>(usage) * kAllocationSourceTypeCount +
            static_cast<int>(source.first),
        kAllocationSourceHistogramIndex, Add(source_memory_in_mb),
        base::Histogram::FactoryGet(
            GetPeakMemoryAllocationSourceUMAName(usage, source.first),
            kMemoryHistogramMin, kMemoryHistogramMax,
            kMemoryHistogramBucketCount,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }

  std::move(testing_callback).Run();
}

}  // namespace content
