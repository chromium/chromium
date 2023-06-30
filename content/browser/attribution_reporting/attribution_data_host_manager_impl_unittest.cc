// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/registration_type.mojom.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/attribution_reporting/test/mock_content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_response_headers.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::DestinationSet;
using ::attribution_reporting::FilterPair;
using ::attribution_reporting::SourceRegistration;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::TriggerRegistration;
using ::attribution_reporting::mojom::RegistrationType;
using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;

using AttributionFilters = ::attribution_reporting::FiltersDisjunction;

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Optional;

using Checkpoint = ::testing::MockFunction<void(int step)>;

constexpr char kRegisterSourceJson[] =
    R"json({"source_event_id":"5","destination":"https://destination.example"})json";

constexpr char kNavigationDataHostStatusHistogram[] =
    "Conversions.NavigationDataHostStatus3";

constexpr char kRegisterDataHostOutcomeHistogram[] =
    "Conversions.RegisterDataHostOutcome";

constexpr char kProcessRegisterDataHostDelayHistogram[] =
    "Conversions.ProcessRegisterDataHostDelay";

const GlobalRenderFrameHostId kFrameId = {0, 1};

constexpr BeaconId kBeaconId(123);
constexpr int64_t kNavigationId(456);

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

TEST_F(AttributionDataHostManagerImplTest, SourceDataHost_SourceRegistered) {
  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");
  auto aggregation_keys = *attribution_reporting::AggregationKeys::FromKeys(
      {{"key", absl::MakeUint128(/*high=*/5, /*low=*/345)}});

  EXPECT_CALL(
      mock_manager_,
      HandleSource(AllOf(SourceRegistrationIs(SourceRegistrationMatches(
                             SourceRegistrationMatcherConfig(
                                 /*source_event_id=*/10,
                                 *attribution_reporting::DestinationSet::Create(
                                     {destination_site}),
                                 /*priority=*/20,
                                 /*debug_key=*/Optional(789), aggregation_keys,
                                 /*debug_reporting=*/true))),
                         SourceTypeIs(SourceType::kEvent),
                         ImpressionOriginIs(page_origin),
                         ReportingOriginIs(reporting_origin),
                         SourceIsWithinFencedFrameIs(false)),
                   kFrameId));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
  source_data.source_event_id = 10;
  source_data.priority = 20;
  source_data.debug_key = 789;
  source_data.aggregation_keys = aggregation_keys;
  source_data.debug_reporting = true;
  data_host_remote->SourceDataAvailable(reporting_origin,
                                        std::move(source_data));
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_ReceiverDestinationsMayDiffer) {
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
  auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
  data_host_remote->SourceDataAvailable(reporting_origin, source_data);
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->SourceDataAvailable(reporting_origin, source_data);
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  source_data.destination_set = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://other-trigger.example")});
  data_host_remote->SourceDataAvailable(reporting_origin, source_data);
  data_host_remote.FlushForTesting();

  checkpoint.Call(3);
  data_host_remote->SourceDataAvailable(std::move(reporting_origin),
                                        std::move(source_data));
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, TriggerDataHost_TriggerRegistered) {
  auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  auto filters = AttributionFilters({{{"a", {"b"}}}});
  FilterPair event_trigger_data_filters(
      /*positive=*/{{{"c", {"d"}}}},
      /*negative=*/{{{"e", {"f"}}}});

  std::vector<attribution_reporting::AggregatableDedupKey>
      aggregatable_dedup_keys = {attribution_reporting::AggregatableDedupKey(
          /*dedup_key=*/123, FilterPair())};

  auto aggregation_coordinator_origin =
      SuitableOrigin::Deserialize("https://coordinator.test");

  EXPECT_CALL(
      mock_manager_,
      HandleTrigger(
          AttributionTriggerMatches(AttributionTriggerMatcherConfig(
              reporting_origin,
              TriggerRegistrationMatches(TriggerRegistrationMatcherConfig(
                  FilterPair(/*positive=*/filters, /*negative=*/{}),
                  Optional(789),
                  ElementsAre(
                      EventTriggerDataMatches(EventTriggerDataMatcherConfig(
                          1, 2, Optional(3), event_trigger_data_filters)),
                      EventTriggerDataMatches(EventTriggerDataMatcherConfig(
                          4, 5, Eq(absl::nullopt), FilterPair()))),
                  aggregatable_dedup_keys,
                  /*debug_reporting=*/true,
                  std::vector<attribution_reporting::AggregatableTriggerData>(),
                  attribution_reporting::AggregatableValues(),
                  aggregation_coordinator_origin,
                  attribution_reporting::mojom::SourceRegistrationTimeConfig::
                      kExclude)),
              destination_origin)),
          kFrameId));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), destination_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId,
      /*last_navigation_id=*/kNavigationId);

  TriggerRegistration trigger_data;
  trigger_data.debug_key = 789;
  trigger_data.filters.positive = filters;
  trigger_data.event_triggers = {
      attribution_reporting::EventTriggerData(
          /*data=*/1, /*priority=*/2,
          /*dedup_key=*/3, event_trigger_data_filters),
      attribution_reporting::EventTriggerData(
          /*data=*/4, /*priority=*/5,
          /*dedup_key=*/absl::nullopt, FilterPair())};

  trigger_data.aggregatable_dedup_keys = aggregatable_dedup_keys;
  trigger_data.debug_reporting = true;
  trigger_data.aggregation_coordinator_origin = aggregation_coordinator_origin;

  data_host_remote->TriggerDataAvailable(reporting_origin,
                                         std::move(trigger_data),
                                         /*verifications=*/{});
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_ReceiverModeCheckPerformed) {
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

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), destination_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kTrigger, kFrameId,
      /*last_navigation_id=*/kNavigationId);

  TriggerRegistration trigger_data;

  data_host_remote->TriggerDataAvailable(reporting_origin, trigger_data,
                                         /*verifications=*/{});
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->TriggerDataAvailable(reporting_origin, trigger_data,
                                         /*verifications=*/{});
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  {
    mojo::test::BadMessageObserver bad_message_observer;

    SourceRegistration source_data(
        *DestinationSet::Create({net::SchemefulSite(destination_origin)}));

    data_host_remote->SourceDataAvailable(reporting_origin,
                                          std::move(source_data));
    data_host_remote.FlushForTesting();

    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "AttributionDataHost: Not eligible for source.");
  }

    checkpoint.Call(3);

    data_host_remote->TriggerDataAvailable(std::move(reporting_origin),
                                           std::move(trigger_data),
                                           /*verifications=*/{});
    data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       MixedDataHost_AllowsSourcesAndTriggers) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleSource);
  }

  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId,
      /*last_navigation_id=*/kNavigationId);

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));

  data_host_remote->SourceDataAvailable(reporting_origin, source_data);
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->SourceDataAvailable(reporting_origin, source_data);
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  data_host_remote->TriggerDataAvailable(reporting_origin,
                                         TriggerRegistration(),
                                         /*verifications=*/{});
  data_host_remote.FlushForTesting();

  checkpoint.Call(3);

  data_host_remote->SourceDataAvailable(std::move(reporting_origin),
                                        std::move(source_data));
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_ReceiverModeCheckPerformed) {
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
  auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kSource, kFrameId,
      /*last_navigation_id=*/kNavigationId);

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));

  data_host_remote->SourceDataAvailable(reporting_origin, source_data);
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->SourceDataAvailable(reporting_origin, source_data);
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  {
    mojo::test::BadMessageObserver bad_message_observer;

    data_host_remote->TriggerDataAvailable(reporting_origin,
                                           TriggerRegistration(),
                                           /*verifications=*/{});
    data_host_remote.FlushForTesting();

    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "AttributionDataHost: Not eligible for trigger.");
  }

    checkpoint.Call(3);

    data_host_remote->SourceDataAvailable(std::move(reporting_origin),
                                          std::move(source_data));
    data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_NavigationSourceRegistered) {
  base::HistogramTester histograms;

  const auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  const auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");
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
        HandleSource(
            AllOf(SourceRegistrationIs(
                      SourceRegistrationMatches(SourceRegistrationMatcherConfig(
                          /*source_event_id=*/10,
                          *attribution_reporting::DestinationSet::Create(
                              {destination_site}),
                          /*priority=*/20, /*debug_key=*/Optional(789),
                          aggregation_keys,
                          /*debug_reporting=*/true))),
                  SourceTypeIs(SourceType::kNavigation),
                  ImpressionOriginIs(page_origin),
                  ReportingOriginIs(reporting_origin),
                  SourceIsWithinFencedFrameIs(false)),
            kFrameId));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
  }

  const blink::AttributionSrcToken attribution_src_token;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token,
      AttributionInputEvent());

  task_environment_.FastForwardBy(base::Milliseconds(1));

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, page_origin,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
  source_data.source_event_id = 10;
  source_data.priority = 20;
  source_data.debug_key = 789;
  source_data.aggregation_keys = aggregation_keys;
  source_data.debug_reporting = true;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data);
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  // This should succeed even though the destination site doesn't match the
  // final navigation site.
  source_data.destination_set = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger2.example")});
  data_host_remote->SourceDataAvailable(reporting_origin,
                                        std::move(source_data));
  data_host_remote.FlushForTesting();

  // kRegistered = 0, kProcessed = 3.
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 0, 1);
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 3, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHostDisconnectedBeforeBinding_NavigationSourceRegistered) {
  base::HistogramTester histograms;

  const auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(SourceTypeIs(SourceType::kNavigation),
                                 ImpressionOriginIs(page_origin),
                                 ReportingOriginIs(reporting_origin)),
                           kFrameId))
      .Times(2);

  const blink::AttributionSrcToken attribution_src_token;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token,
      AttributionInputEvent());

  SourceRegistration source_data(*DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger.example")}));
  source_data.source_event_id = 10;
  source_data.priority = 20;
  source_data.debug_key = 789;
  source_data.aggregation_keys =
      *attribution_reporting::AggregationKeys::FromKeys(
          {{"key", absl::MakeUint128(/*high=*/5, /*low=*/345)}});
  source_data.debug_reporting = true;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data);

  // This should succeed even though the destination site doesn't match the
  // final navigation site.
  source_data.destination_set = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger2.example")});
  data_host_remote->SourceDataAvailable(reporting_origin,
                                        std::move(source_data));

  data_host_remote.reset();

  task_environment_.FastForwardBy(base::Milliseconds(1));

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, page_origin,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);
  task_environment_.RunUntilIdle();

  // kRegistered = 0, kProcessed = 3.
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 0, 1);
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 3, 1);
}

// Ensures correct behavior in
// `AttributionDataHostManagerImpl::OnDataHostDisconnected()` when a data host
// is registered but disconnects before registering a source or trigger.
TEST_F(AttributionDataHostManagerImplTest, NoSourceOrTrigger) {
  base::HistogramTester histograms;
  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);
  data_host_remote.reset();
  task_environment_.RunUntilIdle();

  // kImmediately = 0
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 0, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnected_TriggerNotDelayed) {
  constexpr RegistrationType kTestCases[] = {
      RegistrationType::kSourceOrTrigger,
      RegistrationType::kSource,
  };

  for (auto registration_type : kTestCases) {
    base::HistogramTester histograms;
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);

    mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
    data_host_manager_.RegisterDataHost(
        source_data_host_remote.BindNewPipeAndPassReceiver(),
        *SuitableOrigin::Deserialize("https://page1.example"),
        /*is_within_fenced_frame=*/false, registration_type, kFrameId,
        /*last_navigation_id=*/kNavigationId);

    mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
    data_host_manager_.RegisterDataHost(
        trigger_data_host_remote.BindNewPipeAndPassReceiver(),
        *SuitableOrigin::Deserialize("https://page2.example"),
        /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
        kFrameId, /*last_navigation_id=*/kNavigationId);

    task_environment_.FastForwardBy(base::Milliseconds(1));

    // Because the connected data host in source mode is not linked to
    // navigation this trigger should NOT be delayed.
    trigger_data_host_remote->TriggerDataAvailable(
        /*reporting_origin=*/*SuitableOrigin::Deserialize(
            "https://report.test"),
        TriggerRegistration(), /*verifications=*/{});

    task_environment_.FastForwardBy(base::TimeDelta());

    // kImmediately = 0
    histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 0, 2);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerModeReceiverConnected_TriggerNotDelayed) {
  EXPECT_CALL(mock_manager_, HandleTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote1;
  data_host_manager_.RegisterDataHost(
      data_host_remote1.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kTrigger, kFrameId,
      /*last_navigation_id=*/kNavigationId);

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote2;
  data_host_manager_.RegisterDataHost(
      data_host_remote2.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kTrigger, kFrameId,
      /*last_navigation_id=*/kNavigationId);

  // Because there is no data host in source mode, this trigger should not be
  // delayed.
  data_host_remote2->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});

  data_host_remote2.FlushForTesting();
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

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  // Because there is a connected data host in source mode, this trigger should
  // be delayed.
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});

  checkpoint.Call(1);
  source_data_host_remote.reset();
  task_environment_.FastForwardBy(base::Microseconds(1));

  // kDeferred = 1
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnected_TriggerDroppedDueToLimitReached) {
  constexpr size_t kMaxDeferredReceiversPerNavigation = 30;
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger)
        .Times(kMaxDeferredReceiversPerNavigation);
  }

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token, AttributionInputEvent());

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  std::vector<mojo::Remote<blink::mojom::AttributionDataHost>>
      trigger_data_hosts;
  for (size_t i = 0; i < kMaxDeferredReceiversPerNavigation + 2; ++i) {
    mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
    data_host_manager_.RegisterDataHost(
        trigger_data_host_remote.BindNewPipeAndPassReceiver(),
        *SuitableOrigin::Deserialize("https://page2.example"),
        /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
        kFrameId, /*last_navigation_id=*/kNavigationId);
    trigger_data_host_remote->TriggerDataAvailable(
        /*reporting_origin=*/*SuitableOrigin::Deserialize(
            "https://report.test"),
        TriggerRegistration(), /*verifications=*/{});
    trigger_data_hosts.emplace_back(std::move(trigger_data_host_remote));
  }
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  source_data_host_remote.reset();
  task_environment_.FastForwardBy(base::TimeDelta());
  // kDeferred = 1, kDropped = 2
  histograms.ExpectBucketCount(kRegisterDataHostOutcomeHistogram, 1,
                               kMaxDeferredReceiversPerNavigation);
  histograms.ExpectBucketCount(kRegisterDataHostOutcomeHistogram, 2, 2);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnectedAndRedirect_TriggerDelayed) {
  base::HistogramTester histograms;
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto reporting_origin = *SuitableOrigin::Deserialize("https://report.test");
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  // 1 - There is a background attribution request
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token, AttributionInputEvent());

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, source_origin,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(), source_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/reporting_origin, TriggerRegistration(),
      /*verifications=*/{});

  // 2 - On the main navigation, a source is registered.
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporting_origin, source_origin,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/true);

  // 3 - The background attribution request completes
  source_data_host_remote.reset();

  // 4- We are still parsing the foreground registration headers, so the trigger
  //    should not have been processed yet.
  checkpoint.Call(1);

  // 5 - Parsing completes, the trigger gets handled.
  task_environment_.RunUntilIdle();

  // kDeferred = 1
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnected_TriggerDelayedUntilTimeout) {
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

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});

  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample(
      "Conversions.DeferredDataHostProcessedAfterTimeout", true, 1);
  // kDeferred = 1
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnected_MultipleTriggersDelayedUntilTimeout) {
  base::HistogramTester histograms;
  Checkpoint checkpoint;
  {
    InSequence seq;

    // Only the second registered should have been handled
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);
  }

  // First
  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token, AttributionInputEvent());
  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, kFrameId, /*navigation_id=*/1);
  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/1);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});

  // Second
  const blink::AttributionSrcToken attribution_src_token_2;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote_2;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote_2.BindNewPipeAndPassReceiver(),
      attribution_src_token_2, AttributionInputEvent());
  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token_2,
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, kFrameId, /*navigation_id=*/2);
  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote_2;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote_2.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/2);
  trigger_data_host_remote_2->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});

  // Third
  task_environment_.FastForwardBy(base::Seconds(5));

  const blink::AttributionSrcToken attribution_src_token_3;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote_3;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote_3.BindNewPipeAndPassReceiver(),
      attribution_src_token_3, AttributionInputEvent());
  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token_3,
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, kFrameId, /*navigation_id=*/3);
  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote_3;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote_3.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/3);
  trigger_data_host_remote_3->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});

  source_data_host_remote_2.reset();
  task_environment_.FastForwardBy(base::Microseconds(1));
  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Seconds(5));
  checkpoint.Call(2);

  task_environment_.FastForwardBy(base::Seconds(5));

  histograms.ExpectBucketCount(
      "Conversions.DeferredDataHostProcessedAfterTimeout", true, 2);
  histograms.ExpectBucketCount(
      "Conversions.DeferredDataHostProcessedAfterTimeout", false, 1);
  // kDeferred = 1
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 1, 3);
  histograms.ExpectTimeBucketCount(kProcessRegisterDataHostDelayHistogram,
                                   base::Seconds(5), 1);
  histograms.ExpectTimeBucketCount(kProcessRegisterDataHostDelayHistogram,
                                   base::Seconds(10), 2);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnected_TriggerNotDelayed) {
  EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token, AttributionInputEvent());

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  // The trigger is linked to a different navigation id, so it should not be
  // deferred.
  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/2);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});
  trigger_data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_SourceRegisteredBeforeNav) {
  EXPECT_CALL(mock_manager_, HandleSource);

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, source_site,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, /*headers=*/nullptr, reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/true);
}

TEST_F(AttributionDataHostManagerImplTest,
       CrossAppWebRuntimeDisabled_OsSourceNotRegistered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x")");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      // The cross to web runtime feature defaults to false.
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest, NavigationRedirectOsSource) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(
      mock_manager_,
      HandleOsRegistration(
          OsRegistration(GURL("https://r.test/x"), /*debug_reporting=*/false,
                         *source_site, AttributionInputEvent(),
                         /*is_within_fenced_frame=*/false),
          kFrameId));

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x")");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      {network::AttributionReportingRuntimeFeature::kCrossAppWeb},
      /*is_final_response=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectOsSource_InvalidOsHeader) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader, "!");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      {network::AttributionReportingRuntimeFeature::kCrossAppWeb},
      /*is_final_response=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest, NavigationRedirectOsSource_InOrder) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  {
    InSequence seq;

    EXPECT_CALL(
        mock_manager_,
        HandleOsRegistration(
            OsRegistration(GURL("https://b.test/"), /*debug_reporting=*/true,
                           *source_site, AttributionInputEvent(),
                           /*is_within_fenced_frame=*/false),
            kFrameId));
    EXPECT_CALL(
        mock_manager_,
        HandleOsRegistration(
            OsRegistration(GURL("https://a.test/"), /*debug_reporting=*/false,
                           *source_site, AttributionInputEvent(),
                           /*is_within_fenced_frame=*/true),
            kFrameId));
  }

  const blink::AttributionSrcToken attribution_src_token;

  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://b.test/"; debug-reporting)");

    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporter, source_site,
        AttributionInputEvent(),
        /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
        {network::AttributionReportingRuntimeFeature::kCrossAppWeb},
        /*is_final_response=*/false);
  }

  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://a.test/")");

    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporter, source_site,
        AttributionInputEvent(),
        /*is_within_fenced_frame=*/true, kFrameId, kNavigationId,
        {network::AttributionReportingRuntimeFeature::kCrossAppWeb},
        /*is_final_response=*/false);
  }

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectOsSource_WebAndOsHeaders) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  const auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x")");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      {network::AttributionReportingRuntimeFeature::kCrossAppWeb},
      /*is_final_response=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_NavigationFinishesBeforeParsing) {
  EXPECT_CALL(mock_manager_, HandleSource);

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest, NavigationRedirectSource_InOrder) {
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_,
                HandleSource(SourceRegistrationIs(SourceRegistrationMatches(
                                 SourceRegistrationMatcherConfig(
                                     /*source_event_id=*/2))),
                             kFrameId));
    EXPECT_CALL(mock_manager_,
                HandleSource(SourceRegistrationIs(SourceRegistrationMatches(
                                 SourceRegistrationMatcherConfig(
                                     /*source_event_id=*/1))),
                             kFrameId));
  }

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;

  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(
        kAttributionReportingRegisterSourceHeader,
        R"json({"source_event_id":"2","destination":"https://dest.test"})json");

    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporter, source_site,
        AttributionInputEvent(),
        /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
        network::AttributionReportingRuntimeFeatures(),
        /*is_final_response=*/false);
  }

  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(
        kAttributionReportingRegisterSourceHeader,
        R"json({"source_event_id":"1","destination":"https://dest.test"})json");

    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporter, source_site,
        AttributionInputEvent(),
        /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
        network::AttributionReportingRuntimeFeatures(),
        /*is_final_response=*/false);
  }

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ParsingFinishesBeforeAndAfterNav) {
  EXPECT_CALL(mock_manager_,
              HandleSource(SourceIsWithinFencedFrameIs(false), kFrameId))
      .Times(2);

  auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, source_site,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);

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
                                 SourceType::kNavigation,
                                 SourceRegistrationError::kInvalidJson));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, source_site,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     "!!!invalid json");
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  histograms.ExpectUniqueSample("Conversions.SourceRegistrationError3",
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
  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, source_site,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);
  trigger_data_host_remote->TriggerDataAvailable(reporter,
                                                 TriggerRegistration(),
                                                 /*verifications=*/{});
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, /*headers=*/nullptr, reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/true);
  // The redirection source is still being processed by the data decoder. The
  // registration is also linked to the trigger registration last navigation as
  // such, the trigger should be delayed until the source is done processing.

  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::TimeDelta());
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

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, source_site,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);

  // Wait for parsing.
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});
  trigger_data_host_remote.FlushForTesting();
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

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter, source_site,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);

  // Wait for parsing.
  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, source_site,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});
  trigger_data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, TwoTriggerReceivers) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger).Times(2);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote1;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote1.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote2;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote2.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  auto reporting_origin = *SuitableOrigin::Deserialize("https://report.test");

  TriggerRegistration trigger_data;

  trigger_data_host_remote1->TriggerDataAvailable(
      reporting_origin, trigger_data, /*verifications=*/{});
  trigger_data_host_remote2->TriggerDataAvailable(std::move(reporting_origin),
                                                  std::move(trigger_data),
                                                  /*verifications=*/{});

  trigger_data_host_remote1.FlushForTesting();
  trigger_data_host_remote2.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnectsFails_TriggerNotDelayed) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger);

  auto reporting_origin = *SuitableOrigin::Deserialize("https://report.test");
  auto source_origin = *SuitableOrigin::Deserialize("https://page2.example");

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token, AttributionInputEvent());
  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(), source_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  // `AttributionDataHostManager::NotifyNavigationRegistrationStartedForDataHost()`
  // is not called, therefore the data host is not bound.

  task_environment_.FastForwardBy(base::Milliseconds(1));

  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, /*headers=*/nullptr, reporting_origin,
      source_origin, AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/true);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});
  trigger_data_host_remote.FlushForTesting();

  // kRegistered = 0, kNavigationFailed = 2.
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 0, 1);
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 2, 1);
  // kImmediately = 0
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 0, 1);
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
    EXPECT_CALL(
        mock_manager_,
        HandleTrigger(AttributionTriggerMatches(
                          AttributionTriggerMatcherConfig(reporting_origin1)),
                      kFrameId));
    EXPECT_CALL(
        mock_manager_,
        HandleTrigger(AttributionTriggerMatches(
                          AttributionTriggerMatcherConfig(reporting_origin2)),
                      kFrameId));
  }

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token, AttributionInputEvent());

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  auto send_trigger = [&](const SuitableOrigin& reporting_origin) {
    trigger_data_host_remote->TriggerDataAvailable(
        reporting_origin, TriggerRegistration(), /*verifications=*/{});
  };

  send_trigger(reporting_origin1);
  send_trigger(reporting_origin2);

  checkpoint.Call(1);

  source_data_host_remote.reset();
  trigger_data_host_remote.FlushForTesting();

  // kDeferred = 1
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnectsDisconnects_DelayedTriggersFlushed) {
  base::HistogramTester histograms;

  base::RunLoop loop;
  EXPECT_CALL(mock_manager_, HandleTrigger)
      .WillOnce([&](AttributionTrigger trigger,
                    GlobalRenderFrameHostId render_frame_id) { loop.Quit(); });

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page1.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(2));
  source_data_host_remote.reset();
  loop.Run();
}

TEST_F(AttributionDataHostManagerImplTest, NavigationDataHostNotRegistered) {
  base::HistogramTester histograms;

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page.example"),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  // kNotFound = 1.
  histograms.ExpectUniqueSample(kNavigationDataHostStatusHistogram, 1, 1);
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

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, *SuitableOrigin::Deserialize("https://s.test"),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  mojo::test::BadMessageObserver bad_message_observer;

  data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://r.test"),
      TriggerRegistration(), /*verifications=*/{});
  data_host_remote.FlushForTesting();

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "AttributionDataHost: Not eligible for trigger.");
}

TEST_F(AttributionDataHostManagerImplTest,
       DuplicateAttributionSrcToken_NotRegistered) {
  EXPECT_CALL(
      mock_manager_,
      HandleSource(SourceRegistrationIs(SourceRegistrationMatches(
                       SourceRegistrationMatcherConfig(/*source_event_id=*/1))),
                   kFrameId));

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
    histograms.ExpectUniqueSample(kNavigationDataHostStatusHistogram, 0, 1);
  }

  const auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token,
      *SuitableOrigin::Deserialize("https://page.example"),
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
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
  auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  EXPECT_CALL(
      mock_manager_,
      HandleSource(AllOf(SourceRegistrationIs(SourceRegistrationMatches(
                             SourceRegistrationMatcherConfig(
                                 /*source_event_id=*/10,
                                 *attribution_reporting::DestinationSet::Create(
                                     {destination_site})))),
                         SourceTypeIs(SourceType::kEvent),
                         ImpressionOriginIs(page_origin),
                         ReportingOriginIs(reporting_origin),
                         SourceIsWithinFencedFrameIs(true)),
                   kFrameId));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin,
      /*is_within_fenced_frame=*/true, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
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
                        /*is_within_fenced_frame=*/true)),
                    kFrameId));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), destination_origin,
      /*is_within_fenced_frame=*/true, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  data_host_remote->TriggerDataAvailable(
      reporting_origin, TriggerRegistration(), /*verifications=*/{});
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceWithinFencedFrame_SourceRegistered) {
  EXPECT_CALL(mock_manager_,
              HandleSource(SourceIsWithinFencedFrameIs(true), kFrameId));

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;

  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token,
      AttributionInputEvent());

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://source.test"),
      /*is_within_fenced_frame=*/true, kFrameId,
      /*navigation_id=*/kNavigationId);

  data_host_remote->SourceDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      SourceRegistration(*DestinationSet::Create(
          {net::SchemefulSite::Deserialize("https://destination.test")})));
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSourceWithinFencedFrame_SourceRegistered) {
  EXPECT_CALL(mock_manager_,
              HandleSource(SourceIsWithinFencedFrameIs(true), kFrameId));

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(),
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*source_origin=*/*SuitableOrigin::Deserialize("https://source.test"),
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/true, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/false);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest, NavigationBeaconSource_Registered) {
  EXPECT_CALL(mock_manager_, HandleSource);

  auto reporting_origin = url::Origin::Create(GURL("https://report.test"));
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId, source_origin,
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, network::AttributionReportingRuntimeFeatures(),
      reporting_origin, headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconOsSource_Registered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  auto reporting_origin = url::Origin::Create(GURL("https://report.test"));
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(
      mock_manager_,
      HandleOsRegistration(
          OsRegistration(GURL("https://r.test/x"), /*debug_reporting=*/false,
                         *source_origin, AttributionInputEvent(),
                         /*is_within_fenced_frame=*/false),
          kFrameId));

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x")");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, /*navigation_id=*/absl::nullopt, source_origin,
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, {network::AttributionReportingRuntimeFeature::kCrossAppWeb},
      reporting_origin, headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconSource_ParsingFailed) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto reporting_origin = url::Origin::Create(GURL("https://report.test"));
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_, NotifyFailedSourceRegistration(
                                 "!!!invalid json", source_origin,
                                 *SuitableOrigin::Create(reporting_origin),
                                 SourceType::kNavigation,
                                 SourceRegistrationError::kInvalidJson));

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId, source_origin, /*is_within_fenced_frame=*/false,
      AttributionInputEvent(), kFrameId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     "!!!invalid json");

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, network::AttributionReportingRuntimeFeatures(),
      reporting_origin, headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconSource_UntrustworthyReportingOrigin) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId, source_origin,
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/url::Origin::Create(GURL("http://insecure.test")),
      headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  // This is irrelevant to beacon source registrations.
  data_host_manager_.NotifyNavigationRegistrationStarted(
      blink::AttributionSrcToken(), source_origin,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconSource_TriggerDelayed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  // Because we are waiting for beacon data linked to the same navigation, the
  // trigger should be delayed.
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});
  // Leave time for the trigger to be processed (if it weren't delayed) to
  // enseure the test past because it is delayed.
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Seconds(2));
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/kBeaconId, network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  // kDeferred = 1
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 1, 1);
  histograms.ExpectTimeBucketCount(kProcessRegisterDataHostDelayHistogram,
                                   base::Seconds(2), 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       MultipleNavigationBeaconSource_TriggerDelayed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      BeaconId(2), kNavigationId,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);
  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      BeaconId(3), kNavigationId,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});

  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/kBeaconId, network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(2);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/BeaconId(2), network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/BeaconId(3), network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);

  task_environment_.FastForwardBy(base::TimeDelta());
  task_environment_.RunUntilIdle();

  // kDeferred = 1
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 1, 1);
  histograms.ExpectTimeBucketCount(kProcessRegisterDataHostDelayHistogram,
                                   base::Microseconds(0), 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       AllChannelsRegistrattions_TriggerDelayed) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto reporting_origin = *SuitableOrigin::Deserialize("https://report.test");
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;

  // 1 - A source is registered with a background attribution request
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token, AttributionInputEvent());

  data_host_manager_.NotifyNavigationRegistrationStarted(
      attribution_src_token, source_origin,
      /*is_within_fenced_frame=*/false, kFrameId,
      /*navigation_id=*/kNavigationId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(), source_origin,
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/reporting_origin, TriggerRegistration(),
      /*verifications=*/{});

  // 2 - A source is registered on the foreground navigation request
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporting_origin, source_origin,
      AttributionInputEvent(),
      /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
      network::AttributionReportingRuntimeFeatures(),
      /*is_final_response=*/true);

  // 3 - Sources can be registered via a Fenced Frame beacon
  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  // 4 - The background attribution request completes.
  source_data_host_remote.reset();
  checkpoint.Call(1);

  // 5 - The foreground registration completes
  task_environment_.RunUntilIdle();
  checkpoint.Call(2);

  // 6 - Beacon registrations complete, the trigger can now be registered.
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);
  task_environment_.RunUntilIdle();
}

TEST_F(AttributionDataHostManagerImplTest,
       MultipleNavigationBeaconSource_TriggerDelayedUntilTimeout) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);
  }

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      BeaconId(2), kNavigationId,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);
  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      BeaconId(3), kNavigationId,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), /*verifications=*/{});

  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/kBeaconId, network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);
  task_environment_.FastForwardBy(base::TimeDelta());
  // BeaconId(2) never gets called
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/BeaconId(3), network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);

  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(2);

  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample(
      "Conversions.DeferredDataHostProcessedAfterTimeout", true, 1);

  histograms.ExpectTimeBucketCount(kProcessRegisterDataHostDelayHistogram,
                                   base::Seconds(10), 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconSource_NavigationBeaconFinishedQueueSkipped) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource).Times(2);
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto reporting_origin = url::Origin::Create(GURL("https://report.test"));
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId, std::move(source_origin),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, network::AttributionReportingRuntimeFeatures(),
      reporting_origin, headers.get(),
      /*is_final_response=*/false);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, network::AttributionReportingRuntimeFeatures(),
      reporting_origin, headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing.
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  trigger_data_host_remote->TriggerDataAvailable(
      *SuitableOrigin::Create(std::move(reporting_origin)),
      TriggerRegistration(), /*verifications=*/{});
  trigger_data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconSource_NavigationBeaconFailedQueueSkipped) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger);

  auto reporting_origin = url::Origin::Create(GURL("https://report.test"));
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId, std::move(source_origin),
      /*is_within_fenced_frame=*/false, AttributionInputEvent(), kFrameId);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/url::Origin(), /*headers=*/nullptr,
      /*is_final_response=*/true);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      *SuitableOrigin::Deserialize("https://page2.example"),
      /*is_within_fenced_frame=*/false, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  trigger_data_host_remote->TriggerDataAvailable(
      *SuitableOrigin::Create(std::move(reporting_origin)),
      TriggerRegistration(), /*verifications=*/{});
  trigger_data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, EventBeaconSource_DataReceived) {
  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(SourceTypeIs(SourceType::kEvent),
                                 SourceIsWithinFencedFrameIs(true)),
                           kFrameId));

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, /*navigation_id=*/absl::nullopt,
      /*source_origin=*/*SuitableOrigin::Deserialize("https://source.test"),
      /*is_within_fenced_frame=*/true,
      /*input_event=*/AttributionInputEvent(), kFrameId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, network::AttributionReportingRuntimeFeatures(),
      /*reporting_origin=*/url::Origin::Create(GURL("https://report.test")),
      headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest, OsSourceAvailable) {
  const auto kTopLevelOrigin = *SuitableOrigin::Deserialize("https://a.test");
  const GURL kRegistrationUrl("https://b.test/x");

  EXPECT_CALL(mock_manager_,
              HandleOsRegistration(
                  OsRegistration(kRegistrationUrl, /*debug_reporting=*/true,
                                 *kTopLevelOrigin, AttributionInputEvent(),
                                 /*is_within_fenced_frame=*/true),
                  kFrameId));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), kTopLevelOrigin,
      /*is_within_fenced_frame=*/true, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  data_host_remote->OsSourceDataAvailable(
      {attribution_reporting::OsRegistrationItem{.url = kRegistrationUrl,
                                                 .debug_reporting = true}});
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, OsTriggerAvailable) {
  const auto kTopLevelOrigin = *SuitableOrigin::Deserialize("https://a.test");
  const GURL kRegistrationUrl("https://b.test/x");

  EXPECT_CALL(
      mock_manager_,
      HandleOsRegistration(
          OsRegistration(
              kRegistrationUrl, /*debug_reporting=*/true, *kTopLevelOrigin,
              /*input_event=*/absl::nullopt, /*is_within_fenced_frame=*/true),
          kFrameId));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), kTopLevelOrigin,
      /*is_within_fenced_frame=*/true, RegistrationType::kSourceOrTrigger,
      kFrameId, /*last_navigation_id=*/kNavigationId);

  data_host_remote->OsTriggerDataAvailable(
      {attribution_reporting::OsRegistrationItem{.url = kRegistrationUrl,
                                                 .debug_reporting = true}});
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, WebDisabled_SourceNotRegistered) {
  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client, IsWebAttributionReportingAllowed())
      .WillRepeatedly(testing::Return(false));
  ScopedContentBrowserClientSetting setting(&browser_client);

  const auto reporter = *SuitableOrigin::Deserialize("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  for (auto state : {AttributionOsLevelManager::ApiState::kDisabled,
                     AttributionOsLevelManager::ApiState::kEnabled}) {
    AttributionOsLevelManager::ScopedApiStateForTesting
        scoped_api_state_setting(state);

    EXPECT_CALL(mock_manager_, HandleSource).Times(0);

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);

    const blink::AttributionSrcToken attribution_src_token;
    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporter, source_site,
        AttributionInputEvent(),
        /*is_within_fenced_frame=*/false, kFrameId, kNavigationId,
        network::AttributionReportingRuntimeFeatures(),
        /*is_final_response=*/false);
    // Wait for parsing to finish.
    task_environment_.FastForwardBy(base::TimeDelta());
  }
}

}  // namespace
}  // namespace content
