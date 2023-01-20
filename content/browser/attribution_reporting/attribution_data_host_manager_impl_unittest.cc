// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/registration_type.mojom.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_attestation.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::SourceRegistration;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::TriggerRegistration;
using ::attribution_reporting::mojom::RegistrationType;
using ::attribution_reporting::mojom::SourceRegistrationError;

using ::blink::mojom::AttributionNavigationType;

using AttributionFilters = ::attribution_reporting::Filters;

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Optional;

using Checkpoint = ::testing::MockFunction<void(int step)>;

constexpr char kSourceDataHandleStatusMetric[] =
    "Conversions.SourceDataHandleStatus2";
constexpr char kTriggerDataHandleStatusMetric[] =
    "Conversions.TriggerDataHandleStatus2";

constexpr char kRegisterSourceJson[] =
    R"json({"source_event_id":"5","destination":"https://destination.example"})json";

struct ExpectedTriggerQueueEventCounts {
  base::HistogramBase::Count skipped_queue = 0;
  base::HistogramBase::Count dropped = 0;
  base::HistogramBase::Count enqueued = 0;
  base::HistogramBase::Count processed_with_delay = 0;
  base::HistogramBase::Count flushed = 0;

  base::flat_map<base::TimeDelta, base::HistogramBase::Count> delays;
};

void CheckTriggerQueueHistograms(const base::HistogramTester& histograms,
                                 ExpectedTriggerQueueEventCounts expected) {
  static constexpr char kEventsMetric[] = "Conversions.TriggerQueueEvents";
  static constexpr char kDelayMetric[] = "Conversions.TriggerQueueDelay";

  histograms.ExpectBucketCount(kEventsMetric, 0, expected.skipped_queue);
  histograms.ExpectBucketCount(kEventsMetric, 1, expected.dropped);
  histograms.ExpectBucketCount(kEventsMetric, 2, expected.enqueued);
  histograms.ExpectBucketCount(kEventsMetric, 3, expected.processed_with_delay);
  histograms.ExpectBucketCount(kEventsMetric, 4, expected.flushed);

  base::HistogramBase::Count total = 0;

  for (const auto& [delay, count] : expected.delays) {
    histograms.ExpectTimeBucketCount(kDelayMetric, delay, count);
    total += count;
  }

  histograms.ExpectTotalCount(kDelayMetric, total);
}

struct RemoteDataHost {
  const raw_ref<BrowserTaskEnvironment> task_environment;
  mojo::Remote<blink::mojom::AttributionDataHost> data_host;

  ~RemoteDataHost() {
    // Disconnect the data host.
    data_host.reset();
    task_environment->RunUntilIdle();
  }
};

class AttributionDataHostManagerImplTest : public testing::Test {
 public:
  AttributionDataHostManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        data_host_manager_(&mock_manager_) {}

 protected:
  BrowserTaskEnvironment task_environment_;
  MockAttributionManager mock_manager_;
  AttributionDataHostManagerImpl data_host_manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

MATCHER_P(SourceIsWithinFencedFrameIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.is_within_fenced_frame(),
                            result_listener);
}

MATCHER_P(SourceDebugReportingIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.debug_reporting(), result_listener);
}

TEST_F(AttributionDataHostManagerImplTest, SourceDataHost_SourceRegistered) {
  base::HistogramTester histograms;

  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");
  auto aggregation_keys = *attribution_reporting::AggregationKeys::FromKeys(
      {{"key", absl::MakeUint128(/*high=*/5, /*low=*/345)}});

  EXPECT_CALL(
      mock_manager_,
      HandleSource(AllOf(
          SourceTypeIs(AttributionSourceType::kEvent), SourceEventIdIs(10),
          DestinationOriginIs(destination_origin),
          ImpressionOriginIs(page_origin), ReportingOriginIs(reporting_origin),
          SourcePriorityIs(20), SourceDebugKeyIs(789),
          AggregationKeysAre(aggregation_keys),
          SourceIsWithinFencedFrameIs(false), SourceDebugReportingIs(true))));
  {
    RemoteDataHost data_host_remote{.task_environment =
                                        raw_ref(task_environment_)};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(), page_origin,
        /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

    task_environment_.FastForwardBy(base::Milliseconds(1));

    SourceRegistration source_data(destination_origin);
    source_data.source_event_id = 10;
    source_data.priority = 20;
    source_data.debug_key = 789;
    source_data.aggregation_keys = aggregation_keys;
    source_data.debug_reporting = true;
    data_host_remote.data_host->SourceDataAvailable(reporting_origin,
                                                    std::move(source_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 1,
                                1);
  histograms.ExpectTimeBucketCount("Conversions.SourceEligibleDataHostLifeTime",
                                   base::Milliseconds(1), 1);
  // kSuccess = 0.
  histograms.ExpectUniqueSample(kSourceDataHandleStatusMetric, 0, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_ReceiverDestinationsMayDiffer) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleSource);
  }

  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  {
    RemoteDataHost data_host_remote{.task_environment =
                                        raw_ref(task_environment_)};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(), page_origin,
        /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

    SourceRegistration source_data(destination_origin);
    data_host_remote.data_host->SourceDataAvailable(reporting_origin,
                                                    source_data);
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(1);

    data_host_remote.data_host->SourceDataAvailable(reporting_origin,
                                                    source_data);
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(2);

    source_data.destination =
        *SuitableOrigin::Deserialize("https://other-trigger.example");
    data_host_remote.data_host->SourceDataAvailable(reporting_origin,
                                                    source_data);
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(3);
    data_host_remote.data_host->SourceDataAvailable(std::move(reporting_origin),
                                                    std::move(source_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 4,
                                1);
  // kSuccess = 0.
  histograms.ExpectBucketCount(kSourceDataHandleStatusMetric, 0, 4);
}

TEST_F(AttributionDataHostManagerImplTest, TriggerDataHost_TriggerRegistered) {
  base::HistogramTester histograms;

  auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  auto filters = *AttributionFilters::Create({{"a", {"b"}}});
  auto event_trigger_data_filters = *AttributionFilters::Create({{"c", {"d"}}});
  auto event_trigger_data_not_filters =
      *AttributionFilters::Create({{"e", {"f"}}});

  EXPECT_CALL(
      mock_manager_,
      HandleTrigger(AttributionTriggerMatches(AttributionTriggerMatcherConfig(
          reporting_origin,
          TriggerRegistrationMatches(TriggerRegistrationMatcherConfig(
              filters, AttributionFilters(), Optional(789),
              EventTriggerDataListMatches(
                  EventTriggerDataListMatcherConfig(ElementsAre(
                      EventTriggerDataMatches(EventTriggerDataMatcherConfig(
                          1, 2, Optional(3), event_trigger_data_filters,
                          event_trigger_data_not_filters)),
                      EventTriggerDataMatches(EventTriggerDataMatcherConfig(
                          4, 5, Eq(absl::nullopt), AttributionFilters(),
                          AttributionFilters()))))),
              Optional(123),
              /*debug_reporting=*/true,
              attribution_reporting::AggregatableTriggerDataList(),
              attribution_reporting::AggregatableValues(),
              ::aggregation_service::mojom::AggregationCoordinator::kDefault)),
          destination_origin))));

  {
    RemoteDataHost data_host_remote{.task_environment =
                                        raw_ref(task_environment_)};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(),
        destination_origin, /*is_within_fenced_frame=*/false,
        RegistrationType::kSourceOrTrigger);

    TriggerRegistration trigger_data;
    trigger_data.debug_key = 789;
    trigger_data.filters = filters;
    trigger_data.event_triggers =
        *attribution_reporting::EventTriggerDataList::Create(
            {attribution_reporting::EventTriggerData(
                 /*data=*/1, /*priority=*/2,
                 /*dedup_key=*/3, event_trigger_data_filters,
                 event_trigger_data_not_filters),
             attribution_reporting::EventTriggerData(
                 /*data=*/4, /*priority=*/5,
                 /*dedup_key=*/absl::nullopt, /*filters=*/AttributionFilters(),
                 /*not_filters=*/AttributionFilters())});

    trigger_data.aggregatable_dedup_key = 123;
    trigger_data.debug_reporting = true;

    data_host_remote.data_host->TriggerDataAvailable(
        reporting_origin, std::move(trigger_data),
        /*attestation=*/absl::nullopt);
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectBucketCount("Conversions.RegisteredTriggersPerDataHost", 1,
                               1);
  // kSuccess = 0.
  histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric, 0, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_ReceiverModeCheckPerformed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  {
    RemoteDataHost data_host_remote{.task_environment =
                                        raw_ref(task_environment_)};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(),
        destination_origin, /*is_within_fenced_frame=*/false,
        RegistrationType::kSourceOrTrigger);

    TriggerRegistration trigger_data;

    data_host_remote.data_host->TriggerDataAvailable(
        reporting_origin, trigger_data, /*attestation=*/absl::nullopt);
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(1);

    data_host_remote.data_host->TriggerDataAvailable(
        reporting_origin, trigger_data, /*attestation=*/absl::nullopt);
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(2);

    {
      mojo::test::BadMessageObserver bad_message_observer;

      SourceRegistration source_data(destination_origin);

      data_host_remote.data_host->SourceDataAvailable(reporting_origin,
                                                      std::move(source_data));
      data_host_remote.data_host.FlushForTesting();

      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Not eligible for sources.");
    }

    checkpoint.Call(3);

    data_host_remote.data_host->TriggerDataAvailable(
        std::move(reporting_origin), std::move(trigger_data),
        /*attestation=*/absl::nullopt);
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectTotalCount("Conversions.RegisteredSourcesPerDataHost", 0);
  histograms.ExpectUniqueSample("Conversions.RegisteredTriggersPerDataHost", 3,
                                1);
  // kSuccess = 0.
  histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric, 0, 3);
  // kContextError = 1.
  histograms.ExpectUniqueSample(kSourceDataHandleStatusMetric, 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_ReceiverModeCheckPerformed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleSource);
  }

  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  {
    RemoteDataHost data_host_remote{.task_environment =
                                        raw_ref(task_environment_)};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(), page_origin,
        /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

    SourceRegistration source_data(destination_origin);

    data_host_remote.data_host->SourceDataAvailable(reporting_origin,
                                                    source_data);
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(1);

    data_host_remote.data_host->SourceDataAvailable(reporting_origin,
                                                    source_data);
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(2);

    {
      mojo::test::BadMessageObserver bad_message_observer;

      data_host_remote.data_host->TriggerDataAvailable(
          reporting_origin, TriggerRegistration(),
          /*attestation=*/absl::nullopt);
      data_host_remote.data_host.FlushForTesting();

      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Not eligible for triggers.");
    }

    checkpoint.Call(3);

    data_host_remote.data_host->SourceDataAvailable(std::move(reporting_origin),
                                                    std::move(source_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 3,
                                1);
  histograms.ExpectTotalCount("Conversions.RegisteredTriggersPerDataHost", 0);
  // kSuccess = 0.
  histograms.ExpectUniqueSample(kSourceDataHandleStatusMetric, 0, 3);
  // kContextError = 1.
  histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric, 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_NavigationSourceRegistered) {
  base::HistogramTester histograms;

  const auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  const auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  const auto aggregation_keys =
      *attribution_reporting::AggregationKeys::FromKeys(
          {{"key", absl::MakeUint128(/*high=*/5, /*low=*/345)}});

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(
        mock_manager_,
        HandleSource(AllOf(
            SourceTypeIs(AttributionSourceType::kNavigation),
            SourceEventIdIs(10), DestinationOriginIs(destination_origin),
            ImpressionOriginIs(page_origin),
            ReportingOriginIs(reporting_origin), SourcePriorityIs(20),
            SourceDebugKeyIs(789), AggregationKeysAre(aggregation_keys),
            SourceIsWithinFencedFrameIs(false), SourceDebugReportingIs(true))));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
  }

  const blink::AttributionSrcToken attribution_src_token;

  {
    RemoteDataHost data_host_remote{.task_environment =
                                        raw_ref(task_environment_)};
    data_host_manager_.RegisterNavigationDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(),
        attribution_src_token, AttributionInputEvent());

    task_environment_.FastForwardBy(base::Milliseconds(1));

    data_host_manager_.NotifyNavigationForDataHost(
        attribution_src_token, page_origin,
        AttributionNavigationType::kContextMenu,
        /*is_within_fenced_frame=*/false);

    SourceRegistration source_data(destination_origin);
    source_data.source_event_id = 10;
    source_data.priority = 20;
    source_data.debug_key = 789;
    source_data.aggregation_keys = aggregation_keys;
    source_data.debug_reporting = true;
    data_host_remote.data_host->SourceDataAvailable(reporting_origin,
                                                    source_data);
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(1);

    // This should succeed even though the destination site doesn't match the
    // final navigation site.
    source_data.destination =
        *SuitableOrigin::Deserialize("https://trigger2.example");
    data_host_remote.data_host->SourceDataAvailable(reporting_origin,
                                                    std::move(source_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectTimeBucketCount("Conversions.SourceEligibleDataHostLifeTime",
                                   base::Milliseconds(1), 1);

  // kRegistered = 0, kProcessed = 3.
  histograms.ExpectBucketCount("Conversions.NavigationDataHostStatus2", 0, 1);
  histograms.ExpectBucketCount("Conversions.NavigationDataHostStatus2", 3, 1);

  // kSuccess = 0, kContextError = 1.
  histograms.ExpectBucketCount(kSourceDataHandleStatusMetric, 0, 2);
  histograms.ExpectBucketCount(kSourceDataHandleStatusMetric, 1, 0);

  // kContextMenu = 2.
  histograms.ExpectBucketCount(
      "Conversions.SourceRegistration.NavigationType.Background", 2, 2);
}

// Ensures correct behavior in
// `AttributionDataHostManagerImpl::OnDataHostDisconnected()` when a data host
// is registered but disconnects before registering a source or trigger.
TEST_F(AttributionDataHostManagerImplTest, NoSourceOrTrigger) {
  base::HistogramTester histograms;

  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");

  {
    RemoteDataHost data_host_remote{.task_environment =
                                        raw_ref(task_environment_)};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(), page_origin,
        /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);
  }

  histograms.ExpectTotalCount("Conversions.RegisteredSourcesPerDataHost", 0);
  histograms.ExpectTotalCount("Conversions.RegisteredTriggersPerDataHost", 0);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnected_TriggerDelayed) {
  constexpr RegistrationType kTestCases[] = {
      RegistrationType::kSourceOrTrigger,
      RegistrationType::kSource,
  };

  for (auto registration_type : kTestCases) {
    base::HistogramTester histograms;

    Checkpoint checkpoint;
    {
      InSequence seq;

      EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
      EXPECT_CALL(checkpoint, Call(1));
      EXPECT_CALL(mock_manager_, HandleTrigger);
    }

    {
      RemoteDataHost source_data_host_remote{.task_environment =
                                                 raw_ref(task_environment_)};
      data_host_manager_.RegisterDataHost(
          source_data_host_remote.data_host.BindNewPipeAndPassReceiver(),
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_within_fenced_frame=*/false, registration_type);

      mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
      data_host_manager_.RegisterDataHost(
          trigger_data_host_remote.BindNewPipeAndPassReceiver(),
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

      task_environment_.FastForwardBy(base::Milliseconds(1));

      // Because there is a connected data host in source mode, this trigger
      // should be delayed.
      trigger_data_host_remote->TriggerDataAvailable(
          /*reporting_origin=*/*SuitableOrigin::Deserialize(
              "https://report.test"),
          TriggerRegistration(), /*attestation=*/absl::nullopt);
      trigger_data_host_remote.FlushForTesting();

      task_environment_.FastForwardBy(base::Seconds(5) - base::Microseconds(1));
      checkpoint.Call(1);
      task_environment_.FastForwardBy(base::Microseconds(1));
    }

    CheckTriggerQueueHistograms(histograms, {
                                                .enqueued = 1,
                                                .processed_with_delay = 1,
                                                .delays =
                                                    {
                                                        {base::Seconds(5), 1},
                                                    },
                                            });

    // Recorded when source data host was disconnected.
    histograms.ExpectTimeBucketCount(
        "Conversions.SourceEligibleDataHostLifeTime", base::Seconds(5), 1);
    // Recorded when trigger data was available.
    histograms.ExpectTimeBucketCount(
        "Conversions.SourceEligibleDataHostLifeTime", base::Milliseconds(1), 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerModeReceiverConnected_TriggerNotDelayed) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote1;
  data_host_manager_.RegisterDataHost(
      data_host_remote1.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote2;
  data_host_manager_.RegisterDataHost(
      data_host_remote2.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kTrigger);

  // Because there is no data host in source mode, this trigger should not be
  // delayed.
  data_host_remote2->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*attestation=*/absl::nullopt);

  data_host_remote2.FlushForTesting();

  CheckTriggerQueueHistograms(histograms, {.skipped_queue = 1});
  histograms.ExpectTotalCount("Conversions.SourceEligibleDataHostLifeTime", 0);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnected_TriggerDelayed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token, AttributionInputEvent());

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  // Because there is a connected data host in source mode, this trigger should
  // be delayed.
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*attestation=*/absl::nullopt);
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(5) - base::Microseconds(1));
  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Microseconds(1));

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 1,
                                              .processed_with_delay = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(5), 1},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_NavigationFailed) {
  EXPECT_CALL(mock_manager_, HandleSource);

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationFailure(attribution_src_token);

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site, AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_NavigationFailedBeforeParsing) {
  EXPECT_CALL(mock_manager_, HandleSource);

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);
  data_host_manager_.NotifyNavigationFailure(attribution_src_token);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ParsingFinishesBeforeAndAfterNav) {
  EXPECT_CALL(mock_manager_, HandleSource(SourceIsWithinFencedFrameIs(false)))
      .Times(2);

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site, AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ParsingFailsBeforeAndSucceedsAfterNav) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleSource).Times(1);

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_, NotifyFailedSourceRegistration(
                                 "!!!invalid json", source_site, reporter,
                                 SourceRegistrationError::kInvalidJson));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, "!!!invalid json", reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site, AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  histograms.ExpectUniqueSample("Conversions.SourceRegistrationError",
                                SourceRegistrationError::kInvalidJson, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_TriggerDelayed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  // Because there is a connected data host in source mode, this trigger should
  // be delayed.
  trigger_data_host_remote->TriggerDataAvailable(std::move(reporter),
                                                 TriggerRegistration(),
                                                 /*attestation=*/absl::nullopt);
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(5) - base::Microseconds(1));
  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Microseconds(1));

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 1,
                                              .processed_with_delay = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(5), 1},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_NavigationFinishedQueueSkipped) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource).Times(2);
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);

  // Wait for parsing.
  task_environment_.FastForwardBy(base::TimeDelta());
  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site, AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);

  checkpoint.Call(1);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*attestation=*/absl::nullopt);
  trigger_data_host_remote.FlushForTesting();

  CheckTriggerQueueHistograms(histograms, {.skipped_queue = 1});
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ParsingAfterNavigationFinishedQueueSkipped) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource).Times(2);
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson, reporter, source_site,
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);

  // Wait for parsing.
  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site, AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/false);

  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*attestation=*/absl::nullopt);
  trigger_data_host_remote.FlushForTesting();

  CheckTriggerQueueHistograms(histograms, {.skipped_queue = 1});
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnectsDisconnects_TriggerNotDelayed) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  source_data_host_remote.reset();

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*attestation=*/absl::nullopt);
  trigger_data_host_remote.FlushForTesting();

  CheckTriggerQueueHistograms(histograms, {.skipped_queue = 1});
}

TEST_F(AttributionDataHostManagerImplTest, TwoTriggerReceivers) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger).Times(2);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote1;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote1.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote2;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote2.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  auto reporting_origin = *SuitableOrigin::Deserialize("https://report.test");

  TriggerRegistration trigger_data;

  trigger_data_host_remote1->TriggerDataAvailable(
      reporting_origin, trigger_data, /*attestation=*/absl::nullopt);
  trigger_data_host_remote2->TriggerDataAvailable(
      std::move(reporting_origin), std::move(trigger_data),
      /*attestation=*/absl::nullopt);

  trigger_data_host_remote1.FlushForTesting();
  trigger_data_host_remote2.FlushForTesting();

  // 1. Trigger 1 is enqueued because the other data host is connected in source
  //    mode.
  // 2. Trigger 2 resets the source mode receiver count to 0, which flushes
  //    trigger 1.
  // 3. Trigger 2 skips the queue.
  CheckTriggerQueueHistograms(histograms, {
                                              .skipped_queue = 1,
                                              .enqueued = 1,
                                              .flushed = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(0), 1},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnectsFails_TriggerNotDelayed) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger);

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token, AttributionInputEvent());

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  data_host_manager_.NotifyNavigationFailure(attribution_src_token);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*attestation=*/absl::nullopt);
  trigger_data_host_remote.FlushForTesting();

  CheckTriggerQueueHistograms(histograms, {.skipped_queue = 1});

  histograms.ExpectTotalCount("Conversions.TriggerQueueDelay", 0);
  histograms.ExpectTimeBucketCount("Conversions.SourceEligibleDataHostLifeTime",
                                   base::Milliseconds(1), 2);

  // kRegistered = 0, kNavigationFailed = 2.
  histograms.ExpectBucketCount("Conversions.NavigationDataHostStatus2", 0, 1);
  histograms.ExpectBucketCount("Conversions.NavigationDataHostStatus2", 2, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnected_DelayedTriggersHandledInOrder) {
  base::HistogramTester histograms;

  const auto reporting_origin1 =
      *SuitableOrigin::Deserialize("https://report1.test");
  const auto reporting_origin2 =
      *SuitableOrigin::Deserialize("https://report2.test");

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_,
                HandleTrigger(AttributionTriggerMatches(
                    AttributionTriggerMatcherConfig(reporting_origin1))));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_,
                HandleTrigger(AttributionTriggerMatches(
                    AttributionTriggerMatcherConfig(reporting_origin2))));
  }

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  auto send_trigger = [&](const SuitableOrigin& reporting_origin) {
    trigger_data_host_remote->TriggerDataAvailable(
        reporting_origin, TriggerRegistration(), /*attestation=*/absl::nullopt);
  };

  send_trigger(reporting_origin1);
  task_environment_.FastForwardBy(base::Seconds(1));
  send_trigger(reporting_origin2);
  trigger_data_host_remote.FlushForTesting();

  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Seconds(4));
  checkpoint.Call(2);
  task_environment_.FastForwardBy(base::Seconds(1));

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 2,
                                              .processed_with_delay = 2,
                                              .delays =
                                                  {
                                                      {base::Seconds(5), 2},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnectsDisconnects_DelayedTriggersFlushed) {
  base::HistogramTester histograms;

  base::RunLoop loop;
  EXPECT_CALL(mock_manager_, HandleTrigger)
      .WillOnce([&](AttributionTrigger trigger) { loop.Quit(); });

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*attestation=*/absl::nullopt);
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(2));
  source_data_host_remote.reset();
  loop.Run();

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 1,
                                              .flushed = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(2), 1},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnected_ExcessiveDelayedTriggersDropped) {
  constexpr size_t kMaxDelayedTriggers = 30;

  base::HistogramTester histograms;

  base::RunLoop loop;
  auto barrier = base::BarrierClosure(kMaxDelayedTriggers, loop.QuitClosure());

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  auto send_trigger = [&](const SuitableOrigin& reporting_origin) {
    trigger_data_host_remote->TriggerDataAvailable(
        reporting_origin, TriggerRegistration(), /*attestation=*/absl::nullopt);
  };

  for (size_t i = 0; i < kMaxDelayedTriggers; i++) {
    auto reporting_origin = *SuitableOrigin::Deserialize(
        base::StrCat({"https://report", base::NumberToString(i), ".test"}));

    EXPECT_CALL(mock_manager_,
                HandleTrigger(AttributionTriggerMatches(
                    AttributionTriggerMatcherConfig(reporting_origin))))
        .WillOnce([&](AttributionTrigger trigger) { barrier.Run(); });

    send_trigger(reporting_origin);
  }

  // This one should be dropped.
  send_trigger(*SuitableOrigin::Deserialize("https://excessive.test"));

  trigger_data_host_remote.FlushForTesting();
  source_data_host_remote.reset();
  loop.Run();

  CheckTriggerQueueHistograms(
      histograms, {
                      .dropped = 1,
                      .enqueued = kMaxDelayedTriggers,
                      .flushed = kMaxDelayedTriggers,
                      .delays =
                          {
                              {base::Seconds(0), kMaxDelayedTriggers},
                          },
                  });
}

TEST_F(AttributionDataHostManagerImplTest, SourceThenTrigger_TriggerDelayed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger);

  SourceRegistration source_data(
      *SuitableOrigin::Deserialize("https://dest.test"));
  source_data_host_remote->SourceDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report1.test"),
      std::move(source_data));
  source_data_host_remote.FlushForTesting();

  // Because there is still a connected data host in source mode, this trigger
  // should be delayed.
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report2.test"),
      TriggerRegistration(), /*attestation=*/absl::nullopt);
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(5) - base::Microseconds(1));
  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Microseconds(1));

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 1,
                                              .processed_with_delay = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(5), 1},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest, NavigationDataHostNotRegistered) {
  base::HistogramTester histograms;

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page.example"),
      AttributionNavigationType::kAnchor, /*is_within_fenced_frame=*/false);

  // kNotFound = 1.
  histograms.ExpectUniqueSample("Conversions.NavigationDataHostStatus2", 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationDataHost_CannotRegisterTrigger) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token,
      AttributionInputEvent());

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, *SuitableOrigin::Deserialize("https://s.test"),
      AttributionNavigationType::kAnchor, /*is_within_fenced_frame=*/false);

  mojo::test::BadMessageObserver bad_message_observer;

  data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://r.test"),
      TriggerRegistration(), /*attestation=*/absl::nullopt);
  data_host_remote.FlushForTesting();

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "AttributionDataHost: Not eligible for triggers.");

  // kContextError = 1.
  histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric, 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       DuplicateAttributionSrcToken_NotRegistered) {
  EXPECT_CALL(mock_manager_, HandleSource(SourceEventIdIs(1)));

  const blink::AttributionSrcToken attribution_src_token;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote1,
      data_host_remote2;

  {
    base::HistogramTester histograms;

    EXPECT_TRUE(data_host_manager_.RegisterNavigationDataHost(
        data_host_remote1.BindNewPipeAndPassReceiver(), attribution_src_token,
        AttributionInputEvent()));

    // This one should not be registered, as `attribution_src_token` is already
    // associated with a receiver.
    EXPECT_FALSE(data_host_manager_.RegisterNavigationDataHost(
        data_host_remote2.BindNewPipeAndPassReceiver(), attribution_src_token,
        AttributionInputEvent()));

    // kRegistered = 0.
    histograms.ExpectUniqueSample("Conversions.NavigationDataHostStatus2", 0,
                                  1);
  }

  const auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page.example"),
      AttributionNavigationType::kAnchor, /*is_within_fenced_frame=*/false);

  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  SourceRegistration source_data(destination_origin);
  source_data.source_event_id = 1;
  data_host_remote1->SourceDataAvailable(reporting_origin, source_data);
  data_host_remote1.FlushForTesting();

  source_data.source_event_id = 2;
  data_host_remote2->SourceDataAvailable(std::move(reporting_origin),
                                         std::move(source_data));
  data_host_remote2.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHostWithinFencedFrame_SourceRegistered) {
  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  EXPECT_CALL(
      mock_manager_,
      HandleSource(AllOf(
          SourceTypeIs(AttributionSourceType::kEvent), SourceEventIdIs(10),
          DestinationOriginIs(destination_origin),
          ImpressionOriginIs(page_origin), ReportingOriginIs(reporting_origin),
          SourceIsWithinFencedFrameIs(true))));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin,
      /*is_within_fenced_frame=*/true, RegistrationType::kSourceOrTrigger);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  SourceRegistration source_data(destination_origin);
  source_data.source_event_id = 10;
  data_host_remote->SourceDataAvailable(reporting_origin,
                                        std::move(source_data));
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHostWithinFencedFrame_TriggerRegistered) {
  auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");
  EXPECT_CALL(
      mock_manager_,
      HandleTrigger(AttributionTriggerMatches(AttributionTriggerMatcherConfig(
          reporting_origin, _, destination_origin,
          /*is_within_fenced_frame=*/true))));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), destination_origin,
      /*is_within_fenced_frame=*/true, RegistrationType::kSourceOrTrigger);

  data_host_remote->TriggerDataAvailable(
      reporting_origin, TriggerRegistration(), /*attestation=*/absl::nullopt);
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceWithinFencedFrame_SourceRegistered) {
  EXPECT_CALL(mock_manager_, HandleSource(SourceIsWithinFencedFrameIs(true)));

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;

  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token,
      AttributionInputEvent());

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://source.test"),
      AttributionNavigationType::kAnchor, /*is_within_fenced_frame=*/true);

  data_host_remote->SourceDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      SourceRegistration(
          /*destination=*/*SuitableOrigin::Deserialize(
              "https://destination.test")));
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSourceWithinFencedFrame_SourceRegistered) {
  EXPECT_CALL(mock_manager_, HandleSource(SourceIsWithinFencedFrameIs(true)));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistration(
      attribution_src_token, kRegisterSourceJson,
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*source_origin=*/*SuitableOrigin::Deserialize("https://source.test"),
      AttributionInputEvent(), AttributionNavigationType::kAnchor,
      /*is_within_fenced_frame=*/true);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

}  // namespace
}  // namespace content
