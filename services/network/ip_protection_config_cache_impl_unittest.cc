// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/ip_protection_config_cache_impl.h"
#include "services/network/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection_proxy_list_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

class MockIpProtectionTokenCacheManager : public IpProtectionTokenCacheManager {
 public:
  bool IsAuthTokenAvailable() override { return auth_token_.has_value(); }

  void InvalidateTryAgainAfterTime() override {}

  absl::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken()
      override {
    return std::move(auth_token_);
  }

  void SetAuthToken(
      absl::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token) {
    auth_token_ = std::move(auth_token);
  }

 private:
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> auth_token_;
};

class MockIpProtectionProxyListManager : public IpProtectionProxyListManager {
 public:
  bool IsProxyListAvailable() override { return proxy_list_.has_value(); }

  const std::vector<std::string>& ProxyList() override { return *proxy_list_; }

  void RequestRefreshProxyList() override {
    if (on_force_refresh_proxy_list_) {
      std::move(on_force_refresh_proxy_list_).Run();
    }
  }

  // Set the proxy list returned from `ProxyList()`.
  void SetProxyList(std::vector<std::string> proxy_list) {
    proxy_list_ = std::move(proxy_list);
  }

  void SetOnRequestRefreshProxyList(
      base::OnceClosure on_force_refresh_proxy_list) {
    on_force_refresh_proxy_list_ = std::move(on_force_refresh_proxy_list);
  }

 private:
  absl::optional<std::vector<std::string>> proxy_list_;
  base::OnceClosure on_force_refresh_proxy_list_;
};

}  // namespace

class IpProtectionConfigCacheImplTest : public testing::Test {
 protected:
  IpProtectionConfigCacheImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        receiver_(nullptr),
        ipp_config_cache_(std::make_unique<IpProtectionConfigCacheImpl>(
            receiver_.BindNewPipeAndPassRemote())) {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  mojo::Receiver<network::mojom::IpProtectionConfigGetter> receiver_;

  // The IpProtectionConfigCache being tested.
  std::unique_ptr<IpProtectionConfigCacheImpl> ipp_config_cache_;
};

// Token cache manager returns available token.
TEST_F(IpProtectionConfigCacheImplTest, GetAuthTokenFromManager) {
  auto exp_token = mojom::BlindSignedAuthToken::New();
  exp_token->token = "a-token";
  auto ipp_token_cache_manager_ =
      std::make_unique<MockIpProtectionTokenCacheManager>();
  ipp_token_cache_manager_->SetAuthToken(std::move(exp_token));
  ipp_config_cache_->SetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer::kProxyA,
      std::move(ipp_token_cache_manager_));

  ASSERT_TRUE(ipp_config_cache_->AreAuthTokensAvailable());
  ASSERT_TRUE(ipp_config_cache_->GetAuthToken(
      network::mojom::IpProtectionProxyLayer::kProxyA));
}

// Proxy list manager returns currently cached proxy hostnames.
TEST_F(IpProtectionConfigCacheImplTest, GetProxyListFromManager) {
  std::vector<std::string> exp_proxy_list = {"a-proxy"};
  auto ipp_proxy_list_manager_ =
      std::make_unique<MockIpProtectionProxyListManager>();
  ipp_proxy_list_manager_->SetProxyList(exp_proxy_list);
  ipp_config_cache_->SetIpProtectionProxyListManagerForTesting(
      std::move(ipp_proxy_list_manager_));

  ASSERT_TRUE(ipp_config_cache_->IsProxyListAvailable());
  EXPECT_EQ(ipp_config_cache_->GetProxyList(), exp_proxy_list);
}

}  // namespace network
