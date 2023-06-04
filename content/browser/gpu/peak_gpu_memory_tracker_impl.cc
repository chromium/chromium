// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/peak_gpu_memory_tracker_impl.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/gpu_data_manager.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace content {

namespace {

// These count values should be recalculated in case of changes to the number
// of values in their respective enums.
constexpr int kUsageTypeCount =
    static_cast<int>(PeakGpuMemoryTracker::Usage::USAGE_MAX) + 1;
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

constexpr const char* GetUsageName(PeakGpuMemoryTracker::Usage usage) {
  switch (usage) {
    case PeakGpuMemoryTracker::Usage::CHANGE_TAB:
      return "ChangeTab2";
    case PeakGpuMemoryTracker::Usage::PAGE_LOAD:
      return "PageLoad";
    case PeakGpuMemoryTracker::Usage::SCROLL:
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

std::string GetPeakMemoryUsageUMAName(PeakGpuMemoryTracker::Usage usage) {
  return base::StrCat({"Memory.GPU.PeakMemoryUsage2.", GetUsageName(usage)});
}

std::string GetPeakMemoryAllocationSourceUMAName(
    PeakGpuMemoryTracker::Usage usage,
    gpu::GpuPeakMemoryAllocationSource source) {
  return base::StrCat({"Memory.GPU.PeakMemoryAllocationSource2.",
                       GetUsageName(usage), ".",
                       GetAllocationSourceName(source)});
}

// Callback provided to the GpuService, which will be notified of the
// |peak_memory| used. This will then report that to UMA Histograms, for the
// requested |usage|. Some tests may provide an optional |testing_callback| in
// order to sync tests with the work done here on the IO thread.
void PeakMemoryCallback(PeakGpuMemoryTracker::Usage usage,
                        base::OnceClosure testing_callback,
                        const uint64_t peak_memory,
                        const base::flat_map<gpu::GpuPeakMemoryAllocationSource,
                                             uint64_t>& allocation_per_source) {
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

}  // namespace

// static
std::unique_ptr<PeakGpuMemoryTracker> PeakGpuMemoryTracker::Create(
    PeakGpuMemoryTracker::Usage usage) {
  return std::make_unique<PeakGpuMemoryTrackerImpl>(usage);
}

// static
uint32_t PeakGpuMemoryTrackerImpl::next_sequence_number_ = 0;

PeakGpuMemoryTrackerImpl::PeakGpuMemoryTrackerImpl(
    PeakGpuMemoryTracker::Usage usage)
    : usage_(usage) {
  // TODO(thiabaud): Do this call inline, since this happens on the UI thread.
  //
  // Actually performs request to GPU service to begin memory tracking for
  // |sequence_number_|. This will normally be created from the UI thread, so
  // repost to the IO thread.
  GpuProcessHost::CallOnUI(
      FROM_HERE, GPU_PROCESS_KIND_SANDBOXED, /* force_create=*/false,
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
  if (canceled_)
    return;

  // TODO(thiabaud): Do this call inline, since this happens on the UI thread.
  GpuProcessHost::CallOnUI(
      FROM_HERE, GPU_PROCESS_KIND_SANDBOXED, /* force_create=*/false,
      base::BindOnce(
          [](uint32_t sequence_num, PeakGpuMemoryTracker::Usage usage,
             base::OnceClosure testing_callback, GpuProcessHost* host) {
            // There may be no host nor service available. This may occur during
            // shutdown, when the service is fully disabled, and in some tests.
            // In those cases there is nothing to report to UMA. However we
            // still run the optional testing callback.
            if (!host) {
              std::move(testing_callback).Run();
              return;
            }
            if (auto* gpu_service = host->gpu_service()) {
              gpu_service->GetPeakMemoryUsage(
                  sequence_num, base::BindOnce(&PeakMemoryCallback, usage,
                                               std::move(testing_callback)));
            }
          },
          sequence_num_, usage_,
          std::move(post_gpu_service_callback_for_testing_)));
}

void PeakGpuMemoryTrackerImpl::Cancel() {
  canceled_ = true;
  // TODO(thiabaud): Do this call inline, since this happens on the UI thread.
  //
  // Notify the GpuProcessHost that we are done observing this sequence.
  GpuProcessHost::CallOnUI(FROM_HERE, GPU_PROCESS_KIND_SANDBOXED,
                           /* force_create=*/false,
                           base::BindOnce(
                               [](uint32_t sequence_num, GpuProcessHost* host) {
                                 if (!host)
                                   return;
                                 if (auto* gpu_service = host->gpu_service())
                                   gpu_service->GetPeakMemoryUsage(
                                       sequence_num, base::DoNothing());
                               },
                               sequence_num_));
}

}  // namespace content
