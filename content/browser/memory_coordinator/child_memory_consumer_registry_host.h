// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_HOST_H_
#define CONTENT_BROWSER_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_HOST_H_

#include <memory>
#include <string>

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
// registries in child processes with the main registry in the browser process.
// There is only one instance of this class and it handles all connections.
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

  explicit ChildMemoryConsumerRegistryHost(Delegate& delegate);

  ChildMemoryConsumerRegistryHost(const ChildMemoryConsumerRegistryHost&) =
      delete;
  ChildMemoryConsumerRegistryHost& operator=(
      const ChildMemoryConsumerRegistryHost&) = delete;

  ~ChildMemoryConsumerRegistryHost() override;

  // Binds a mojo receiver from a child process to this host. All
  // MemoryConsumers registered through this receiver will be associated with
  // the given `process_type` and `child_process_id`.
  void Bind(
      ProcessType process_type,
      ChildProcessId child_process_id,
      mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver);

  // mojom::ChildMemoryConsumerRegistryHost:
  void Register(
      const std::string& consumer_id,
      base::MemoryConsumerTraits traits,
      mojo::PendingRemote<mojom::ChildMemoryConsumer> remote_consumer) override;

 private:
  // An implementation of base::MemoryConsumer that encapsulates a connection to
  // to a mojom::ChildMemoryConsumer in a child process. This enables uniform
  // handling of local and remote MemoryConsumers.
  class ChildMemoryConsumer : public base::MemoryConsumer {
   public:
    ChildMemoryConsumer(
        mojo::PendingRemote<mojom::ChildMemoryConsumer> remote_consumer,
        base::OnceCallback<void(ChildMemoryConsumer*)> on_disconnect_handler);
    ~ChildMemoryConsumer() override;

    // base::MemoryConsumer:
    void OnReleaseMemory() override;
    void OnUpdateMemoryLimit() override;

   private:
    mojo::Remote<mojom::ChildMemoryConsumer> remote_consumer_;
  };

  void OnChildMemoryConsumerDisconnected(
      ChildProcessId child_process_id,
      const std::string& consumer_id,
      ChildMemoryConsumer* child_memory_consumer);

  const raw_ref<Delegate> delegate_;

  struct ConnectionContext {
    ProcessType process_type;
    ChildProcessId child_process_id;
  };

  mojo::ReceiverSet<mojom::ChildMemoryConsumerRegistryHost, ConnectionContext>
      receivers_;

  using ConsumerKey = std::pair<std::string, ChildProcessId>;

  // Holds a ChildMemoryConsumer for each consumer group that lives in a
  // child process.
  absl::flat_hash_map<ConsumerKey, std::unique_ptr<ChildMemoryConsumer>>
      child_memory_consumers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_HOST_H_
