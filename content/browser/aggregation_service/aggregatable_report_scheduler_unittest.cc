// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using testing::_;
using testing::Field;
using testing::Invoke;
using testing::Property;

// Will be used to verify the sequence of expected function calls.
using Checkpoint = ::testing::MockFunction<void(int)>;

// TODO(alexmt): Consider changing tests to avoid the assumption that this time
// is after `base::Time::Now()`.
constexpr auto kExampleTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1652984901234);

}  // namespace

class AggregatableReportSchedulerTest : public testing::Test {
 public:
  AggregatableReportSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        storage_context_(base::DefaultClock::GetInstance()) {}

  void SetUp() override {
    scheduler_ = std::make_unique<AggregatableReportScheduler>(
        &storage_context_,
        /*on_scheduled_report_time_reached=*/mock_callback_.Get());
  }

 protected:
  void VerifyHistograms(base::HistogramBase::Count timer_fired_count) {
    histograms_.ExpectTotalCount(
        "PrivacySandbox.AggregationService.Scheduler.TimerFireDelay",
        timer_fired_count);
    histograms_.ExpectTotalCount(
        "PrivacySandbox.AggregationService.Storage.RequestsRetrievalTime",
        timer_fired_count);
  }

  base::test::TaskEnvironment task_environment_;
  TestAggregationServiceStorageContext storage_context_;
  base::MockRepeatingCallback<void(
      std::vector<AggregationServiceStorage::RequestAndId>)>
      mock_callback_;
  std::unique_ptr<AggregatableReportScheduler> scheduler_;
  base::HistogramTester histograms_;
};

TEST_F(AggregatableReportSchedulerTest,
       ScheduleRequest_RetrievedAtAppropriateTime) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(expected_shared_info),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
          .value();

  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max(), /*limit=*/std::nullopt)
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 0u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(
            Invoke([&expected_request](
                       std::vector<AggregationServiceStorage::RequestAndId>
                           requests_and_ids) {
              ASSERT_EQ(requests_and_ids.size(), 1u);
              EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
                  requests_and_ids[0].request, expected_request));
            }));
  }

  scheduler_->ScheduleRequest(
      aggregation_service::CloneReportRequest(expected_request));

  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max(), /*limit=*/std::nullopt)
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 1u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();
  task_environment_.FastForwardBy(fast_forward_required -
                                  base::Microseconds(1));

  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max(), /*limit=*/std::nullopt)
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 1u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Microseconds(1));

  VerifyHistograms(/*timer_fired_count=*/1);
}

TEST_F(AggregatableReportSchedulerTest,
       InProgressRequestCompleted_DeletedFromStorage) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  scheduler_->ScheduleRequest(
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(expected_shared_info),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
          .value());

  EXPECT_CALL(mock_callback_, Run);
  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();

  task_environment_.FastForwardBy(fast_forward_required);

  // The request is still stored even while it's in-progress.
  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max(), /*limit=*/std::nullopt)
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 1u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  // Request IDs are incremented from 1.
  scheduler_->NotifyInProgressRequestSucceeded(
      AggregationServiceStorage::RequestId(1));
  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max(), /*limit=*/std::nullopt)
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 0u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  VerifyHistograms(/*timer_fired_count=*/1);
}

TEST_F(AggregatableReportSchedulerTest,
       FinalSendAttemptFailed_DeletedFromStorage) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  scheduler_->ScheduleRequest(
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(expected_shared_info),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
          .value());

  EXPECT_CALL(mock_callback_, Run);
  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();

  task_environment_.FastForwardBy(fast_forward_required);

  // The request is still stored even while it's in-progress.
  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max(), /*limit=*/std::nullopt)
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 1u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  // Request IDs are incremented from 1.
  bool will_retry = scheduler_->NotifyInProgressRequestFailed(
      AggregationServiceStorage::RequestId(1),
      /*previous_failed_attempts=*/AggregatableReportScheduler::kMaxRetries);
  EXPECT_FALSE(will_retry);
  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max(), /*limit=*/std::nullopt)
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 0u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  VerifyHistograms(/*timer_fired_count=*/1);
}

TEST_F(AggregatableReportSchedulerTest,
       InProgressRequestFailed_UpdateStorageAndReschedule) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  auto request = AggregatableReportRequest::Create(
      example_request.payload_contents(), std::move(expected_shared_info),
      AggregatableReportRequest::DelayType::ScheduledWithReducedDelay);
  scheduler_->ScheduleRequest(std::move(request.value()));

  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();

  int before_first_notification = 0;
  int before_first_retry = 1;
  int after_first_retry = 2;
  int before_second_retry = 3;
  int after_second_retry = 4;

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(mock_callback_, Run)
        .Times(1);  // Called once for the initial request
    EXPECT_CALL(checkpoint, Call(before_first_notification));

    // First delay not expired yet
    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(before_first_retry));

    // First retry done
    EXPECT_CALL(mock_callback_, Run).Times(1);
    EXPECT_CALL(checkpoint, Call(after_first_retry));

    // Second delay not expired yet
    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(before_second_retry));

    // Second retry done
    EXPECT_CALL(mock_callback_, Run).Times(1);
    EXPECT_CALL(checkpoint, Call(after_second_retry));
  }

  task_environment_.FastForwardBy(fast_forward_required);

  checkpoint.Call(before_first_notification);

  // Request is still in storage and its number of failed attempts has been
  // incremented.
  EXPECT_TRUE(scheduler_->NotifyInProgressRequestFailed(
      AggregationServiceStorage::RequestId(1), /*previous_failed_attempts=*/0));
  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max(), /*limit=*/std::nullopt)
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_THAT(
                  requests_and_ids,
                  testing::ElementsAre(Field(
                      "request",
                      &AggregationServiceStorage::RequestAndId::request,
                      Property("failed_send_attempts()",
                               &AggregatableReportRequest::failed_send_attempts,
                               1))));
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  task_environment_.FastForwardBy(base::Minutes(5) - base::Microseconds(1));
  checkpoint.Call(before_first_retry);

  task_environment_.FastForwardBy(base::Microseconds(1));
  checkpoint.Call(after_first_retry);

  EXPECT_TRUE(scheduler_->NotifyInProgressRequestFailed(
      AggregationServiceStorage::RequestId(1), 1));

  task_environment_.FastForwardBy(base::Minutes(15) - base::Microseconds(1));
  checkpoint.Call(before_second_retry);

  task_environment_.FastForwardBy(base::Microseconds(1));
  checkpoint.Call(after_second_retry);

  // It should not retry anymore
  EXPECT_FALSE(scheduler_->NotifyInProgressRequestFailed(
      AggregationServiceStorage::RequestId(1), /*previous_failed_attempts=*/2));

  VerifyHistograms(/*timer_fired_count=*/3);
}

TEST_F(AggregatableReportSchedulerTest,
       MultipleRequests_RetrievedAtAppropriateTime) {
  // Test both simultaneous and non-simultaneous reports.
  std::vector<base::Time> scheduled_report_times = {
      kExampleTime, kExampleTime, kExampleTime + base::Hours(1)};

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(Invoke([](std::vector<AggregationServiceStorage::RequestAndId>
                                requests_and_ids) {
          ASSERT_EQ(requests_and_ids.size(), 2u);

          // Ignore request ordering. Storage IDs should be incremented from 1
          EXPECT_EQ(base::flat_set<AggregationServiceStorage::RequestId>(
                        {requests_and_ids[0].id, requests_and_ids[1].id}),
                    base::flat_set<AggregationServiceStorage::RequestId>(
                        {AggregationServiceStorage::RequestId(1),
                         AggregationServiceStorage::RequestId(2)}));
        }));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(Invoke([](std::vector<AggregationServiceStorage::RequestAndId>
                                requests_and_ids) {
          ASSERT_EQ(requests_and_ids.size(), 1u);
          EXPECT_EQ(requests_and_ids[0].id,
                    AggregationServiceStorage::RequestId(3));
        }));
  }

  for (base::Time scheduled_report_time : scheduled_report_times) {
    AggregatableReportSharedInfo expected_shared_info =
        example_request.shared_info().Clone();
    expected_shared_info.scheduled_report_time = scheduled_report_time;

    scheduler_->ScheduleRequest(
        AggregatableReportRequest::Create(
            example_request.payload_contents(), std::move(expected_shared_info),
            AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
            .value());
  }

  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max(), /*limit=*/std::nullopt)
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 3u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  checkpoint.Call(1);

  task_environment_.FastForwardBy(kExampleTime - base::Time::Now());

  checkpoint.Call(2);

  task_environment_.FastForwardBy(base::Hours(1));

  VerifyHistograms(/*timer_fired_count=*/2);
}

TEST_F(AggregatableReportSchedulerTest,
       MultipleRequestsReturned_OrderedByReportTime) {
  // Order them to check reports are not returned by storage order.
  std::vector<base::Time> scheduled_report_times = {
      kExampleTime, kExampleTime + base::Hours(3),
      kExampleTime + base::Hours(1), kExampleTime + base::Hours(2)};

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  Checkpoint checkpoint;
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(Invoke([](std::vector<AggregationServiceStorage::RequestAndId>
                                requests_and_ids) {
          ASSERT_EQ(requests_and_ids.size(), 2u);

          // Ordered correctly. Storage IDs should be incremented from 1.
          EXPECT_EQ(requests_and_ids[0].id,
                    AggregationServiceStorage::RequestId(1));
          EXPECT_EQ(requests_and_ids[1].id,
                    AggregationServiceStorage::RequestId(3));
        }));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_callback_, Run)
        .WillOnce(Invoke([](std::vector<AggregationServiceStorage::RequestAndId>
                                requests_and_ids) {
          ASSERT_EQ(requests_and_ids.size(), 2u);

          // Ordered correctly. Storage IDs should be incremented from 1.
          EXPECT_EQ(requests_and_ids[0].id,
                    AggregationServiceStorage::RequestId(4));
          EXPECT_EQ(requests_and_ids[1].id,
                    AggregationServiceStorage::RequestId(2));
        }));
  }

  for (base::Time scheduled_report_time : scheduled_report_times) {
    AggregatableReportSharedInfo expected_shared_info =
        example_request.shared_info().Clone();
    expected_shared_info.scheduled_report_time = scheduled_report_time;

    scheduler_->ScheduleRequest(
        AggregatableReportRequest::Create(
            example_request.payload_contents(), std::move(expected_shared_info),
            AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
            .value());
  }

  checkpoint.Call(1);

  // Can't use `FastForwardBy()` as that will result in one call per report
  // request.
  task_environment_.AdvanceClock(kExampleTime + base::Hours(1) -
                                 base::Time::Now());
  task_environment_.RunUntilIdle();

  checkpoint.Call(2);

  // Can't use `FastForwardBy()` as that will result in one call per report
  // request.
  task_environment_.AdvanceClock(base::Hours(2));
  task_environment_.RunUntilIdle();

  VerifyHistograms(/*timer_fired_count=*/2);
}

TEST_F(AggregatableReportSchedulerTest,
       NetworkOffline_ReportsAreNotRetrievedUntilOnline) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);  // Offline

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  scheduler_->ScheduleRequest(
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(expected_shared_info),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
          .value());

  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();

  Checkpoint checkpoint;
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_callback_, Run);
  }

  // Need to fast forward beyond the report time so that it's in the past and
  // will be updated.
  task_environment_.FastForwardBy(fast_forward_required +
                                  base::Microseconds(1));

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);  // Online

  checkpoint.Call(1);

  // As the new report should've been delayed from 'now', we fast forward
  // through that delay to trigger the report.
  task_environment_.FastForwardBy(
      AggregatableReportScheduler::kOfflineReportTimeMaximumDelay);

  VerifyHistograms(/*timer_fired_count=*/1);
}

TEST_F(AggregatableReportSchedulerTest,
       OnlineConnectionChanges_ReportsAreNotRetrieved) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_3G);

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  scheduler_->ScheduleRequest(
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(expected_shared_info),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
          .value());

  Checkpoint checkpoint;
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_callback_, Run);
  }

  // Deliberately avoid running tasks so that the connection change and time
  // advance can be "atomic", which is necessary because
  // `AttributionStorage::AdjustOfflineReportTimes()` only adjusts times for
  // reports that should have been sent before now. In other words, the call to
  // `AdjustOfflineReportTimes()` would have no effect if we used
  // `FastForwardBy()` here, and we wouldn't be able to detect it below.
  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();

  task_environment_.AdvanceClock(fast_forward_required + base::Microseconds(1));
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);

  checkpoint.Call(1);

  // Cause any scheduled tasks to run. If the report was delayed, this wouldn't
  // be late enough for the report to be sent. There is a tiny chance that the
  // report was only delayed by 0 or 1 microsecond, but this flake is rare
  // enough to ignore (1 in 30 million runs).
  task_environment_.FastForwardBy(base::TimeDelta());

  VerifyHistograms(/*timer_fired_count=*/1);
}

TEST_F(AggregatableReportSchedulerTest,
       StorageLimitReached_ReportSilentlyDropped) {
  // Attempt to schedule one too many reports.
  for (int i = 0;
       i < AggregationService::kMaxStoredReportsPerReportingOrigin + 1; ++i) {
    AggregatableReportRequest example_request =
        aggregation_service::CreateExampleRequest();

    AggregatableReportSharedInfo expected_shared_info =
        example_request.shared_info().Clone();
    expected_shared_info.scheduled_report_time = kExampleTime;

    scheduler_->ScheduleRequest(
        AggregatableReportRequest::Create(
            example_request.payload_contents(), std::move(expected_shared_info),
            AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
            .value());
  }

  // One report has been silently dropped.
  EXPECT_CALL(
      mock_callback_,
      Run(Property(&std::vector<AggregationServiceStorage::RequestAndId>::size,
                   AggregationService::kMaxStoredReportsPerReportingOrigin)));
  task_environment_.FastForwardBy(kExampleTime - base::Time::Now());

  VerifyHistograms(/*timer_fired_count=*/1);
}

class AggregatableReportSchedulerDeveloperModeTest
    : public AggregatableReportSchedulerTest {
 public:
  AggregatableReportSchedulerDeveloperModeTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kPrivateAggregationDeveloperMode);
  }
};

TEST_F(AggregatableReportSchedulerDeveloperModeTest,
       NetworkOffline_ReportsAreSentImmediatelyWhenOnline) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);  // Offline

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  scheduler_->ScheduleRequest(
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(expected_shared_info),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
          .value());

  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();

  Checkpoint checkpoint;
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_callback_, Run);
  }

  // Need to fast forward beyond the report time so that it's in the past and
  // will be updated.
  task_environment_.FastForwardBy(fast_forward_required +
                                  base::Microseconds(1));

  checkpoint.Call(1);

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);  // Online

  // With the developer mode flag, the report should be sent immediately, so all
  // we need to do is run any pending tasks.
  task_environment_.RunUntilIdle();

  VerifyHistograms(/*timer_fired_count=*/1);
}

}  // namespace content
