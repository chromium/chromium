// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/throttling_network_transaction_factory.h"

#include <memory>

#include "net/http/http_network_transaction.h"
#include "services/network/throttling/throttling_network_transaction.h"

namespace network {

ThrottlingNetworkTransactionFactory::ThrottlingNetworkTransactionFactory(
    std::unique_ptr<net::HttpTransactionFactory> network_layer)
    : network_layer_(std::move(network_layer)) {}

ThrottlingNetworkTransactionFactory::~ThrottlingNetworkTransactionFactory() {}

std::unique_ptr<net::HttpTransaction>
ThrottlingNetworkTransactionFactory::CreateTransaction(
    net::RequestPriority priority) {
  return std::make_unique<ThrottlingNetworkTransaction>(
      network_layer_->CreateTransaction(priority));
}

net::HttpCache* ThrottlingNetworkTransactionFactory::GetCache() {
  return network_layer_->GetCache();
}

net::HttpNetworkSession* ThrottlingNetworkTransactionFactory::GetSession() {
  return network_layer_->GetSession();
}

}  // namespace network
