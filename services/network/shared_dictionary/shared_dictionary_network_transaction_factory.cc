// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_network_transaction_factory.h"

#include "services/network/shared_dictionary/shared_dictionary_network_transaction.h"

namespace network {

SharedDictionaryNetworkTransactionFactory::
    SharedDictionaryNetworkTransactionFactory(
        SharedDictionaryManager& shared_dictionary_manager,
        std::unique_ptr<net::HttpTransactionFactory> network_layer)
    : shared_dictionary_manager_(shared_dictionary_manager),
      network_layer_(std::move(network_layer)) {}

SharedDictionaryNetworkTransactionFactory::
    ~SharedDictionaryNetworkTransactionFactory() = default;

int SharedDictionaryNetworkTransactionFactory::CreateTransaction(
    net::RequestPriority priority,
    std::unique_ptr<net::HttpTransaction>* trans) {
  std::unique_ptr<net::HttpTransaction> network_transaction;
  int rv = network_layer_->CreateTransaction(priority, &network_transaction);
  if (rv != net::OK) {
    return rv;
  }
  *trans = std::make_unique<SharedDictionaryNetworkTransaction>(
      *shared_dictionary_manager_, std::move(network_transaction));
  return net::OK;
}

net::HttpCache* SharedDictionaryNetworkTransactionFactory::GetCache() {
  return network_layer_->GetCache();
}

net::HttpNetworkSession*
SharedDictionaryNetworkTransactionFactory::GetSession() {
  return network_layer_->GetSession();
}

}  // namespace network
