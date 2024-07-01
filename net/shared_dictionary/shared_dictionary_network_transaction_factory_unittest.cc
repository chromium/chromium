// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/shared_dictionary/shared_dictionary_network_transaction_factory.h"

#include "net/base/net_errors.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class DummyHttpTransactionFactory : public HttpTransactionFactory {
 public:
  explicit DummyHttpTransactionFactory()
      : network_layer_(std::make_unique<MockNetworkLayer>()) {}

  DummyHttpTransactionFactory(const DummyHttpTransactionFactory&) = delete;
  DummyHttpTransactionFactory& operator=(const DummyHttpTransactionFactory&) =
      delete;

  ~DummyHttpTransactionFactory() override = default;

  // HttpTransactionFactory methods:
  int CreateTransaction(RequestPriority priority,
                        std::unique_ptr<HttpTransaction>* trans) override {
    create_transaction_called_ = true;
    if (is_broken_) {
      return ERR_FAILED;
    }
    return network_layer_->CreateTransaction(priority, trans);
  }
  HttpCache* GetCache() override {
    get_cache_called_ = true;
    return network_layer_->GetCache();
  }
  HttpNetworkSession* GetSession() override {
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
  std::unique_ptr<HttpTransactionFactory> network_layer_;
};

TEST(SharedDictionaryNetworkTransactionFactoryTest, CreateTransaction) {
  auto dummy_factory = std::make_unique<DummyHttpTransactionFactory>();
  DummyHttpTransactionFactory* dummy_factory_ptr = dummy_factory.get();
  SharedDictionaryNetworkTransactionFactory factory =
      SharedDictionaryNetworkTransactionFactory(std::move(dummy_factory),
                                                /*enable_shared_zstd=*/true);
  std::unique_ptr<HttpTransaction> transaction;
  EXPECT_FALSE(dummy_factory_ptr->create_transaction_called());
  EXPECT_EQ(OK, factory.CreateTransaction(DEFAULT_PRIORITY, &transaction));
  EXPECT_TRUE(dummy_factory_ptr->create_transaction_called());
  EXPECT_TRUE(transaction);
}

TEST(SharedDictionaryNetworkTransactionFactoryTest, CreateTransactionFailure) {
  auto dummy_factory = std::make_unique<DummyHttpTransactionFactory>();
  DummyHttpTransactionFactory* dummy_factory_ptr = dummy_factory.get();
  SharedDictionaryNetworkTransactionFactory factory =
      SharedDictionaryNetworkTransactionFactory(std::move(dummy_factory),
                                                /*enable_shared_zstd=*/true);
  dummy_factory_ptr->set_is_broken();
  std::unique_ptr<HttpTransaction> transaction;
  EXPECT_EQ(ERR_FAILED,
            factory.CreateTransaction(DEFAULT_PRIORITY, &transaction));
  EXPECT_FALSE(transaction);
}

TEST(SharedDictionaryNetworkTransactionFactoryTest, GetCache) {
  auto dummy_factory = std::make_unique<DummyHttpTransactionFactory>();
  DummyHttpTransactionFactory* dummy_factory_ptr = dummy_factory.get();
  SharedDictionaryNetworkTransactionFactory factory =
      SharedDictionaryNetworkTransactionFactory(std::move(dummy_factory),
                                                /*enable_shared_zstd=*/true);
  EXPECT_FALSE(dummy_factory_ptr->get_cache_called());
  factory.GetCache();
  EXPECT_TRUE(dummy_factory_ptr->get_cache_called());
}

TEST(SharedDictionaryNetworkTransactionFactoryTest, GetSession) {
  auto dummy_factory = std::make_unique<DummyHttpTransactionFactory>();
  DummyHttpTransactionFactory* dummy_factory_ptr = dummy_factory.get();
  SharedDictionaryNetworkTransactionFactory factory =
      SharedDictionaryNetworkTransactionFactory(std::move(dummy_factory),
                                                /*enable_shared_zstd=*/true);
  EXPECT_FALSE(dummy_factory_ptr->get_session_called());
  factory.GetSession();
  EXPECT_TRUE(dummy_factory_ptr->get_session_called());
}

}  // namespace
}  // namespace net
