// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/ip_protection_auth_token_cache_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

const int kExpectedBatchSize = 1024;
const base::Time kFutureExpiration = base::Time::Now() + base::Hours(1);

class MockIpProtectionAuthTokenGetter
    : public network::mojom::IpProtectionAuthTokenGetter {
  using TryGetAuthTokensResult =
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>;

 public:
  MockIpProtectionAuthTokenGetter() : num_try_get_auth_token_calls_(0) {}

  // Register an expectation of a call to `TryGetAuthToken()` returning the
  // given tokens.
  void ExpectTryGetAuthTokensCall(uint32_t batch_size,
                                  TryGetAuthTokensResult result) {
    expected_try_get_auth_token_calls_.emplace_back(batch_size,
                                                    std::move(result));
  }

  // True if all expected `TryGetAuthToken` calls have occurred.
  bool GotAllExpectedTryGetAuthTokensCalls() {
    return num_try_get_auth_token_calls_ ==
           expected_try_get_auth_token_calls_.size();
  }

  // Reset all test expectations.
  void Reset() {
    num_try_get_auth_token_calls_ = 0;
    expected_try_get_auth_token_calls_.clear();
  }

  void TryGetAuthTokens(uint32_t batch_size,
                        TryGetAuthTokensCallback callback) override {
    ASSERT_LT(num_try_get_auth_token_calls_,
              expected_try_get_auth_token_calls_.size())
        << "Unexpected call to TryGetAuthToken";
    auto pair = std::move(
        expected_try_get_auth_token_calls_[num_try_get_auth_token_calls_++]);
    EXPECT_EQ(batch_size, pair.first);

    std::move(callback).Run(std::move(pair.second));
  }

 protected:
  // The expected responses to TryGetAuthToken calls.
  std::vector<std::pair<uint32_t, TryGetAuthTokensResult>>
      expected_try_get_auth_token_calls_;
  size_t num_try_get_auth_token_calls_;
};

}  // namespace

class IpProtectionAuthTokenCacheImplTest : public testing::Test {
 protected:
  IpProtectionAuthTokenCacheImplTest()
      : receiver_(&mock_),
        auth_token_cache_(std::make_unique<IpProtectionAuthTokenCacheImpl>(
            receiver_.BindNewPipeAndPassRemote())) {}

  base::test::TaskEnvironment task_environment_;

  MockIpProtectionAuthTokenGetter mock_;
  mojo::Receiver<network::mojom::IpProtectionAuthTokenGetter> receiver_;

  // The IpProtectionAuthTokenCache being tested.
  std::unique_ptr<IpProtectionAuthTokenCacheImpl> auth_token_cache_;
};

// `MayNeedAuthTokenSoon()` triggers a request for a single token, and once that
// token is delivered, `GetAuthToken()` returns it. That token is only handed
// out once, but a new token can be fetched.
TEST_F(IpProtectionAuthTokenCacheImplTest, MayNeedAuthTokenSoon_Fills_Cache) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
  tokens.emplace_back(
      network::mojom::BlindSignedAuthToken::New("token1", kFutureExpiration));
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));

  // Indicate that a token will be required soon.
  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  // Get the single token in that batch.
  auto got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "token1");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);

  // But that's the only one.
  got_token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(got_token);

  // Request another token and wait for it.
  mock_.Reset();
  tokens.clear();
  tokens.emplace_back(
      network::mojom::BlindSignedAuthToken::New("token2", kFutureExpiration));
  tokens.emplace_back(
      network::mojom::BlindSignedAuthToken::New("token3", kFutureExpiration));
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));

  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  // Get the two new tokens.
  got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "token2");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
  got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "token3");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
  got_token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(got_token);
}

// `MayNeedAuthTokenSoon()` can be called repeatedly, and only gets one token
// batch.
TEST_F(IpProtectionAuthTokenCacheImplTest, MayNeedAuthTokenSoon_Repeatedly) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
  for (int i = 0; i < kExpectedBatchSize; i++) {
    tokens.emplace_back(
        network::mojom::BlindSignedAuthToken::New("token", kFutureExpiration));
  }
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));

  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();
  auth_token_cache_->MayNeedAuthTokenSoon();
  auth_token_cache_->MayNeedAuthTokenSoon();

  EXPECT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
}

// If `TryGetAuthTokens()` returns `nullopt`, the cache size does not change.
TEST_F(IpProtectionAuthTokenCacheImplTest, MayNeedAuthTokenSoon_Nullopt) {
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, absl::nullopt);

  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
  auto token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(token);
}

// If `TryGetAuthTokens()` returns an empty batch but not `nullopt`, the cache
// size does not change.
TEST_F(IpProtectionAuthTokenCacheImplTest, MayNeedAuthTokenSoon_EmptyBatch) {
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, {});

  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
  auto token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(token);
}

// `GetAuthToken()` skips expired tokens and returns a non-expired token,
// if one is found in the cache.
TEST_F(IpProtectionAuthTokenCacheImplTest, SkipExpiredTokens) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
  const base::Time kExpired = base::Time::Now() - base::Hours(10);
  for (int i = 0; i < kExpectedBatchSize - 1; i++) {
    tokens.emplace_back(
        network::mojom::BlindSignedAuthToken::New("old-token", kExpired));
  }
  tokens.emplace_back(network::mojom::BlindSignedAuthToken::New(
      "good-token", kFutureExpiration));
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));

  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();

  auto got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "good-token");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
}

// `GetAuthToken()` returns nothing if `MayNeedAuthTokenSoon()` is not called.
TEST_F(IpProtectionAuthTokenCacheImplTest, GetToken_Returns_Nothing) {
  auto token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(token);
}

// If the `IpProtectionAuthTokenGetter` is nullptr, no tokens are gotten,
// but things don't crash.
TEST_F(IpProtectionAuthTokenCacheImplTest, Null_Getter) {
  auto auth_token_cache = IpProtectionAuthTokenCacheImpl(
      mojo::PendingRemote<network::mojom::IpProtectionAuthTokenGetter>());
  auth_token_cache.MayNeedAuthTokenSoon();
  auto token = auth_token_cache.GetAuthToken();
  ASSERT_FALSE(token);
}

}  // namespace network
