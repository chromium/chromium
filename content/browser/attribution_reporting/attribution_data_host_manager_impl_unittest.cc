// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/data_host.mojom.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/os_registration_error.mojom-shared.h"
#include "components/attribution_reporting/registrar.h"
#include "components/attribution_reporting/registration_eligibility.mojom.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/registration_info.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-shared.h"
#include "content/browser/attribution_reporting/attribution_background_registrations_id.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/attribution_reporting/test/mock_content_browser_client.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_response_headers.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::DestinationSet;
using ::attribution_reporting::FilterPair;
using ::attribution_reporting::OsRegistrationItem;
using ::attribution_reporting::OsSourceRegistrationError;
using ::attribution_reporting::OsTriggerRegistrationError;
using ::attribution_reporting::RegistrationInfoError;
using ::attribution_reporting::SourceRegistration;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::TriggerRegistration;
using ::attribution_reporting::mojom::OsRegistrationError;
using ::attribution_reporting::mojom::RegistrationEligibility;
using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::network::mojom::AttributionSupport;

using AttributionFilters = ::attribution_reporting::FiltersDisjunction;
using FilterConfig = ::attribution_reporting::FilterConfig;

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

using Checkpoint = ::testing::MockFunction<void(int step)>;

constexpr char kRegisterSourceJson[] =
    R"json({"source_event_id":"5","destination":"https://destination.example"})json";

constexpr char kRegisterTriggerJson[] =
    R"json({ "event_trigger_data":[{"trigger_data":"6"}] })json";

constexpr char kNavigationDataHostStatusHistogram[] =
    "Conversions.NavigationDataHostStatus3";

constexpr char kRegisterDataHostOutcomeHistogram[] =
    "Conversions.RegisterDataHostOutcome";

constexpr char kProcessRegisterDataHostDelayHistogram[] =
    "Conversions.ProcessRegisterDataHostDelay";

constexpr char kNavigationUnexpectedRegistrationHistogram[] =
    "Conversions.NavigationUnexpectedRegistration";

constexpr char kBackgroundNavigationOutcome[] =
    "Conversions.BackgroundNavigation.Outcome";

constexpr char kRegistrationMethod[] = "Conversions.RegistrationMethod2";

using attribution_reporting::kAttributionReportingRegisterOsSourceHeader;
using attribution_reporting::kAttributionReportingRegisterOsTriggerHeader;
using attribution_reporting::kAttributionReportingRegisterSourceHeader;
using attribution_reporting::kAttributionReportingRegisterTriggerHeader;

const GlobalRenderFrameHostId kFrameId = {0, 1};
constexpr attribution_reporting::Registrar kRegistrar =
    attribution_reporting::Registrar::kWeb;

constexpr BeaconId kBeaconId(123);
constexpr int64_t kNavigationId(456);
constexpr int64_t kLastNavigationId(1234);
constexpr char kDevtoolsRequestId[] = "devtools-request-id-1";
constexpr BackgroundRegistrationsId kBackgroundId(789);

constexpr bool kViaServiceWorker = false;
constexpr bool kIsForBackgroundRequests = true;

// Value used to call `RegisterNavigationDataHost`. It is inconsequential unless
// kKeepAliveInBrowserMigration is enabled and background registrations are
// received.
constexpr size_t kExpectedRegistrations = 1;

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
  base::HistogramTester histograms;

  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");
  auto aggregation_keys = *attribution_reporting::AggregationKeys::FromKeys(
      {{"key", absl::MakeUint128(/*high=*/5, /*low=*/345)}});

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
  source_data.source_event_id = 10;
  source_data.priority = 20;
  source_data.debug_key = 789;
  source_data.aggregation_keys = aggregation_keys;
  source_data.debug_reporting = true;

  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(SourceRegistrationIs(source_data),
                                 SourceTypeIs(SourceType::kEvent),
                                 ImpressionOriginIs(page_origin),
                                 ReportingOriginIs(reporting_origin),
                                 SourceIsWithinFencedFrameIs(false)),
                           kFrameId))
      .Times(2);

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger,
      /*is_for_background_requests=*/true);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  data_host_remote->SourceDataAvailable(
      reporting_origin, source_data, /*was_fetched_via_service_worker=*/true);
  data_host_remote->SourceDataAvailable(
      reporting_origin, source_data, /*was_fetched_via_service_worker=*/false);
  data_host_remote.FlushForTesting();

  // kBackgroundBlink = 8, kBackgroundBlinkViaSW = 9
  histograms.ExpectBucketCount(kRegistrationMethod, 8, 1);
  histograms.ExpectBucketCount(kRegistrationMethod, 9, 1);
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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  source_data.destination_set = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://other-trigger.example")});
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(3);
  data_host_remote->SourceDataAvailable(
      std::move(reporting_origin), std::move(source_data), kViaServiceWorker);
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, TriggerDataHost_TriggerRegistered) {
  base::HistogramTester histograms;

  auto destination_origin =
      *SuitableOrigin::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  auto filters = AttributionFilters({*FilterConfig::Create({{"a", {"b"}}})});
  FilterPair event_trigger_data_filters(
      /*positive=*/{*FilterConfig::Create({{"c", {"d"}}})},
      /*negative=*/{*FilterConfig::Create({{"e", {"f"}}})});

  std::vector<attribution_reporting::AggregatableDedupKey>
      aggregatable_dedup_keys = {attribution_reporting::AggregatableDedupKey(
          /*dedup_key=*/123, FilterPair())};

  TriggerRegistration trigger_data;
  trigger_data.debug_key = 789;
  trigger_data.filters.positive = filters;
  trigger_data.event_triggers = {
      attribution_reporting::EventTriggerData(
          /*data=*/1, /*priority=*/2,
          /*dedup_key=*/3, event_trigger_data_filters),
      attribution_reporting::EventTriggerData(
          /*data=*/4, /*priority=*/5,
          /*dedup_key=*/std::nullopt, FilterPair())};

  trigger_data.aggregatable_dedup_keys = aggregatable_dedup_keys;
  trigger_data.debug_reporting = true;
  trigger_data.aggregation_coordinator_origin =
      SuitableOrigin::Deserialize("https://coordinator.test");

  EXPECT_CALL(
      mock_manager_,
      HandleTrigger(
          AttributionTrigger(reporting_origin, trigger_data, destination_origin,
                             /*is_within_fenced_frame=*/false),
          kFrameId))
      .Times(2);

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          destination_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger,
      /*is_for_background_requests=*/true);

  data_host_remote->TriggerDataAvailable(
      reporting_origin, trigger_data,
      /*was_fetched_via_service_worker=*/true);
  data_host_remote->TriggerDataAvailable(
      reporting_origin, trigger_data,
      /*was_fetched_via_service_worker=*/false);
  data_host_remote.FlushForTesting();

  // kBackgroundBlink = 8, kBackgroundBlinkViaSW = 9
  histograms.ExpectBucketCount(kRegistrationMethod, 8, 1);
  histograms.ExpectBucketCount(kRegistrationMethod, 9, 1);
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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          destination_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kTrigger, /*is_for_background_requests=*/false);

  TriggerRegistration trigger_data;

  data_host_remote->TriggerDataAvailable(reporting_origin, trigger_data,
                                         kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->TriggerDataAvailable(reporting_origin, trigger_data,
                                         kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  {
    mojo::test::BadMessageObserver bad_message_observer;

    SourceRegistration source_data(
        *DestinationSet::Create({net::SchemefulSite(destination_origin)}));

    data_host_remote->SourceDataAvailable(
        reporting_origin, std::move(source_data), kViaServiceWorker);
    data_host_remote.FlushForTesting();

    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "DataHost: Not eligible for source.");
  }

  checkpoint.Call(3);

  data_host_remote->TriggerDataAvailable(
      std::move(reporting_origin), std::move(trigger_data),
      /*was_fetched_via_service_worker=*/true);
  data_host_remote.FlushForTesting();

  // kForegroundBlink = 6, kForegroundBlinkViaSW = 7
  histograms.ExpectBucketCount(kRegistrationMethod, 6, 2);
  histograms.ExpectBucketCount(kRegistrationMethod, 7, 1);
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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));

  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  data_host_remote->TriggerDataAvailable(
      reporting_origin, TriggerRegistration(), kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(3);

  data_host_remote->SourceDataAvailable(
      std::move(reporting_origin), std::move(source_data), kViaServiceWorker);
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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSource, kIsForBackgroundRequests);

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));

  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  {
    mojo::test::BadMessageObserver bad_message_observer;

    data_host_remote->TriggerDataAvailable(
        reporting_origin, TriggerRegistration(), kViaServiceWorker);
    data_host_remote.FlushForTesting();

    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "DataHost: Not eligible for trigger.");
  }

  checkpoint.Call(3);

  data_host_remote->SourceDataAvailable(
      std::move(reporting_origin), std::move(source_data), kViaServiceWorker);
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_InvalidForSourceType) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSource, kIsForBackgroundRequests);

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
  // Non-whole-day expiry is invalid for `SourceType::kEvent`.
  source_data.expiry = base::Days(1) + base::Microseconds(1);
  source_data.aggregatable_report_window = source_data.expiry;
  source_data.trigger_specs = attribution_reporting::TriggerSpecs(
      SourceType::kEvent,
      *attribution_reporting::EventReportWindows::FromDefaults(
          source_data.expiry, SourceType::kEvent),
      attribution_reporting::MaxEventLevelReports());

  {
    mojo::test::BadMessageObserver bad_message_observer;

    data_host_remote->SourceDataAvailable(
        reporting_origin, std::move(source_data), kViaServiceWorker);
    data_host_remote.FlushForTesting();

    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "DataHost: Source invalid for source type.");
  }
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

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
  source_data.source_event_id = 10;
  source_data.priority = 20;
  source_data.debug_key = 789;
  source_data.aggregation_keys = aggregation_keys;
  source_data.debug_reporting = true;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_,
                HandleSource(AllOf(SourceRegistrationIs(source_data),
                                   SourceTypeIs(SourceType::kNavigation),
                                   ImpressionOriginIs(page_origin),
                                   ReportingOriginIs(reporting_origin),
                                   SourceIsWithinFencedFrameIs(false)),
                             kFrameId));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
  }

  const blink::AttributionSrcToken attribution_src_token;

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false,
          /*root_render_frame_id=*/kFrameId,
          /*last_navigation_id=*/kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  data_host_remote->SourceDataAvailable(
      reporting_origin, source_data,
      /*was_fetched_via_service_worker=*/true);
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  // This should succeed even though the destination site doesn't match the
  // final navigation site.
  source_data.destination_set = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger2.example")});
  data_host_remote->SourceDataAvailable(
      reporting_origin, source_data,
      /*was_fetched_via_service_worker=*/false);
  data_host_remote.FlushForTesting();

  // kRegistered = 0, kProcessed = 3.
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 0, 1);
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 3, 1);

  // kNavBackgroundBlink = 1, kNavBackgroundBlinkViaSW = 2
  histograms.ExpectBucketCount(kRegistrationMethod, /*sample=*/1, 1);
  histograms.ExpectBucketCount(kRegistrationMethod, /*sample=*/2, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       UnexpectedNavigationRegistrationsPatterns) {
  base::HistogramTester histograms;

  auto reporting_url = GURL("https://report.test");
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;

  // 1 - An initial navigation registration starts.
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  // 2 - A second navigation registrations, with the same
  // attribution_src_token,
  //     starts. It should be ignored.
  const int64_t second_navigation_id(878);  // different nav-id, the identity
                                            // is based only on the token.
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token,
      /*navigation_id=*/second_navigation_id, kDevtoolsRequestId);
  // kRegistrationAlreadyExists = 0
  histograms.ExpectBucketCount(kNavigationUnexpectedRegistrationHistogram,
                               /*sample=*/0, /*expected_count=*/1);

  // 3 - We register some data, the foreground navigation is still active, it
  //     should be successful.
  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
    EXPECT_CALL(mock_manager_, HandleSource).Times(1);
    EXPECT_TRUE(data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url));
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  // 4 - The first navigation finishes.
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  // 5 - The second navigation tries to register data, it should be ignored.
  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
    EXPECT_FALSE(data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url));
    // kRegistrationMissingUponReceivingData = 1
    histograms.ExpectBucketCount(kNavigationUnexpectedRegistrationHistogram,
                                 /*sample=*/1, /*expected_count=*/1);
  }

  // 6 - The second navigation completes, it should be ignored.
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);

  SourceRegistration source_data(*DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger.example")}));
  source_data.source_event_id = 10;
  source_data.priority = 20;
  source_data.debug_key = 789;
  source_data.aggregation_keys =
      *attribution_reporting::AggregationKeys::FromKeys(
          {{"key", absl::MakeUint128(/*high=*/5, /*low=*/345)}});
  source_data.debug_reporting = true;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  // This should succeed even though the destination site doesn't match the
  // final navigation site.
  source_data.destination_set = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger2.example")});
  data_host_remote->SourceDataAvailable(
      reporting_origin, std::move(source_data), kViaServiceWorker);

  data_host_remote.reset();

  task_environment_.FastForwardBy(base::Milliseconds(1));

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  task_environment_.RunUntilIdle();

  // kRegistered = 0, kProcessed = 3.
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 0, 1);
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 3, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHostRegistration_RegistrationsPerNavigationRecorded) {
  base::HistogramTester histograms;

  const auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);

  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(SourceTypeIs(SourceType::kNavigation),
                                 ImpressionOriginIs(page_origin),
                                 ReportingOriginIs(reporting_origin)),
                           kFrameId))
      .Times(3);

  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(SourceTypeIs(SourceType::kEvent),
                                 ImpressionOriginIs(page_origin),
                                 ReportingOriginIs(reporting_origin),
                                 SourceIsWithinFencedFrameIs(true)),
                           kFrameId));

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;

  SourceRegistration source_data(*DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger.example")}));
  source_data.source_event_id = 10;

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);

  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  source_data.destination_set = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger2.example")});
  data_host_remote->SourceDataAvailable(
      reporting_origin, std::move(source_data), kViaServiceWorker);

  data_host_remote.reset();

  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin, /*is_nested_within_fenced_frame=*/true, kFrameId,
          kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  source_data.destination_set = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger3.example")});
  data_host_remote->SourceDataAvailable(
      reporting_origin, std::move(source_data), kViaServiceWorker);

  data_host_remote.reset();

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, /*navigation_id=*/1, kDevtoolsRequestId);

  const blink::AttributionSrcToken attribution_src_token_2;
  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote_2;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote_2.BindNewPipeAndPassReceiver(), attribution_src_token_2);

  source_data.destination_set = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger4.example")});
  data_host_remote_2->SourceDataAvailable(
      reporting_origin, std::move(source_data), kViaServiceWorker);

  data_host_remote_2.reset();

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token_2, /*navigation_id=*/2, kDevtoolsRequestId);

  task_environment_.RunUntilIdle();
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  // kProcessed = 3.
  histograms.ExpectBucketCount(kNavigationDataHostStatusHistogram, 3, 2);

  histograms.ExpectBucketCount(
      "Conversions.NavigationSourceRegistrationsPerReportingOriginPerBatch", 2,
      1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceUniqueScopesSet_NoScopes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      attribution_reporting::features::kAttributionScopes);

  base::HistogramTester histograms;

  const auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);

  const auto attribution_scope_set =
      *attribution_reporting::AttributionScopesData::Create(
          attribution_reporting::AttributionScopesSet({"a", "b"}),
          /*attribution_scope_limit=*/2, /*max_event_states=*/3);

  constexpr char kRegisterSourceJsonWithScopesSet[] =
      R"json({"source_event_id":"6","destination":"https://destination.example","attribution_scopes":{"limit":2,"max_event_states":3,"values":["a","b"]}})json";
  constexpr char kRegisterSourceJsonNoScopes[] =
      R"json({"source_event_id":"5","destination":"https://destination.example"})json";

  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(1), _));
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(2), _))
      .Times(0);
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(3), _));
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(4), _));
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(5), _));
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(6), _))
      .Times(0);
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(7), _));

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;

  // 1 - Renderer registration, no scopes defined, should register properly.
  SourceRegistration source_data(*DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger.example")}));
  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, /*navigation_id=*/1, kDevtoolsRequestId);

  source_data.source_event_id = 1;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  // 2 - Renderer registration, same navigation, non-empty scopes, should be
  // dropped.
  source_data.source_event_id = 2;
  source_data.attribution_scopes_data = attribution_scope_set;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  // 3 - Renderer registration, same navigation, no scopes, should register
  // properly.
  source_data.source_event_id = 3;
  source_data.attribution_scopes_data = std::nullopt;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  data_host_remote.FlushForTesting();

  data_host_remote.reset();

  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin, /*is_nested_within_fenced_frame=*/true, kFrameId,
          kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  // 4 - Event registration, not tied to a navigation ID, should register
  // properly.
  source_data.source_event_id = 4;
  source_data.attribution_scopes_data = attribution_scope_set;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  data_host_remote.FlushForTesting();

  // 5 - Browser registration, same navigation, no scopes, should be registered.
  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJsonNoScopes);
    EXPECT_TRUE(data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url));
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  // 6 - Browser registration, same navigation, non-empty scopes, should be
  // dropped.
  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJsonWithScopesSet);
    EXPECT_TRUE(data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url));
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  data_host_remote.reset();

  // 7 - Renderer registration, different navigation, non-empty scopes, should
  // register properly.
  const blink::AttributionSrcToken attribution_src_token_2;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token_2);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token_2, /*navigation_id=*/2, kDevtoolsRequestId);

  source_data.source_event_id = 7;
  source_data.attribution_scopes_data = attribution_scope_set;
  data_host_remote->SourceDataAvailable(
      reporting_origin, std::move(source_data), kViaServiceWorker);

  data_host_remote.FlushForTesting();

  EXPECT_THAT(histograms.GetAllSamples(
                  "Conversions.NavigationSourceScopesLimitOutcome"),
              UnorderedElementsAre(base::Bucket(0, 3), base::Bucket(2, 1),
                                   base::Bucket(3, 2)));
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceUniqueScopesSet_WithScopes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      attribution_reporting::features::kAttributionScopes);

  base::HistogramTester histograms;

  const auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);

  const auto attribution_scope_set_1 =
      *attribution_reporting::AttributionScopesData::Create(
          attribution_reporting::AttributionScopesSet({"a", "b"}),
          /*attribution_scope_limit=*/2, /*max_event_states=*/3);
  const auto attribution_scope_set_2 =
      *attribution_reporting::AttributionScopesData::Create(
          attribution_reporting::AttributionScopesSet({"a"}),
          /*attribution_scope_limit=*/2, /*max_event_states=*/3);

  constexpr char kRegisterSourceJsonWithScopesSet1[] =
      R"json({"source_event_id":"6","destination":"https://destination.example","attribution_scopes":{"limit":2,"max_event_states":3,"values":["a","b"]}})json";
  constexpr char kRegisterSourceJsonWithScopesSet2[] =
      R"json({"source_event_id":"7","destination":"https://destination.example","attribution_scopes":{"limit":2,"max_event_states":3,"values":["a"]}})json";
  constexpr char kRegisterSourceJsonNoScopes[] =
      R"json({"source_event_id":"8","destination":"https://destination.example"})json";

  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(1), _));
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(2), _))
      .Times(0);
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(3), _))
      .Times(0);
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(4), _));
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(5), _));
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(6), _));
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(7), _))
      .Times(0);
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(8), _))
      .Times(0);
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(9), _));
  EXPECT_CALL(mock_manager_, HandleSource(RegistrationSourceEventIdIs(10), _));

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;

  // 1 - Renderer registration, non-empty scopes defined, should register
  // properly.
  SourceRegistration source_data(*DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://trigger.example")}));
  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, /*navigation_id=*/1, kDevtoolsRequestId);

  source_data.source_event_id = 1;
  source_data.attribution_scopes_data = attribution_scope_set_1;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  // 2 - Renderer registration, same navigation, different scopes, should be
  // dropped.
  source_data.source_event_id = 2;
  source_data.attribution_scopes_data = attribution_scope_set_2;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  // 3 - Renderer registration, same navigation, no scopes, should be dropped.
  source_data.source_event_id = 3;
  source_data.attribution_scopes_data.reset();
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  // 4 - Renderer registration, same navigation, same scopes as registration 1,
  // should register properly.
  source_data.source_event_id = 4;
  source_data.attribution_scopes_data = attribution_scope_set_1;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  data_host_remote.FlushForTesting();

  data_host_remote.reset();

  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin, /*is_nested_within_fenced_frame=*/true, kFrameId,
          kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  // 5 - Event registration, not tied to a navigation ID, should register
  // properly.
  source_data.source_event_id = 5;
  source_data.attribution_scopes_data = attribution_scope_set_2;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  data_host_remote.FlushForTesting();

  // 6 - Browser registration, same navigation, same scopes as the first source,
  // should register properly.
  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJsonWithScopesSet1);
    EXPECT_TRUE(data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url));
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  // 7 - Browser registration, same navigation, different scopes, should be
  // dropped.
  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJsonWithScopesSet2);
    EXPECT_TRUE(data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url));
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  // 8 - Browser registration, same navigation, no scopes, should be dropped.
  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJsonNoScopes);
    EXPECT_TRUE(data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url));
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  data_host_remote.reset();

  // 9 - Renderer registration, different navigation, different scopes, should
  // register properly.
  const blink::AttributionSrcToken attribution_src_token_2;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token_2);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token_2, /*navigation_id=*/2, kDevtoolsRequestId);

  source_data.source_event_id = 9;
  source_data.attribution_scopes_data = attribution_scope_set_2;
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  data_host_remote.FlushForTesting();
  data_host_remote.reset();

  // 10 - Renderer registration, different navigation, no scopes, should
  // register properly.
  const blink::AttributionSrcToken attribution_src_token_3;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token_3);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token_3, /*navigation_id=*/3, kDevtoolsRequestId);

  source_data.source_event_id = 10;
  source_data.attribution_scopes_data.reset();
  data_host_remote->SourceDataAvailable(reporting_origin, source_data,
                                        kViaServiceWorker);

  data_host_remote.FlushForTesting();

  EXPECT_THAT(histograms.GetAllSamples(
                  "Conversions.NavigationSourceScopesLimitOutcome"),
              UnorderedElementsAre(base::Bucket(0, 1), base::Bucket(1, 2),
                                   base::Bucket(2, 4), base::Bucket(3, 2)));
}

// Ensures correct behavior in
// `AttributionDataHostManagerImpl::OnDataHostDisconnected()` when a data host
// is registered but disconnects before registering a source or trigger.
TEST_F(AttributionDataHostManagerImplTest, NoSourceOrTrigger) {
  base::HistogramTester histograms;
  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  data_host_remote.reset();
  task_environment_.RunUntilIdle();

  // kImmediately = 0
  histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 0, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnected_TriggerNotDelayed) {
  constexpr RegistrationEligibility kTestCases[] = {
      RegistrationEligibility::kSourceOrTrigger,
      RegistrationEligibility::kSource,
  };

  for (auto registration_eligibility : kTestCases) {
    base::HistogramTester histograms;
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);

    mojo::Remote<attribution_reporting::mojom::DataHost>
        source_data_host_remote;
    data_host_manager_.RegisterDataHost(
        source_data_host_remote.BindNewPipeAndPassReceiver(),
        AttributionSuitableContext::CreateForTesting(
            *SuitableOrigin::Deserialize("https://page1.example"),
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        registration_eligibility, kIsForBackgroundRequests);

    mojo::Remote<attribution_reporting::mojom::DataHost>
        trigger_data_host_remote;
    data_host_manager_.RegisterDataHost(
        trigger_data_host_remote.BindNewPipeAndPassReceiver(),
        AttributionSuitableContext::CreateForTesting(
            *SuitableOrigin::Deserialize("https://page1.example"),
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

    task_environment_.FastForwardBy(base::Milliseconds(1));

    // Because the connected data host in source mode is not linked to
    // navigation this trigger should NOT be delayed.
    trigger_data_host_remote->TriggerDataAvailable(
        /*reporting_origin=*/*SuitableOrigin::Deserialize(
            "https://report.test"),
        TriggerRegistration(), kViaServiceWorker);

    task_environment_.FastForwardBy(base::TimeDelta());

    // kImmediately = 0
    histograms.ExpectUniqueSample(kRegisterDataHostOutcomeHistogram, 0, 2);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerModeReceiverConnected_TriggerNotDelayed) {
  EXPECT_CALL(mock_manager_, HandleTrigger);

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote1;
  data_host_manager_.RegisterDataHost(
      data_host_remote1.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kTrigger, kIsForBackgroundRequests);

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote2;
  data_host_manager_.RegisterDataHost(
      data_host_remote2.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kTrigger, kIsForBackgroundRequests);

  // Because there is no data host in source mode, this trigger should not be
  // delayed.
  data_host_remote2->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);

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
  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  // Because there is a connected data host in source mode, this trigger should
  // be delayed.
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);

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
  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token,
      /*navigation_id=*/kNavigationId, kDevtoolsRequestId);
  // We complete the foreground navigation immediately to avoid trigger being
  // delayed due to waiting on foreground registrations.
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  std::vector<mojo::Remote<attribution_reporting::mojom::DataHost>>
      trigger_data_hosts;
  for (size_t i = 0; i < kMaxDeferredReceiversPerNavigation + 2; ++i) {
    mojo::Remote<attribution_reporting::mojom::DataHost>
        trigger_data_host_remote;
    data_host_manager_.RegisterDataHost(
        trigger_data_host_remote.BindNewPipeAndPassReceiver(),
        AttributionSuitableContext::CreateForTesting(
            *SuitableOrigin::Deserialize("https://page2.example"),
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            /*last_navigation_id=*/kNavigationId),
        RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
    trigger_data_host_remote->TriggerDataAvailable(
        /*reporting_origin=*/*SuitableOrigin::Deserialize(
            "https://report.test"),
        TriggerRegistration(), kViaServiceWorker);
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

  auto reporting_url = GURL("https://report.test");
  auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  // 1 - There is a background attribution request
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/reporting_origin, TriggerRegistration(),
      kViaServiceWorker);

  // 2 - On the main navigation, a source is registered.
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporting_url);

  // 3 - The background attribution request completes
  source_data_host_remote.reset();

  // 4- We are still parsing the foreground registration headers, so the trigger
  // should not have been processed yet.
  checkpoint.Call(1);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

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
  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);

  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Seconds(20));
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
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);
  }

  // First
  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kLastNavigationId),
      attribution_src_token, /*navigation_id=*/1, kDevtoolsRequestId);
  // We complete the foreground navigation immediately to avoid trigger being
  // delayed due to waiting on foreground registrations.
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  source_data_host_remote.reset();
  task_environment_.FastForwardBy(base::TimeDelta());
  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/1),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);

  // The first trigger should be processed immediately as the previous
  // navigation has completed and had no sources registered alongside it.
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  // Second
  const blink::AttributionSrcToken attribution_src_token_2;
  mojo::Remote<attribution_reporting::mojom::DataHost>
      source_data_host_remote_2;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote_2.BindNewPipeAndPassReceiver(),
      attribution_src_token_2);
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kLastNavigationId),
      attribution_src_token_2, /*navigation_id=*/2, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token_2);
  mojo::Remote<attribution_reporting::mojom::DataHost>
      trigger_data_host_remote_2;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote_2.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/2),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  trigger_data_host_remote_2->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);

  // The second trigger should be delayed as the source datahost is still
  // connected, it could still receive more sources. The 20s timeouts also isn't
  // reached yet.
  task_environment_.FastForwardBy(base::Seconds(19));
  checkpoint.Call(2);

  // Third

  const blink::AttributionSrcToken attribution_src_token_3;
  mojo::Remote<attribution_reporting::mojom::DataHost>
      source_data_host_remote_3;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote_3.BindNewPipeAndPassReceiver(),
      attribution_src_token_3);
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kLastNavigationId),
      attribution_src_token_3, /*navigation_id=*/3, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token_3);
  mojo::Remote<attribution_reporting::mojom::DataHost>
      trigger_data_host_remote_3;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote_3.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/3),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  trigger_data_host_remote_3->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);

  // The third trigger registration should be delayed as the source data host
  // is still connected.
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(3);

  // Disconnecting the second source datahost should allow the second trigger to
  // be processed.
  source_data_host_remote_2.reset();
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(4);

  // We wait for the 20s timeout, this should allow the trigger to be processed
  // even if the datahost has not disconnected yet.
  task_environment_.FastForwardBy(base::Seconds(20));

  histograms.ExpectBucketCount(
      "Conversions.DeferredDataHostProcessedAfterTimeout", true, 1);
  histograms.ExpectBucketCount(
      "Conversions.DeferredDataHostProcessedAfterTimeout", false, 2);
  // kProcessedImmediately = 0, kDeferred = 1
  histograms.ExpectBucketCount(kRegisterDataHostOutcomeHistogram, 0, 1);
  histograms.ExpectBucketCount(kRegisterDataHostOutcomeHistogram, 1, 2);
  histograms.ExpectTimeBucketCount(kProcessRegisterDataHostDelayHistogram,
                                   base::Seconds(20), 2);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnected_TriggerNotDelayed) {
  EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  // The trigger is linked to a different navigation id, so it should not be
  // deferred.
  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/2),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);
  trigger_data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_SourceRegisteredBeforeNav) {
  EXPECT_CALL(mock_manager_, HandleSource);

  GURL reporter_url = GURL("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, /*headers=*/nullptr, reporter_url);
}

TEST_F(AttributionDataHostManagerImplTest,
       ClientOsAttributionDisabled_OsSourceNotRegistered) {
  base::test::ScopedFeatureList scoped_feature_list(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const GURL reporter_url("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x")");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId,
          AttributionInputEvent(),
          {ContentBrowserClient::AttributionReportingOsRegistrar::kDisabled,
           ContentBrowserClient::AttributionReportingOsRegistrar::kDisabled},
          /*attribution_data_host_manager=*/nullptr),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  EXPECT_TRUE(data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url));

  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest, NavigationRedirectOsSource) {
  base::HistogramTester histograms;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const GURL reporter_url("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_,
              HandleOsRegistration(OsRegistration(
                  {OsRegistrationItem(GURL("https://r.test/x"),
                                      /*debug_reporting=*/false),
                   OsRegistrationItem(GURL("https://r.test/y"),
                                      /*debug_reporting=*/false)},
                  *source_site, AttributionInputEvent(),
                  /*is_within_fenced_frame=*/false, kFrameId, kRegistrar)))
      .Times(1);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x", "https://r.test/y")");
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  // kNavForeground = 0
  histograms.ExpectBucketCount(kRegistrationMethod, 0, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectOsSource_InvalidOsHeader) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const GURL reporter_url("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader, "!");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectOsSource_WebAndOsHeaders) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  const GURL reporter_url("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x")");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       DataHost_NavigationTiedOsRegistrationsAreBuffered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kAttributionReportingCrossAppWeb}, {});
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    // foreground registrations done
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(0));

    // first background registrations received
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(1));

    // second background registrations received
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(2));

    // third and final registration received - all registrations in one call
    EXPECT_CALL(mock_manager_,
                HandleOsRegistration(
                    Field(&OsRegistration::registration_items, SizeIs(3))))
        .Times(1);
  }

  // The navigation starts, register data and completes.
  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers_2.get(), reporting_url);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(0);

  // A first source is received through the data host.
  data_host_remote->OsSourceDataAvailable(
      reporting_origin,
      {attribution_reporting::OsRegistrationItem{.url =
                                                     GURL("https://b.test/x")}},
      kViaServiceWorker);
  data_host_remote.FlushForTesting();
  checkpoint.Call(1);

  // A second source is received through the data host.
  data_host_remote->OsSourceDataAvailable(
      reporting_origin,
      {attribution_reporting::OsRegistrationItem{.url =
                                                     GURL("https://b.test/x")}},
      kViaServiceWorker);
  data_host_remote.FlushForTesting();
  checkpoint.Call(2);

  data_host_remote.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(AttributionDataHostManagerImplTest,
       FencedFrame_NavigationTiedOsRegistrationsAreBuffered) {
  base::HistogramTester histograms;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kAttributionReportingCrossAppWeb}, {});
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    // first source received
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(0));

    // second source received
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(1));

    // third and final registration received - all registrations in one call
    EXPECT_CALL(mock_manager_,
                HandleOsRegistration(
                    Field(&OsRegistration::registration_items, SizeIs(3))))
        .Times(1);
  }

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  // A first OS source is received.
  auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_1->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers_1.get(),
      /*is_final_response=*/false);
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(0);

  // A second OS source is received.
  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers_2.get(),
      /*is_final_response=*/false);
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  // A third and final OS source is received.
  auto headers_3 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_3->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers_3.get(),
      /*is_final_response=*/true);
  task_environment_.FastForwardBy(base::TimeDelta());

  // kNavigationDone = 0
  histograms.ExpectUniqueSample("Conversions.OsRegistrationsBufferFlushReason",
                                0, 1);
  histograms.ExpectUniqueSample(
      "Conversions.OsRegistrationsBufferWithSameContext", true, 2);
}

TEST_F(
    AttributionDataHostManagerImplTest,
    NavigationTiedOsRegistrationsAreBuffered_AfterTimeoutRegistrationsAreSentDirectlyToTheOS) {
  base::HistogramTester histograms;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kAttributionReportingCrossAppWeb}, {});
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    // first source received
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(0));

    // second source received
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(1));

    // no final response is received after the timeout
    EXPECT_CALL(mock_manager_,
                HandleOsRegistration(
                    Field(&OsRegistration::registration_items, SizeIs(2))))
        .Times(1);
    EXPECT_CALL(checkpoint, Call(2));

    // after the timeout, a final source is received
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(1);
  }

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  // A first OS source is received.
  auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_1->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers_1.get(),
      /*is_final_response=*/false);
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(0);

  // A second OS source is received.
  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers_2.get(),
      /*is_final_response=*/false);
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  // No final `is_final_response=true` data is received, yet after a timeout
  // duration of 30s, the two received sources should be processed.
  task_environment_.FastForwardBy(base::Seconds(20));
  task_environment_.FastForwardBy(base::TimeDelta());

  // kTimeout = 2
  histograms.ExpectUniqueSample("Conversions.OsRegistrationsBufferFlushReason",
                                2, 1);
  histograms.ExpectUniqueSample(
      "Conversions.OsRegistrationsBufferWithSameContext", true, 1);
  checkpoint.Call(2);

  // If any additional registrations are received past a timeout, they should
  // be processed without being buffered.
  auto headers_3 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_3->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers_3.get(),
      /*is_final_response=*/true);
  task_environment_.FastForwardBy(base::TimeDelta());
  histograms.ExpectBucketCount(
      "Conversions.OsRegistrationsSkipBufferRegistrationsSize",
      /*sample=*/1, /*expected_count=*/1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_NavigationFinishesBeforeParsing) {
  EXPECT_CALL(mock_manager_, HandleSource);

  const GURL reporter_url("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site, /*is_nested_within_fenced_frame=*/false, kFrameId,
          kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest, NavigationRedirectSource_InOrder) {
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_,
                HandleSource(SourceRegistrationIs(Field(
                                 &SourceRegistration::source_event_id, 2)),
                             kFrameId));
    EXPECT_CALL(mock_manager_,
                HandleSource(SourceRegistrationIs(Field(
                                 &SourceRegistration::source_event_id, 1)),
                             kFrameId));
  }

  const GURL reporter_url("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site, /*is_nested_within_fenced_frame=*/false, kFrameId,
          kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(
        kAttributionReportingRegisterSourceHeader,
        R"json({"source_event_id":"2","destination":"https://dest.test"})json");

    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporter_url);
  }

  {
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(
        kAttributionReportingRegisterSourceHeader,
        R"json({"source_event_id":"1","destination":"https://dest.test"})json");

    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporter_url);
  }

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ParsingFinishesBeforeAndAfterNav) {
  EXPECT_CALL(mock_manager_,
              HandleSource(SourceIsWithinFencedFrameIs(false), kFrameId))
      .Times(2);

  const GURL reporter_url("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site, /*is_nested_within_fenced_frame=*/false, kFrameId,
          kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ParsingFailsBeforeAndSucceedsAfterNav) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleSource).Times(1);

  const GURL reporter_url("https://report.test");
  const auto reporter = *SuitableOrigin::Create(reporter_url);
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site, /*is_nested_within_fenced_frame=*/false, kFrameId,
          kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     "!!!invalid json");
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  histograms.ExpectUniqueSample("Conversions.SourceRegistrationError13",
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

  const GURL reporter_url("https://report.test");
  const auto reporter = *SuitableOrigin::Create(reporter_url);
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site, /*is_nested_within_fenced_frame=*/false, kFrameId,
          kLastNavigationId),

      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  trigger_data_host_remote->TriggerDataAvailable(
      reporter, TriggerRegistration(), kViaServiceWorker);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, /*headers=*/nullptr, reporter_url);
  // We complete the foreground navigation immediately to avoid trigger being
  // delayed due to waiting on foreground registrations.
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
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

  const GURL reporter_url("https://report.test");
  auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site, /*is_nested_within_fenced_frame=*/false, kFrameId,
          kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);

  // Wait for parsing.
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);
  trigger_data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, TwoTriggerReceivers) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger).Times(2);

  mojo::Remote<attribution_reporting::mojom::DataHost>
      trigger_data_host_remote1;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote1.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  mojo::Remote<attribution_reporting::mojom::DataHost>
      trigger_data_host_remote2;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote2.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  auto reporting_origin = *SuitableOrigin::Deserialize("https://report.test");

  TriggerRegistration trigger_data;

  trigger_data_host_remote1->TriggerDataAvailable(
      reporting_origin, trigger_data, kViaServiceWorker);
  trigger_data_host_remote2->TriggerDataAvailable(
      std::move(reporting_origin), std::move(trigger_data), kViaServiceWorker);

  trigger_data_host_remote1.FlushForTesting();
  trigger_data_host_remote2.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnectsFails_TriggerNotDelayed) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger);

  const GURL reporting_url("https://report.test");
  auto source_origin = *SuitableOrigin::Deserialize("https://page2.example");

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);
  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  // `AttributionDataHostManager::NotifyNavigationRegistrationStarted()`
  // is not called, therefore the data host is not bound.

  task_environment_.FastForwardBy(base::Milliseconds(1));

  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, /*headers=*/nullptr, reporting_url);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);
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
    EXPECT_CALL(mock_manager_,
                HandleTrigger(Property(&AttributionTrigger::reporting_origin,
                                       reporting_origin1),
                              kFrameId));
    EXPECT_CALL(mock_manager_,
                HandleTrigger(Property(&AttributionTrigger::reporting_origin,
                                       reporting_origin2),
                              kFrameId));
  }

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  auto send_trigger = [&](const SuitableOrigin& reporting_origin) {
    trigger_data_host_remote->TriggerDataAvailable(
        reporting_origin, TriggerRegistration(), kViaServiceWorker);
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

  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page1.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(2));
  source_data_host_remote.reset();
  loop.Run();
}

TEST_F(AttributionDataHostManagerImplTest, NavigationDataHostNotRegistered) {
  base::HistogramTester histograms;

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token,
      /*navigation_id=*/kNavigationId, kDevtoolsRequestId);

  // kNotFound = 1.
  histograms.ExpectUniqueSample(kNavigationDataHostStatusHistogram, 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationDataHost_CannotRegisterTrigger) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://s.test"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  mojo::test::BadMessageObserver bad_message_observer;

  data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://r.test"),
      TriggerRegistration(), kViaServiceWorker);
  data_host_remote.FlushForTesting();

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "DataHost: Not eligible for trigger.");
}

TEST_F(AttributionDataHostManagerImplTest,
       DuplicateAttributionSrcToken_NotRegistered) {
  EXPECT_CALL(mock_manager_,
              HandleSource(SourceRegistrationIs(
                               Field(&SourceRegistration::source_event_id, 1)),
                           kFrameId));

  const blink::AttributionSrcToken attribution_src_token;

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote1,
      data_host_remote2;

  {
    base::HistogramTester histograms;

    EXPECT_TRUE(data_host_manager_.RegisterNavigationDataHost(
        data_host_remote1.BindNewPipeAndPassReceiver(), attribution_src_token));

    // This one should not be registered, as `attribution_src_token` is already
    // associated with a receiver.
    EXPECT_FALSE(data_host_manager_.RegisterNavigationDataHost(
        data_host_remote2.BindNewPipeAndPassReceiver(), attribution_src_token));

    // kRegistered = 0.
    histograms.ExpectUniqueSample(kNavigationDataHostStatusHistogram, 0, 1);
  }

  const auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
  source_data.source_event_id = 1;
  data_host_remote1->SourceDataAvailable(reporting_origin, source_data,
                                         kViaServiceWorker);
  data_host_remote1.FlushForTesting();

  source_data.source_event_id = 2;
  data_host_remote2->SourceDataAvailable(
      std::move(reporting_origin), std::move(source_data), kViaServiceWorker);
  data_host_remote2.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHostWithinFencedFrame_SourceRegistered) {
  auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  auto destination_site =
      net::SchemefulSite::Deserialize("https://trigger.example");
  auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(SourceTypeIs(SourceType::kEvent),
                                 ImpressionOriginIs(page_origin),
                                 ReportingOriginIs(reporting_origin),
                                 SourceIsWithinFencedFrameIs(true)),
                           kFrameId));

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin, /*is_nested_within_fenced_frame=*/true, kFrameId,
          kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  task_environment_.FastForwardBy(base::Milliseconds(1));

  SourceRegistration source_data(*DestinationSet::Create({destination_site}));
  source_data.source_event_id = 10;
  data_host_remote->SourceDataAvailable(
      reporting_origin, std::move(source_data), kViaServiceWorker);
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
      HandleTrigger(Property(&AttributionTrigger::is_within_fenced_frame, true),
                    kFrameId));

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          destination_origin,
          /*is_nested_within_fenced_frame=*/true, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  data_host_remote->TriggerDataAvailable(
      reporting_origin, TriggerRegistration(), kViaServiceWorker);
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceWithinFencedFrame_SourceRegistered) {
  EXPECT_CALL(mock_manager_,
              HandleSource(SourceIsWithinFencedFrameIs(true), kFrameId));

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;

  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://source.test"),
          /*is_nested_within_fenced_frame=*/true, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  data_host_remote->SourceDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      SourceRegistration(*DestinationSet::Create(
          {net::SchemefulSite::Deserialize("https://destination.test")})),
      kViaServiceWorker);
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
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://source.test"),
          /*is_nested_within_fenced_frame=*/true, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(),
      /*reporting_url=*/GURL("https://report.test"));
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest, NavigationBeaconSource_Registered) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleSource);

  auto reporting_url = GURL("https://report.test");
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          source_origin, /*is_nested_within_fenced_frame=*/false, kFrameId,
          kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  // kFencedFrameAutomaticBeacon = 5
  histograms.ExpectBucketCount(kRegistrationMethod, 5, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconOsSource_Registered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  auto reporting_url = GURL("https://report.test");
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_,
              HandleOsRegistration(OsRegistration(
                  {OsRegistrationItem(GURL("https://r.test/x"),
                                      /*debug_reporting=*/false)},
                  *source_origin, AttributionInputEvent(),
                  /*is_within_fenced_frame=*/false,
                  /*render_frame_id=*/kFrameId, kRegistrar)));

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x")");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      /*navigation_id=*/std::nullopt, kDevtoolsRequestId);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconSource_ParsingFailed) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  GURL reporting_url("https://report.test");
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          source_origin, /*is_nested_within_fenced_frame=*/false, kFrameId,
          kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     "!!!invalid json");

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconSource_UntrustworthyReportingOrigin) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),

      kNavigationId, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId,
      /*reporting_url=*/GURL("http://insecure.test"), headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  // This is irrelevant to beacon source registrations.
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      blink::AttributionSrcToken(), kNavigationId, kDevtoolsRequestId);
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
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://report.test"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  // Because we are waiting for beacon data linked to the same navigation, the
  // trigger should be delayed.
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);
  // Leave time for the trigger to be processed (if it weren't delayed) to
  // enseure the test past because it is delayed.
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  task_environment_.FastForwardBy(base::Seconds(2));
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/kBeaconId,
      /*reporting_url=*/GURL("https://report.test"),
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
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://report.test"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      BeaconId(2),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://report.test"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      BeaconId(3),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://report.test"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);

  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/kBeaconId,
      /*reporting_url=*/GURL("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(2);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/BeaconId(2),
      /*reporting_url=*/GURL("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/BeaconId(3),
      /*reporting_url=*/GURL("https://report.test"),
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

  const GURL reporting_url("https://report.test");
  auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  const blink::AttributionSrcToken attribution_src_token;

  // 1 - A source is registered with a background attribution request
  mojo::Remote<attribution_reporting::mojom::DataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/reporting_origin, TriggerRegistration(),
      kViaServiceWorker);

  // 2 - A source is registered on the foreground navigation request
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporting_url);

  // 3 - Sources can be registered via a Fenced Frame beacon
  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://report.test"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  // 4 - The background attribution request completes.
  source_data_host_remote.reset();
  checkpoint.Call(1);

  // 5 - The foreground registration completes
  task_environment_.RunUntilIdle();
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  checkpoint.Call(2);

  // 6 - Beacon registrations complete, the trigger can now be registered.
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url,
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
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://report.test"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      BeaconId(2),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://report.test"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      BeaconId(3),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://report.test"),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  trigger_data_host_remote->TriggerDataAvailable(
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://report.test"),
      TriggerRegistration(), kViaServiceWorker);

  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/kBeaconId,
      /*reporting_url=*/GURL("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);
  task_environment_.FastForwardBy(base::TimeDelta());
  // BeaconId(2) never gets called
  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      /*beacon_id=*/BeaconId(3),
      /*reporting_url=*/GURL("https://report.test"),
      /*headers=*/nullptr,
      /*is_final_response=*/true);

  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(2);

  task_environment_.FastForwardBy(base::Seconds(20));
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample(
      "Conversions.DeferredDataHostProcessedAfterTimeout", true, 1);

  histograms.ExpectTimeBucketCount(kProcessRegisterDataHostDelayHistogram,
                                   base::Seconds(20), 1);
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

  GURL reporting_url("https://report.test");
  auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          std::move(source_origin),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers.get(),
      /*is_final_response=*/false);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing.
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  trigger_data_host_remote->TriggerDataAvailable(
      *SuitableOrigin::Create(std::move(reporting_origin)),
      TriggerRegistration(), kViaServiceWorker);
  trigger_data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationBeaconSource_NavigationBeaconFailedQueueSkipped) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger);

  GURL reporting_url("https://report.test");
  auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          std::move(source_origin),
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId,
      /*reporting_url=*/GURL(), /*headers=*/nullptr,
      /*is_final_response=*/true);

  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  trigger_data_host_remote->TriggerDataAvailable(
      reporting_origin, TriggerRegistration(), kViaServiceWorker);
  trigger_data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, EventBeaconSource_DataReceived) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(SourceTypeIs(SourceType::kEvent),
                                 SourceIsWithinFencedFrameIs(true)),
                           kFrameId));

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://source.test"),
          /*is_nested_within_fenced_frame=*/true, kFrameId, kLastNavigationId),
      /*navigation_id=*/std::nullopt, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId,
      /*reporting_url=*/GURL("https://report.test"), headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  // kFencedFrameBeacon = 4
  histograms.ExpectBucketCount(kRegistrationMethod, 4, 1);
}

TEST_F(AttributionDataHostManagerImplTest, OsSourceAvailable) {
  const auto kTopLevelOrigin = *SuitableOrigin::Deserialize("https://a.test");
  const GURL kRegistrationUrl("https://b.test/x");

  MockAttributionReportingContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_CALL(browser_client, GetAttributionSupport)
      .WillOnce(Return(AttributionSupport::kOs));

  EXPECT_CALL(mock_manager_, HandleOsRegistration(OsRegistration(
                                 {OsRegistrationItem(kRegistrationUrl,
                                                     /*debug_reporting=*/true)},
                                 *kTopLevelOrigin, AttributionInputEvent(),
                                 /*is_within_fenced_frame=*/true,
                                 /*render_frame_id=*/kFrameId, kRegistrar)));

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          kTopLevelOrigin,
          /*is_nested_within_fenced_frame=*/true, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://report.test");

  // A call with no items should be ignored.
  data_host_remote->OsSourceDataAvailable(reporting_origin, {},
                                          kViaServiceWorker);

  data_host_remote->OsSourceDataAvailable(
      reporting_origin,
      {attribution_reporting::OsRegistrationItem{.url = kRegistrationUrl,
                                                 .debug_reporting = true}},
      kViaServiceWorker);
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, OsTriggerAvailable) {
  const auto kTopLevelOrigin = *SuitableOrigin::Deserialize("https://a.test");
  const GURL kRegistrationUrl("https://b.test/x");

  MockAttributionReportingContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_CALL(browser_client, GetAttributionSupport)
      .WillOnce(Return(AttributionSupport::kOs));

  EXPECT_CALL(
      mock_manager_,
      HandleOsRegistration(OsRegistration(
          {OsRegistrationItem(kRegistrationUrl, /*debug_reporting=*/true)},
          *kTopLevelOrigin,
          /*input_event=*/std::nullopt,
          /*is_within_fenced_frame=*/true, kFrameId, kRegistrar)));

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          kTopLevelOrigin,
          /*is_nested_within_fenced_frame=*/true, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://report.test");

  // A call with no items should be ignored.
  data_host_remote->OsTriggerDataAvailable(reporting_origin, {},
                                           kViaServiceWorker);

  data_host_remote->OsTriggerDataAvailable(
      reporting_origin,
      {attribution_reporting::OsRegistrationItem{.url = kRegistrationUrl,
                                                 .debug_reporting = true}},
      kViaServiceWorker);
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest, WebDisabled_SourceNotRegistered) {
  MockAttributionReportingContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  const GURL reporter_url("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  for (auto state : {AttributionOsLevelManager::ApiState::kDisabled,
                     AttributionOsLevelManager::ApiState::kEnabled}) {
    AttributionOsLevelManager::ScopedApiStateForTesting
        scoped_api_state_setting(state);

    EXPECT_CALL(mock_manager_, HandleSource).Times(0);

    if (state ==
        ContentBrowserClient::AttributionReportingOsApiState::kDisabled) {
      EXPECT_CALL(
          browser_client,
          GetAttributionSupport(
              ContentBrowserClient::AttributionReportingOsApiState::kDisabled,
              testing::_))
          .WillOnce(Return(AttributionSupport::kNone));
    } else if (state ==
               ContentBrowserClient::AttributionReportingOsApiState::kEnabled) {
      EXPECT_CALL(
          browser_client,
          GetAttributionSupport(
              ContentBrowserClient::AttributionReportingOsApiState::kEnabled,
              testing::_))
          .WillOnce(Return(AttributionSupport::kOs));
    }

    const blink::AttributionSrcToken attribution_src_token;
    data_host_manager_.NotifyNavigationRegistrationStarted(
        AttributionSuitableContext::CreateForTesting(
            source_site,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        attribution_src_token, kNavigationId, kDevtoolsRequestId);
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);

    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporter_url);

    data_host_manager_.NotifyNavigationRegistrationCompleted(
        attribution_src_token);

    // Wait for parsing to finish.
    task_environment_.FastForwardBy(base::TimeDelta());
  }
}

TEST_F(AttributionDataHostManagerImplTest, HeadersSize_SourceMetricsRecorded) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  auto reporting_url = GURL("https://report.test");
  auto source_origin = *SuitableOrigin::Deserialize("https://source.test");
  std::string_view os_registration(R"("https://r.test/x")");

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers.get(),
      /*is_final_response=*/true);
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterSource",
                                strlen(kRegisterSourceJson), 1);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  // OS registration
  headers->RemoveHeader(kAttributionReportingRegisterSourceHeader);
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     os_registration);

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          source_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      /*navigation_id=*/std::nullopt, kDevtoolsRequestId);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId, reporting_url, headers.get(),
      /*is_final_response=*/true);
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterOsSource",
                                os_registration.length(), 1);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

class AttributionDataHostManagerImplWithInBrowserMigrationTest
    : public AttributionDataHostManagerImplTest {
 public:
  explicit AttributionDataHostManagerImplWithInBrowserMigrationTest(
      std::vector<base::test::FeatureRef> enabled_features = {}) {
    enabled_features.emplace_back(
        blink::features::kKeepAliveInBrowserMigration);
    enabled_features.emplace_back(
        blink::features::kAttributionReportingInBrowserMigration);
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest
    : public AttributionDataHostManagerImplWithInBrowserMigrationTest {
 public:
  AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest()
      : AttributionDataHostManagerImplWithInBrowserMigrationTest(
            /*enabled_features=*/{
                network::features::kAttributionReportingCrossAppWeb}),
        scoped_api_state_setting_(
            AttributionOsLevelManager::ScopedApiStateForTesting(
                AttributionOsLevelManager::ApiState::kEnabled)) {}

 private:
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting_;
};

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       Background_NavigationTiedOsRegistrationsAreBuffered) {
  base::HistogramTester histograms;

  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    // first background registrations done
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(0));

    // foreground registrations done
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(1));

    // second background registrations done but still parsing
    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(mock_manager_,
                HandleOsRegistration(
                    Field(&OsRegistration::registration_items, SizeIs(3))))
        .Times(1);
  }

  // A background registration starts and completes without registering data.
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/1234),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);
  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);
  task_environment_.FastForwardBy(base::TimeDelta());

  // A background registration starts, registers data and completes.
  BackgroundRegistrationsId first_background_id(1111);
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      first_background_id,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/1234),
      RegistrationEligibility::kSourceOrTrigger, attribution_src_token,
      kDevtoolsRequestId);
  auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_1->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      first_background_id, headers_1.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(first_background_id);
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(0);

  // The navigation starts, register data and completes.
  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.NotifyNavigationWithBackgroundRegistrationsWillStart(
      attribution_src_token, /*expected_registrations=*/3);
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers_2.get(), reporting_url);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  // A third background registration starts, registers data and completes.
  BackgroundRegistrationsId second_background_id(2222);
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      second_background_id,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/1234),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);
  auto headers_3 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_3->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      second_background_id, headers_3.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(
      second_background_id);

  checkpoint.Call(2);

  task_environment_.FastForwardBy(base::TimeDelta());

  // kNavForeground = 0, kNavBackgroundBrowser = 3
  histograms.ExpectBucketCount(kRegistrationMethod, 0, 1);
  // Even if OS registrations are buffered, each data received should record.
  histograms.ExpectBucketCount(kRegistrationMethod, 3, 2);

  histograms.ExpectUniqueSample(
      "Conversions.OsRegistrationsBufferWithSameContext", true, 2);
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       BufferedNavigationTiedOsRegistrations_FlushedUponReachingLimit) {
  base::HistogramTester histograms;

  const blink::AttributionSrcToken attribution_src_token;

  const GURL reporting_url("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  static base::AtomicSequenceNumber unique_id_counter;
  const auto register_n_os_registrations = [&](size_t n) {
    std::vector<std::string> urls;
    for (size_t i = 0; i < n; ++i) {
      urls.push_back(
          base::JoinString({"\"https://", base::ToString(i), ".test\""}, ""));
    }
    BackgroundRegistrationsId id(unique_id_counter.GetNext());
    data_host_manager_.NotifyBackgroundRegistrationStarted(
        id,
        AttributionSuitableContext::CreateForTesting(
            context_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        RegistrationEligibility::kSource, attribution_src_token,
        kDevtoolsRequestId);
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       base::JoinString(urls, ", "));
    EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
        id, headers.get(), reporting_url));
    data_host_manager_.NotifyBackgroundRegistrationCompleted(id);
    task_environment_.FastForwardBy(base::TimeDelta());
  };

  Checkpoint checkpoint;
  {
    testing::InSequence seq;

    EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);
    EXPECT_CALL(checkpoint, Call(0));

    EXPECT_CALL(mock_manager_,
                HandleOsRegistration(
                    Field(&OsRegistration::registration_items, SizeIs(20))))
        .Times(1);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(mock_manager_,
                HandleOsRegistration(
                    Field(&OsRegistration::registration_items, SizeIs(20))))
        .Times(2);
    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(mock_manager_,
                HandleOsRegistration(
                    Field(&OsRegistration::registration_items, SizeIs(20))))
        .Times(1);
    EXPECT_CALL(checkpoint, Call(3));

    EXPECT_CALL(mock_manager_,
                HandleOsRegistration(
                    Field(&OsRegistration::registration_items, SizeIs(2))))
        .Times(1);
  }

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.NotifyNavigationWithBackgroundRegistrationsWillStart(
      attribution_src_token, /*expected_registrations=*/5);

  // The navigation starts, register data and completes.
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers_2.get(), reporting_url);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  task_environment_.FastForwardBy(base::TimeDelta());

  register_n_os_registrations(1);
  // Buffer not full yet, no registrations should have been received.
  checkpoint.Call(0);

  register_n_os_registrations(20);
  // First buffer is full, it should have processed a first batch of 80.
  checkpoint.Call(1);
  // kBufferFull = 1
  histograms.ExpectBucketCount("Conversions.OsRegistrationsBufferFlushReason",
                               /*sample=*/1, /*expected_count=*/1);

  register_n_os_registrations(39);
  // It should have filled the buffer twice and have processed two more batch.
  checkpoint.Call(2);

  register_n_os_registrations(19);
  // It should have filled a buffer exactly and flushed it.
  checkpoint.Call(3);

  // Fifth and last background registration received. It should process the
  // remaining items.
  register_n_os_registrations(2);

  // kNavigationDone = 0, kBufferFull = 1
  histograms.ExpectBucketCount("Conversions.OsRegistrationsBufferFlushReason",
                               /*sample=*/0, /*expected_count=*/1);
  histograms.ExpectBucketCount("Conversions.OsRegistrationsBufferFlushReason",
                               /*sample=*/1, /*expected_count=*/4);

  histograms.ExpectBucketCount("Conversions.OsRegistrationItemsPerBatch", 20,
                               /*expected_count=*/4);
  histograms.ExpectBucketCount("Conversions.OsRegistrationItemsPerBatch",
                               /*sample=*/2, /*expected_count=*/1);
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       HeadersSize_TriggerMetricsRecorded) {
  base::HistogramTester histograms;

  const auto reporting_url = GURL("https://report.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  // Web
  {
    data_host_manager_.NotifyBackgroundRegistrationStarted(
        kBackgroundId,
        AttributionSuitableContext::CreateForTesting(
            context_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        RegistrationEligibility::kTrigger,
        /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterTriggerHeader,
                       kRegisterTriggerJson);
    data_host_manager_.NotifyBackgroundRegistrationData(
        kBackgroundId, headers.get(), reporting_url);
    data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

    task_environment_.FastForwardBy(base::TimeDelta());

    histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterTrigger",
                                  strlen(kRegisterTriggerJson), 1);
  }

  // OS
  {
    std::string_view os_header_value(R"("https://r.test/x")");
    data_host_manager_.NotifyBackgroundRegistrationStarted(
        kBackgroundId,
        AttributionSuitableContext::CreateForTesting(
            context_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        RegistrationEligibility::kTrigger,
        /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterOsTriggerHeader,
                       os_header_value);
    data_host_manager_.NotifyBackgroundRegistrationData(
        kBackgroundId, headers.get(), reporting_url);

    data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

    task_environment_.FastForwardBy(base::TimeDelta());

    histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterOsTrigger",
                                  os_header_value.length(), 1);
  }
}

TEST_F(
    AttributionDataHostManagerImplWithInBrowserMigrationTest,
    Background_NavigationTiedOnOngoingNavigation_TriggerDeferredUntilBackgroundSourceRegistrationCompletes) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  Checkpoint checkpoint;
  base::HistogramTester histograms;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_,
                HandleSource(AllOf(SourceTypeIs(SourceType::kNavigation),
                                   ImpressionOriginIs(context_origin),
                                   ReportingOriginIs(reporting_origin),
                                   SourceIsWithinFencedFrameIs(false)),
                             kFrameId))
        .Times(2);
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  data_host_manager_.NotifyNavigationWithBackgroundRegistrationsWillStart(
      attribution_src_token, /*expected_registrations=*/2);

  BackgroundRegistrationsId second_background_id(101112);
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/1234),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      second_background_id,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/1234),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);

  // It should defer the trigger registration.
  BackgroundRegistrationsId trigger_background_id(321);
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      trigger_background_id,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger,
      /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);
  auto triggerHeaders = base::MakeRefCounted<net::HttpResponseHeaders>("");
  triggerHeaders->SetHeader(kAttributionReportingRegisterTriggerHeader,
                            kRegisterTriggerJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      trigger_background_id, triggerHeaders.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(
      trigger_background_id);
  task_environment_.FastForwardBy(base::TimeDelta());

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url));
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      second_background_id, headers.get(), reporting_url));
  task_environment_.FastForwardBy(base::TimeDelta());

  // Both the foreground & background registrations needs to be done for
  // the trigger to be processed.
  checkpoint.Call(1);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(2);
  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);
  checkpoint.Call(3);
  data_host_manager_.NotifyBackgroundRegistrationCompleted(
      second_background_id);

  task_environment_.FastForwardBy(base::TimeDelta());

  // kTiedImmediately=0
  histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 0, 2);
}

TEST_F(
    AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
    Background_NavigationTiedOnCompletedNavigation_TriggerDeferredUntilBackgroundSourceRegistrationCompletes) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  base::HistogramTester histograms;
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }
  data_host_manager_.NotifyNavigationWithBackgroundRegistrationsWillStart(
      attribution_src_token, kExpectedRegistrations);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);

  // It should defer the trigger registration.
  BackgroundRegistrationsId trigger_background_id(321);
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      trigger_background_id,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger,
      /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);
  auto triggerHeaders = base::MakeRefCounted<net::HttpResponseHeaders>("");
  triggerHeaders->SetHeader(kAttributionReportingRegisterTriggerHeader,
                            kRegisterTriggerJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      trigger_background_id, triggerHeaders.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(
      trigger_background_id);
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(1);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  task_environment_.FastForwardBy(base::TimeDelta());

  checkpoint.Call(2);

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url));
  // The background source registration must be completed for the trigger to
  // be processed.
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(3);
  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());

  // kTiedImmediately=0
  histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 0, 1);
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationTest,
       Background_NavigationTiedToCompletedNavigation) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(SourceTypeIs(SourceType::kNavigation),
                                 ImpressionOriginIs(context_origin),
                                 ReportingOriginIs(reporting_origin),
                                 SourceIsWithinFencedFrameIs(false)),
                           kFrameId))
      .Times(1);

  data_host_manager_.NotifyNavigationWithBackgroundRegistrationsWillStart(
      attribution_src_token, kExpectedRegistrations);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  task_environment_.FastForwardBy(base::TimeDelta());

  // The background registrations is started after the foreground navigation
  // completed.
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);
  task_environment_.FastForwardBy(base::TimeDelta());

  // kTiedImmediately=0
  histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 0, 1);
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationTest,
       Background_NavigationTiedToCompletedIneligibleNavigation) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  // A first background registrations starts, register data and complete.
  BackgroundRegistrationsId first_background_id(1111);
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      first_background_id,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);
  auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_1->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      first_background_id, headers_1.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(first_background_id);
  task_environment_.FastForwardBy(base::TimeDelta());

  // A navigation completes without starting indicating that it is ineligible.
  data_host_manager_.NotifyNavigationWithBackgroundRegistrationsWillStart(
      attribution_src_token, /*expected_registrations=*/2);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  // A second background registrations starts, register data and complete.
  BackgroundRegistrationsId second_background_id(2222);
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      second_background_id,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);
  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  EXPECT_FALSE(data_host_manager_.NotifyBackgroundRegistrationData(
      second_background_id, headers_2.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(
      second_background_id);
  task_environment_.FastForwardBy(base::TimeDelta());

  // kNeverTiedIneligible=3
  histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 3, 2);
}

TEST_F(
    AttributionDataHostManagerImplWithInBrowserMigrationTest,
    BackgroundNavigationTied_FewerThanExpectedBackgroundRegistrationsReceivedInTime) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleSource).Times(1);

  // The navigation expects two background registrations.
  data_host_manager_.NotifyNavigationWithBackgroundRegistrationsWillStart(
      attribution_src_token,
      /*expected_registrations=*/2);

  // A first background registrations starts, register data and complete.
  BackgroundRegistrationsId first_background_id(1111);
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      first_background_id,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);
  auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_1->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      first_background_id, headers_1.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(first_background_id);
  task_environment_.FastForwardBy(base::TimeDelta());

  // The navigation starts and completes.
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  // The second background registration is not being received promptly, the
  // cached navigation context gets cleared.
  task_environment_.FastForwardBy(base::Seconds(3));

  // The second background registration eventually starts, it won't be able to
  // register data as it will start waiting on a navigation that won't start.
  BackgroundRegistrationsId second_background_id(2222);
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      second_background_id,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSource, attribution_src_token,
      kDevtoolsRequestId);
  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      second_background_id, headers_2.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(
      second_background_id);

  // After waiting an additional 3 seconds, the background registration should
  // stop waiting on the navigation and be considered never tied, it won't
  // register any data.
  task_environment_.FastForwardBy(base::Seconds(3));
  task_environment_.FastForwardBy(base::Microseconds(1));

  // kTiedWithDelay=1, kNeverTiedTimeout=2
  histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 2, 1);
  histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 1, 1);
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationTest,
       Background_MultipleRegistrationTiedToCompletedNavigation) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleSource).Times(2);

  BackgroundRegistrationsId first_background_id(1111);
  BackgroundRegistrationsId second_background_id(2222);

  const auto suitable_context = AttributionSuitableContext::CreateForTesting(
      context_origin,
      /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId);

  // A first background registration is started before the navigation starts.
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      first_background_id, suitable_context, RegistrationEligibility::kSource,
      attribution_src_token, kDevtoolsRequestId);

  data_host_manager_.NotifyNavigationWithBackgroundRegistrationsWillStart(
      attribution_src_token, /*expected_registrations=*/2);

  data_host_manager_.NotifyNavigationRegistrationStarted(
      suitable_context, attribution_src_token, kNavigationId,
      kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);
  task_environment_.FastForwardBy(base::TimeDelta());

  // A second background registrations is started after the foreground
  // navigation completed.
  data_host_manager_.NotifyBackgroundRegistrationStarted(
      second_background_id, suitable_context, RegistrationEligibility::kSource,
      attribution_src_token, kDevtoolsRequestId);
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      first_background_id, headers.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(first_background_id);

  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      second_background_id, headers_2.get(), reporting_url));

  data_host_manager_.NotifyBackgroundRegistrationCompleted(
      second_background_id);
  task_environment_.FastForwardBy(base::TimeDelta());

  // kTiedImmediately=0, kTiedWithDelay=1
  histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 0, 1);
  histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 1, 1);
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationTest,
       Background_NavigationTiedWithDelay) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_url2 = GURL("https://report2.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  const auto suitable_context = AttributionSuitableContext::CreateForTesting(
      context_origin,
      /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId);

  for (bool navigation_eventually_starts : {false, true}) {
    base::HistogramTester histograms;

    EXPECT_CALL(mock_manager_, HandleSource)
        .Times(navigation_eventually_starts ? 3 : 0);

    BackgroundRegistrationsId second_background_id(101112);
    data_host_manager_.NotifyBackgroundRegistrationStarted(
        kBackgroundId, suitable_context, RegistrationEligibility::kSource,
        attribution_src_token, kDevtoolsRequestId);
    data_host_manager_.NotifyBackgroundRegistrationStarted(
        second_background_id, suitable_context,
        RegistrationEligibility::kSource, attribution_src_token,
        kDevtoolsRequestId);

    // No navigation seen yet we receive data for the first request.
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
    EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
        kBackgroundId, headers.get(), reporting_url));
    auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers_2->SetHeader(kAttributionReportingRegisterSourceHeader,
                         kRegisterSourceJson);
    EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
        kBackgroundId, headers_2.get(), reporting_url2));

    if (navigation_eventually_starts) {
      data_host_manager_.NotifyNavigationRegistrationStarted(
          suitable_context, attribution_src_token, kNavigationId,
          kDevtoolsRequestId);
    }

    data_host_manager_.NotifyNavigationRegistrationCompleted(
        attribution_src_token);

    // We receive more data after the navigation completes, potentially without
    // having started.
    auto headers_3 = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers_3->SetHeader(kAttributionReportingRegisterSourceHeader,
                         kRegisterSourceJson);
    EXPECT_EQ(data_host_manager_.NotifyBackgroundRegistrationData(
                  kBackgroundId, headers_3.get(), reporting_url),
              navigation_eventually_starts ? true : false);

    data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);
    data_host_manager_.NotifyBackgroundRegistrationCompleted(
        second_background_id);

    task_environment_.FastForwardBy(base::TimeDelta());

    // kTiedWithDelay=1, kNeverTiedIneligible=3
    if (navigation_eventually_starts) {
      histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 1, 2);
      // reporting_url1
      histograms.ExpectBucketCount(
          "Conversions.NavigationSourceRegistrationsPerReportingOriginPerBatch",
          2, 1);
      // reporting_url2
      histograms.ExpectBucketCount(
          "Conversions.NavigationSourceRegistrationsPerReportingOriginPerBatch",
          1, 1);
    } else {
      histograms.ExpectBucketCount(kBackgroundNavigationOutcome, 3, 2);
    }
  }
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       Background_NavigationTiedWithDelay_OsAndWebHeaders) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");
  const auto suitable_context = AttributionSuitableContext::CreateForTesting(
      context_origin,
      /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId);

  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(1);
  EXPECT_CALL(mock_manager_, HandleSource).Times(1);

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId, suitable_context, RegistrationEligibility::kSource,
      attribution_src_token, kDevtoolsRequestId);

  // No navigation seen yet we receive data for the first request.
  auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_1->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers_1.get(), reporting_url));
  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers_2.get(), reporting_url));

  // The navigation now start and complete.
  data_host_manager_.NotifyNavigationRegistrationStarted(
      suitable_context, attribution_src_token, kNavigationId,
      kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       BackgroundOsSource) {
  base::HistogramTester histograms;

  const auto reporting_url = GURL("https://report.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");
  const auto suitable_context = AttributionSuitableContext::CreateForTesting(
      context_origin,
      /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId);

  EXPECT_CALL(mock_manager_,
              HandleOsRegistration(OsRegistration(
                  {OsRegistrationItem(GURL("https://r.test/x"),
                                      /*debug_reporting=*/false)},
                  context_origin, /*input_event=*/AttributionInputEvent(),
                  /*is_within_fenced_frame=*/false, kFrameId, kRegistrar)));

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId, suitable_context,
      RegistrationEligibility::kSourceOrTrigger,
      /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x")");
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url));

  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());

  // kForegroundOrBackgroundBrowser = 10
  histograms.ExpectBucketCount(kRegistrationMethod, 10, 1);
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       BackgroundOsSource_OsAttributionClientDisabled) {
  const GURL reporting_url("https://report.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId,
          AttributionInputEvent(),
          {ContentBrowserClient::AttributionReportingOsRegistrar::kDisabled,
           ContentBrowserClient::AttributionReportingOsRegistrar::kDisabled},
          /*attribution_data_host_manager=*/nullptr),
      RegistrationEligibility::kSourceOrTrigger,
      /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                     R"("https://r.test/x")");
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url));

  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationTest,
       Background_NonNavigationTied) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://source.test");

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger);
    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_,
                HandleSource(AllOf(SourceTypeIs(SourceType::kEvent),
                                   ImpressionOriginIs(context_origin),
                                   ReportingOriginIs(reporting_origin),
                                   SourceIsWithinFencedFrameIs(false)),
                             kFrameId));
  }

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSource,
      /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

  // Trigger registration that should not be delayed.
  mojo::Remote<attribution_reporting::mojom::DataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          *SuitableOrigin::Deserialize("https://page2.example"),
          /*is_nested_within_fenced_frame=*/false, kFrameId,
          /*last_navigation_id=*/kNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
  trigger_data_host_remote->TriggerDataAvailable(
      reporting_origin, TriggerRegistration(), kViaServiceWorker);

  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url);
  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationTest,
       BackgroundTrigger) {
  base::HistogramTester histograms;

  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto reporting_origin = *SuitableOrigin::Create(reporting_url);
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  EXPECT_CALL(mock_manager_, HandleTrigger).Times(1);

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kTrigger, /*attribution_src_token=*/std::nullopt,
      kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterTriggerHeader,
                     kRegisterTriggerJson);
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url));
  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());

  // kForegroundOrBackgroundBrowser = 10
  histograms.ExpectBucketCount(kRegistrationMethod, 10, 1);
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationTest,
       Background_NonSuitableReportingUrl) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto non_suitable_reporting_url = GURL("http://a.test");
  const auto suitable_reporting_url = GURL("https://b.test");

  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  for (bool suitable : {true, false}) {
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(suitable ? 1 : 0);

    data_host_manager_.NotifyBackgroundRegistrationStarted(
        kBackgroundId,
        AttributionSuitableContext::CreateForTesting(
            context_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        RegistrationEligibility::kTrigger,
        /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterTriggerHeader,
                       kRegisterTriggerJson);
    EXPECT_EQ(
        data_host_manager_.NotifyBackgroundRegistrationData(
            kBackgroundId, headers.get(),
            suitable ? suitable_reporting_url : non_suitable_reporting_url),
        suitable);
    data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

    task_environment_.FastForwardBy(base::TimeDelta());
  }
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       BackgroundOsTrigger) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  EXPECT_CALL(mock_manager_,
              HandleOsRegistration(OsRegistration(
                  {OsRegistrationItem(GURL("https://r.test/x"),
                                      /*debug_reporting=*/false)},
                  context_origin, /*input_event=*/std::nullopt,
                  /*is_within_fenced_frame=*/false, kFrameId, kRegistrar)));

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kTrigger,
      /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsTriggerHeader,
                     R"("https://r.test/x")");
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url));

  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       BackgroundOsTrigger_OsAttributionClientDisabled) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(0);

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId,
          AttributionInputEvent(),
          {ContentBrowserClient::AttributionReportingOsRegistrar::kDisabled,
           ContentBrowserClient::AttributionReportingOsRegistrar::kDisabled},
          nullptr),
      RegistrationEligibility::kTrigger,
      /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterOsTriggerHeader,
                     R"("https://r.test/x")");
  EXPECT_TRUE(data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url));

  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationTest,
       BackgroundTrigger_ParsingFails) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  for (const auto& devtools_request_id :
       std::vector<std::optional<std::string>>{std::nullopt,
                                               kDevtoolsRequestId}) {
    base::HistogramTester histograms;
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);

    data_host_manager_.NotifyBackgroundRegistrationStarted(
        kBackgroundId,
        AttributionSuitableContext::CreateForTesting(
            context_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        RegistrationEligibility::kTrigger,
        /*attribution_src_token=*/std::nullopt, devtools_request_id);

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterTriggerHeader, "");
    data_host_manager_.NotifyBackgroundRegistrationData(
        kBackgroundId, headers.get(), reporting_url);
    data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

    task_environment_.FastForwardBy(base::TimeDelta());
    histograms.ExpectUniqueSample("Conversions.TriggerRegistrationError11",
                                  TriggerRegistrationError::kInvalidJson, 1);
  }
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       Background_InvalidHeaders) {
  const blink::AttributionSrcToken attribution_src_token;

  const auto reporting_url = GURL("https://report.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  const std::pair<std::string, std::string> web_source = {
      kAttributionReportingRegisterSourceHeader, kRegisterSourceJson};
  const std::pair<std::string, std::string> os_source = {
      kAttributionReportingRegisterOsSourceHeader, kRegisterSourceJson};
  const std::pair<std::string, std::string> web_trigger = {
      kAttributionReportingRegisterTriggerHeader, kRegisterTriggerJson};
  const std::pair<std::string, std::string> os_trigger = {
      kAttributionReportingRegisterOsTriggerHeader, kRegisterTriggerJson};

  // todo(anthonygarant): Add test observer to confirm that the right audit
  // issues are reported.
  const struct {
    std::string description;
    RegistrationEligibility eligibility;
    bool expect_registration;
    std::vector<std::pair<std::string, std::string>> headers;
  } kTestCases[] = {
      {.description = "source or trigger could be parsed (web)",
       .eligibility = RegistrationEligibility::kSourceOrTrigger,
       .expect_registration = false,
       .headers = {web_source, web_trigger}},
      {.description = "source or trigger could be parsed (os)",
       .eligibility = RegistrationEligibility::kSourceOrTrigger,
       .expect_registration = false,
       .headers = {os_source, os_trigger}},
      {.description = "source or trigger could be parsed (os & web)",
       .eligibility = RegistrationEligibility::kSourceOrTrigger,
       .expect_registration = false,
       .headers = {os_source, web_trigger}},
      {.description = "source or trigger could be parsed (web & os)",
       .eligibility = RegistrationEligibility::kSourceOrTrigger,
       .expect_registration = false,
       .headers = {web_source, os_trigger}},
      //
      {.description = "trigger is ignored (web)",
       .eligibility = RegistrationEligibility::kSource,
       .expect_registration = false,
       .headers = {web_trigger}},
      {.description = "trigger is ignored (os)",
       .eligibility = RegistrationEligibility::kSource,
       .expect_registration = false,
       .headers = {os_trigger}},
      {.description = "source is ignored (web)",
       .eligibility = RegistrationEligibility::kTrigger,
       .expect_registration = false,
       .headers = {web_source}},
      {.description = "source is ignored (os)",
       .eligibility = RegistrationEligibility::kTrigger,
       .expect_registration = false,
       .headers = {os_source}},
      //
      {.description = "trigger is ignored, source is processed (web)",
       .eligibility = RegistrationEligibility::kSource,
       .expect_registration = true,
       .headers = {web_source, web_trigger}},
      {.description = "trigger is ignored, source is processed (os)",
       .eligibility = RegistrationEligibility::kSource,
       .expect_registration = true,
       .headers = {os_source, os_trigger}},
      {.description = "source is ignored, trigger is processed (web)",
       .eligibility = RegistrationEligibility::kTrigger,
       .expect_registration = true,
       .headers = {web_source, web_trigger}},
      {.description = "source is ignored, trigger is processed (os)",
       .eligibility = RegistrationEligibility::kTrigger,
       .expect_registration = true,
       .headers = {os_source, os_trigger}},
      //
      {.description = "os or web could be parsed (source)",
       .eligibility = RegistrationEligibility::kSource,
       .expect_registration = true,
       .headers = {os_source, web_source}},
      {.description = "os or web could be parsed (trigger)",
       .eligibility = RegistrationEligibility::kTrigger,
       .expect_registration = true,
       .headers = {os_trigger, web_trigger}},
  };

  for (const auto& devtools_request_id :
       std::vector<std::optional<std::string>>{std::nullopt,
                                               kDevtoolsRequestId}) {
    for (const auto& test_case : kTestCases) {
      data_host_manager_.NotifyBackgroundRegistrationStarted(
          kBackgroundId,
          AttributionSuitableContext::CreateForTesting(
              context_origin,
              /*is_nested_within_fenced_frame=*/false, kFrameId,
              kLastNavigationId),
          test_case.eligibility, /*attribution_src_token=*/std::nullopt,
          devtools_request_id);

      auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
      for (const auto& header : test_case.headers) {
        headers->SetHeader(header.first, header.second);
      }
      EXPECT_EQ(data_host_manager_.NotifyBackgroundRegistrationData(
                    kBackgroundId, headers.get(), reporting_url),
                test_case.expect_registration)
          << test_case.description;
      data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

      task_environment_.FastForwardBy(base::TimeDelta());
    }
  }
}

struct PreferredPlatformTestCase {
  bool feature_enabled = true;
  const char* info_header;
  bool has_web_header;
  bool has_os_header;
  AttributionSupport support;
  bool expected_web;
  bool expected_os;
};

const PreferredPlatformTestCase kPreferredPlatformTestCases[] = {
    {
        .info_header = nullptr,
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWebAndOs,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = nullptr,
        .has_web_header = true,
        .has_os_header = false,
        .support = AttributionSupport::kWebAndOs,
        .expected_web = true,
        .expected_os = false,
    },
    {
        .info_header = nullptr,
        .has_web_header = false,
        .has_os_header = true,
        .support = AttributionSupport::kWebAndOs,
        .expected_web = false,
        .expected_os = true,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWebAndOs,
        .expected_web = false,
        .expected_os = true,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kOs,
        .expected_web = false,
        .expected_os = true,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWeb,
        .expected_web = true,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kNone,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = false,
        .has_os_header = true,
        .support = AttributionSupport::kWeb,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = false,
        .support = AttributionSupport::kWeb,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWebAndOs,
        .expected_web = true,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWeb,
        .expected_web = true,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kOs,
        .expected_web = false,
        .expected_os = true,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kNone,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = false,
        .support = AttributionSupport::kOs,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = false,
        .has_os_header = true,
        .support = AttributionSupport::kOs,
        .expected_web = false,
        .expected_os = false,
    },
};

class AttributionDataHostManagerImplPreferredPlatformEnabledTest
    : public AttributionDataHostManagerImplTest,
      public ::testing::WithParamInterface<PreferredPlatformTestCase> {
 public:
  AttributionDataHostManagerImplPreferredPlatformEnabledTest() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      network::features::kAttributionReportingCrossAppWeb};
};

INSTANTIATE_TEST_SUITE_P(
    ,
    AttributionDataHostManagerImplPreferredPlatformEnabledTest,
    ::testing::ValuesIn(kPreferredPlatformTestCases));

TEST_P(AttributionDataHostManagerImplPreferredPlatformEnabledTest,
       NavigationRegistration) {
  MockAttributionReportingContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  const auto& test_case = GetParam();

  EXPECT_CALL(browser_client, GetAttributionSupport)
      .WillRepeatedly(Return(test_case.support));

  const GURL reporter_url("https://report.test");
  const auto source_site = *SuitableOrigin::Deserialize("https://source.test");

  EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.expected_web);
  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(test_case.expected_os);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  if (test_case.has_web_header) {
    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  }
  if (test_case.has_os_header) {
    headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  }
  if (test_case.info_header) {
    headers->SetHeader(kAttributionReportingInfoHeader, test_case.info_header);
  }

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRegistrationStarted(
      AttributionSuitableContext::CreateForTesting(
          source_site,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      attribution_src_token, kNavigationId, kDevtoolsRequestId);
  data_host_manager_.NotifyNavigationRegistrationData(
      attribution_src_token, headers.get(), reporter_url);
  data_host_manager_.NotifyNavigationRegistrationCompleted(
      attribution_src_token);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_P(AttributionDataHostManagerImplPreferredPlatformEnabledTest,
       BeaconRegistration) {
  MockAttributionReportingContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  const auto& test_case = GetParam();

  EXPECT_CALL(browser_client, GetAttributionSupport)
      .WillRepeatedly(Return(test_case.support));

  EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.expected_web);
  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(test_case.expected_os);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  if (test_case.has_web_header) {
    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  }
  if (test_case.has_os_header) {
    headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  }
  if (test_case.info_header) {
    headers->SetHeader(kAttributionReportingInfoHeader, test_case.info_header);
  }

  data_host_manager_.NotifyFencedFrameReportingBeaconStarted(
      kBeaconId,
      AttributionSuitableContext::CreateForTesting(
          /*context_origin=*/*SuitableOrigin::Deserialize(
              "https://source.test"),
          /*is_nested_within_fenced_frame=*/true, kFrameId, kLastNavigationId),
      kNavigationId, kDevtoolsRequestId);

  data_host_manager_.NotifyFencedFrameReportingBeaconData(
      kBeaconId,
      /*reporting_url=*/GURL("https://report.test"), headers.get(),
      /*is_final_response=*/true);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

class
    AttributionDataHostManagerImplWithInBrowserMigrationAndPreferredPlatformTest
    : public AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
      public ::testing::WithParamInterface<PreferredPlatformTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    AttributionDataHostManagerImplWithInBrowserMigrationAndPreferredPlatformTest,
    ::testing::ValuesIn(kPreferredPlatformTestCases));

TEST_P(
    AttributionDataHostManagerImplWithInBrowserMigrationAndPreferredPlatformTest,
    BackgroundSource) {
  MockAttributionReportingContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  const auto& test_case = GetParam();

  EXPECT_CALL(browser_client, GetAttributionSupport)
      .WillRepeatedly(Return(test_case.support));

  EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.expected_web);
  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(test_case.expected_os);

  const auto reporting_url = GURL("https://report.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  if (test_case.has_web_header) {
    headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  }
  if (test_case.has_os_header) {
    headers->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  }
  if (test_case.info_header) {
    headers->SetHeader(kAttributionReportingInfoHeader, test_case.info_header);
  }

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger,
      /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

  data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url);
  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_P(
    AttributionDataHostManagerImplWithInBrowserMigrationAndPreferredPlatformTest,
    BackgroundTrigger) {
  MockAttributionReportingContentBrowserClient browser_client;
  ScopedContentBrowserClientSetting setting(&browser_client);

  const auto& test_case = GetParam();

  EXPECT_CALL(browser_client, GetAttributionSupport)
      .WillRepeatedly(Return(test_case.support));

  EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.expected_web);
  EXPECT_CALL(mock_manager_, HandleOsRegistration).Times(test_case.expected_os);

  const auto reporting_url = GURL("https://report.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  if (test_case.has_web_header) {
    headers->SetHeader(kAttributionReportingRegisterTriggerHeader,
                       kRegisterTriggerJson);
  }
  if (test_case.has_os_header) {
    headers->SetHeader(kAttributionReportingRegisterOsTriggerHeader,
                       R"("https://r.test/x")");
  }
  if (test_case.info_header) {
    headers->SetHeader(kAttributionReportingInfoHeader, test_case.info_header);
  }

  data_host_manager_.NotifyBackgroundRegistrationStarted(
      kBackgroundId,
      AttributionSuitableContext::CreateForTesting(
          context_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger,
      /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

  data_host_manager_.NotifyBackgroundRegistrationData(
      kBackgroundId, headers.get(), reporting_url);
  data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);

  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       DataHost_ReportRegistrationHeaderError) {
  const auto page_origin = *SuitableOrigin::Deserialize("https://page.example");
  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.example");

  attribution_reporting::RegistrationHeaderError error(
      /*header_value=*/"!!!", SourceRegistrationError ::kInvalidJson);

  EXPECT_CALL(mock_manager_, ReportRegistrationHeaderError(
                                 reporting_origin, error, page_origin,
                                 /*is_within_fenced_frame=*/false, kFrameId));

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      AttributionSuitableContext::CreateForTesting(
          page_origin,
          /*is_nested_within_fenced_frame=*/false, kFrameId, kLastNavigationId),
      RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);

  data_host_remote->ReportRegistrationHeaderError(reporting_origin, error);
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRegistration_ReportRegistrationHeaderError) {
  const GURL reporting_url("https://report.test");
  const auto page_origin = *SuitableOrigin::Deserialize("https://source.test");

  for (const bool report_header_errors : {false, true}) {
    SCOPED_TRACE(report_header_errors);

    EXPECT_CALL(
        mock_manager_,
        ReportRegistrationHeaderError(
            *SuitableOrigin::Create(reporting_url),
            attribution_reporting::RegistrationHeaderError(
                /*header_value=*/"!!!", SourceRegistrationError::kInvalidJson),
            page_origin, /*is_within_fenced_frame=*/false, kFrameId))
        .Times(report_header_errors);

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterSourceHeader, R"(!!!)");
    if (report_header_errors) {
      headers->SetHeader(kAttributionReportingInfoHeader,
                         R"(report-header-errors)");
    }

    const blink::AttributionSrcToken attribution_src_token;
    data_host_manager_.NotifyNavigationRegistrationStarted(
        AttributionSuitableContext::CreateForTesting(
            page_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        attribution_src_token, kNavigationId, kDevtoolsRequestId);
    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url);
    data_host_manager_.NotifyNavigationRegistrationCompleted(
        attribution_src_token);
    // Wait for parsing to finish.
    task_environment_.FastForwardBy(base::TimeDelta());
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRegistrationOsSource_ReportRegistrationHeaderError) {
  base::test::ScopedFeatureList scoped_feature_list(
      network::features::kAttributionReportingCrossAppWeb);

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const GURL reporting_url("https://report.test");
  const auto page_origin = *SuitableOrigin::Deserialize("https://source.test");

  for (const bool report_header_errors : {false, true}) {
    SCOPED_TRACE(report_header_errors);

    EXPECT_CALL(
        mock_manager_,
        ReportRegistrationHeaderError(
            *SuitableOrigin::Create(reporting_url),
            attribution_reporting::RegistrationHeaderError(
                /*header_value=*/"!!!",
                OsSourceRegistrationError(OsRegistrationError::kInvalidList)),
            page_origin, /*is_within_fenced_frame=*/false, kFrameId))
        .Times(report_header_errors);

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterOsSourceHeader, R"(!!!)");
    if (report_header_errors) {
      headers->SetHeader(kAttributionReportingInfoHeader,
                         R"(report-header-errors)");
    }

    const blink::AttributionSrcToken attribution_src_token;
    data_host_manager_.NotifyNavigationRegistrationStarted(
        AttributionSuitableContext::CreateForTesting(
            page_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        attribution_src_token, kNavigationId, kDevtoolsRequestId);
    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url);
    data_host_manager_.NotifyNavigationRegistrationCompleted(
        attribution_src_token);
    // Wait for parsing to finish.
    task_environment_.FastForwardBy(base::TimeDelta());
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       DataHostRegistration_RegistrarSupportChecked) {
  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://reporter.test");
  const auto context_origin =
      *SuitableOrigin::Deserialize("https://context.test");

  for (const bool has_support : {false, true}) {
    MockAttributionReportingContentBrowserClient browser_client;
    ScopedContentBrowserClientSetting setting(&browser_client);

    AttributionSupport web_support =
        has_support ? AttributionSupport::kWeb : AttributionSupport::kNone;
    AttributionSupport os_support =
        has_support ? AttributionSupport::kOs : AttributionSupport::kNone;

    EXPECT_CALL(browser_client, GetAttributionSupport)
        .WillOnce(Return(web_support))
        .WillOnce(Return(web_support))
        .WillOnce(Return(os_support))
        .WillOnce(Return(os_support));

    EXPECT_CALL(mock_manager_, HandleSource).Times(has_support);
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(has_support);
    EXPECT_CALL(
        mock_manager_,
        HandleOsRegistration(Field(
            &OsRegistration::registration_items,
            ElementsAre(Field(&attribution_reporting::OsRegistrationItem::url,
                              GURL("https://a.test/x"))))))
        .Times(has_support);
    EXPECT_CALL(
        mock_manager_,
        HandleOsRegistration(Field(
            &OsRegistration::registration_items,
            ElementsAre(Field(&attribution_reporting::OsRegistrationItem::url,
                              GURL("https://b.test/x"))))))
        .Times(has_support);

    mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        AttributionSuitableContext::CreateForTesting(
            context_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        RegistrationEligibility::kSourceOrTrigger, kIsForBackgroundRequests);
    data_host_remote->SourceDataAvailable(
        reporting_origin,
        SourceRegistration(*DestinationSet::Create(
            {net::SchemefulSite::Deserialize("https://destination.example")})),
        kViaServiceWorker);
    data_host_remote->TriggerDataAvailable(
        reporting_origin, TriggerRegistration(), kViaServiceWorker);
    data_host_remote->OsSourceDataAvailable(
        reporting_origin,
        {attribution_reporting::OsRegistrationItem{
            .url = GURL("https://a.test/x")}},
        kViaServiceWorker);
    data_host_remote->OsTriggerDataAvailable(
        reporting_origin,
        {attribution_reporting::OsRegistrationItem{
            .url = GURL("https://b.test/x")}},
        kViaServiceWorker);
    data_host_remote.FlushForTesting();
  }
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationTest,
       BackgroundTrigger_ReportRegistrationHeaderError) {
  const auto reporting_url = GURL("https://report.test");
  const auto page_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  for (const bool report_header_errors : {false, true}) {
    SCOPED_TRACE(report_header_errors);

    EXPECT_CALL(
        mock_manager_,
        ReportRegistrationHeaderError(
            *SuitableOrigin::Create(reporting_url),
            attribution_reporting::RegistrationHeaderError(
                /*header_value=*/"!!!", TriggerRegistrationError::kInvalidJson),
            page_origin, /*is_within_fenced_frame=*/false, kFrameId))
        .Times(report_header_errors);

    data_host_manager_.NotifyBackgroundRegistrationStarted(
        kBackgroundId,
        AttributionSuitableContext::CreateForTesting(
            page_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        RegistrationEligibility::kTrigger,
        /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterTriggerHeader, "!!!");
    if (report_header_errors) {
      headers->SetHeader(kAttributionReportingInfoHeader,
                         R"(report-header-errors)");
    }
    data_host_manager_.NotifyBackgroundRegistrationData(
        kBackgroundId, headers.get(), reporting_url);
    data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);
    // Wait for parsing to finish.
    task_environment_.FastForwardBy(base::TimeDelta());
  }
}

TEST_F(AttributionDataHostManagerImplWithInBrowserMigrationAndAppToWebTest,
       BackgroundOsTrigger_ReportRegistrationHeaderError) {
  const GURL reporting_url("https://report.test");
  const auto page_origin =
      *SuitableOrigin::Deserialize("https://destination.test");

  for (const bool report_header_errors : {false, true}) {
    EXPECT_CALL(
        mock_manager_,
        ReportRegistrationHeaderError(
            *SuitableOrigin::Create(reporting_url),
            attribution_reporting::RegistrationHeaderError(
                /*header_value=*/"!!!",
                OsTriggerRegistrationError(OsRegistrationError::kInvalidList)),
            page_origin, /*is_within_fenced_frame=*/false, kFrameId))
        .Times(report_header_errors);

    data_host_manager_.NotifyBackgroundRegistrationStarted(
        kBackgroundId,
        AttributionSuitableContext::CreateForTesting(
            page_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        RegistrationEligibility::kTrigger,
        /*attribution_src_token=*/std::nullopt, kDevtoolsRequestId);

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterOsTriggerHeader, "!!!");
    if (report_header_errors) {
      headers->SetHeader(kAttributionReportingInfoHeader,
                         R"(report-header-errors)");
    }
    data_host_manager_.NotifyBackgroundRegistrationData(
        kBackgroundId, headers.get(), reporting_url);
    data_host_manager_.NotifyBackgroundRegistrationCompleted(kBackgroundId);
    // Wait for parsing to finish.
    task_environment_.FastForwardBy(base::TimeDelta());
  }
}

TEST_F(AttributionDataHostManagerImplTest, RegistrationInfoErrorMetric) {
  const GURL reporting_url("https://report.test");
  const auto page_origin = *SuitableOrigin::Deserialize("https://source.test");

  const struct {
    const char* header;
    std::optional<RegistrationInfoError> expected;
  } kTestCases[] = {
      {
          "!",
          RegistrationInfoError::kRootInvalid,
      },
      {
          "preferred-platform=1",
          RegistrationInfoError::kInvalidPreferredPlatform,
      },
      {
          "report-header-errors=abc",
          RegistrationInfoError::kInvalidReportHeaderErrors,
      },
      {
          "preferred-platform=web,report-header-errors",
          std::nullopt,
      },
  };

  static constexpr char kRegistrationInfoErrorMetric[] =
      "Conversions.RegistrationInfoError";

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;

    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers->SetHeader(kAttributionReportingRegisterSourceHeader, R"(!!!)");
    headers->SetHeader(kAttributionReportingInfoHeader, test_case.header);

    const blink::AttributionSrcToken attribution_src_token;
    data_host_manager_.NotifyNavigationRegistrationStarted(
        AttributionSuitableContext::CreateForTesting(
            page_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId,
            kLastNavigationId),
        attribution_src_token, kNavigationId, kDevtoolsRequestId);
    data_host_manager_.NotifyNavigationRegistrationData(
        attribution_src_token, headers.get(), reporting_url);
    data_host_manager_.NotifyNavigationRegistrationCompleted(
        attribution_src_token);
    // Wait for parsing to finish.
    task_environment_.FastForwardBy(base::TimeDelta());

    if (test_case.expected.has_value()) {
      histograms.ExpectUniqueSample(kRegistrationInfoErrorMetric,
                                    test_case.expected.value(), 1);
    } else {
      histograms.ExpectTotalCount(kRegistrationInfoErrorMetric, 0);
    }
  }
}

}  // namespace
}  // namespace content
