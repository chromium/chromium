// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_FACTORY_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_FACTORY_H_

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "net/http/http_transaction_factory.h"

namespace network {

class SharedDictionaryManager;

// A HttpTransactionFactory to create SharedDictionaryNetworkTransactions.
class COMPONENT_EXPORT(NETWORK_SERVICE)
    SharedDictionaryNetworkTransactionFactory
    : public net::HttpTransactionFactory {
 public:
  SharedDictionaryNetworkTransactionFactory(
      SharedDictionaryManager& shared_dictionary_manager,
      std::unique_ptr<net::HttpTransactionFactory> network_layer);

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
  raw_ref<SharedDictionaryManager> shared_dictionary_manager_;
  std::unique_ptr<net::HttpTransactionFactory> network_layer_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_FACTORY_H_
