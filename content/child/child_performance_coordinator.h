// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PERFORMANCE_COORDINATOR_H_
#define CONTENT_CHILD_CHILD_PERFORMANCE_COORDINATOR_H_

#include <optional>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Communicates performance information with the browser over a
// performance_manager::mojom::ChildProcessCoordinationUnit interface.
class CONTENT_EXPORT ChildPerformanceCoordinator {
 public:
  ChildPerformanceCoordinator();
  ~ChildPerformanceCoordinator();

  ChildPerformanceCoordinator(const ChildPerformanceCoordinator&) = delete;
  ChildPerformanceCoordinator& operator=(const ChildPerformanceCoordinator&) =
      delete;

  // Returns a PendingReceiver to forward to the browser process, and queues an
  // InitializeChildProcessCoordination message on it.
  mojo::PendingReceiver<
      performance_manager::mojom::ChildProcessCoordinationUnit>
  InitializeAndPassReceiver();

 private:
  // Receives the results of
  // ChildProcessCoordinationUnit::InitializeChildProcessCoordination().
  void OnInitializeChildProcessCoordination(
      base::ReadOnlySharedMemoryRegion global_region,
      base::ReadOnlySharedMemoryRegion process_region);

  SEQUENCE_CHECKER(sequence_checker_);

  // An interface to the browser's PerformanceManager.
  mojo::Remote<performance_manager::mojom::ChildProcessCoordinationUnit>
      coordination_unit_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Scopers to manage the lifetime of shared memory regions for performance
  // scenarios. These regions are read by functions in
  // //components/performance_manager/scenario_api/performance_scenarios.h.
  std::optional<performance_scenarios::ScopedReadOnlyScenarioMemory>
      process_scenario_memory_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<performance_scenarios::ScopedReadOnlyScenarioMemory>
      global_scenario_memory_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<ChildPerformanceCoordinator> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_PERFORMANCE_COORDINATOR_H_
