// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_BRIDGE_H_
#define CONTENT_CHILD_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_BRIDGE_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/memory_coordinator/traits.h"
#include "base/sequence_checker.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
#include "content/common/memory_coordinator/mojom/memory_coordinator_diagnostics.mojom.h"
#endif

namespace content {

// Implementation of MemoryCoordinatorPolicy that bridges memory coordinator
// signals between the browser process and the child process.
class BrowserMemoryCoordinatorBridge
    : public MemoryCoordinatorPolicy,
      public mojom::ChildMemoryCoordinator
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
    ,
      public MemoryCoordinatorPolicyManager::DiagnosticObserver
#endif
{
 public:
  explicit BrowserMemoryCoordinatorBridge(
      MemoryCoordinatorPolicyManager& manager);
  ~BrowserMemoryCoordinatorBridge() override;

  // MemoryCoordinatorPolicy:
  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            base::MemoryConsumerTraits traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override;
  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override;

  // mojom::ChildMemoryCoordinator:
  void UpdateConsumers(std::vector<MemoryConsumerUpdate> updates) override;
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  void EnableDiagnosticsReporting(
      mojo::PendingRemote<mojom::MemoryCoordinatorDiagnosticsHost> host)
      override;

  // MemoryCoordinatorPolicyManager::DiagnosticObserver:
  void OnMemoryLimitChanged(uint32_t consumer_id,
                            ChildProcessId child_process_id,
                            int memory_limit) override;
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

  // Binds this policy to the browser registry host.
  mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
  BindAndPassReceiver();

 private:
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  void OnReportingHostDisconnected();
#endif

  // Posts FlushPendingRegistrations() if the host is connected, there are
  // pending registrations, and a flush isn't already scheduled. Consumers that
  // register in a burst (e.g. during process startup) are thereby coalesced
  // into a single Register() IPC.
  void ScheduleRegistrationFlush();

  // sends all `pending_registrations_` to the browser as a single batched
  // Register() IPC.
  void FlushPendingRegistrations();

  // Used to register consumers in the child process with the browser process.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> registry_host_;

  // A mojom::ChildMemoryCoordinator connection with the browser process.
  mojo::Receiver<mojom::ChildMemoryCoordinator> receiver_{this};

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  // The remote used to push updates back to the browser when diagnostics
  // reporting is enabled.
  mojo::Remote<mojom::MemoryCoordinatorDiagnosticsHost> diagnostics_host_;
#endif

  struct ConsumerDetails {
    std::string consumer_name;
    base::MemoryConsumerTraits traits;
  };
  // Tracks all consumer groups known to this class.
  absl::flat_hash_map<uint32_t, ConsumerDetails> groups_;

  // Consumer ids that have been added but whose registration has not yet been
  // flushed to the browser. A strict subset of `groups_`. Coalesced into one
  // Register() by FlushPendingRegistrations().
  absl::flat_hash_set<uint32_t> pending_registrations_;

  // Whether a FlushPendingRegistrations() task is already posted.
  bool flush_scheduled_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrowserMemoryCoordinatorBridge> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_CHILD_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_BRIDGE_H_
