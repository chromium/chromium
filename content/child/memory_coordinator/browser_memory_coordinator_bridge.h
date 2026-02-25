// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_BRIDGE_H_
#define CONTENT_CHILD_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_BRIDGE_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/sequence_checker.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
#include "content/common/memory_coordinator/mojom/memory_coordinator_diagnostics.mojom.h"
#endif

namespace content {

// Implementation of MemoryCoordinatorPolicy that bridges memory coordinator
// signals between the browser process and the child process.
class BrowserMemoryCoordinatorBridge
    : public MemoryCoordinatorPolicy,
      public MemoryCoordinatorPolicyManager::Observer,
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

  // MemoryCoordinatorPolicyManager::Observer:
  void OnConsumerGroupAdded(std::string_view consumer_id,
                            std::optional<base::MemoryConsumerTraits> traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override;
  void OnConsumerGroupRemoved(std::string_view consumer_id,
                              ChildProcessId child_process_id) override;

  // mojom::ChildMemoryCoordinator:
  void UpdateConsumers(std::vector<MemoryConsumerUpdate> updates) override;
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  void EnableDiagnosticsReporting(
      mojo::PendingRemote<mojom::MemoryCoordinatorDiagnosticsHost> host)
      override;

  // MemoryCoordinatorPolicyManager::DiagnosticObserver:
  void OnMemoryLimitChanged(std::string_view consumer_id,
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

  // Used to register consumers in the child process with the browser process.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> registry_host_;

  // A mojom::ChildMemoryCoordinator connection with the browser process.
  mojo::Receiver<mojom::ChildMemoryCoordinator> receiver_{this};

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  // The remote used to push updates back to the browser when diagnostics
  // reporting is enabled.
  mojo::Remote<mojom::MemoryCoordinatorDiagnosticsHost> diagnostics_host_;
#endif

  // Tracks all consumer groups known to this class.
  absl::flat_hash_map<std::string, std::optional<base::MemoryConsumerTraits>>
      groups_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_CHILD_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_BRIDGE_H_
