// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_SERVICE_H_
#define CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_SERVICE_H_

#include <map>
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/messaging/cloneable_message.mojom.h"

namespace content {

class BroadcastChannelService {
 public:
  BroadcastChannelService();
  // Not copyable or moveable, since this will be a singleton owned by
  // StoragePartitionImpl
  BroadcastChannelService(const BroadcastChannelService&) = delete;
  BroadcastChannelService& operator=(const BroadcastChannelService&) = delete;
  ~BroadcastChannelService();

  void ConnectToChannel(
      const blink::StorageKey& storage_key,
      const std::string& name,
      mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient>
          client,
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
          connection);

  void AddReceiver(std::unique_ptr<BroadcastChannelProvider> provider,
                   mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider>
                       pending_receiver);

  void AddAssociatedReceiver(
      std::unique_ptr<BroadcastChannelProvider> provider,
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelProvider>
          pending_associated_receiver);

 private:
  class Connection;

  void UnregisterConnection(Connection*);
  void ReceivedMessageOnConnection(Connection*,
                                   const blink::CloneableMessage& message);

  // Holds non-associated receivers corresponding to the per-thread remote
  // used for BroadcastChannel instances created from a given worker context.
  mojo::UniqueReceiverSet<blink::mojom::BroadcastChannelProvider> receivers_;

  // Holds associated receivers that correspond to each renderer-side
  // BroadcastChannel instance created from frame contexts.
  mojo::UniqueAssociatedReceiverSet<blink::mojom::BroadcastChannelProvider>
      associated_receivers_;

  std::multimap<std::pair<blink::StorageKey, std::string>,
                std::unique_ptr<Connection>>
      connections_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_SERVICE_H_
