// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_IMPL_H_
#define CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_IMPL_H_

#include <cstddef>
#include <memory>

#include "base/memory_coordinator/memory_consumer_registry.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_registry.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/common/memory_coordinator/memory_pressure_listener_policy.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom-forward.h"
#include "content/public/browser/browser_memory_coordinator.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

class ChildMemoryConsumerRegistryHost;

// BrowserMemoryCoordinatorImpl is a singleton that owns the
// MemoryConsumerRegistry, the ChildMemoryConsumerRegistryHost
// instances, and the MemoryCoordinatorPolicyManager.
class CONTENT_EXPORT BrowserMemoryCoordinatorImpl
    : public BrowserMemoryCoordinator {
 public:
  static BrowserMemoryCoordinatorImpl& Get();

  BrowserMemoryCoordinatorImpl();

  BrowserMemoryCoordinatorImpl(const BrowserMemoryCoordinatorImpl&) = delete;
  BrowserMemoryCoordinatorImpl& operator=(const BrowserMemoryCoordinatorImpl&) =
      delete;

  ~BrowserMemoryCoordinatorImpl() override;

  // BrowserMemoryCoordinator:
  MemoryCoordinatorPolicyManager& policy_manager() override;

  MemoryConsumerRegistry& registry() { return registry_.Get(); }

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  // Adds/removes a diagnostic observer. When the first observer is added,
  // diagnostic reporting is enabled in all child processes.
  void AddDiagnosticObserver(
      MemoryCoordinatorPolicyManager::DiagnosticObserver* observer);
  void RemoveDiagnosticObserver(
      MemoryCoordinatorPolicyManager::DiagnosticObserver* observer);
#endif

  // Connects a MemoryConsumerRegistry in a child process with the browser.
  // Returns false for a duplicate `child_process_id`, leaving the existing
  // host intact and closing `receiver`. The caller must handle rejection.
  [[nodiscard]] bool Bind(
      ProcessType process_type,
      ChildProcessId child_process_id,
      mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver);

 private:
  void OnHostDisconnected(ChildProcessId child_process_id);

  MemoryCoordinatorPolicyManager policy_manager_;
  base::ScopedMemoryConsumerRegistry<MemoryConsumerRegistry> registry_{
      PROCESS_TYPE_BROWSER, ChildProcessId(), policy_manager_};
  absl::flat_hash_map<ChildProcessId,
                      std::unique_ptr<ChildMemoryConsumerRegistryHost>>
      hosts_;

  MemoryPressureListenerPolicy memory_pressure_listener_policy_{
      policy_manager_};
  MemoryCoordinatorPolicyRegistration
      memory_pressure_listener_policy_registration_{
          policy_manager_, memory_pressure_listener_policy_};

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  size_t diagnostic_observer_count_ = 0u;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_IMPL_H_
