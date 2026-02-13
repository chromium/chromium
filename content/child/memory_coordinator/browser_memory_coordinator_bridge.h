// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_BRIDGE_H_
#define CONTENT_CHILD_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_BRIDGE_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/sequence_checker.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

// Implementation of MemoryCoordinatorPolicy that bridges memory coordinator
// signals between the browser process and the child process.
class BrowserMemoryCoordinatorBridge : public MemoryCoordinatorPolicy,
                                       public mojom::ChildMemoryCoordinator {
 public:
  explicit BrowserMemoryCoordinatorBridge(
      MemoryCoordinatorPolicyManager& manager);
  ~BrowserMemoryCoordinatorBridge() override;

  // MemoryCoordinatorPolicy:
  void OnConsumerGroupAdded(std::string_view consumer_id,
                            base::MemoryConsumerTraits traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override;
  void OnConsumerGroupRemoved(std::string_view consumer_id,
                              ChildProcessId child_process_id) override;

  // mojom::ChildMemoryCoordinator:
  void UpdateConsumers(std::vector<MemoryConsumerUpdate> updates) override;

  // Binds this policy to the browser registry host.
  mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
  BindAndPassReceiver();

 private:
  // Used to register consumers in the child process with the browser process.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> registry_host_;

  // A mojom::ChildMemoryCoordinator connection with the browser process.
  mojo::Receiver<mojom::ChildMemoryCoordinator> receiver_{this};

  // Tracks all consumer groups known to this class.
  absl::flat_hash_map<std::string, base::MemoryConsumerTraits> groups_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_CHILD_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_BRIDGE_H_
