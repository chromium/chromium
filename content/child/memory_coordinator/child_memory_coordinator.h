// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_
#define CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_

#include "base/memory_coordinator/memory_consumer_registry.h"
#include "content/child/memory_coordinator/browser_memory_coordinator_bridge.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_registry.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/common/memory_coordinator/memory_pressure_listener_policy.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom-forward.h"

namespace content {

// ChildMemoryCoordinator is a singleton that owns both the
// ChildMemoryConsumerRegistry and the MemoryCoordinatorPolicyManager.
class CONTENT_EXPORT ChildMemoryCoordinator {
 public:
  static ChildMemoryCoordinator& Get();

  ChildMemoryCoordinator();

  ChildMemoryCoordinator(const ChildMemoryCoordinator&) = delete;
  ChildMemoryCoordinator& operator=(const ChildMemoryCoordinator&) = delete;

  ~ChildMemoryCoordinator();

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
  MemoryCoordinatorPolicyRegistration<BrowserMemoryCoordinatorBridge>
      browser_bridge_registration_{policy_manager_,
                                   browser_memory_coordinator_bridge_};

  MemoryPressureListenerPolicy memory_pressure_listener_policy_{
      policy_manager_};
  MemoryCoordinatorPolicyRegistration<MemoryPressureListenerPolicy>
      pressure_listener_registration_{policy_manager_,
                                      memory_pressure_listener_policy_};
};

}  // namespace content

#endif  // CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_COORDINATOR_H_
