// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_
#define CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_

#include <optional>

#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/scoped_observation.h"
#include "content/child/memory_coordinator/browser_memory_coordinator_bridge.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/background_policy.h"
#include "content/common/memory_coordinator/memory_consumer_registry.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/common/memory_coordinator/memory_pressure_policy.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom-forward.h"
#include "content/common/process_priority_tracker.h"
#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/public/common/process_type.h"

namespace content {

// ChildMemoryCoordinator is a singleton that owns both the
// MemoryConsumerRegistry and the MemoryCoordinatorPolicyManager.
class CONTENT_EXPORT ChildMemoryCoordinator
    : public ProcessPriorityTracker::ProcessPriorityObserver {
 public:
  static ChildMemoryCoordinator& Get();

  ChildMemoryCoordinator();

  ChildMemoryCoordinator(const ChildMemoryCoordinator&) = delete;
  ChildMemoryCoordinator& operator=(const ChildMemoryCoordinator&) = delete;

  ~ChildMemoryCoordinator() override;

  // Applies feature-gated configuration to the registry and instantiates the
  // policies that don't depend on the process type. Must be called once, after
  // the FeatureList is initialized, and before BindAndPassReceiver(). This is
  // separate from the constructor because the coordinator may be created before
  // the FeatureList (e.g. in the zygote). No policy exists before this call, so
  // that no consumer is notified before the registry applies the "disabled"
  // overrides.
  void InitializePolicies();

  // Instantiates the policies that depend on `process_type`. Must be called
  // once, after InitializePolicies(), on the main thread once its task runner
  // is initialized. Called from the main function of the process types that
  // have such policies (e.g. RendererMain()).
  void InitializeProcessTypePolicies(ProcessType process_type);

  MemoryConsumerRegistry& registry() { return registry_.Get(); }
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

  // Allows connecting this process's global instance with the browser process.
  static mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
  BindAndPassReceiver();

 private:
  // ProcessPriorityTracker::ProcessPriorityObserver:
  void OnPriorityChanged(base::Process::Priority process_priority) override;

  MemoryCoordinatorPolicyManager policy_manager_;
  base::ScopedMemoryConsumerRegistry<MemoryConsumerRegistry> registry_{
      PROCESS_TYPE_UNKNOWN, ChildProcessId(), policy_manager_};

  BrowserMemoryCoordinatorBridge browser_memory_coordinator_bridge_{
      policy_manager_};

  // The policies are registered by InitializePolicies().
  std::optional<MemoryCoordinatorPolicyRegistration>
      browser_memory_coordinator_bridge_registration_;
  std::optional<MemoryPressurePolicy> memory_pressure_policy_;
  std::optional<MemoryCoordinatorPolicyRegistration>
      memory_pressure_policy_registration_;

  // Used to CHECK that InitializeProcessTypePolicies() is called only once.
  bool process_type_policies_initialized_ = false;

  // Driven by the ProcessPriorityTracker.
  std::optional<BackgroundPolicy> background_policy_;

  // Declared after `background_policy_` so that observation stops before the
  // policy is destroyed.
  base::ScopedObservation<ProcessPriorityTracker,
                          ProcessPriorityTracker::ProcessPriorityObserver>
      process_priority_observation_{this};
};

}  // namespace content

#endif  // CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_
