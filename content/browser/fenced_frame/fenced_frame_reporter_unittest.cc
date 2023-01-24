// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_reporter.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FencedFrameReporterTest : public testing::Test {
 public:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return test_url_loader_factory_.GetSafeWeakWrapper();
  }

  void ValidateRequest(const network::ResourceRequest& request,
                       const GURL& expected_url,
                       const std::string& event_data) {
    EXPECT_EQ(request.url, expected_url);
    EXPECT_EQ(request.mode, network::mojom::RequestMode::kCors);
    EXPECT_EQ(request.request_initiator, request_initiator_);
    EXPECT_EQ(request.credentials_mode, network::mojom::CredentialsMode::kOmit);
    EXPECT_EQ(request.method, net::HttpRequestHeaders::kPostMethod);
    EXPECT_TRUE(request.trusted_params->isolation_info.network_isolation_key()
                    .IsTransient());

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
              event_data);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  const url::Origin request_initiator_{
      url::Origin::Create(GURL("https://initiator.test/"))};
  const GURL report_destination_{"https://report_destination.test"};
  const GURL report_destination2_{"https://report_destination2.test"};
  const GURL report_destination3_{"https://report_destination3.test"};
};

// ReportingDestination has no map.
TEST_F(FencedFrameReporterTest, NoReportNoMap) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(
          shared_url_loader_factory(),
          /*reporting_url_map=*/{{"event_type", report_destination_}});
  std::string error_message;
  // FencedFrameReporters for Shared Storage a non-existent maps for FLEDGE
  // destinations.
  EXPECT_FALSE(
      reporter->SendReport("event_type", "event_data",
                           blink::FencedFrame::ReportingDestination::kBuyer,
                           request_initiator_, error_message));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'Buyer'.");
  EXPECT_FALSE(
      reporter->SendReport("event_type", "event_data",
                           blink::FencedFrame::ReportingDestination::kSeller,
                           request_initiator_, error_message));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'Seller'.");
  EXPECT_FALSE(reporter->SendReport(
      "event_type", "event_data",
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      request_initiator_, error_message));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'ComponentSeller'.");

  // A FLEDGE FencedFrameReporter has no map for Shared Storage.
  reporter = FencedFrameReporter::CreateForFledge(shared_url_loader_factory());
  EXPECT_FALSE(reporter->SendReport(
      "event_type", "event_data",
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      request_initiator_, error_message));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'SharedStorageSelectUrl'.");

  // No requests should have been made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// ReportingDestination has an empty map.
TEST_F(FencedFrameReporterTest, NoReportEmptyMap) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(shared_url_loader_factory(),
                                                  /*reporting_url_map=*/{});
  std::string error_message;
  EXPECT_FALSE(reporter->SendReport(
      "event_type", "event_data",
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      request_initiator_, error_message));
  EXPECT_EQ(error_message,
            "This frame did not register reporting metadata for destination "
            "'SharedStorageSelectUrl'.");

  // No requests should have been made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// Non-empty reporting URL map, but passed in event type isn't registered.
TEST_F(FencedFrameReporterTest, NoReportEventTypeNotRegistered) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(
          shared_url_loader_factory(), /*reporting_url_map=*/{
              {"registered_event_type", report_destination_}});
  std::string error_message;
  EXPECT_FALSE(reporter->SendReport(
      "unregistered_event_type", "event_data",
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      request_initiator_, error_message));
  EXPECT_EQ(
      error_message,
      "This frame did not register reporting url for destination "
      "'SharedStorageSelectUrl' and event_type 'unregistered_event_type'.");

  // No requests should have been made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

// Event types map to disallowed URLs (empty URLs, non-HTTP/HTTPS URLs).
TEST_F(FencedFrameReporterTest, NoReportBadUrl) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(
          shared_url_loader_factory(), /*reporting_url_map=*/{
              {"no_url", GURL()},
              {"data_url", GURL("data:,only http is allowed")}});
  std::string error_message;
  EXPECT_FALSE(reporter->SendReport(
      "no_url", "event_data",
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      request_initiator_, error_message));
  EXPECT_EQ(error_message,
            "This frame registered invalid reporting url for destination "
            "'SharedStorageSelectUrl' and event_type 'no_url'.");
  EXPECT_FALSE(reporter->SendReport(
      "data_url", "event_data",
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      request_initiator_, error_message));
  EXPECT_EQ(error_message,
            "This frame registered invalid reporting url for destination "
            "'SharedStorageSelectUrl' and event_type 'data_url'.");

  // No requests should have been made.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(FencedFrameReporterTest, SendReports) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForSharedStorage(
          shared_url_loader_factory(),
          /*reporting_url_map=*/{{"event_type", report_destination_},
                                 {"event_type2", report_destination2_}});

  // Make a report.
  std::string error_message;
  EXPECT_TRUE(reporter->SendReport(
      "event_type", "event_data",
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      request_initiator_, error_message));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_, "event_data");

  // Make another report to the same URL with different data. Should also
  // succeed.
  EXPECT_TRUE(reporter->SendReport(
      "event_type", "event_data2",
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      request_initiator_, error_message));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[1].request,
                  report_destination_, "event_data2");

  // Make a report using another event type.
  EXPECT_TRUE(reporter->SendReport(
      "event_type2", "event_data3",
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      request_initiator_, error_message));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[2].request,
                  report_destination2_, "event_data3");
}

// Test reports in the FLEDGE case, where reporting URL maps are received before
// SendReport() calls.
TEST_F(FencedFrameReporterTest, SendFledgeReportsAfterMapsReceived) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(shared_url_loader_factory());

  // Receive all mappings.
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kSeller,
      /*reporting_url_map=*/{{"event_type", report_destination_}});
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      /*reporting_url_map=*/{{"event_type", report_destination2_}});
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      /*reporting_url_map=*/{{"event_type", report_destination3_}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Make reports. Each should be sent immediately.

  std::string error_message;
  EXPECT_TRUE(
      reporter->SendReport("event_type", "event_data",
                           blink::FencedFrame::ReportingDestination::kSeller,
                           request_initiator_, error_message));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_, "event_data");

  EXPECT_TRUE(reporter->SendReport(
      "event_type", "event_data",
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      request_initiator_, error_message));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[1].request,
                  report_destination2_, "event_data");

  EXPECT_TRUE(
      reporter->SendReport("event_type", "event_data",
                           blink::FencedFrame::ReportingDestination::kBuyer,
                           request_initiator_, error_message));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[2].request,
                  report_destination3_, "event_data");
}

// Test reports in the FLEDGE case, where reporting URL maps are received after
// SendReport() calls.
TEST_F(FencedFrameReporterTest, SendReportsFledgeBeforeMapsReceived) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(shared_url_loader_factory());

  // Make reports. They should be queued, since mappings haven't been received
  // yet.
  std::string error_message;
  EXPECT_TRUE(
      reporter->SendReport("event_type", "event_data",
                           blink::FencedFrame::ReportingDestination::kSeller,
                           request_initiator_, error_message));
  EXPECT_TRUE(reporter->SendReport(
      "event_type", "event_data",
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      request_initiator_, error_message));
  EXPECT_TRUE(
      reporter->SendReport("event_type", "event_data",
                           blink::FencedFrame::ReportingDestination::kBuyer,
                           request_initiator_, error_message));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Each report should be sent as its mapping is received.

  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kSeller,
      /*reporting_url_map=*/{{"event_type", report_destination_}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[0].request,
                  report_destination_, "event_data");

  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      /*reporting_url_map=*/{{"event_type", report_destination2_}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[1].request,
                  report_destination2_, "event_data");

  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      /*reporting_url_map=*/{{"event_type", report_destination3_}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 3);
  ValidateRequest((*test_url_loader_factory_.pending_requests())[2].request,
                  report_destination3_, "event_data");
}

// Test reports in the FLEDGE case, where reporting URL maps are received after
// SendReport() calls, but no reports are sent because of errors (bad URL, no
// URL, missing event types). No error messages are generated in this case
// because there's nowhere to pass them
TEST_F(FencedFrameReporterTest, SendFledgeReportsBeforeMapsReceivedWithErrors) {
  scoped_refptr<FencedFrameReporter> reporter =
      FencedFrameReporter::CreateForFledge(shared_url_loader_factory());

  // SendReport() is called, and then a mapping is received that doesn't have
  // the report's event type. No request should be made.
  std::string error_message;
  EXPECT_TRUE(
      reporter->SendReport("event_type2", "event_data",
                           blink::FencedFrame::ReportingDestination::kSeller,
                           request_initiator_, error_message));
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kSeller,
      /*reporting_url_map=*/{{"event_type", report_destination_}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // SendReport() is called, and then a mapping is received that maps the
  // report's event type to a data URL. No request should be made.
  EXPECT_TRUE(reporter->SendReport(
      "event_type", "event_data",
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      request_initiator_, error_message));
  reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      /*reporting_url_map=*/{
          {"event_type", GURL("data:,only http is allowed")}});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // SendReport() is called, and then a mapping is received with an empty map.
  // No request should be made.
  EXPECT_TRUE(
      reporter->SendReport("event_type", "event_data",
                           blink::FencedFrame::ReportingDestination::kBuyer,
                           request_initiator_, error_message));
  reporter->OnUrlMappingReady(blink::FencedFrame::ReportingDestination::kBuyer,
                              /*reporting_url_map=*/{});
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

}  // namespace content
