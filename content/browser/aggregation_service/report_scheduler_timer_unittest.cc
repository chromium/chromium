// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/report_scheduler_timer.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using testing::_;
using testing::InSequence;
using testing::Invoke;

using Checkpoint = testing::MockFunction<void(int step)>;

constexpr auto kExampleTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1652984901234);

class MockReportSchedulerTimerDelegate : public ReportSchedulerTimer::Delegate {
 public:
  MOCK_METHOD(void,
              GetNextReportTime,
              (base::OnceCallback<void(std::optional<base::Time>)>, base::Time),
              (override));
  MOCK_METHOD(void,
              OnReportingTimeReached,
              (base::Time, base::Time),
              (override));

  MOCK_METHOD(void,
              AdjustOfflineReportTimes,
              (base::OnceCallback<void(std::optional<base::Time>)>),
              (override));

  MOCK_METHOD(void, OnReportingPaused, (), (override));
};

class ReportSchedulerTimerTest : public testing::Test {
 public:
  void SetUp() override {
    auto timer_delegate = std::make_unique<MockReportSchedulerTimerDelegate>();
    timer_delegate_ = timer_delegate.get();
    timer_ = std::make_unique<ReportSchedulerTimer>(std::move(timer_delegate));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Must outlive `timer_delegate_`.
  std::unique_ptr<ReportSchedulerTimer> timer_;

  raw_ptr<MockReportSchedulerTimerDelegate> timer_delegate_;
};

TEST_F(ReportSchedulerTimerTest, SetTimer_FiredAtAppropriateTime) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*timer_delegate_, OnReportingTimeReached).Times(0);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(*timer_delegate_, OnReportingTimeReached);
    EXPECT_CALL(*timer_delegate_, GetNextReportTime);  // Called via Refresh().
  }

  timer_->MaybeSet(kExampleTime);

  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();
  task_environment_.FastForwardBy(fast_forward_required - base::Seconds(1));

  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_F(ReportSchedulerTimerTest, MultipleSetTimers_FiredAtAppropriateTime) {
  Checkpoint checkpoint;
  base::OnceCallback<void(std::optional<base::Time>)> saved_cb_1;
  base::OnceCallback<void(std::optional<base::Time>)> saved_cb_2;

  {
    InSequence seq;

    EXPECT_CALL(*timer_delegate_, OnReportingTimeReached).Times(0);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(*timer_delegate_, OnReportingTimeReached);
    EXPECT_CALL(*timer_delegate_, GetNextReportTime)
        .WillOnce(MoveArg<0>(&saved_cb_1));  // Called via Refresh().

    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(*timer_delegate_, OnReportingTimeReached).Times(0);

    EXPECT_CALL(checkpoint, Call(3));

    EXPECT_CALL(*timer_delegate_, OnReportingTimeReached);
    EXPECT_CALL(*timer_delegate_, GetNextReportTime)
        .WillOnce(MoveArg<0>(&saved_cb_2));  // Called via Refresh().

    EXPECT_CALL(checkpoint, Call(4));
  }
  // Test both simultaneous and non-simultaneous times.
  const base::Time kReportTimes[] = {kExampleTime, kExampleTime,
                                     kExampleTime + base::Hours(1)};

  for (base::Time report_time : kReportTimes) {
    timer_->MaybeSet(report_time);
  }

  checkpoint.Call(1);

  task_environment_.FastForwardBy(kExampleTime - base::Time::Now());

  checkpoint.Call(2);

  std::move(saved_cb_1).Run(kExampleTime + base::Hours(1));

  checkpoint.Call(3);

  task_environment_.FastForwardBy(base::Hours(1));

  checkpoint.Call(4);

  // Nothing should happen if no reports are left.
  std::move(saved_cb_2).Run(std::nullopt);
}

TEST_F(ReportSchedulerTimerTest, NetworkChange) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(*timer_delegate_, OnReportingTimeReached).Times(0);
    EXPECT_CALL(*timer_delegate_, OnReportingPaused).Times(1);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*timer_delegate_, AdjustOfflineReportTimes);
  }

  timer_->MaybeSet(kExampleTime);

  // Go offline
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  task_environment_.FastForwardBy(kExampleTime - base::Time::Now());

  checkpoint.Call(1);

  // Go back online.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  // Ensure that the network connection observers have been notified before
  // this call returns.
  task_environment_.RunUntilIdle();
}

// TODO(apaseltiner): Figure out how to test the case in which the network
// connection tracker is uninitialized.
TEST(ReportSchedulerTimer, Constructor_AdjustsOfflineReportTimes) {
  constexpr struct {
    network::mojom::ConnectionType connection_type;
    bool call_expected;
  } kTestCases[] = {
      // Call is skipped because the browser is offline.
      {
          network::mojom::ConnectionType::CONNECTION_NONE,
          false,
      },
      // Call is made because the browser is online.
      {
          network::mojom::ConnectionType::CONNECTION_ETHERNET,
          true,
      },
  };

  for (const auto& test_case : kTestCases) {
    for (bool synchronous : {true, false}) {
      base::test::TaskEnvironment task_environment{
          base::test::TaskEnvironment::TimeSource::MOCK_TIME};

      auto* tracker = network::TestNetworkConnectionTracker::GetInstance();
      tracker->SetRespondSynchronously(synchronous);
      tracker->SetConnectionType(test_case.connection_type);

      auto timer_delegate =
          std::make_unique<MockReportSchedulerTimerDelegate>();

      EXPECT_CALL(*timer_delegate, AdjustOfflineReportTimes)
          .Times(test_case.call_expected);

      ReportSchedulerTimer timer(std::move(timer_delegate));

      // Flush the async call.
      if (!synchronous) {
        task_environment.RunUntilIdle();
      }
    }
  }
}

}  // namespace

}  // namespace content
