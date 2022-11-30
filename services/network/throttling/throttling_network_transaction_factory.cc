// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/throttling_network_transaction_factory.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "net/base/net_errors.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_transaction.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_transaction.h"

namespace network {

ThrottlingNetworkTransactionFactory::ThrottlingNetworkTransactionFactory(
    net::HttpNetworkSession* session)
    : network_layer_(new net::HttpNetworkLayer(session)) {}

ThrottlingNetworkTransactionFactory::~ThrottlingNetworkTransactionFactory() {}

int ThrottlingNetworkTransactionFactory::CreateTransaction(
    net::RequestPriority priority,
    std::unique_ptr<net::HttpTransaction>* trans) {
  std::unique_ptr<net::HttpTransaction> network_transaction;
  int rv = network_layer_->CreateTransaction(priority, &network_transaction);
  if (rv != net::OK) {
    return rv;
  }
  *trans = std::make_unique<ThrottlingNetworkTransaction>(
      std::move(network_transaction));
  return net::OK;
}

net::HttpCache* ThrottlingNetworkTransactionFactory::GetCache() {
  return network_layer_->GetCache();
}

net::HttpNetworkSession* ThrottlingNetworkTransactionFactory::GetSession() {
  return network_layer_->GetSession();
}

}  // namespace network
