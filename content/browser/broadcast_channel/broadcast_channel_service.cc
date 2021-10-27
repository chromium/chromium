// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/broadcast_channel/broadcast_channel_service.h"

#include "base/bind.h"
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
      const url::Origin& origin,
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
  const url::Origin& origin() const { return origin_; }
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
  BroadcastChannelService* service_;
  const url::Origin origin_;
  const std::string name_;
};

BroadcastChannelService::Connection::Connection(
    const url::Origin& origin,
    const std::string& name,
    mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient> client,
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
        connection,
    BroadcastChannelService* service)
    : receiver_(this, std::move(connection)),
      client_(std::move(client)),
      service_(service),
      origin_(origin),
      name_(name) {}

void BroadcastChannelService::Connection::OnMessage(
    blink::CloneableMessage message) {
  service_->ReceivedMessageOnConnection(this, message);
}

BroadcastChannelService::BroadcastChannelService() = default;
BroadcastChannelService::~BroadcastChannelService() = default;

void BroadcastChannelService::UnregisterConnection(Connection* c) {
  const url::Origin origin = c->origin();
  auto& connections = connections_[origin];
  for (auto it = connections.lower_bound(c->name()),
            end = connections.upper_bound(c->name());
       it != end; ++it) {
    if (it->second.get() == c) {
      connections.erase(it);
      break;
    }
  }
  if (connections.empty())
    connections_.erase(origin);
}

void BroadcastChannelService::ReceivedMessageOnConnection(
    Connection* c,
    const blink::CloneableMessage& message) {
  auto& connections = connections_[c->origin()];
  for (auto it = connections.lower_bound(c->name()),
            end = connections.upper_bound(c->name());
       it != end; ++it) {
    if (it->second.get() != c)
      it->second->MessageToClient(message);
  }
}

void BroadcastChannelService::ConnectToChannel(
    const url::Origin& origin,
    const std::string& name,
    mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient> client,
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
        connection) {
  std::unique_ptr<Connection> c = std::make_unique<Connection>(
      origin, name, std::move(client), std::move(connection), this);

  c->set_connection_error_handler(
      base::BindRepeating(&BroadcastChannelService::UnregisterConnection,
                          base::Unretained(this), c.get()));
  connections_[origin].insert(std::make_pair(name, std::move(c)));
}
}  // namespace content
