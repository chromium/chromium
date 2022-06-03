// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_PENDING_TRUST_TOKEN_STORE_H_
#define SERVICES_NETWORK_TRUST_TOKENS_PENDING_TRUST_TOKEN_STORE_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "services/network/trust_tokens/trust_token_store.h"

namespace network {

// PendingTrustTokenStore provides an asynchronous interface with which to
// wrap operations against a TrustTokenStore that becomes ready at some point
// after the pending store's creation.
class PendingTrustTokenStore {
 public:
  PendingTrustTokenStore();
  virtual ~PendingTrustTokenStore();

  PendingTrustTokenStore(const PendingTrustTokenStore&) = delete;
  PendingTrustTokenStore& operator=(const PendingTrustTokenStore&) = delete;

  // Accepts a token store with which to execute operations previously (and
  // subsequently) passed to |ExecuteOrEnqueue|. Call only once.
  void OnStoreReady(std::unique_ptr<TrustTokenStore> store);

  // Executes |fn| immediately if the store is available. Otherwise, defers the
  // operation in FIFO order until the store finishes initializing and then
  // immediately executes it.
  void ExecuteOrEnqueue(base::OnceCallback<void(TrustTokenStore*)> fn);

 private:
  // Clears |queue_|, popping and executing its operations from front to back.
  void ExecuteAll();

  base::circular_deque<base::OnceCallback<void(TrustTokenStore*)>> queue_;
  std::unique_ptr<TrustTokenStore> store_;
};

}  // namespace network

#endif  //  SERVICES_NETWORK_TRUST_TOKENS_PENDING_TRUST_TOKEN_STORE_H_
