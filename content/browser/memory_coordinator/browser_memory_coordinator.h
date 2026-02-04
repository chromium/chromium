// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_H_
#define CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_H_

#include "base/memory_coordinator/memory_consumer_registry.h"
#include "content/browser/memory_coordinator/browser_memory_consumer_registry.h"
#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// BrowserMemoryCoordinator is a singleton that owns both the
// BrowserMemoryConsumerRegistry and the ChildMemoryConsumerRegistryHost.
class CONTENT_EXPORT BrowserMemoryCoordinator {
 public:
  static BrowserMemoryCoordinator& Get();

  BrowserMemoryCoordinator();

  BrowserMemoryCoordinator(const BrowserMemoryCoordinator&) = delete;
  BrowserMemoryCoordinator& operator=(const BrowserMemoryCoordinator&) = delete;

  ~BrowserMemoryCoordinator();

  BrowserMemoryConsumerRegistry& registry() { return registry_.Get(); }
  ChildMemoryConsumerRegistryHost& host() { return host_; }

  // Connects a ChildMemoryConsumerRegistry in a child process with the browser
  // process.
  void Bind(
      ProcessType process_type,
      ChildProcessId child_process_id,
      mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver);

 private:
  MemoryCoordinatorPolicyManager policy_manager_;
  base::ScopedMemoryConsumerRegistry<BrowserMemoryConsumerRegistry> registry_{
      policy_manager_};
  ChildMemoryConsumerRegistryHost host_{registry_.Get()};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_COORDINATOR_H_
