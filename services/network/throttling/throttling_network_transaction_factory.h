// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_TRANSACTION_FACTORY_H_
#define SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_TRANSACTION_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction_factory.h"

namespace net {
class HttpCache;
class HttpNetworkSession;
class HttpTransaction;
}  // namespace net

namespace network {

// NetworkTransactionFactory wraps HttpNetworkTransactions.
class COMPONENT_EXPORT(NETWORK_SERVICE) ThrottlingNetworkTransactionFactory
    : public net::HttpTransactionFactory {
 public:
  explicit ThrottlingNetworkTransactionFactory(
      net::HttpNetworkSession* session);

  ThrottlingNetworkTransactionFactory(
      const ThrottlingNetworkTransactionFactory&) = delete;
  ThrottlingNetworkTransactionFactory& operator=(
      const ThrottlingNetworkTransactionFactory&) = delete;

  ~ThrottlingNetworkTransactionFactory() override;

  // net::HttpTransactionFactory methods:
  int CreateTransaction(net::RequestPriority priority,
                        std::unique_ptr<net::HttpTransaction>* trans) override;
  net::HttpCache* GetCache() override;
  net::HttpNetworkSession* GetSession() override;

 private:
  std::unique_ptr<net::HttpTransactionFactory> network_layer_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_TRANSACTION_FACTORY_H_
