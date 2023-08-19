// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_network_transaction_factory.h"

#include "net/base/net_errors.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class DummySharedDictionaryManager : public SharedDictionaryManager {
 public:
  DummySharedDictionaryManager() = default;
  ~DummySharedDictionaryManager() override = default;

  scoped_refptr<SharedDictionaryStorage> CreateStorage(
      const net::SharedDictionaryIsolationKey& isolation_key) override {
    return nullptr;
  }
  void SetCacheMaxSize(uint64_t cache_max_size) override {}
  void ClearData(base::Time start_time,
                 base::Time end_time,
                 base::RepeatingCallback<bool(const GURL&)> url_matcher,
                 base::OnceClosure callback) override {}
  void ClearDataForIsolationKey(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceClosure callback) override {}
  void GetUsageInfo(base::OnceCallback<
                    void(const std::vector<net::SharedDictionaryUsageInfo>&)>
                        callback) override {}
  void GetSharedDictionaryInfo(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceCallback<
          void(std::vector<network::mojom::SharedDictionaryInfoPtr>)> callback)
      override {}
  void GetOriginsBetween(
      base::Time start_time,
      base::Time end_time,
      base::OnceCallback<void(const std::vector<url::Origin>&)> callback)
      override {}
};

class DummyHttpTransactionFactory : public net::HttpTransactionFactory {
 public:
  explicit DummyHttpTransactionFactory()
      : network_layer_(std::make_unique<net::MockNetworkLayer>()) {}

  DummyHttpTransactionFactory(const DummyHttpTransactionFactory&) = delete;
  DummyHttpTransactionFactory& operator=(const DummyHttpTransactionFactory&) =
      delete;

  ~DummyHttpTransactionFactory() override = default;

  // HttpTransactionFactory methods:
  int CreateTransaction(net::RequestPriority priority,
                        std::unique_ptr<net::HttpTransaction>* trans) override {
    create_transaction_called_ = true;
    if (is_broken_) {
      return net::ERR_FAILED;
    }
    return network_layer_->CreateTransaction(priority, trans);
  }
  net::HttpCache* GetCache() override {
    get_cache_called_ = true;
    return network_layer_->GetCache();
  }
  net::HttpNetworkSession* GetSession() override {
    get_session_called_ = true;
    return network_layer_->GetSession();
  }

  void set_is_broken() { is_broken_ = true; }
  bool create_transaction_called() const { return create_transaction_called_; }
  bool get_cache_called() const { return get_cache_called_; }
  bool get_session_called() const { return get_session_called_; }

 private:
  bool is_broken_ = false;
  bool create_transaction_called_ = false;
  bool get_cache_called_ = false;
  bool get_session_called_ = false;
  std::unique_ptr<net::HttpTransactionFactory> network_layer_;
};

TEST(SharedDictionaryNetworkTransactionFactoryTest, CreateTransaction) {
  DummySharedDictionaryManager dummy_manager;
  auto dummy_factory = std::make_unique<DummyHttpTransactionFactory>();
  DummyHttpTransactionFactory* dummy_factory_ptr = dummy_factory.get();
  SharedDictionaryNetworkTransactionFactory factory =
      SharedDictionaryNetworkTransactionFactory(dummy_manager,
                                                std::move(dummy_factory));
  std::unique_ptr<net::HttpTransaction> transaction;
  EXPECT_FALSE(dummy_factory_ptr->create_transaction_called());
  EXPECT_EQ(net::OK,
            factory.CreateTransaction(net::DEFAULT_PRIORITY, &transaction));
  EXPECT_TRUE(dummy_factory_ptr->create_transaction_called());
  EXPECT_TRUE(transaction);
}

TEST(SharedDictionaryNetworkTransactionFactoryTest, CreateTransactionFailure) {
  DummySharedDictionaryManager dummy_manager;
  auto dummy_factory = std::make_unique<DummyHttpTransactionFactory>();
  DummyHttpTransactionFactory* dummy_factory_ptr = dummy_factory.get();
  SharedDictionaryNetworkTransactionFactory factory =
      SharedDictionaryNetworkTransactionFactory(dummy_manager,
                                                std::move(dummy_factory));
  dummy_factory_ptr->set_is_broken();
  std::unique_ptr<net::HttpTransaction> transaction;
  EXPECT_EQ(net::ERR_FAILED,
            factory.CreateTransaction(net::DEFAULT_PRIORITY, &transaction));
  EXPECT_FALSE(transaction);
}

TEST(SharedDictionaryNetworkTransactionFactoryTest, GetCache) {
  DummySharedDictionaryManager dummy_manager;
  auto dummy_factory = std::make_unique<DummyHttpTransactionFactory>();
  DummyHttpTransactionFactory* dummy_factory_ptr = dummy_factory.get();
  SharedDictionaryNetworkTransactionFactory factory =
      SharedDictionaryNetworkTransactionFactory(dummy_manager,
                                                std::move(dummy_factory));
  EXPECT_FALSE(dummy_factory_ptr->get_cache_called());
  factory.GetCache();
  EXPECT_TRUE(dummy_factory_ptr->get_cache_called());
}

TEST(SharedDictionaryNetworkTransactionFactoryTest, GetSession) {
  DummySharedDictionaryManager dummy_manager;
  auto dummy_factory = std::make_unique<DummyHttpTransactionFactory>();
  DummyHttpTransactionFactory* dummy_factory_ptr = dummy_factory.get();
  SharedDictionaryNetworkTransactionFactory factory =
      SharedDictionaryNetworkTransactionFactory(dummy_manager,
                                                std::move(dummy_factory));
  EXPECT_FALSE(dummy_factory_ptr->get_session_called());
  factory.GetSession();
  EXPECT_TRUE(dummy_factory_ptr->get_session_called());
}

}  // namespace
}  // namespace network
