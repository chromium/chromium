// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/broadcast_channel/broadcast_channel_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"

namespace content {

// There is a one-to-one mapping of BroadcastChannel instances in the renderer
// and Connection instances in the browser. The Connection is owned by a
// BroadcastChannelService.
class BroadcastChannelService::Connection
    : public blink::mojom::BroadcastChannelClient {
 public:
  Connection(
      const blink::StorageKey& storage_key,
      const std::string& name,
      mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient>
          client,
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
          connection,
      BroadcastChannelService* service);

  void OnMessage(blink::CloneableMessage message) override;
  void MessageToClient(const blink::CloneableMessage& message) const {
    client_->OnMessage(message.ShallowClone());
  }
  const blink::StorageKey& storage_key() const { return storage_key_; }
  const std::string& name() const { return name_; }

  void set_connection_error_handler(
      const base::RepeatingClosure& error_handler) {
    receiver_.set_disconnect_handler(error_handler);
    client_.set_disconnect_handler(error_handler);
  }

 private:
  mojo::AssociatedReceiver<blink::mojom::BroadcastChannelClient> receiver_;
  mojo::AssociatedRemote<blink::mojom::BroadcastChannelClient> client_;

  // Note: We use a raw pointer here because each Connection is owned by
  // BroadcastChannelService, so the lifetime of each Connection object
  // should not exceed the lifetime of `service_`.
  raw_ptr<BroadcastChannelService> service_;
  const blink::StorageKey storage_key_;
  const std::string name_;
};

BroadcastChannelService::Connection::Connection(
    const blink::StorageKey& storage_key,
    const std::string& name,
    mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient> client,
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
        connection,
    BroadcastChannelService* service)
    : receiver_(this, std::move(connection)),
      client_(std::move(client)),
      service_(service),
      storage_key_(storage_key),
      name_(name) {}

void BroadcastChannelService::Connection::OnMessage(
    blink::CloneableMessage message) {
  service_->ReceivedMessageOnConnection(this, message);
}

BroadcastChannelService::BroadcastChannelService() = default;
BroadcastChannelService::~BroadcastChannelService() = default;

void BroadcastChannelService::UnregisterConnection(Connection* c) {
  const auto key = std::make_pair(c->storage_key(), c->name());
  for (auto it = connections_.lower_bound(key),
            end = connections_.upper_bound(key);
       it != end; ++it) {
    if (it->second.get() == c) {
      connections_.erase(it);
      break;
    }
  }
}

void BroadcastChannelService::ReceivedMessageOnConnection(
    Connection* c,
    const blink::CloneableMessage& message) {
  const auto key = std::make_pair(c->storage_key(), c->name());
  for (auto it = connections_.lower_bound(key),
            end = connections_.upper_bound(key);
       it != end; ++it) {
    if (it->second.get() != c)
      it->second->MessageToClient(message);
  }
}

void BroadcastChannelService::ConnectToChannel(
    const blink::StorageKey& storage_key,
    const std::string& name,
    mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient> client,
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
        connection) {
  std::unique_ptr<Connection> c = std::make_unique<Connection>(
      storage_key, name, std::move(client), std::move(connection), this);

  c->set_connection_error_handler(
      base::BindRepeating(&BroadcastChannelService::UnregisterConnection,
                          base::Unretained(this), c.get()));
  connections_.emplace(std::make_pair(storage_key, name), std::move(c));
}

void BroadcastChannelService::AddReceiver(
    std::unique_ptr<BroadcastChannelProvider> provider,
    mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider>
        pending_receiver) {
  receivers_.Add(std::move(provider), std::move(pending_receiver));
}

void BroadcastChannelService::AddAssociatedReceiver(
    std::unique_ptr<BroadcastChannelProvider> provider,
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelProvider>
        pending_associated_receiver) {
  associated_receivers_.Add(std::move(provider),
                            std::move(pending_associated_receiver));
}
}  // namespace content
