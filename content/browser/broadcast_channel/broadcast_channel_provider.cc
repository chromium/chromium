// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/broadcast_channel/broadcast_channel_provider.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {

// There is a one-to-one mapping of BroadcastChannel instances in the renderer
// and Connection instances in the browser. The Connection is owned by a
// BroadcastChannelProvider.
class BroadcastChannelProvider::Connection
    : public blink::mojom::BroadcastChannelClient {
 public:
  Connection(
      const url::Origin& origin,
      const std::string& name,
      mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient>
          client,
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
          connection,
      BroadcastChannelProvider* service);

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

  BroadcastChannelProvider* service_;
  url::Origin origin_;
  std::string name_;
};

BroadcastChannelProvider::Connection::Connection(
    const url::Origin& origin,
    const std::string& name,
    mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient> client,
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
        connection,
    BroadcastChannelProvider* service)
    : receiver_(this, std::move(connection)),
      client_(std::move(client)),
      service_(service),
      origin_(origin),
      name_(name) {}

void BroadcastChannelProvider::Connection::OnMessage(
    blink::CloneableMessage message) {
  service_->ReceivedMessageOnConnection(this, message);
}

BroadcastChannelProvider::BroadcastChannelProvider() = default;

BroadcastChannelProvider::~BroadcastChannelProvider() = default;

mojo::ReceiverId BroadcastChannelProvider::Connect(
    SecurityPolicyHandle security_policy_handle,
    mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver) {
  return receivers_.Add(this, std::move(receiver),
                        std::make_unique<SecurityPolicyHandle>(
                            std::move(security_policy_handle)));
}

void BroadcastChannelProvider::ConnectToChannel(
    const url::Origin& origin,
    const std::string& name,
    mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient> client,
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
        connection) {
  const auto& security_policy_handle = receivers_.current_context();
  if (!security_policy_handle->CanAccessDataForOrigin(origin)) {
    mojo::ReportBadMessage("BROADCAST_CHANNEL_INVALID_ORIGIN");
    return;
  }

  std::unique_ptr<Connection> c(new Connection(origin, name, std::move(client),
                                               std::move(connection), this));
  c->set_connection_error_handler(
      base::BindRepeating(&BroadcastChannelProvider::UnregisterConnection,
                          base::Unretained(this), c.get()));
  connections_[origin].insert(std::make_pair(name, std::move(c)));
}

void BroadcastChannelProvider::UnregisterConnection(Connection* c) {
  url::Origin origin = c->origin();
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

void BroadcastChannelProvider::ReceivedMessageOnConnection(
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

}  // namespace content
