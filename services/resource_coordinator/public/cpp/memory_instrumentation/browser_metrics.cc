// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"

#include <cmath>

#include "base/rand_util.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace memory_instrumentation {
namespace {

const char kAudioServiceHistogramName[] = "AudioService";
const char kBrowserHistogramName[] = "Browser";
const char kCdmServiceHistogramName[] = "CdmService";
const char kExtensionHistogramName[] = "Extension";
const char kGpuHistogramName[] = "Gpu";
#if BUILDFLAG(IS_WIN)
const char kMediaFoundationServiceHistogramName[] = "MediaFoundationService";
#endif
const char kNetworkServiceHistogramName[] = "NetworkService";
const char kPaintPreviewCompositorHistogramName[] = "PaintPreviewCompositor";
const char kRendererHistogramName[] = "Renderer";
const char kUtilityHistogramName[] = "Utility";

}  // namespace

const char kMemoryHistogramPrefix[] = "Memory.";

const char* HistogramProcessTypeToString(HistogramProcessType type) {
  switch (type) {
    case HistogramProcessType::kAudioService:
      return kAudioServiceHistogramName;
    case HistogramProcessType::kBrowser:
      return kBrowserHistogramName;
    case HistogramProcessType::kCdmService:
      return kCdmServiceHistogramName;
    case HistogramProcessType::kExtension:
      return kExtensionHistogramName;
    case HistogramProcessType::kGpu:
      return kGpuHistogramName;
#if BUILDFLAG(IS_WIN)
    case HistogramProcessType::kMediaFoundationService:
      return kMediaFoundationServiceHistogramName;
#endif
    case HistogramProcessType::kNetworkService:
      return kNetworkServiceHistogramName;
    case HistogramProcessType::kPaintPreviewCompositor:
      return kPaintPreviewCompositorHistogramName;
    case HistogramProcessType::kRenderer:
      return kRendererHistogramName;
    case HistogramProcessType::kUtility:
      return kUtilityHistogramName;
  }
}

std::string GetPrivateFootprintHistogramName(HistogramProcessType type) {
  return std::string(kMemoryHistogramPrefix) +
         HistogramProcessTypeToString(type) + ".PrivateMemoryFootprint";
}

base::TimeDelta GetDelayForNextMemoryLog() {
#if BUILDFLAG(IS_ANDROID)
  base::TimeDelta mean_time = base::Minutes(5);
#else
  base::TimeDelta mean_time = base::Minutes(30);
#endif
  // Compute the actual delay before sampling using a Poisson process. Use
  // `1-RandDouble()` to avoid log(0).
  double uniform = base::RandDouble();
  return -std::log(1 - uniform) * mean_time;
}

}  // namespace memory_instrumentation
