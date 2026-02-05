// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_HOST_H_
#define CONTENT_BROWSER_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_HOST_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

// An implementation of mojom::ChildMemoryConsumerRegistryHost that connects
// memory consumers in a child process with the main registry in the browser
// process. An instance of this class is created for each child process
// connection.
class CONTENT_EXPORT ChildMemoryConsumerRegistryHost
    : public mojom::ChildMemoryConsumerRegistryHost {
 public:
  // A delegate interface that receives registrations and deregistrations of
  // remote MemoryConsumers.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void AddMemoryConsumerFromChildProcess(
        std::string_view consumer_id,
        base::MemoryConsumerTraits traits,
        ProcessType process_type,
        ChildProcessId child_process_id,
        base::MemoryConsumer* consumer) = 0;

    virtual void RemoveMemoryConsumerFromChildProcess(
        std::string_view consumer_id,
        ChildProcessId child_process_id,
        base::MemoryConsumer* consumer) = 0;
  };

  ChildMemoryConsumerRegistryHost(Delegate& delegate,
                                  ProcessType process_type,
                                  ChildProcessId child_process_id);

  ChildMemoryConsumerRegistryHost(const ChildMemoryConsumerRegistryHost&) =
      delete;
  ChildMemoryConsumerRegistryHost& operator=(
      const ChildMemoryConsumerRegistryHost&) = delete;

  ~ChildMemoryConsumerRegistryHost() override;

  // Sets a callback that will be run when the coordinator remote disconnects.
  void SetDisconnectHandler(base::OnceClosure handler);

  // mojom::ChildMemoryConsumerRegistryHost:
  void BindCoordinator(mojo::PendingRemote<mojom::ChildMemoryCoordinator>
                           coordinator_remote) override;
  void Register(const std::string& consumer_id,
                base::MemoryConsumerTraits traits) override;
  void Unregister(const std::string& consumer_id) override;

 private:
  class ChildMemoryConsumer;

  void NotifyReleaseMemory(const std::string& consumer_id);
  void NotifyUpdateMemoryLimit(const std::string& consumer_id, int percentage);

  const raw_ref<Delegate> delegate_;

  const ProcessType process_type_;
  const ChildProcessId child_process_id_;

  mojo::Remote<mojom::ChildMemoryCoordinator> coordinator_remote_;

  // Handles a disconnection with `coordinator_remote_`.
  base::OnceClosure disconnect_handler_;

  // Holds a ChildMemoryConsumer for each consumer group that lives in a
  // child process.
  absl::flat_hash_map<std::string, std::unique_ptr<ChildMemoryConsumer>>
      consumers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_HOST_H_
