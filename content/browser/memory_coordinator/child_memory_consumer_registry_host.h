// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_HOST_H_
#define CONTENT_BROWSER_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_HOST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
#include "content/common/memory_coordinator/mojom/memory_coordinator_diagnostics.mojom.h"
#endif

namespace content {

// An implementation of mojom::ChildMemoryConsumerRegistryHost that registers
// memory consumer groups in a child process with a
// MemoryConsumerGroupController. An instance of this class is created for each
// child process connection.
class CONTENT_EXPORT ChildMemoryConsumerRegistryHost
    : public mojom::ChildMemoryConsumerRegistryHost,
      public MemoryConsumerGroupHost
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
    ,
      public mojom::MemoryCoordinatorDiagnosticsHost
#endif
{
 public:
  // `disconnect_handler` is the callback that will be run when the connection
  // with the child process is lost (i.e. a Mojo pipe is closed, or the child
  // process exited). This must delete the instance.
  ChildMemoryConsumerRegistryHost(
      MemoryConsumerGroupController& controller,
      ProcessType process_type,
      ChildProcessId child_process_id,
      mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver,
      base::OnceClosure disconnect_handler);

  ChildMemoryConsumerRegistryHost(const ChildMemoryConsumerRegistryHost&) =
      delete;
  ChildMemoryConsumerRegistryHost& operator=(
      const ChildMemoryConsumerRegistryHost&) = delete;

  ~ChildMemoryConsumerRegistryHost() override;

  // mojom::ChildMemoryConsumerRegistryHost:
  void BindCoordinator(mojo::PendingRemote<mojom::ChildMemoryCoordinator>
                           coordinator_remote) override;
  void Register(const std::string& consumer_id,
                std::optional<base::MemoryConsumerTraits> traits) override;
  void Unregister(const std::string& consumer_id) override;

  // MemoryConsumerGroupHost:
  void UpdateConsumers(std::vector<MemoryConsumerUpdate> updates) override;

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  // mojom::MemoryCoordinatorDiagnosticsHost:
  void OnMemoryLimitChanged(const std::string& consumer_id,
                            int32_t memory_limit) override;

  // Enables/disables additional diagnostics reported by the child process.
  void EnableDiagnosticsReporting();
  void DisableDiagnosticsReporting();
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

 private:
  class RenderProcessExitedObserver;

  void RunDisconnectHandler();

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  void EnableReportingImpl();
#endif

  const raw_ref<MemoryConsumerGroupController> controller_;

  const ProcessType process_type_;
  const ChildProcessId child_process_id_;

  mojo::Receiver<mojom::ChildMemoryConsumerRegistryHost> receiver_;
  mojo::Remote<mojom::ChildMemoryCoordinator> coordinator_remote_;

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  bool diagnostics_enabled_ = false;
  mojo::Receiver<mojom::MemoryCoordinatorDiagnosticsHost>
      diagnostics_host_receiver_{this};
#endif

  // Handles a disconnection with the child process.
  base::OnceClosure disconnect_handler_;

  // Holds the IDs of consumers living in the child process.
  absl::flat_hash_set<std::string> consumers_;

  std::unique_ptr<RenderProcessExitedObserver> process_observer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_HOST_H_
