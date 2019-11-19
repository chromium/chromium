// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/resource_coordinator_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"

namespace resource_coordinator {

ResourceCoordinatorService::ResourceCoordinatorService(
    mojo::PendingReceiver<mojom::ResourceCoordinatorService> receiver)
    : receiver_(this, std::move(receiver)) {}

ResourceCoordinatorService::~ResourceCoordinatorService() = default;

void ResourceCoordinatorService::BindMemoryInstrumentationCoordinatorController(
    mojo::PendingReceiver<memory_instrumentation::mojom::CoordinatorController>
        receiver) {
  memory_instrumentation_coordinator_.BindController(std::move(receiver));
}

void ResourceCoordinatorService::RegisterHeapProfiler(
    mojo::PendingRemote<memory_instrumentation::mojom::HeapProfiler> profiler,
    mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfilerHelper>
        receiver) {
  memory_instrumentation_coordinator_.RegisterHeapProfiler(std::move(profiler),
                                                           std::move(receiver));
}

}  // namespace resource_coordinator
