// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/ip_protection_auth_token_cache_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

const int kExpectedBatchSize = 64;
const int kCacheLowWaterMark = 16;
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
            receiver_.BindNewPipeAndPassRemote(),
            /* disable_cache_management_for_testing=*/true)) {}

  void ExpectHistogramState(HistogramState state) {
    histogram_tester_.ExpectBucketCount(kGetAuthTokenResultHistogram, true,
                                        state.success);
    histogram_tester_.ExpectBucketCount(kGetAuthTokenResultHistogram, false,
                                        state.failure);
  }

  // Create a batch of tokens.
  std::vector<network::mojom::BlindSignedAuthTokenPtr> TokenBatch(
      int count,
      base::Time expiration) {
    std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
    for (int i = 0; i < count; i++) {
      tokens.emplace_back(network::mojom::BlindSignedAuthToken::New(
          base::StringPrintf("token-%d", i), expiration));
    }
    return tokens;
  }

  // Call `FillCacheForTesting()` and wait until it completes.
  void FillCacheAndWait() {
    auth_token_cache_->FillCacheForTesting(task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  // Wait until the cache fills itself.
  void WaitForCacheFill() {
    auth_token_cache_->SetOnCacheRefilledForTesting(
        task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Expiration times with respect to the TaskEnvironment's mock time.
  const base::Time kFutureExpiration = base::Time::Now() + base::Hours(1);
  const base::Time kPastExpiration = base::Time::Now() - base::Hours(1);

  MockIpProtectionAuthTokenGetter mock_;
  mojo::Receiver<network::mojom::IpProtectionAuthTokenGetter> receiver_;

  // The IpProtectionAuthTokenCache being tested.
  std::unique_ptr<IpProtectionAuthTokenCacheImpl> auth_token_cache_;

  base::HistogramTester histogram_tester_;
};

// `IsAuthTokenAvailable()` returns false on an empty cache.
TEST_F(IpProtectionAuthTokenCacheImplTest, IsAuthTokenAvailable_False_Empty) {
  EXPECT_FALSE(auth_token_cache_->IsAuthTokenAvailable());
}

// `IsAuthTokenAvailable()` returns true on a cache containing unexpired tokens.
TEST_F(IpProtectionAuthTokenCacheImplTest, IsAuthTokenAvailable_True) {
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize,
                                   TokenBatch(1, kFutureExpiration));
  FillCacheAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
  EXPECT_TRUE(auth_token_cache_->IsAuthTokenAvailable());
}

// `IsAuthTokenAvailable()` returns false on a cache containing expired tokens.
TEST_F(IpProtectionAuthTokenCacheImplTest, IsAuthTokenAvailable_False_Expired) {
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize,
                                   TokenBatch(1, kPastExpiration));
  FillCacheAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
  EXPECT_FALSE(auth_token_cache_->IsAuthTokenAvailable());
}

// `GetAuthToken()` returns nullopt on an empty cache.
TEST_F(IpProtectionAuthTokenCacheImplTest, GetAuthToken_Empty) {
  EXPECT_FALSE(auth_token_cache_->GetAuthToken());
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// `GetAuthToken()` returns a token on a cache containing unexpired tokens.
TEST_F(IpProtectionAuthTokenCacheImplTest, GetAuthToken_True) {
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize,
                                   TokenBatch(1, kFutureExpiration));
  FillCacheAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> token =
      auth_token_cache_->GetAuthToken();
  ASSERT_TRUE(token);
  EXPECT_EQ((*token)->token, "token-0");
  EXPECT_EQ((*token)->expiration, kFutureExpiration);
  ExpectHistogramState(HistogramState{.success = 1, .failure = 0});
}

// `GetAuthToken()` returns nullopt on a cache containing expired tokens.
TEST_F(IpProtectionAuthTokenCacheImplTest, GetAuthToken_False_Expired) {
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize,
                                   TokenBatch(1, kPastExpiration));
  FillCacheAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
  EXPECT_FALSE(auth_token_cache_->GetAuthToken());
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// If `TryGetAuthTokens()` returns an empty batch, the cache remains empty.
TEST_F(IpProtectionAuthTokenCacheImplTest, EmptyBatch) {
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize,
                                   TokenBatch(0, kFutureExpiration));
  FillCacheAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  ASSERT_FALSE(auth_token_cache_->IsAuthTokenAvailable());
  ASSERT_FALSE(auth_token_cache_->GetAuthToken());
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// If `TryGetAuthTokens()` returns an backoff due to an error, the cache remains
// empty.
TEST_F(IpProtectionAuthTokenCacheImplTest, ErrorBatch) {
  const base::TimeDelta kBackoff = base::Seconds(10);
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize,
                                   base::Time::Now() + kBackoff);
  FillCacheAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  ASSERT_FALSE(auth_token_cache_->IsAuthTokenAvailable());
  ASSERT_FALSE(auth_token_cache_->GetAuthToken());
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// `GetAuthToken()` skips expired tokens and returns a non-expired token,
// if one is found in the cache.
TEST_F(IpProtectionAuthTokenCacheImplTest, SkipExpiredTokens) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens =
      TokenBatch(10, kPastExpiration);
  tokens.emplace_back(network::mojom::BlindSignedAuthToken::New(
      "good-token", kFutureExpiration));
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));
  FillCacheAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  auto got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "good-token");
  EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
  ExpectHistogramState(HistogramState{.success = 1, .failure = 0});
}

// If the `IpProtectionAuthTokenGetter` is nullptr, no tokens are gotten,
// but things don't crash.
TEST_F(IpProtectionAuthTokenCacheImplTest, Null_Getter) {
  auto auth_token_cache = IpProtectionAuthTokenCacheImpl(
      mojo::PendingRemote<network::mojom::IpProtectionAuthTokenGetter>(),
      /* disable_cache_management_for_testing=*/true);
  EXPECT_FALSE(auth_token_cache_->IsAuthTokenAvailable());
  auto token = auth_token_cache.GetAuthToken();
  ASSERT_FALSE(token);
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
}

// Verify that the token spend rate is measured correctly.
TEST_F(IpProtectionAuthTokenCacheImplTest, TokenSpendRate) {
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;

  // Fill the cache with 5 tokens.
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize,
                                   TokenBatch(5, kFutureExpiration));
  FillCacheAndWait();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  // Get four tokens from the batch.
  for (int i = 0; i < 4; i++) {
    auto got_token = auth_token_cache_->GetAuthToken();
    EXPECT_EQ(got_token.value()->token, base::StringPrintf("token-%d", i));
    EXPECT_EQ(got_token.value()->expiration, kFutureExpiration);
  }

  // Fast-forward to run the measurement timer.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // Four tokens in five minutes is a rate of 36 tokens per hour.
  histogram_tester_.ExpectUniqueSample(kTokenSpendRateHistogram, 48, 1);

  // Get the remaining token in the batch.
  auto got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "token-4");
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
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize,
                                   TokenBatch(1024, kPastExpiration));
  FillCacheAndWait();
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

// The cache will pre-fill itself with a batch of tokens after a startup delay.
TEST_F(IpProtectionAuthTokenCacheImplTest, Prefill) {
  mock_.ExpectTryGetAuthTokensCall(
      kExpectedBatchSize, TokenBatch(kExpectedBatchSize, kFutureExpiration));
  auth_token_cache_->EnableCacheManagementForTesting();
  WaitForCacheFill();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
  EXPECT_TRUE(auth_token_cache_->IsAuthTokenAvailable());
}

// The cache will initiate a refill when it reaches the low-water mark.
TEST_F(IpProtectionAuthTokenCacheImplTest, Refill_LowWaterMark) {
  mock_.ExpectTryGetAuthTokensCall(
      kExpectedBatchSize, TokenBatch(kExpectedBatchSize, kFutureExpiration));
  auth_token_cache_->EnableCacheManagementForTesting();
  WaitForCacheFill();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  // Spend tokens down to (but not below) the low-water mark.
  for (int i = kExpectedBatchSize - 1; i > kCacheLowWaterMark; i--) {
    ASSERT_TRUE(auth_token_cache_->IsAuthTokenAvailable());
    ASSERT_TRUE(auth_token_cache_->GetAuthToken());
    ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
  }

  mock_.ExpectTryGetAuthTokensCall(
      kExpectedBatchSize, TokenBatch(kExpectedBatchSize, kFutureExpiration));

  // Next call to `GetAuthToken()` should call `MaybeRefillCache()`.
  auth_token_cache_->SetOnCacheRefilledForTesting(
      task_environment_.QuitClosure());
  ASSERT_TRUE(auth_token_cache_->GetAuthToken());
  task_environment_.RunUntilQuit();

  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());
}

// If a fill results in a backoff request, the cache will try again after that
// time.
TEST_F(IpProtectionAuthTokenCacheImplTest, Refill_AfterBackoff) {
  base::Time try_again_at = base::Time::Now() + base::Seconds(20);
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, try_again_at);
  auth_token_cache_->EnableCacheManagementForTesting();
  WaitForCacheFill();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  base::Time try_again_at_2 = base::Time::Now() + base::Seconds(20);
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, try_again_at_2);
  WaitForCacheFill();
  EXPECT_EQ(base::Time::Now(), try_again_at);
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  base::Time try_again_at_3 = base::Time::Now() + base::Seconds(20);
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, try_again_at_3);
  WaitForCacheFill();
  EXPECT_EQ(base::Time::Now(), try_again_at_2);
}

// When enough tokens expire to bring the cache size below the low water mark,
// it will automatically refill.
TEST_F(IpProtectionAuthTokenCacheImplTest, Refill_AfterExpiration) {
  // Make a batch of tokens almost all with `expiration2`, except one expiring
  // sooner and the one expiring later. These are returned in incorrect order to
  // verify that the cache sorts by expiration time.
  std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
  base::Time expiration1 = base::Time::Now() + base::Minutes(10);
  base::Time expiration2 = base::Time::Now() + base::Minutes(15);
  base::Time expiration3 = base::Time::Now() + base::Minutes(20);
  for (int i = 0; i < kExpectedBatchSize - 2; i++) {
    tokens.emplace_back(
        network::mojom::BlindSignedAuthToken::New("exp2", expiration2));
  }
  tokens.emplace_back(
      network::mojom::BlindSignedAuthToken::New("exp3", expiration3));
  tokens.emplace_back(
      network::mojom::BlindSignedAuthToken::New("exp1", expiration1));
  mock_.ExpectTryGetAuthTokensCall(kExpectedBatchSize, std::move(tokens));
  auth_token_cache_->EnableCacheManagementForTesting();
  WaitForCacheFill();
  ASSERT_TRUE(mock_.GotAllExpectedTryGetAuthTokensCalls());

  // After the first expiration, tokens should still be available and no
  // refill should have begun (which would have caused an error).
  task_environment_.FastForwardBy(expiration1 - base::Time::Now());
  ASSERT_TRUE(auth_token_cache_->IsAuthTokenAvailable());

  // After the second expiration, tokens should still be available, and
  // a second batch should have been requested.
  mock_.ExpectTryGetAuthTokensCall(
      kExpectedBatchSize, TokenBatch(kExpectedBatchSize, kFutureExpiration));
  task_environment_.FastForwardBy(expiration2 - base::Time::Now());
  ASSERT_TRUE(auth_token_cache_->IsAuthTokenAvailable());

  // The un-expired token should be returned.
  auto got_token = auth_token_cache_->GetAuthToken();
  EXPECT_EQ(got_token.value()->token, "exp3");
}

}  // namespace network
