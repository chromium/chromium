// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/ip_protection_auth_token_cache_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

const int kExpectedBatchSize = 64;
const base::Time kFutureExpiration = base::Time::Now() + base::Hours(1);
constexpr char kGetAuthTokenResultHistogram[] =
    "NetworkService.IpProtection.GetAuthTokenResult";
constexpr char kTokenSpendRateHistogram[] =
    "NetworkService.IpProtection.TokenSpendRate";
constexpr char kTokenExpirationRateHistogram[] =
    "NetworkService.IpProtection.TokenExpirationRate";
const base::TimeDelta kTokenRateMeasurementInterval = base::Minutes(5);

struct ExpectedTryGetAuthTokensCall {
  // The expected batch_size argument for the call.
  uint32_t batch_size;
  // The response to the call.
  absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
      bsa_tokens;
  absl::optional<base::Time> try_again_after;
};

class MockIpProtectionAuthTokenGetter
    : public network::mojom::IpProtectionAuthTokenGetter {
 public:
  MockIpProtectionAuthTokenGetter() : num_try_get_auth_token_calls_(0) {}

  // Register an expectation of a call to `TryGetAuthTokens()` returning the
  // given tokens.
  void ExpectTryGetAuthTokensCall(
      uint32_t batch_size,
      std::vector<network::mojom::BlindSignedAuthTokenPtr> bsa_tokens) {
    expected_try_get_auth_token_calls_.emplace_back(
        ExpectedTryGetAuthTokensCall{
            .batch_size = batch_size,
            .bsa_tokens = std::move(bsa_tokens),
            .try_again_after = absl::nullopt,
        });
  }

  // Register an expectation of a call to `TryGetAuthTokens()` returning no
  // tokens and the given `try_again_after`.
  void ExpectTryGetAuthTokensCall(uint32_t batch_size,
                                  base::Time try_again_after) {
    expected_try_get_auth_token_calls_.emplace_back(
        ExpectedTryGetAuthTokensCall{
            .batch_size = batch_size,
            .bsa_tokens = absl::nullopt,
            .try_again_after = try_again_after,
        });
  }

  // True if all expected `TryGetAuthTokens` calls have occurred.
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
        << "Unexpected call to TryGetAuthTokens";
    auto exp = std::move(
        expected_try_get_auth_token_calls_[num_try_get_auth_token_calls_++]);
    EXPECT_EQ(batch_size, exp.batch_size);

    std::move(callback).Run(std::move(exp.bsa_tokens), exp.try_again_after);
  }

 protected:
  std::vector<ExpectedTryGetAuthTokensCall> expected_try_get_auth_token_calls_;
  size_t num_try_get_auth_token_calls_;
};

}  // namespace

struct HistogramState {
  // Number of successful requests (true).
  int success;
  // Number of failed requests (false).
  int failure;
};

class IpProtectionAuthTokenCacheImplTest : public testing::Test {
 protected:
  IpProtectionAuthTokenCacheImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        receiver_(&mock_),
        auth_token_cache_(std::make_unique<IpProtectionAuthTokenCacheImpl>(
            receiver_.BindNewPipeAndPassRemote())) {}

  void ExpectHistogramState(HistogramState state) {
    histogram_tester_.ExpectBucketCount(kGetAuthTokenResultHistogram, true,
                                        state.success);
    histogram_tester_.ExpectBucketCount(kGetAuthTokenResultHistogram, false,
                                        state.failure);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockIpProtectionAuthTokenGetter mock_;
  mojo::Receiver<network::mojom::IpProtectionAuthTokenGetter> receiver_;

  // The IpProtectionAuthTokenCache being tested.
  std::unique_ptr<IpProtectionAuthTokenCacheImpl> auth_token_cache_;

  base::HistogramTester histogram_tester_;
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
  ExpectHistogramState(HistogramState{.success = 1, .failure = 0});

  // But that's the only one.
  got_token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(got_token);
  ExpectHistogramState(HistogramState{.success = 1, .failure = 1});

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
  ExpectHistogramState(HistogramState{.success = 2, .failure = 1});
  got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "token3");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
  ExpectHistogramState(HistogramState{.success = 3, .failure = 1});
  got_token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(got_token);
  ExpectHistogramState(HistogramState{.success = 3, .failure = 2});
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

// If `TryGetAuthTokens()` returns an empty batch but not `nullopt`, the cache
// size does not change.
TEST_F(IpProtectionAuthTokenCacheImplTest, MayNeedAuthTokenSoon_EmptyBatch) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));

  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
  auto token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(token);
}

// If `TryGetAuthTokens()` returns nullopt, `MayNeedAuthTokenSoon()` will not
// try again until `try_again_after`.
TEST_F(IpProtectionAuthTokenCacheImplTest, MayNeedAuthTokenSoon_TryAgainAfter) {
  const base::TimeDelta kBackoff = base::Seconds(10);
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize,
                                   base::Time::Now() + kBackoff);

  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();
  EXPECT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  // There are no tokens in the cache.
  auto token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(token);

  // Additional calls do nothing and specifically do not trigger an "Unexpected
  // call to TryGetAuthTokens()" failure.
  auth_token_cache_->MayNeedAuthTokenSoon();
  auth_token_cache_->MayNeedAuthTokenSoon();

  // After the backoff has elapsed, `MayNeedAuthTokenSoon()` triggers another
  // call to `TryGetAuthTokens()`.
  task_environment_.FastForwardBy(kBackoff);
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
  tokens.emplace_back(
      network::mojom::BlindSignedAuthToken::New("token1", kFutureExpiration));
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));

  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();
  EXPECT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  auto got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "token1");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
  ExpectHistogramState(HistogramState{.success = 1, .failure = 1});
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
  ExpectHistogramState(HistogramState{.success = 1, .failure = 0});
}

// `GetAuthToken()` returns nothing if `MayNeedAuthTokenSoon()` is not called.
TEST_F(IpProtectionAuthTokenCacheImplTest, GetToken_Returns_Nothing) {
  auto token = auth_token_cache_->GetAuthToken();
  ASSERT_FALSE(token);
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// If the `IpProtectionAuthTokenGetter` is nullptr, no tokens are gotten,
// but things don't crash.
TEST_F(IpProtectionAuthTokenCacheImplTest, Null_Getter) {
  auto auth_token_cache = IpProtectionAuthTokenCacheImpl(
      mojo::PendingRemote<network::mojom::IpProtectionAuthTokenGetter>());
  auth_token_cache.MayNeedAuthTokenSoon();
  auto token = auth_token_cache.GetAuthToken();
  ASSERT_FALSE(token);
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// Verify that the token spend rate is measured correctly.
TEST_F(IpProtectionAuthTokenCacheImplTest, TokenSpendRate) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;

  // Fill the cache with 5 tokens.
  for (int i = 0; i < 5; i++) {
    tokens.emplace_back(
        network::mojom::BlindSignedAuthToken::New("token", kFutureExpiration));
  }
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));

  // Indicate that a token will be required soon.
  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  // Get four tokens from the batch.
  for (int i = 0; i < 4; i++) {
    auto got_token = auth_token_cache_->GetAuthToken();
    EXPECT_EQ(got_token.value()->token, "token");
    EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
  }

  // Fast-forward to run the measurement timer.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // Four tokens in five minutes is a rate of 36 tokens per hour.
  histogram_tester_.ExpectUniqueSample(kTokenSpendRateHistogram, 48, 1);

  // Get the remaining token in the batch.
  auto got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "token");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);

  // Fast-forward to run the measurement timer again, for another interval.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // One token in five minutes is a rate of 12 tokens per hour.
  histogram_tester_.ExpectBucketCount(kTokenSpendRateHistogram, 12, 1);
  histogram_tester_.ExpectTotalCount(kTokenSpendRateHistogram, 2);
}

// Verify that the token expiration rate is measured correctly.
TEST_F(IpProtectionAuthTokenCacheImplTest, TokenExpirationRate) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;

  // Fill the cache with 1024 expired tokens. An entire batch expiring
  // in one 5-minute interval is a very likely event.
  const base::Time kExpired = base::Time::Now() - base::Hours(10);
  tokens.reserve(1024);
  for (int i = 0; i < 1024; i++) {
    tokens.emplace_back(
        network::mojom::BlindSignedAuthToken::New("token", kExpired));
  }
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));

  // Indicate that a token will be required soon.
  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  auth_token_cache_->MayNeedAuthTokenSoon();
  task_environment_.RunUntilQuit();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  // Try to get a token, which will incidentally record the expired tokens.
  auto got_token = auth_token_cache_->GetAuthToken();
  EXPECT_FALSE(got_token);

  // Fast-forward to run the measurement timer.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // 1024 tokens in five minutes is a rate of 12288 tokens per hour.
  histogram_tester_.ExpectUniqueSample(kTokenExpirationRateHistogram, 12288, 1);

  // Fast-forward to run the measurement timer again.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // Zero tokens expired in this interval.
  histogram_tester_.ExpectBucketCount(kTokenExpirationRateHistogram, 0, 1);
  histogram_tester_.ExpectTotalCount(kTokenExpirationRateHistogram, 2);
}

}  // namespace network
