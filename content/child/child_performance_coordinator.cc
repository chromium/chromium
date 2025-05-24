// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_performance_coordinator.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace content {

ChildPerformanceCoordinator::ChildPerformanceCoordinator() = default;

ChildPerformanceCoordinator::~ChildPerformanceCoordinator() = default;

mojo::PendingReceiver<performance_manager::mojom::ChildProcessCoordinationUnit>
ChildPerformanceCoordinator::InitializeAndPassReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto receiver = coordination_unit_.BindNewPipeAndPassReceiver();
  coordination_unit_->InitializeChildProcessCoordination(
      perfetto::ProcessTrack::Current().uuid,
      base::BindOnce(
          &ChildPerformanceCoordinator::OnInitializeChildProcessCoordination,
          weak_factory_.GetWeakPtr()));
  return receiver;
}

void ChildPerformanceCoordinator::OnInitializeChildProcessCoordination(
    base::ReadOnlySharedMemoryRegion global_region,
    base::ReadOnlySharedMemoryRegion process_region) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  using performance_scenarios::ScenarioScope;
  if (global_region.IsValid()) {
    global_scenario_memory_.emplace(ScenarioScope::kGlobal,
                                    std::move(global_region));
  }
  if (process_region.IsValid()) {
    process_scenario_memory_.emplace(ScenarioScope::kCurrentProcess,
                                     std::move(process_region));
  }
}

}  // namespace content
