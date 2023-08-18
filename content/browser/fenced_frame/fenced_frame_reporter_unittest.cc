// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_reporter.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/test/mock_attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/interest_group/test_interest_group_private_aggregation_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using ::testing::_;

using PrivateAggregationRequests =
    FencedFrameReporter::PrivateAggregationRequests;

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            auction_worklet::mojom::AggregatableReportContribution::
                NewHistogramContribution(
                    blink::mojom::AggregatableReportHistogramContribution::New(
                        /*bucket=*/1,
                        /*value=*/2)),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New());

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kPrivateAggregationRequest2 =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            auction_worklet::mojom::AggregatableReportContribution::
                NewHistogramContribution(
                    blink::mojom::AggregatableReportHistogramContribution::New(
                        /*bucket=*/3,
                        /*value=*/4)),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New());

// Helper to avoid excess boilerplate.
template <typename... Ts>
auto ElementsAreRequests(Ts&... requests) {
  static_assert(
      std::conjunction<std::is_same<
          std::remove_const_t<Ts>,
          auction_worklet::mojom::PrivateAggregationRequestPtr>...>::value);
  // Need to use `std::ref` as `mojo::StructPtr`s are move-only.
  return testing::UnorderedElementsAre(testing::Eq(std::ref(requests))...);
}

class InterestGroupEnabledContentBrowserClient
    : public TestContentBrowserClient {
 public:
  // ContentBrowserClient overrides:
  // This is needed so that the interest group related APIs can run without
  // failing with the result AuctionResult::kSellerRejected.
  bool IsPrivacySandboxReportingDestinationAttested(
      content::BrowserContext* browser_context,
      const url::Origin& destination_origin,
      content::PrivacySandboxInvokingAPI invoking_api) override {
    return true;
  }
};

class FencedFrameReporterTest : public RenderViewHostTestHarness {
 public:
  FencedFrameReporterTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAttributionFencedFrameReportingBeacon);
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return test_url_loader_factory_.GetSafeWeakWrapper();
  }

  AttributionManager* attribution_manager() {
    return AttributionManager::FromBrowserContext(browser_context());
  }

  void SetUp() override {
    old_content_browser_client_ =
        SetBrowserClientForTesting(&test_content_browser_client_);
    RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(request_initiator_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_content_browser_client_);
    RenderViewHostTestHarness::TearDown();
  }

  void ValidateRequest(const network::ResourceRequest& request,
                       const GURL& expected_url,
                       const absl::optional<std::string>& event_data) {
    EXPECT_EQ(request.url, expected_url);
    EXPECT_EQ(request.mode, network::mojom::RequestMode::kCors);
    EXPECT_EQ(request.request_initiator,
              url::Origin::Create(request_initiator_));
    EXPECT_EQ(request.credentials_mode, network::mojom::CredentialsMode::kOmit);
    EXPECT_TRUE(request.trusted_params->isolation_info.network_isolation_key()
                    .IsTransient());

    if (!event_data.has_value()) {
      EXPECT_EQ(request.method, net::HttpRequestHeaders::kGetMethod);
      return;
    }

    EXPECT_EQ(request.method, net::HttpRequestHeaders::kPostMethod);

    std::string content_type;
    ASSERT_TRUE(request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                          &content_type));
    EXPECT_EQ(content_type, "text/plain;charset=UTF-8");

    ASSERT_TRUE(request.request_body);
    ASSERT_EQ(request.request_body->elements()->size(), 1u);
    ASSERT_EQ((*request.request_body->elements())[0].type(),
              network::DataElement::Tag::kBytes);
    EXPECT_EQ((*request.request_body->elements())[0]
                  .As<network::DataElementBytes>()
                  .AsStringPiece(),
              *event_data);
  }

 protected:
  RenderFrameHostImpl* main_rfh_impl() {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }

  void ShutDownAttributionManager() {
    auto* partition = static_cast<StoragePartitionImpl*>(
        browser_context()->GetDefaultStoragePartition());
    partition->OverrideAttributionManagerForTesting(
        /*attribution_manager=*/nullptr);
  }

  network::TestURLLoaderFactory test_url_loader_factory_;

  const GURL request_initiator_{"https://initiator.test/"};
  const GURL report_destination_{"https://report_destination.test"};
  const GURL report_destination2_{"https://report_destination2.test"};
  const GURL report_destination3_{"https://report_destination3.test"};
  const url::Origin main_frame_origin_ =
      url::Origin::Create(request_initiator_);
  const url::Origin report_destination_origin_ =
      url::Origin::Create(report_destination_);
  const url::Origin report_destination2_origin_ =
      url::Origin::Create(report_destination2_);
  const url::Origin report_destination3_origin_ =
      url::Origin::Create(report_destination3_);

  TestInterestGroupPrivateAggregationManager private_aggregation_manager_{
      main_frame_origin_};

  InterestGroupEnabledContentBrowserClient test_content_browser_client_;
  raw_ptr<ContentBrowserClient> old_content_browser_client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// ReportingDestination has no map.
TEST_F(FencedFrameReporterTest, NoReportNoMap) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(
          shared_url_loader_factory(), browser_context(),
          /*reporting_url_map=*/{{"event_type", report_destination_}});
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;

  // A Shared Storage FencedFrameReporter has no map for FLEDGE destinations.
  EXPECT_FALSE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'Buyer'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kWarning);

  EXPECT_FALSE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kSeller, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'Seller'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kWarning);

  EXPECT_FALSE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kDirectSeller, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'ComponentSeller'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kWarning);

  EXPECT_FALSE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'ComponentSeller'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kWarning);

  EXPECT_FALSE(reporter->SendReport(
      DestinationURLEvent(report_destination_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'Buyer'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kWarning);

  // A FLEDGE FencedFrameReporter has no map for Shared Storage.
  reporter = FencedFrameReporter::CreateForFledge(
      shared_url_loader_factory(), browser_context(),
      /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
      main_frame_origin_,
      /*winner_origin=*/report_destination_origin_);
  EXPECT_FALSE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'SharedStorageSelectUrl'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kWarning);

  // No requests should have been made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// ReportingDestination has an empty map.
TEST_F(FencedFrameReporterTest, NoReportEmptyMap) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(shared_url_loader_factory(),
                                                  browser_context(),
                                                  /*reporting_url_map=*/{});
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_FALSE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'SharedStorageSelectUrl'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kWarning);

  // No requests should have been made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// Non-empty reporting URL map, but passed in event type isn't registered.
TEST_F(FencedFrameReporterTest, NoReportEventTypeNotRegistered) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(
          shared_url_loader_factory(), browser_context(),
          /*reporting_url_map=*/
          {{"registered_event_type", report_destination_}});
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_FALSE(reporter->SendReport(
      DestinationEnumEvent("unregistered_event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(
      error_message,
      "This frame did not register reporting url for destination "
      "'SharedStorageSelectUrl' and event_type 'unregistered_event_type'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kWarning);

  // No requests should have been made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// Event types map to disallowed URLs (empty URLs, non-HTTP/HTTPS URLs).
TEST_F(FencedFrameReporterTest, NoReportBadUrl) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(
          shared_url_loader_factory(), browser_context(),
          /*reporting_url_map=*/
          {{"no_url", GURL()},
           {"data_url", GURL("data:,only http is allowed")}});
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_FALSE(reporter->SendReport(
      DestinationEnumEvent("no_url", "event_data"),
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(error_message,
            "This frame registered invalid reporting url for destination "
            "'SharedStorageSelectUrl' and event_type 'no_url'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kError);

  EXPECT_FALSE(reporter->SendReport(
      DestinationEnumEvent("data_url", "event_data"),
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(error_message,
            "This frame registered invalid reporting url for destination "
            "'SharedStorageSelectUrl' and event_type 'data_url'.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kError);

  // No requests should have been made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(FencedFrameReporterTest, SendReports) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(
          shared_url_loader_factory(), browser_context(),
          /*reporting_url_map=*/
          {{"event_type", report_destination_},
           {"event_type2", report_destination2_}});

  // Make a report.
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_, "event_data");

  // Make another report to the same URL with different data. Should also
  // succeed.
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data2"),
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[1].request,
                  report_destination_, "event_data2");

  // Make a report using another event type.
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type2", "event_data3"),
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[2].request,
                  report_destination2_, "event_data3");
}

// Test reports in the FLEDGE case, where reporting URL maps are received before
// SendReport() calls.
TEST_F(FencedFrameReporterTest, SendFledgeReportsAfterMapsReceived) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_,
          /*allowed_reporting_origins=*/{{report_destination_origin_}});

  // Receive all mappings.
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kSeller,
      /*reporting_url_map=*/{{"event_type", report_destination_}});
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      /*reporting_url_map=*/{{"event_type", report_destination2_}});
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      /*reporting_url_map=*/{{"event_type", report_destination3_}},
      /*reporting_ad_macro_map=*/FencedFrameReporter::ReportingMacroMap());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Make reports. Each should be sent immediately.

  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kSeller, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_, "event_data");

  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[1].request,
                  report_destination2_, "event_data");

  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[2].request,
                  report_destination3_, "event_data");

  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kDirectSeller, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 4);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[3].request,
                  report_destination2_, "event_data");

  EXPECT_TRUE(reporter->SendReport(
      DestinationURLEvent(report_destination_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 5);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[4].request,
                  report_destination_, absl::nullopt);
}

// Test reports in the FLEDGE case, where reporting URL maps are received after
// SendReport() calls.
TEST_F(FencedFrameReporterTest, SendReportsFledgeBeforeMapsReceived) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/true, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_,
          /*allowed_reporting_origins=*/{{report_destination_origin_}});

  // Make reports. They should be queued, since mappings haven't been received
  // yet.
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kSeller, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kDirectSeller, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_TRUE(reporter->SendReport(
      DestinationURLEvent(report_destination_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Each report should be sent as its mapping is received.

  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kSeller,
      /*reporting_url_map=*/{{"event_type", report_destination_}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_, "event_data");
  // This one is from the "DirectSeller" destination, which was aliased to
  // kSeller.
  ValidateRequest((*test_url_loader_factory_.pending_requests())[1].request,
                  report_destination_, "event_data");

  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      /*reporting_url_map=*/{{"event_type", report_destination2_}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[2].request,
                  report_destination2_, "event_data");

  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      /*reporting_url_map=*/{{"event_type", report_destination3_}},
      /*reporting_ad_macro_map=*/FencedFrameReporter::ReportingMacroMap());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 5);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[3].request,
                  report_destination3_, "event_data");
  // This one is from the DestinationURLEvent report.
  ValidateRequest((*test_url_loader_factory_.pending_requests())[4].request,
                  report_destination_, absl::nullopt);
}

// Test reports in the FLEDGE case, where reporting URL maps are received after
// SendReport() calls, but no reports are sent because of errors (bad URL, no
// URL, missing event types). No error messages are generated in this case
// because there's nowhere to pass them
TEST_F(FencedFrameReporterTest, SendFledgeReportsBeforeMapsReceivedWithErrors) {
  auto attribution_data_host_manager =
      std::make_unique<MockAttributionDataHostManager>();
  auto* mock_attribution_data_host_manager =
      attribution_data_host_manager.get();

  auto mock_manager = std::make_unique<MockAttributionManager>();
  mock_manager->SetDataHostManager(std::move(attribution_data_host_manager));
  static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition())
      ->OverrideAttributionManagerForTesting(std::move(mock_manager));

  // `AttributionDataHostManager` is notified for the errors.
  EXPECT_CALL(*mock_attribution_data_host_manager,
              NotifyFencedFrameReportingBeaconData(_, _, _, /*headers=*/nullptr,
                                                   /*is_final_response=*/true))
      .Times(3);

  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_,
          /*allowed_reporting_origins=*/{{report_destination_origin_}});

  // SendReport() is called, and then a mapping is received that doesn't have
  // the report's event type. No request should be made.
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type2", "event_data"),
      blink::FencedFrame::ReportingDestination::kSeller, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kSeller,
      /*reporting_url_map=*/{{"event_type", report_destination_}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // SendReport() is called, and then a mapping is received that maps the
  // report's event type to a data URL. No request should be made.
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      /*reporting_url_map=*/{
          {"event_type", GURL("data:,only http is allowed")}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // SendReport() is called, and then a mapping is received with an empty map.
  // Only the DestinationURLEvent request should be sent.
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_TRUE(reporter->SendReport(
      DestinationURLEvent(report_destination_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      /*reporting_url_map=*/{},
      /*reporting_ad_macro_map=*/FencedFrameReporter::ReportingMacroMap());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_, absl::nullopt);
}

// Test that absence of an allowlist disables custom destination URL reports.
TEST_F(FencedFrameReporterTest, CustomDestinationURLNoAllowlist) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_,
          /*allowed_reporting_origins=*/absl::nullopt);

  // Receive buyer mapping.
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      /*reporting_url_map=*/{{}},
      /*reporting_ad_macro_map=*/FencedFrameReporter::ReportingMacroMap());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_FALSE(reporter->SendReport(
      DestinationURLEvent(report_destination_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(error_message,
            "This frame attempted to send a report to a custom destination URL "
            "with macro substitution, but no origins are allowed by its "
            "allowlist.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kError);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// Test that absence of an ad macro map disables custom destination URL reports.
TEST_F(FencedFrameReporterTest, CustomDestinationURLNoAdMacroMap) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_,
          /*allowed_reporting_origins=*/{});

  // Receive buyer mapping.
  reporter->OnUrlMappingReady(blink::FencedFrame::ReportingDestination::kBuyer,
                              /*reporting_url_map=*/{{}},
                              /*reporting_ad_macro_map=*/absl::nullopt);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_FALSE(reporter->SendReport(
      DestinationURLEvent(report_destination_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(error_message,
            "This frame attempted to send a report to a custom destination URL "
            "with macro substitution, which is not supported by the API that "
            "created this frame's fenced frame config.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kError);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// Test macro substitution for reports to custom destination URLs, where all
// macros are defined.
TEST_F(FencedFrameReporterTest, CustomDestinationURLCompleteMacroSubstitution) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_,
          /*allowed_reporting_origins=*/
          {{report_destination_origin_}});

  // Receive buyer mapping.
  reporter->OnUrlMappingReady(blink::FencedFrame::ReportingDestination::kBuyer,
                              /*reporting_url_map=*/{{}},
                              /*reporting_ad_macro_map=*/
                              FencedFrameReporter::ReportingMacroMap(
                                  {{"FOO", "foosub"}, {"BAR", "barsub"}}));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  GURL report_destination_template{
      "https://report_destination.test?foo=${FOO}&bar=${BAR}&foo2=${FOO}"};
  GURL report_destination_substituted{
      "https://report_destination.test?foo=foosub&bar=barsub&foo2=foosub"};

  // Send a request from an allowed origin. (It should succeed.)
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_TRUE(reporter->SendReport(
      DestinationURLEvent(report_destination_template),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_substituted, absl::nullopt);
}

// Test macro substitution for reports to custom destination URLs, where only
// some macros are defined. Also test that macros are not substituted
// recursively.
TEST_F(FencedFrameReporterTest, CustomDestinationURLPartialMacroSubstitution) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_,
          /*allowed_reporting_origins=*/
          {{report_destination_origin_}});

  // Receive buyer mapping.
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      /*reporting_url_map=*/{{}},
      /*reporting_ad_macro_map=*/
      FencedFrameReporter::ReportingMacroMap({{"FOO", "${FOO}${FOO}"}}));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  GURL report_destination_template{
      "https://report_destination.test?foo=${FOO}&bar=${BAR}&foo2=${FOO}"};
  GURL report_destination_substituted{
      "https://"
      "report_destination.test?foo=${FOO}${FOO}&bar=${BAR}&foo2=${FOO}${FOO}"};

  // Send a request from an allowed origin. (It should succeed.)
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_TRUE(reporter->SendReport(
      DestinationURLEvent(report_destination_template),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_substituted, absl::nullopt);
}

// Test allowlist for reports to custom destination URLs.
TEST_F(FencedFrameReporterTest, CustomDestinationURLAllowlist) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_,
          /*allowed_reporting_origins=*/
          {{report_destination_origin_, report_destination2_origin_}});

  // Receive buyer mapping.
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      /*reporting_url_map=*/{{"event_type", report_destination_}},
      /*reporting_ad_macro_map=*/FencedFrameReporter::ReportingMacroMap());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Send a request to an allowed origin. (It should succeed.)
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_TRUE(reporter->SendReport(
      DestinationURLEvent(report_destination_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_, absl::nullopt);

  // Send a request to a different allowed origin. (It should succeed.)
  EXPECT_TRUE(reporter->SendReport(
      DestinationURLEvent(report_destination2_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[1].request,
                  report_destination2_, absl::nullopt);

  // Send a request to a disallowed origin. (It should fail, and disable
  // future custom destination URL requests..)
  EXPECT_FALSE(reporter->SendReport(
      DestinationURLEvent(report_destination3_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(
      error_message,
      "This frame attempted to send a report to a custom destination URL "
      "with macro substitution to a disallowed origin. No further "
      "reports to custom destination URLs will be allowed for this fenced "
      "frame config.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kError);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);

  // Send a custom URL request to an allowed origin again. (It should still
  // fail.)
  EXPECT_FALSE(reporter->SendReport(
      DestinationURLEvent(report_destination_),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(
      error_message,
      "This frame attempted to send a report to a custom destination URL "
      "with macro substitution, but this functionality is disabled because "
      "a request was previously attempted to a disallowed origin.");
  EXPECT_EQ(console_message_level, blink::mojom::ConsoleMessageLevel::kError);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);

  // Send a regular report to an allowed origin. (It should succeed, because
  // the allowlist is only for custom URL reports.)
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[2].request,
                  report_destination_, "event_data");

  // Send a regular report to a disallowed origin. (It should succeed, because
  // the allowlist is only for custom URL reports.)
  NavigateAndCommit(report_destination2_);
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kBuyer, main_rfh_impl(),
      network::AttributionReportingRuntimeFeatures(), error_message,
      console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 4);
}

// Test reports in the FLEDGE case, where reporting URL map is never received.
TEST_F(FencedFrameReporterTest, SendFledgeReportsNoMapReceived) {
  auto attribution_data_host_manager =
      std::make_unique<MockAttributionDataHostManager>();
  auto* mock_attribution_data_host_manager =
      attribution_data_host_manager.get();

  auto mock_manager = std::make_unique<MockAttributionManager>();
  mock_manager->SetDataHostManager(std::move(attribution_data_host_manager));
  static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition())
      ->OverrideAttributionManagerForTesting(std::move(mock_manager));

  // `AttributionDataHostManager` is notified for the pending events.
  EXPECT_CALL(*mock_attribution_data_host_manager,
              NotifyFencedFrameReportingBeaconData(_, _, _, /*headers=*/nullptr,
                                                   /*is_final_response=*/true));
  {
    scoped_refptr<FencedFrameReporter> reporter =
        FencedFrameReporter::CreateForFledge(
            shared_url_loader_factory(), browser_context(),
            /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
            main_frame_origin_,
            /*winner_origin=*/report_destination_origin_);

    // SendReport() is called, but a mapping is never received.
    std::string error_message;
    blink::mojom::ConsoleMessageLevel console_message_level =
        blink::mojom::ConsoleMessageLevel::kError;
    EXPECT_TRUE(reporter->SendReport(
        DestinationEnumEvent("event_type2", "event_data"),
        blink::FencedFrame::ReportingDestination::kSeller, main_rfh_impl(),
        network::AttributionReportingRuntimeFeatures(), error_message,
        console_message_level));
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  }
}

// Test sending non-reserved private aggregation requests, when events from
// fenced frame is received after FLEDGE non-reserved PA requests are ready.
TEST_F(FencedFrameReporterTest, FledgeEventsReceivedAfterRequestsReady) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_);

  // Receive all non-reserved private aggregation requests.
  std::map<std::string, PrivateAggregationRequests>
      private_aggregation_event_map;
  private_aggregation_event_map["event_type"].push_back(
      kPrivateAggregationRequest.Clone());
  private_aggregation_event_map["event_type2"].push_back(
      kPrivateAggregationRequest2.Clone());

  std::map<std::string, PrivateAggregationRequests>
      private_aggregation_event_map2;
  private_aggregation_event_map2["event_type"].push_back(
      kPrivateAggregationRequest2.Clone());
  private_aggregation_event_map2["event_type3"].push_back(
      kPrivateAggregationRequest2.Clone());

  reporter->OnForEventPrivateAggregationRequestsReceived(
      std::move(private_aggregation_event_map));
  reporter->OnForEventPrivateAggregationRequestsReceived(
      std::move(private_aggregation_event_map2));
  // Reporter received private_aggregation_event_map.
  EXPECT_THAT(
      reporter->GetPrivateAggregationEventMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair("event_type",
                        ElementsAreRequests(kPrivateAggregationRequest,
                                            kPrivateAggregationRequest2)),
          testing::Pair("event_type2",
                        ElementsAreRequests(kPrivateAggregationRequest2)),
          testing::Pair("event_type3",
                        ElementsAreRequests(kPrivateAggregationRequest2))));
  // No event received from fenced frame yet, so no PA request gets sent.
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());

  // Each call to SendPrivateAggregationRequestsForEvent() should send
  // corresponding PA requests immediately, and the entry for the event type
  // should be removed from reporter's private_aggregation_event_map.
  reporter->SendPrivateAggregationRequestsForEvent("event_type");
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  report_destination_origin_,
                  ElementsAreRequests(kPrivateAggregationRequest,
                                      kPrivateAggregationRequest2))));
  EXPECT_THAT(
      reporter->GetPrivateAggregationEventMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair("event_type2",
                        ElementsAreRequests(kPrivateAggregationRequest2)),
          testing::Pair("event_type3",
                        ElementsAreRequests(kPrivateAggregationRequest2))));

  reporter->SendPrivateAggregationRequestsForEvent("event_type2");
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  report_destination_origin_,
                  ElementsAreRequests(kPrivateAggregationRequest2))));
  EXPECT_THAT(
      reporter->GetPrivateAggregationEventMapForTesting(),
      testing::UnorderedElementsAre(testing::Pair(
          "event_type3", ElementsAreRequests(kPrivateAggregationRequest2))));

  // Private aggregation requests for "event_type" has already been sent and
  // cleared, so no more such requests for the type to send when receiving it
  // again.
  reporter->SendPrivateAggregationRequestsForEvent("event_type");
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());

  // No private aggregation requests for "event_type4", so there's no effect
  // when "event_type4" is received.
  reporter->SendPrivateAggregationRequestsForEvent("event_type4");
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());
}

// Test sending non-reserved private aggregation requests, when events from
// fenced frame is received before FLEDGE non-reserved PA requests are ready.
TEST_F(FencedFrameReporterTest, FledgeEventsReceivedBeforeRequestsReady) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false, &private_aggregation_manager_,
          main_frame_origin_,
          /*winner_origin=*/report_destination_origin_);

  // Calls SendPrivateAggregationRequestsForEvent() with event types. The event
  // types should be queued, since non-reserved private aggregation requests
  // haven't been received yet.
  reporter->SendPrivateAggregationRequestsForEvent("event_type");
  reporter->SendPrivateAggregationRequestsForEvent("event_type");
  reporter->SendPrivateAggregationRequestsForEvent("event_type3");
  // ReceivedPaEvents is a std::set, so duplicate event types are only stored
  // once.
  EXPECT_THAT(reporter->GetReceivedPaEventsForTesting(),
              testing::UnorderedElementsAre("event_type", "event_type3"));
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());

  // Receive all non-reserved private aggregation requests.
  std::map<std::string, PrivateAggregationRequests>
      private_aggregation_event_map;
  private_aggregation_event_map["event_type"].push_back(
      kPrivateAggregationRequest.Clone());
  private_aggregation_event_map["event_type2"].push_back(
      kPrivateAggregationRequest2.Clone());

  reporter->OnForEventPrivateAggregationRequestsReceived(
      std::move(private_aggregation_event_map));

  // `received_pa_events_` is kept, in case needed for new private aggregation
  // requests from reportWin().
  EXPECT_THAT(reporter->GetReceivedPaEventsForTesting(),
              testing::UnorderedElementsAre("event_type", "event_type3"));
  // All pending pa events' PA requests in private_aggregation_event_map should
  // be sent after private_aggregation_event_map is ready.
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  report_destination_origin_,
                  ElementsAreRequests(kPrivateAggregationRequest))));

  // Calling SendPrivateAggregationRequestsForEvent() should send
  // corresponding PA requests immediately.
  reporter->SendPrivateAggregationRequestsForEvent("event_type2");
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  report_destination_origin_,
                  ElementsAreRequests(kPrivateAggregationRequest2))));
  // Although requests for "event_type2" are sent immediately, still store
  // "event_type2" in reporter's `received_pa_events_`, so that further
  // received PA requests of the type can still be triggered.
  EXPECT_THAT(reporter->GetReceivedPaEventsForTesting(),
              testing::UnorderedElementsAre("event_type", "event_type3",
                                            "event_type2"));

  // Private aggregation requests for "event_type" has already been sent and
  // cleared, so no more such requests for the type to sends.
  reporter->SendPrivateAggregationRequestsForEvent("event_type");
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());

  // Receive more non-reserved private aggregation requests. It happens when
  // reportWin() completes and then
  // OnForEventPrivateAggregationRequestsReceived() is called.
  std::map<std::string, PrivateAggregationRequests>
      private_aggregation_event_map2;
  private_aggregation_event_map2["event_type"].push_back(
      kPrivateAggregationRequest2.Clone());
  private_aggregation_event_map2["event_type2"].push_back(
      kPrivateAggregationRequest.Clone());

  // We expect two calls to `SendHistogramReport()` given the two events.
  private_aggregation_manager_.set_allow_multiple_calls_per_origin(true);

  reporter->OnForEventPrivateAggregationRequestsReceived(
      std::move(private_aggregation_event_map2));

  // Requests for both event types are sent immediately since there were such
  // pending PA events.
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  report_destination_origin_,
                  ElementsAreRequests(kPrivateAggregationRequest,
                                      kPrivateAggregationRequest2))));
}

// FencedFrameReporter's `private_aggregation_manager` is nullptr but fenced
// frame sends events unexpectedly. This could happen if the renderer is
// compromised. Should just ignore the events.
TEST_F(FencedFrameReporterTest, FledgeEventsReceivedUnexpectedly) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(
          shared_url_loader_factory(), browser_context(),
          /*direct_seller_is_seller=*/false,
          /*private_aggregation_manager=*/nullptr, main_frame_origin_,
          /*winner_origin=*/report_destination_origin_);

  // Calls SendPrivateAggregationRequestsForEvent() with "event_type".
  // "event_type" should be ignored and not be queued.
  reporter->SendPrivateAggregationRequestsForEvent("event_type");
  EXPECT_TRUE(reporter->GetReceivedPaEventsForTesting().empty());
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
}

TEST_F(FencedFrameReporterTest, AttributionManagerShutDown_NoCrash) {
  EXPECT_TRUE(attribution_manager());

  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(
          shared_url_loader_factory(), browser_context(),
          /*reporting_url_map=*/
          {{"event_type", report_destination_}});

  // Make a report.
  std::string error_message;
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  EXPECT_TRUE(reporter->SendReport(
      DestinationEnumEvent("event_type", "event_data"),
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      main_rfh_impl(), network::AttributionReportingRuntimeFeatures(),
      error_message, console_message_level));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_, "event_data");

  ShutDownAttributionManager();

  EXPECT_FALSE(attribution_manager());

  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      report_destination_.spec(), ""));
}

}  // namespace
}  // namespace content
