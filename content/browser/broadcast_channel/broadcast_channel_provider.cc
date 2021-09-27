// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include <memory>

#include "base/bind.h"
#include "content/browser/storage_partition_impl.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {

BroadcastChannelProvider::~BroadcastChannelProvider() = default;

BroadcastChannelProvider::BroadcastChannelProvider(
    base::WeakPtr<StoragePartitionImpl> storage_partition_impl) {
  storage_partition_impl_ = storage_partition_impl;
}

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

  if (storage_partition_impl_) {
    BroadcastChannelService* service =
        storage_partition_impl_->GetBroadcastChannelService();
    DCHECK(service);
    service->ConnectToChannel(origin, name, std::move(client),
                              std::move(connection));
  }
}

}  // namespace content
