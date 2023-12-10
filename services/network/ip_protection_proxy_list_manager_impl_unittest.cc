// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection_proxy_list_manager_impl.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

class MockIpProtectionConfigGetter
    : public network::mojom::IpProtectionConfigGetter {
 public:
  ~MockIpProtectionConfigGetter() override = default;

  // Register an expectation of a call to `TryGetAuthTokens()` returning the
  // given tokens.
  void ExpectTryGetAuthTokensCall(
      uint32_t batch_size,
      std::vector<network::mojom::BlindSignedAuthTokenPtr> bsa_tokens) {
    NOTREACHED_NORETURN();
  }

  // Register an expectation of a call to `TryGetAuthTokens()` returning no
  // tokens and the given `try_again_after`.
  void ExpectTryGetAuthTokensCall(uint32_t batch_size,
                                  base::Time try_again_after) {
    NOTREACHED_NORETURN();
  }

  // Register an expectation of a call to `GetIpProtectionProxyList()`,
  // returning the given proxy list manager.
  void ExpectGetProxyListCall(
      std::vector<std::vector<std::string>> proxy_list) {
    expected_get_proxy_list_calls_.push_back(std::move(proxy_list));
  }

  // Register an expectation of a call to `GetProxyList()`, returning nullopt.
  void ExpectGetProxyListCallFailure() {
    expected_get_proxy_list_calls_.push_back(absl::nullopt);
  }

  // True if all expected `TryGetAuthTokens` calls have occurred.
  bool GotAllExpectedMockCalls() {
    return expected_get_proxy_list_calls_.empty();
  }

  // Reset all test expectations.
  void Reset() { expected_get_proxy_list_calls_.clear(); }

  void TryGetAuthTokens(uint32_t batch_size,
                        network::mojom::IpProtectionProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override {
    NOTREACHED_NORETURN();
  }

  void GetProxyList(GetProxyListCallback callback) override {
    ASSERT_FALSE(expected_get_proxy_list_calls_.empty())
        << "Unexpected call to GetProxyList";
    auto& exp = expected_get_proxy_list_calls_.front();
    std::move(callback).Run(std::move(exp));
    expected_get_proxy_list_calls_.pop_front();
  }

 protected:
  std::deque<absl::optional<std::vector<std::vector<std::string>>>>
      expected_get_proxy_list_calls_;
};

class IpProtectionProxyListManagerImplTest : public testing::Test {
 protected:
  IpProtectionProxyListManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        mock_(),
        receiver_(&mock_) {
    remote_ = mojo::Remote<network::mojom::IpProtectionConfigGetter>();
    remote_.Bind(receiver_.BindNewPipeAndPassRemote());
    ipp_proxy_list_ = std::make_unique<IpProtectionProxyListManagerImpl>(
        &remote_,
        /* disable_background_tasks_for_testing=*/true);
  }

  // Wait until the proxy list is refreshed.
  void WaitForProxyListRefresh() {
    ipp_proxy_list_->SetOnProxyListRefreshedForTesting(
        task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockIpProtectionConfigGetter mock_;

  mojo::Receiver<network::mojom::IpProtectionConfigGetter> receiver_;

  mojo::Remote<network::mojom::IpProtectionConfigGetter> remote_;

  // The IpProtectionProxyListImpl being tested.
  std::unique_ptr<IpProtectionProxyListManagerImpl> ipp_proxy_list_;
};

// The manager gets the proxy list on startup and once again on schedule.
TEST_F(IpProtectionProxyListManagerImplTest, ProxyListOnStartup) {
  std::vector<std::vector<std::string>> exp_proxy_list = {{"a-proxy"}};
  mock_.ExpectGetProxyListCall(exp_proxy_list);
  ipp_proxy_list_->EnableProxyListRefreshingForTesting();
  WaitForProxyListRefresh();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), exp_proxy_list);

  base::Time start = base::Time::Now();
  mock_.ExpectGetProxyListCall({{"b-proxy"}});
  WaitForProxyListRefresh();
  base::TimeDelta delay = net::features::kIpPrivacyProxyListFetchInterval.Get();
  EXPECT_EQ(base::Time::Now() - start, delay);

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  exp_proxy_list = {{"b-proxy"}};
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), exp_proxy_list);
}

// The manager refreshes the proxy list on demand, but only once even if
// `RequestRefreshProxyList()` is called repeatedly.
TEST_F(IpProtectionProxyListManagerImplTest, ProxyListRefresh) {
  mock_.ExpectGetProxyListCall({{"a-proxy"}});
  ipp_proxy_list_->RequestRefreshProxyList();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitForProxyListRefresh();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  std::vector<std::vector<std::string>> exp_proxy_list = {{"a-proxy"}};
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), exp_proxy_list);
}

// The manager gets the proxy list on startup and once again on schedule.
TEST_F(IpProtectionProxyListManagerImplTest, IsProxyListAvailableEvenIfEmpty) {
  mock_.ExpectGetProxyListCall({});
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitForProxyListRefresh();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
}

// The manager keeps its existing proxy list if it fails to fetch a new one.
TEST_F(IpProtectionProxyListManagerImplTest, ProxyListKeptAfterFailure) {
  std::vector<std::vector<std::string>> exp_proxy_list = {{"a-proxy"}};
  mock_.ExpectGetProxyListCall(exp_proxy_list);
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitForProxyListRefresh();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), exp_proxy_list);

  // Fast-forward long enough that we can fetch again
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  mock_.ExpectGetProxyListCallFailure();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitForProxyListRefresh();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), exp_proxy_list);
}

}  // namespace network
