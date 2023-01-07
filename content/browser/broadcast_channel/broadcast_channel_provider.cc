// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/broadcast_channel/broadcast_channel_service.h"

#include "content/public/browser/storage_partition.h"

namespace content {

BroadcastChannelProvider::~BroadcastChannelProvider() = default;

BroadcastChannelProvider::BroadcastChannelProvider(
    BroadcastChannelService* broadcast_channel_service,
    const blink::StorageKey& storage_key)
    : storage_key_(storage_key),
      broadcast_channel_service_(std::move(broadcast_channel_service)) {}

void BroadcastChannelProvider::ConnectToChannel(
    const std::string& name,
    mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient> client,
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
        connection) {
  DCHECK(broadcast_channel_service_);
  broadcast_channel_service_->ConnectToChannel(
      storage_key_, name, std::move(client), std::move(connection));
}

}  // namespace content
