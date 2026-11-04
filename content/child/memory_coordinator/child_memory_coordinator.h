// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_
#define CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_

#include <optional>

#include "base/memory_coordinator/memory_consumer_registry.h"
#include "content/child/memory_coordinator/browser_memory_coordinator_bridge.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_registry.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/common/memory_coordinator/memory_pressure_listener_policy.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom-forward.h"
#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"

namespace content {

// ChildMemoryCoordinator is a singleton that owns both the
// MemoryConsumerRegistry and the MemoryCoordinatorPolicyManager.
class CONTENT_EXPORT ChildMemoryCoordinator {
 public:
  static ChildMemoryCoordinator& Get();

  ChildMemoryCoordinator();

  ChildMemoryCoordinator(const ChildMemoryCoordinator&) = delete;
  ChildMemoryCoordinator& operator=(const ChildMemoryCoordinator&) = delete;

  ~ChildMemoryCoordinator();

  // Applies feature-gated configuration to the registry and instantiates the
  // policies. Must be called once, after the FeatureList is initialized, and
  // before BindAndPassReceiver(). This is separate from the constructor because
  // the coordinator may be created before the FeatureList (e.g. in the zygote).
  // No policy exists before this call, so that no consumer is notified before
  // the registry applies the "disabled" overrides.
  void InitializePolicies();

  MemoryConsumerRegistry& registry() { return registry_.Get(); }
  MemoryCoordinatorPolicyManager& policy_manager() { return policy_manager_; }

  // Allows connecting this process's global instance with the browser process.
  static mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
  BindAndPassReceiver();

 private:
  MemoryCoordinatorPolicyManager policy_manager_;
  base::ScopedMemoryConsumerRegistry<MemoryConsumerRegistry> registry_{
      PROCESS_TYPE_UNKNOWN, ChildProcessId(), policy_manager_};

  BrowserMemoryCoordinatorBridge browser_memory_coordinator_bridge_{
      policy_manager_};

  // The policies are registered by InitializePolicies().
  std::optional<MemoryCoordinatorPolicyRegistration>
      browser_memory_coordinator_bridge_registration_;
  std::optional<MemoryPressureListenerPolicy> memory_pressure_listener_policy_;
  std::optional<MemoryCoordinatorPolicyRegistration>
      memory_pressure_listener_policy_registration_;
};

}  // namespace content

#endif  // CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_
