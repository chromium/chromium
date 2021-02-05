// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_manager.h"

#include "content/browser/buckets/bucket_manager_host.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content {

BucketManager::BucketManager() = default;
BucketManager::~BucketManager() = default;

void BucketManager::BindReceiver(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = hosts_.find(origin);
  if (it != hosts_.end()) {
    it->second->BindReceiver(std::move(receiver));
    return;
  }

  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    mojo::ReportBadMessage("Called Buckets from an insecure context");
    return;
  }

  bool insert_succeeded;
  std::tie(it, insert_succeeded) = hosts_.insert(
      {origin, std::make_unique<BucketManagerHost>(this, origin)});
  DCHECK(insert_succeeded);
  it->second->BindReceiver(std::move(receiver));
}

void BucketManager::OnHostReceiverDisconnect(BucketManagerHost* host,
                                             base::PassKey<BucketManagerHost>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host != nullptr);
  DCHECK_GT(hosts_.count(host->origin()), 0u);
  DCHECK_EQ(hosts_[host->origin()].get(), host);

  if (host->has_connected_receivers())
    return;

  hosts_.erase(host->origin());
}

}  // namespace content
