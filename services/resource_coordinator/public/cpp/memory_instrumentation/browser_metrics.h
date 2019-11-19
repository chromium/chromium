// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_BROWSER_METRICS_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_BROWSER_METRICS_H_

#include <string>

#include "base/metrics/histogram_functions.h"

// Macro used for logging memory related metrics in mb.
#define MEMORY_METRICS_HISTOGRAM_MB(name, value) \
  base::UmaHistogramCustomCounts(name, value, 1, 64000, 100)

namespace base {
class TimeDelta;
}

namespace memory_instrumentation {

// Prefix for memory related histograms.
extern const char kMemoryHistogramPrefix[];

// Types of processes uses by chrome.
enum class HistogramProcessType {
  kAudioService,
  kBrowser,
  kExtension,
  kGpu,
  kNetworkService,
  kRenderer,
  kUtility,
};

// Returns a string used in histograms for the process of |type|.
const char* HistogramProcessTypeToString(HistogramProcessType type);

// Returns the memory footprint histogram name for the process of the specified
// type.
std::string GetPrivateFootprintHistogramName(HistogramProcessType type);

// Returns the delay used in logging memory related metrics.
base::TimeDelta GetDelayForNextMemoryLog();

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_BROWSER_METRICS_H_
