// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_SERVICE_H_
#define CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_SERVICE_H_

#include <map>
#include "content/common/content_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/messaging/cloneable_message.mojom.h"

namespace content {

class CONTENT_EXPORT BroadcastChannelService {
 public:
  BroadcastChannelService();
  // Not copyable or moveable, since this will be a singleton owned by
  // StoragePartitionImpl
  BroadcastChannelService(const BroadcastChannelService&) = delete;
  BroadcastChannelService& operator=(const BroadcastChannelService&) = delete;
  ~BroadcastChannelService();

  void ConnectToChannel(
      const url::Origin& origin,
      const std::string& name,
      mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient>
          client,
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
          connection);

 private:
  class Connection;

  void UnregisterConnection(Connection*);
  void ReceivedMessageOnConnection(Connection*,
                                   const blink::CloneableMessage& message);

  std::map<url::Origin, std::multimap<std::string, std::unique_ptr<Connection>>>
      connections_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_SERVICE_H_
