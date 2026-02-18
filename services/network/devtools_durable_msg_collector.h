// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_COLLECTOR_H_
#define SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_COLLECTOR_H_

#include <queue>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/devtools_durable_msg.h"
#include "services/network/devtools_durable_msg_accounting_delegate.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace network {

class DevtoolsDurableMessageCollectorManager;

class COMPONENT_EXPORT(NETWORK_SERVICE) DevtoolsDurableMessageCollector
    : public DevtoolsDurableMessageAccountingDelegate,
      public mojom::DurableMessageCollector {
 public:
  explicit DevtoolsDurableMessageCollector(
      base::WeakPtr<DevtoolsDurableMessageCollectorManager> manager);
  ~DevtoolsDurableMessageCollector() override;

  void EnableForProfile(const base::UnguessableToken& profile_id,
                        EnableForProfileCallback callback) override;
  void DisableForProfile(const base::UnguessableToken& profile_id,
                         DisableForProfileCallback callback) override;
  void Configure(mojom::NetworkDurableMessageConfigPtr config,
                 ConfigureCallback) override;
  void Retrieve(const std::string& devtools_request_id,
                RetrieveCallback callback) override;
  base::WeakPtr<DevtoolsDurableMessage> CreateDurableMessage(
      const std::string& devtools_request_id);
  base::WeakPtr<DevtoolsDurableMessageCollector> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }
  void EvictMessage(const DevtoolsDurableMessage& message);

  // Accounting delegate overrides.
  void WillAddBytes(DevtoolsDurableMessage& message, int64_t size) override;
  void WillRemoveBytes(DevtoolsDurableMessage& message) override;
  void WillDestroyMessage(DevtoolsDurableMessage& message) override;

 private:
  base::queue<base::WeakPtr<DevtoolsDurableMessage>> message_queue_;

  int64_t max_buffer_size_ = 0;
  int64_t cur_buffer_size_ = 0;

  absl::flat_hash_map<std::string, std::unique_ptr<DevtoolsDurableMessage>>
      request_id_to_message_map_;

  base::WeakPtr<DevtoolsDurableMessageCollectorManager> manager_;
  base::WeakPtrFactory<DevtoolsDurableMessageCollector> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_COLLECTOR_H_
