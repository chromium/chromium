// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_PROVIDER_H_
#define CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_PROVIDER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT BroadcastChannelProvider
    : public blink::mojom::BroadcastChannelProvider {
 public:
  explicit BroadcastChannelProvider(base::WeakPtr<StoragePartitionImpl>);
  BroadcastChannelProvider() = delete;
  ~BroadcastChannelProvider() override;

  using SecurityPolicyHandle = ChildProcessSecurityPolicyImpl::Handle;
  mojo::ReceiverId Connect(
      SecurityPolicyHandle security_policy_handle,
      mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver);

  void ConnectToChannel(
      const url::Origin& origin,
      const std::string& name,
      mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient>
          client,
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
          connection) override;

  auto& receivers_for_testing() { return receivers_; }

 private:
  mojo::ReceiverSet<blink::mojom::BroadcastChannelProvider,
                    std::unique_ptr<SecurityPolicyHandle>>
      receivers_;

  base::WeakPtr<StoragePartitionImpl> storage_partition_impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROADCAST_CHANNEL_BROADCAST_CHANNEL_PROVIDER_H_
