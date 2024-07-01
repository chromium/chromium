// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/shared_dictionary/shared_dictionary_network_transaction_factory.h"

#include "net/base/net_errors.h"
#include "net/shared_dictionary/shared_dictionary_network_transaction.h"

namespace net {

SharedDictionaryNetworkTransactionFactory::
    SharedDictionaryNetworkTransactionFactory(
        std::unique_ptr<HttpTransactionFactory> network_layer,
        bool enable_shared_zstd)
    : network_layer_(std::move(network_layer)),
      enable_shared_zstd_(enable_shared_zstd) {}

SharedDictionaryNetworkTransactionFactory::
    ~SharedDictionaryNetworkTransactionFactory() = default;

int SharedDictionaryNetworkTransactionFactory::CreateTransaction(
    RequestPriority priority,
    std::unique_ptr<HttpTransaction>* trans) {
  std::unique_ptr<HttpTransaction> network_transaction;
  int rv = network_layer_->CreateTransaction(priority, &network_transaction);
  if (rv != OK) {
    return rv;
  }
  *trans = std::make_unique<SharedDictionaryNetworkTransaction>(
      std::move(network_transaction), enable_shared_zstd_);
  return OK;
}

HttpCache* SharedDictionaryNetworkTransactionFactory::GetCache() {
  return network_layer_->GetCache();
}

HttpNetworkSession* SharedDictionaryNetworkTransactionFactory::GetSession() {
  return network_layer_->GetSession();
}

}  // namespace net
