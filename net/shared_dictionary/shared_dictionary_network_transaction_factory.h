
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_FACTORY_H_
#define NET_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_FACTORY_H_

#include "net/base/net_export.h"
#include "net/http/http_transaction_factory.h"

namespace net {

// A HttpTransactionFactory to create SharedDictionaryNetworkTransactions.
class NET_EXPORT SharedDictionaryNetworkTransactionFactory
    : public net::HttpTransactionFactory {
 public:
  SharedDictionaryNetworkTransactionFactory(
      std::unique_ptr<net::HttpTransactionFactory> network_layer,
      bool enable_shared_zstd);

  SharedDictionaryNetworkTransactionFactory(
      const SharedDictionaryNetworkTransactionFactory&) = delete;
  SharedDictionaryNetworkTransactionFactory& operator=(
      const SharedDictionaryNetworkTransactionFactory&) = delete;

  ~SharedDictionaryNetworkTransactionFactory() override;

  // net::HttpTransactionFactory methods:
  int CreateTransaction(net::RequestPriority priority,
                        std::unique_ptr<net::HttpTransaction>* trans) override;
  net::HttpCache* GetCache() override;
  net::HttpNetworkSession* GetSession() override;

 private:
  std::unique_ptr<net::HttpTransactionFactory> network_layer_;
  const bool enable_shared_zstd_;
};

}  // namespace net

#endif  // NET_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_FACTORY_H_
