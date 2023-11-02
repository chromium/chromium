// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/pending_trust_token_store.h"

namespace network {

PendingTrustTokenStore::PendingTrustTokenStore() = default;
PendingTrustTokenStore::~PendingTrustTokenStore() = default;

void PendingTrustTokenStore::OnStoreReady(
    std::unique_ptr<TrustTokenStore> store) {
  DCHECK(store);
  DCHECK(!store_);
  store_ = std::move(store);
  ExecuteAll();
}

void PendingTrustTokenStore::ExecuteOrEnqueue(
    base::OnceCallback<void(TrustTokenStore*)> fn) {
  if (store_) {
    std::move(fn).Run(store_.get());
    return;
  }
  queue_.push_back(std::move(fn));
}

void PendingTrustTokenStore::ExecuteAll() {
  DCHECK(store_);

  while (!queue_.empty()) {
    std::move(queue_.front()).Run(store_.get());
    queue_.pop_front();
  }
}

}  // namespace network
