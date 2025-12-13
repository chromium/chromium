// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_pool_additional_capacity.h"

#include "base/notimplemented.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job_factory.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace net {

namespace {

TEST(SocketPoolAdditionalCapacityTest, CreateWithDisabledFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kTcpSocketPoolLimitRandomization);
  EXPECT_EQ(SocketPoolAdditionalCapacity::Create(),
            SocketPoolAdditionalCapacity::CreateEmpty());
}

TEST(SocketPoolAdditionalCapacityTest, CreateWithEnabledFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kTcpSocketPoolLimitRandomization,
      {
          {
              "TcpSocketPoolLimitRandomizationBase",
              "0.1",
          },
          {
              "TcpSocketPoolLimitRandomizationCapacity",
              "2",
          },
          {
              "TcpSocketPoolLimitRandomizationMinimum",
              "0.3",
          },
          {
              "TcpSocketPoolLimitRandomizationNoise",
              "0.4",
          },
      });
  EXPECT_EQ(SocketPoolAdditionalCapacity::Create(),
            SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, 0.4));
}

TEST(SocketPoolAdditionalCapacityTest, CreateForTest) {
  EXPECT_EQ(std::string(
                SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, 0.4)),
            "SocketPoolAdditionalCapacity(base:1.000000e-01,capacity:2,minimum:"
            "3.000000e-01,noise:4.000000e-01)");
}

TEST(SocketPoolAdditionalCapacityTest, InvalidCreation) {
  const SocketPoolAdditionalCapacity empty_pool =
      SocketPoolAdditionalCapacity::CreateEmpty();

  // base range
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(-0.1, 2, 0.3, 0.4),
            empty_pool);
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(1.1, 2, 0.3, 0.4),
            empty_pool);
  EXPECT_EQ(
      SocketPoolAdditionalCapacity::CreateForTest(std::nan(""), 2, 0.3, 0.4),
      empty_pool);

  // capacity range
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2000, 0.3, 0.4),
            empty_pool);

  // minimum range
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, -0.3, 0.4),
            empty_pool);
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 1.3, 0.4),
            empty_pool);
  EXPECT_EQ(
      SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, std::nan(""), 0.4),
      empty_pool);

  // noise range
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, -0.4),
            empty_pool);
  EXPECT_EQ(SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, 1.4),
            empty_pool);
  EXPECT_EQ(
      SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, std::nan("")),
      empty_pool);
}

TEST(SocketPoolAdditionalCapacityTest, NextStateBeforeAllocation) {
  // We use a base and noise of 0.0 with a minimum of 0.5 to ensure every roll
  // is a 50/50 shot so that we don't need to run the test millions of times
  // for flakes to be noticeable. The capacity of 2 is needed to test the logic.
  SocketPoolAdditionalCapacity pool_capacity =
      SocketPoolAdditionalCapacity::CreateForTest(0.0, 2, 0.5, 0.0);

  // Test out of bounds cases
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kUncapped, 5, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 5, 2));

  // Below soft cap we are always uncapped
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateBeforeAllocation(
                                            SocketPoolState::kUncapped, 0, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateBeforeAllocation(
                                            SocketPoolState::kCapped, 0, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateBeforeAllocation(
                                            SocketPoolState::kUncapped, 1, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateBeforeAllocation(
                                            SocketPoolState::kCapped, 1, 2));

  // At hard cap we are always capped
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 4, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kUncapped, 4, 2));

  // If capped at or above soft cap we always stay that way
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 2, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 3, 2));

  // When uncapped between soft and hard cap, we should be able to see some
  // distribution of each option. The probability inputs here should make it
  // a coin toss, but to prevent this from being flakey we run it 1000 times.
  bool did_see_uncapped = false;
  bool did_see_capped = false;
  for (size_t i = 0; i < 1000; ++i) {
    if (SocketPoolState::kUncapped == pool_capacity.NextStateBeforeAllocation(
                                          SocketPoolState::kUncapped, 3, 2)) {
      did_see_uncapped = true;
    } else {
      did_see_capped = true;
    }
  }
  EXPECT_TRUE(did_see_uncapped);
  EXPECT_TRUE(did_see_capped);
}

TEST(SocketPoolAdditionalCapacityTest, NextStateAfterRelease) {
  // We use a base and noise of 0.0 with a minimum of 0.5 to ensure every roll
  // is a 50/50 shot so that we don't need to run the test millions of times
  // for flakes to be noticeable. The capacity of 2 is needed to test the logic.
  SocketPoolAdditionalCapacity pool_capacity =
      SocketPoolAdditionalCapacity::CreateForTest(0.0, 2, 0.5, 0.0);

  // Test out of bounds cases
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kUncapped, 5, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kCapped, 5, 2));

  // Below soft cap we are always uncapped
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kUncapped, 0, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kCapped, 0, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kUncapped, 1, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kCapped, 1, 2));

  // At hard cap we are always capped
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kCapped, 4, 2));
  EXPECT_EQ(SocketPoolState::kCapped, pool_capacity.NextStateAfterRelease(
                                          SocketPoolState::kUncapped, 4, 2));

  // If uncapped at or above soft cap we always stay that way
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kUncapped, 2, 2));
  EXPECT_EQ(SocketPoolState::kUncapped, pool_capacity.NextStateAfterRelease(
                                            SocketPoolState::kUncapped, 3, 2));

  // When capped between soft and hard cap, we should be able to see some
  // distribution of each option. The probability inputs here should make it
  // a coin toss, but to prevent this from being flakey we run it 1000 times.
  bool did_see_uncapped = false;
  bool did_see_capped = false;
  for (size_t i = 0; i < 1000; ++i) {
    if (SocketPoolState::kUncapped ==
        pool_capacity.NextStateAfterRelease(SocketPoolState::kCapped, 3, 2)) {
      did_see_uncapped = true;
    } else {
      did_see_capped = true;
    }
  }
  EXPECT_TRUE(did_see_uncapped);
  EXPECT_TRUE(did_see_capped);
}

TEST(SocketPoolAdditionalCapacityTest, EmptyPool) {
  const SocketPoolAdditionalCapacity empty_pool =
      SocketPoolAdditionalCapacity::CreateEmpty();

  // No sockets in use
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      empty_pool.NextStateBeforeAllocation(SocketPoolState::kUncapped, 0, 256));
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      empty_pool.NextStateAfterRelease(SocketPoolState::kUncapped, 0, 256));
  EXPECT_EQ(SocketPoolState::kUncapped, empty_pool.NextStateBeforeAllocation(
                                            SocketPoolState::kCapped, 0, 256));
  EXPECT_EQ(SocketPoolState::kUncapped,
            empty_pool.NextStateAfterRelease(SocketPoolState::kCapped, 0, 256));

  // 50% of soft cap in use
  EXPECT_EQ(SocketPoolState::kUncapped,
            empty_pool.NextStateBeforeAllocation(SocketPoolState::kUncapped,
                                                 128, 256));
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      empty_pool.NextStateAfterRelease(SocketPoolState::kUncapped, 128, 256));
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      empty_pool.NextStateBeforeAllocation(SocketPoolState::kCapped, 128, 256));
  EXPECT_EQ(
      SocketPoolState::kUncapped,
      empty_pool.NextStateAfterRelease(SocketPoolState::kCapped, 128, 256));

  // 100% of soft cap in use
  EXPECT_EQ(SocketPoolState::kCapped,
            empty_pool.NextStateBeforeAllocation(SocketPoolState::kUncapped,
                                                 256, 256));
  EXPECT_EQ(
      SocketPoolState::kCapped,
      empty_pool.NextStateAfterRelease(SocketPoolState::kUncapped, 256, 256));
  EXPECT_EQ(SocketPoolState::kCapped, empty_pool.NextStateBeforeAllocation(
                                          SocketPoolState::kCapped, 256, 256));
  EXPECT_EQ(SocketPoolState::kCapped, empty_pool.NextStateAfterRelease(
                                          SocketPoolState::kCapped, 256, 256));
}

TEST(SocketPoolAdditionalCapacityTest,
     TestDefaultDistributionForFieldTrialConfig) {

  // In order to do that we need an easy way to measure distributions.
  // Since we are applying noise, we run a ten thousand variants.
  auto percentage_transition_for_allocation_and_release =
      [&](size_t sockets_in_use) -> std::tuple<double, double> {
    size_t transition_allocation_count = 0;
    size_t transition_release_count = 0;
    for (size_t i = 0; i < 10000; ++i) {
      if (SocketPoolState::kCapped ==
          kFieldTrialPool.NextStateBeforeAllocation(SocketPoolState::kUncapped,
                                                    sockets_in_use, 256)) {
        ++transition_allocation_count;
      }
      if (SocketPoolState::kUncapped ==
          kFieldTrialPool.NextStateAfterRelease(SocketPoolState::kCapped,
                                                sockets_in_use, 256)) {
        ++transition_release_count;
      }
    }
    return {transition_allocation_count / 10000.0,
            transition_release_count / 10000.0};
  };

  // We want to validate the distribution at three points: 5%, 50%, and 95%.
  auto fifth_percentile = percentage_transition_for_allocation_and_release(268);
  auto fiftieth_percentile =
      percentage_transition_for_allocation_and_release(384);
  auto ninetyfifth_percentile =
      percentage_transition_for_allocation_and_release(500);

  // When allocating sockets and uncapped:
  // We expect a ~1% transition rate if 5% of additional sockets are in use.
  EXPECT_GT(std::get<0>(fifth_percentile), 0.00);
  EXPECT_LT(std::get<0>(fifth_percentile), 0.025);
  // We expect a ~1% transition rate if 50% of additional sockets are in use.
  EXPECT_GT(std::get<0>(fiftieth_percentile), 0.00);
  EXPECT_LT(std::get<0>(fiftieth_percentile), 0.025);
  // We expect a ~50% transition rate if 95% of additional sockets are in use.
  EXPECT_GT(std::get<0>(ninetyfifth_percentile), 0.35);
  EXPECT_LT(std::get<0>(ninetyfifth_percentile), 0.65);

  // When releasing sockets and capped:
  // We expect a ~50% transition rate if 5% of additional sockets are in use.
  EXPECT_GT(std::get<1>(fifth_percentile), 0.35);
  EXPECT_LT(std::get<1>(fifth_percentile), 0.65);
  // We expect a ~1% transition rate if 50% of additional sockets are in use.
  EXPECT_GT(std::get<1>(fiftieth_percentile), 0.00);
  EXPECT_LT(std::get<1>(fiftieth_percentile), 0.025);
  // We expect a ~1% transition rate if 95% of additional sockets are in use.
  EXPECT_GT(std::get<1>(ninetyfifth_percentile), 0.00);
  EXPECT_LT(std::get<1>(ninetyfifth_percentile), 0.025);
}

void ValidateRandomizedInputs(double base,
                              size_t capacity,
                              double minimum,
                              double noise,
                              bool capped,
                              size_t sockets_in_use,
                              size_t socket_soft_cap) {
  SocketPoolAdditionalCapacity pool =
      SocketPoolAdditionalCapacity::CreateForTest(base, capacity, minimum,
                                                  noise);
  // Because there's some randomization here, we want to run these a few times.
  for (size_t i = 0; i < 1000; ++i) {
    pool.NextStateBeforeAllocation(
        capped ? SocketPoolState::kCapped : SocketPoolState::kUncapped,
        sockets_in_use, socket_soft_cap);
    pool.NextStateAfterRelease(
        capped ? SocketPoolState::kCapped : SocketPoolState::kUncapped,
        sockets_in_use, socket_soft_cap);
  }
}
FUZZ_TEST(SocketPoolAdditionalCapacityTest, ValidateRandomizedInputs)
    .WithDomains(fuzztest::Arbitrary<double>(),
                 fuzztest::Arbitrary<size_t>(),
                 fuzztest::Arbitrary<double>(),
                 fuzztest::Arbitrary<double>(),
                 fuzztest::Arbitrary<bool>(),
                 fuzztest::Arbitrary<size_t>(),
                 fuzztest::Arbitrary<size_t>())
    .WithSeeds({
        {std::nan(""), 0, std::nan(""), std::nan(""), false, 0, 0},
        {0.0, 0, 0.0, 0.0, false, 0, 0},
        {0.3, 64, 0.1, 0.1, false, 96, 64},
        {0.6, 128, 0.2, 0.2, true, 192, 128},
        {1.0, 256, 1.0, 1.0, true, 320, 256},
        {1.0, 256, 1.0, 1.0, true, std::numeric_limits<uint32_t>::max(),
         std::numeric_limits<uint32_t>::max()},
    });

// This is mocked up so that we can model the sort of function usage we expect
// in the additions to ClientSocketPool. We won't actually be implementing or
// using the normal public interface functions of a ClientSocketPool.
class MockClientSocketPool : public ClientSocketPool {
 public:
  MockClientSocketPool()
      : ClientSocketPool(/*socket_soft_cap=*/256,
                         kFieldTrialPool,
                         ProxyChain::Direct(),
                         /*is_for_websockets=*/false,
                         /*common_connect_job_params*/ nullptr,
                         /*connect_job_factory*/ nullptr) {}

  SocketPoolState RequestSocket() {
    UpdateStateBeforeAllocation();
    if (State() == SocketPoolState::kUncapped) {
      ++sockets_in_use_;
    }
    CHECK_LE(sockets_in_use_, 512);
    return State();
  }

  SocketPoolState ReleaseSocket() {
    --sockets_in_use_;
    UpdateStateAfterRelease();
    CHECK_GE(sockets_in_use_, 0);
    return State();
  }

  size_t SocketsInUse() const override { return sockets_in_use_; }

 private:
  int RequestSocket(
      const GroupId& group_id,
      scoped_refptr<SocketParams> params,
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      RequestPriority priority,
      const SocketTag& socket_tag,
      RespectLimits respect_limits,
      ClientSocketHandle* handle,
      CompletionOnceCallback callback,
      const ProxyAuthCallback& proxy_auth_callback,
      bool fail_if_alias_requires_proxy_override,
      const NetLogWithSource& net_log) override {
    NOTIMPLEMENTED();
    return ERR_IO_PENDING;
  }
  int RequestSockets(
      const GroupId& group_id,
      scoped_refptr<SocketParams> params,
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      size_t num_sockets,
      bool fail_if_alias_requires_proxy_override,
      CompletionOnceCallback callback,
      const NetLogWithSource& net_log) override {
    NOTIMPLEMENTED();
    return ERR_IO_PENDING;
  }
  void SetPriority(const GroupId& group_id,
                   ClientSocketHandle* handle,
                   RequestPriority priority) override {
    NOTIMPLEMENTED();
  }
  void CancelRequest(const GroupId& group_id,
                     ClientSocketHandle* handle,
                     bool cancel_connect_job) override {
    NOTIMPLEMENTED();
  }
  void ReleaseSocket(const GroupId& group_id,
                     std::unique_ptr<StreamSocket> socket,
                     int64_t generation) override {
    NOTIMPLEMENTED();
  }
  void FlushWithError(int error, const char* net_log_reason_utf8) override {
    NOTIMPLEMENTED();
  }
  void CloseIdleSockets(const char* net_log_reason_utf8) override {
    NOTIMPLEMENTED();
  }
  void CloseIdleSocketsInGroup(const GroupId& group_id,
                               const char* net_log_reason_utf8) override {
    NOTIMPLEMENTED();
  }
  size_t IdleSocketCount() const override {
    NOTIMPLEMENTED();
    return 0;
  }
  size_t IdleSocketCountInGroup(const GroupId& group_id) const override {
    NOTIMPLEMENTED();
    return 0;
  }
  LoadState GetLoadState(const GroupId& group_id,
                         const ClientSocketHandle* handle) const override {
    NOTIMPLEMENTED();
    return LOAD_STATE_IDLE;
  }
  base::Value GetInfoAsValue(const std::string& name,
                             const std::string& type) const override {
    NOTIMPLEMENTED();
    return base::Value();
  }
  bool HasActiveSocket(const GroupId& group_id) const override {
    NOTIMPLEMENTED();
    return false;
  }
  bool IsStalled() const override {
    NOTIMPLEMENTED();
    return false;
  }
  void AddHigherLayeredPool(HigherLayeredPool* higher_pool) override {
    NOTIMPLEMENTED();
  }
  void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) override {
    NOTIMPLEMENTED();
  }

 private:
  size_t sockets_in_use_{0};
};

TEST(SocketPoolAdditionalCapacityTest,
     ValidateAdditionalCapacityForMockClientSocketPool) {
  MockClientSocketPool pool;
  ValidateAdditionalCapacityForSocketPool(
      base::BindLambdaForTesting([&]() { return pool.RequestSocket(); }),
      base::DoNothing(),
      base::BindLambdaForTesting([&]() { return pool.ReleaseSocket(); }),
      base::BindLambdaForTesting([&]() { return pool.SocketsInUse(); }));
}

}  // namespace

}  // namespace net
