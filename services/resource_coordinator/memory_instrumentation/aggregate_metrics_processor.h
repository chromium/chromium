// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_AGGREGATE_METRICS_PROCESSOR_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_AGGREGATE_METRICS_PROCESSOR_H_

#include <map>

#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {
mojom::AggregatedMetricsPtr ComputeGlobalNativeCodeResidentMemoryKb(
    const std::map<base::ProcessId, mojom::RawOSMemDump*>& pid_to_pmd);

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_AGGREGATE_METRICS_PROCESSOR_H_
