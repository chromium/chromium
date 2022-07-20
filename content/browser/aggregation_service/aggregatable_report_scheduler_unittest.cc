// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using testing::_;
using testing::Invoke;

// Will be used to verify the sequence of expected function calls.
using Checkpoint = ::testing::MockFunction<void(int)>;

const base::Time kExampleTime = base::Time::FromJavaTime(1652984901234);

}  // namespace

class AggregatableReportSchedulerTest : public testing::Test {
 public:
  AggregatableReportSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        storage_context_(base::DefaultClock::GetInstance()),
        scheduler_(&storage_context_,
                   /*on_scheduled_report_time_reached=*/mock_callback_.Get()) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  TestAggregationServiceStorageContext storage_context_;
  base::MockRepeatingCallback<void(
      std::vector<AggregationServiceStorage::RequestAndId>)>
      mock_callback_;
  AggregatableReportScheduler scheduler_;
};

TEST_F(AggregatableReportSchedulerTest,
       ScheduleRequest_RetrievedAtAppropriateTime) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  AggregatableReportRequest expected_request =
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        std::move(expected_shared_info))
          .value();

  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max())
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

  scheduler_.ScheduleRequest(
      aggregation_service::CloneReportRequest(expected_request));

  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max())
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
        .WithArgs(base::Time::Max())
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
}

TEST_F(AggregatableReportSchedulerTest,
       InProgressRequestCompleted_DeletedFromStorage) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  scheduler_.ScheduleRequest(
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        std::move(expected_shared_info))
          .value());

  EXPECT_CALL(mock_callback_, Run);
  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();

  task_environment_.FastForwardBy(fast_forward_required);

  // The request is still stored even while it's in-progress.
  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max())
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 1u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  // Request IDs are incremented from 1.
  scheduler_.NotifyInProgressRequestSucceeded(
      AggregationServiceStorage::RequestId(1));
  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max())
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 0u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

// TODO(crbug.com/1340040): Update when retry handling is added.
TEST_F(AggregatableReportSchedulerTest,
       InProgressRequestFailed_DeletedFromStorage) {
  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportSharedInfo expected_shared_info =
      example_request.shared_info().Clone();
  expected_shared_info.scheduled_report_time = kExampleTime;

  scheduler_.ScheduleRequest(
      AggregatableReportRequest::Create(example_request.payload_contents(),
                                        std::move(expected_shared_info))
          .value());

  EXPECT_CALL(mock_callback_, Run);
  base::TimeDelta fast_forward_required = kExampleTime - base::Time::Now();

  task_environment_.FastForwardBy(fast_forward_required);

  // The request is still stored even while it's in-progress.
  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max())
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 1u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  // Request IDs are incremented from 1.
  scheduler_.NotifyInProgressRequestFailed(
      AggregationServiceStorage::RequestId(1));
  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max())
        .Then(base::BindLambdaForTesting(
            [&run_loop](std::vector<AggregationServiceStorage::RequestAndId>
                            requests_and_ids) {
              EXPECT_EQ(requests_and_ids.size(), 0u);
              run_loop.Quit();
            }));

    run_loop.Run();
  }
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

    scheduler_.ScheduleRequest(
        AggregatableReportRequest::Create(example_request.payload_contents(),
                                          std::move(expected_shared_info))
            .value());
  }

  {
    base::RunLoop run_loop;

    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
        .WithArgs(base::Time::Max())
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

    scheduler_.ScheduleRequest(
        AggregatableReportRequest::Create(example_request.payload_contents(),
                                          std::move(expected_shared_info))
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
}

}  // namespace content
