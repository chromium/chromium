// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_PROVIDER_H_
#define CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_PROVIDER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"

namespace content {

class BroadcastChannelService;

class BroadcastChannelProvider : public blink::mojom::BroadcastChannelProvider {
 public:
  BroadcastChannelProvider(BroadcastChannelService* broadcast_channel_service,
                           const blink::StorageKey& storage_key);

  BroadcastChannelProvider() = delete;
  ~BroadcastChannelProvider() override;

  void ConnectToChannel(
      const std::string& name,
      mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient>
          client,
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
          connection) override;

 private:
  const blink::StorageKey storage_key_;
  // Note: We use a raw pointer here because each BroadcastChannelProvider is
  // owned by BroadcastChannelService, so the lifetime of the former should not
  // exceed the lifetime of the latter.
  raw_ptr<BroadcastChannelService> broadcast_channel_service_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_PROVIDER_H_
