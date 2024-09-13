// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report_network_sender.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;

using SentResult = ::content::SendResult::Sent::Result;

using ::testing::_;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;

using Checkpoint = ::testing::MockFunction<void(int)>;

const char kEventLevelReportUrl[] =
    "https://report.test/.well-known/attribution-reporting/"
    "report-event-attribution";
const char kDebugEventLevelReportUrl[] =
    "https://report.test/.well-known/attribution-reporting/debug/"
    "report-event-attribution";

const char kAggregatableReportUrl[] =
    "https://report.test/.well-known/attribution-reporting/"
    "report-aggregate-attribution";
const char kDebugAggregatableReportUrl[] =
    "https://report.test/.well-known/attribution-reporting/debug/"
    "report-aggregate-attribution";

const char kAggregatableDebugReportUrl[] =
    "https://report.test/.well-known/attribution-reporting/debug/"
    "report-aggregate-debug";

const char kAggregatableDebugReportMetricName[] =
    "Conversions.AggregatableDebugReport.HttpResponseOrNetErrorCode";

const char kVerboseDebugReportMetricName[] =
    "Conversions.VerboseDebugReport.HttpResponseOrNetErrorCode";

AttributionReport DefaultEventLevelReport() {
  return ReportBuilder(AttributionInfoBuilder().Build(),
                       SourceBuilder(base::Time()).BuildStored())
      .Build();
}

AttributionReport DefaultAggregatableReport(
    std::optional<std::string> trigger_context_id = std::nullopt,
    bool is_null_report = false) {
  ReportBuilder builder(AttributionInfoBuilder().Build(),
                        SourceBuilder().BuildStored());

  if (trigger_context_id.has_value()) {
    builder.SetTriggerContextId(*std::move(trigger_context_id))
        .SetSourceRegistrationTimeConfig(
            attribution_reporting::mojom::SourceRegistrationTimeConfig::
                kExclude);
  }
  return is_null_report ? builder.BuildNullAggregatable()
                        : builder.BuildAggregatableAttribution();
}

}  // namespace

class AttributionReportNetworkSenderTest : public testing::Test {
 public:
  AttributionReportNetworkSenderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        network_sender_(std::make_unique<AttributionReportNetworkSender>(
            test_url_loader_factory_.GetSafeWeakWrapper())) {}

 protected:
  // |task_environment_| must be initialized first.
  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  base::MockCallback<
      base::OnceCallback<void(const AttributionReport&, SendResult::Sent)>>
      callback_;

  // Unique ptr so it can be reset during testing.
  std::unique_ptr<AttributionReportNetworkSender> network_sender_;
};

TEST_F(AttributionReportNetworkSenderTest,
       ConversionReportReceived_NetworkRequestMade) {
  auto report = DefaultEventLevelReport();
  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              base::DoNothing());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kEventLevelReportUrl, ""));
}

TEST_F(AttributionReportNetworkSenderTest, LoadFlags) {
  auto report = DefaultEventLevelReport();
  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              base::DoNothing());
  int load_flags =
      test_url_loader_factory_.GetPendingRequest(0)->request.load_flags;
  EXPECT_TRUE(load_flags & net::LOAD_BYPASS_CACHE);
  EXPECT_TRUE(load_flags & net::LOAD_DISABLE_CACHE);
}

TEST_F(AttributionReportNetworkSenderTest, SameSite) {
  auto report = DefaultEventLevelReport();
  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              base::DoNothing());
  EXPECT_EQ(test_url_loader_factory_.GetPendingRequest(0)->request.mode,
            network::mojom::RequestMode::kSameOrigin);
  EXPECT_EQ(
      test_url_loader_factory_.GetPendingRequest(0)->request.request_initiator,
      report.reporting_origin());
}

TEST_F(AttributionReportNetworkSenderTest, Isolation) {
  auto report = DefaultEventLevelReport();
  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              base::DoNothing());
  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              base::DoNothing());

  const network::ResourceRequest& request1 =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  const network::ResourceRequest& request2 =
      test_url_loader_factory_.GetPendingRequest(1)->request;

  EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
            request1.trusted_params->isolation_info.request_type());
  EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
            request2.trusted_params->isolation_info.request_type());

  EXPECT_TRUE(request1.trusted_params->isolation_info.network_isolation_key()
                  .IsTransient());
  EXPECT_TRUE(request2.trusted_params->isolation_info.network_isolation_key()
                  .IsTransient());

  EXPECT_NE(request1.trusted_params->isolation_info.network_isolation_key(),
            request2.trusted_params->isolation_info.network_isolation_key());
}

TEST_F(AttributionReportNetworkSenderTest,
       ReportSent_ReportBodyAndURLSetCorrectly) {
  static constexpr char kExpectedReportBody[] =
      R"({"attribution_destination":"https://conversion.test",)"
      R"("randomized_trigger_rate":0.2,)"
      R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
      R"("scheduled_report_time":"3600",)"
      R"("source_event_id":"100",)"
      R"("source_type":"navigation",)"
      R"("trigger_data":"5"})";

  const struct {
    bool is_debug_report;
    const char* expected_url;
  } kTestCases[] = {
      {false, kEventLevelReportUrl},
      {true, kDebugEventLevelReportUrl},
  };

  const AttributionReport report =
      ReportBuilder(AttributionInfoBuilder()
                        .SetTime(base::Time::UnixEpoch() + base::Seconds(1))
                        .Build(),
                    SourceBuilder(base::Time::UnixEpoch())
                        .SetSourceEventId(100)
                        .SetRandomizedResponseRate(0.2)
                        .BuildStored())
          .SetTriggerData(5)
          .SetReportTime(base::Time::UnixEpoch() + base::Hours(1))
          .Build();

  for (const auto& test_case : kTestCases) {
    network_sender_->SendReport(report, test_case.is_debug_report,
                                base::DoNothing());

    const network::ResourceRequest* pending_request;
    EXPECT_TRUE(test_url_loader_factory_.IsPending(test_case.expected_url,
                                                   &pending_request));
    EXPECT_EQ(kExpectedReportBody, network::GetUploadData(*pending_request));
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        test_case.expected_url, ""));
  }
}

TEST_F(AttributionReportNetworkSenderTest, ReportSent_RequestAttributesSet) {
  network_sender_->SendReport(DefaultEventLevelReport(),
                              /*is_debug_report=*/false, base::DoNothing());

  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kEventLevelReportUrl,
                                                 &pending_request));

  // Ensure that the request is sent with no credentials.
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            pending_request->credentials_mode);
  EXPECT_EQ("POST", pending_request->method);

  EXPECT_EQ(GURL(), pending_request->referrer);
}

TEST_F(AttributionReportNetworkSenderTest, ReportSent_CallbackFired) {
  const auto report = DefaultEventLevelReport();

  static const net::HttpStatusCode kTestCases[] = {
      net::HTTP_OK,
      net::HTTP_CREATED,
      net::HTTP_ACCEPTED,
      net::HTTP_NON_AUTHORITATIVE_INFORMATION,
      net::HTTP_NO_CONTENT,
      net::HTTP_RESET_CONTENT,
      net::HTTP_PARTIAL_CONTENT,
  };

  for (net::HttpStatusCode code : kTestCases) {
    EXPECT_CALL(callback_,
                Run(report, SendResult::Sent(SentResult::kSent, code)));

    network_sender_->SendReport(report, /*is_debug_report=*/false,
                                callback_.Get());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kEventLevelReportUrl, "", code));

    Mock::VerifyAndClear(&callback_);
  }
}

TEST_F(AttributionReportNetworkSenderTest, SenderDeletedDuringRequest_NoCrash) {
  EXPECT_CALL(callback_, Run).Times(0);

  auto report = DefaultEventLevelReport();
  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              callback_.Get());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  network_sender_.reset();
  EXPECT_FALSE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kEventLevelReportUrl, ""));
}

TEST_F(AttributionReportNetworkSenderTest, ReportRequestHangs_TimesOut) {
  auto report = DefaultEventLevelReport();

  // Verify that the sent callback runs if the request times out.
  EXPECT_CALL(callback_,
              Run(report, SendResult::Sent(SentResult::kTransientFailure,
                                           net::ERR_TIMED_OUT)));
  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              callback_.Get());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // The request should time out after 30 seconds.
  task_environment_.FastForwardBy(base::Seconds(30));

  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(AttributionReportNetworkSenderTest,
       ReportRequestFailsWithTargetedError_ShouldRetrySet) {
  struct {
    int net_error;
    SentResult expected_status;
  } kTestCases[] = {
      {net::ERR_INTERNET_DISCONNECTED, SentResult::kTransientFailure},
      {net::ERR_TIMED_OUT, SentResult::kTransientFailure},
      {net::ERR_CONNECTION_ABORTED, SentResult::kTransientFailure},
      {net::ERR_CONNECTION_TIMED_OUT, SentResult::kTransientFailure},
      {net::ERR_CONNECTION_REFUSED, SentResult::kFailure},
      {net::ERR_CERT_DATE_INVALID, SentResult::kFailure},
      {net::OK, SentResult::kFailure},
  };

  for (const auto& test_case : kTestCases) {
    auto report = DefaultEventLevelReport();

    EXPECT_CALL(callback_,
                Run(report, SendResult::Sent(test_case.expected_status,
                                             test_case.net_error)));

    network_sender_->SendReport(report, /*is_debug_report=*/false,
                                callback_.Get());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // By default, headers are not sent for network errors.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kEventLevelReportUrl),
        network::URLLoaderCompletionStatus(test_case.net_error),
        network::mojom::URLResponseHead::New(), "");

    Mock::VerifyAndClear(&callback_);
  }
}

TEST_F(AttributionReportNetworkSenderTest,
       ReportRequestFailsWithHeaders_NotRetried) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  // Simulate a retry-able network error with headers received.
  test_url_loader_factory_.AddResponse(
      GURL(kEventLevelReportUrl),
      /*head=*/std::move(head), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);

  auto report = DefaultEventLevelReport();
  EXPECT_CALL(callback_,
              Run(report, SendResult::Sent(SentResult::kFailure,
                                           net::ERR_INTERNET_DISCONNECTED)));
  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              callback_.Get());

  // Ensure the request was replied to.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(AttributionReportNetworkSenderTest,
       ReportRequestFailsWithHttpError_ShouldRetryNotSet) {
  auto report = DefaultEventLevelReport();
  EXPECT_CALL(callback_, Run(report, SendResult::Sent(
                                         SentResult::kFailure,
                                         net::ERR_HTTP_RESPONSE_CODE_FAILURE)));

  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              callback_.Get());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kEventLevelReportUrl, "", net::HttpStatusCode::HTTP_BAD_REQUEST));
}

TEST_F(AttributionReportNetworkSenderTest,
       ReportRequestFailsDueToNetworkChange_Retries) {
  // Retry fails
  {
    base::HistogramTester histograms;

    EXPECT_CALL(callback_, Run);

    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/false,
                                callback_.Get());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kEventLevelReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), "");

    // The sender should automatically retry.
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate a second request failure due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kEventLevelReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), "");

    // We should not retry again. Verify the report sent callback only gets
    // fired once.
    EXPECT_EQ(0, test_url_loader_factory_.NumPending());
    Mock::VerifyAndClear(&callback_);

    histograms.ExpectUniqueSample("Conversions.ReportRetrySucceedEventLevel",
                                  false, 1);
  }

  // Retry succeeds
  {
    base::HistogramTester histograms;

    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/false,
                                base::DoNothing());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kEventLevelReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), "");

    // The sender should automatically retry.
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate a second request failure due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        kEventLevelReportUrl, "");

    histograms.ExpectUniqueSample("Conversions.ReportRetrySucceedEventLevel",
                                  true, 1);
  }
}

TEST_F(AttributionReportNetworkSenderTest,
       ReportResultsInHttpError_SentCallbackRuns) {
  auto report = DefaultEventLevelReport();

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(
        callback_,
        Run(report, SendResult::Sent(SentResult::kFailure,
                                     net::ERR_HTTP_RESPONSE_CODE_FAILURE)));
  }

  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              callback_.Get());
  checkpoint.Call(1);

  // We should run the sent callback even if there is an http error.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kEventLevelReportUrl, "", net::HttpStatusCode::HTTP_BAD_REQUEST));
}

TEST_F(AttributionReportNetworkSenderTest, ManyReports_AllSentSuccessfully) {
  EXPECT_CALL(callback_, Run).Times(10);

  for (int i = 0; i < 10; i++) {
    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/false,
                                callback_.Get());
  }
  EXPECT_EQ(10, test_url_loader_factory_.NumPending());

  // Send reports out of order to guarantee that callback conversion_ids are
  // properly handled.
  for (int i = 9; i >= 0; i--) {
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kEventLevelReportUrl, ""));
  }
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}


TEST_F(AttributionReportNetworkSenderTest, ReportRedirects) {
  auto report = DefaultEventLevelReport();
  EXPECT_CALL(callback_,
              Run(report, SendResult::Sent(SentResult::kSent, net::HTTP_OK)));

  const GURL kNewUrl(
      "https://report2.test/.well-known/attribution-reporting/"
      "report-event-attribution");

  net::RedirectInfo redirect_info;
  redirect_info.status_code = net::HTTP_MOVED_PERMANENTLY;
  redirect_info.new_url = kNewUrl;
  redirect_info.new_method = "POST";
  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, network::mojom::URLResponseHead::New());
  network::URLLoaderCompletionStatus status;
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  test_url_loader_factory_.AddResponse(
      GURL(kEventLevelReportUrl), std::move(head), "", status,
      std::move(redirects),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);

  network_sender_->SendReport(report, /*is_debug_report=*/false,
                              callback_.Get());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(AttributionReportNetworkSenderTest,
       EventLevelReportSent_MetricsRecorded) {
  static constexpr char kStatusMetric[] = "Conversions.ReportStatusEventLevel";
  static constexpr char kErrorCodeMetric[] =
      "Conversions.HttpResponseOrNetErrorCodeEventLevel";
  static constexpr char kReportSizeMetric[] =
      "Conversions.EventLevelReport.ReportBodySize";

  // All OK
  {
    base::HistogramTester histograms;
    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/false,
                                base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kEventLevelReportUrl, ""));
    // kOk = 0.
    histograms.ExpectUniqueSample(kStatusMetric, 0, 1);
    histograms.ExpectUniqueSample(kErrorCodeMetric, net::HTTP_OK, 1);
    histograms.ExpectTotalCount(kReportSizeMetric, 1);
  }

  // Internal error
  {
    base::HistogramTester histograms;
    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/false,
                                base::DoNothing());
    network::URLLoaderCompletionStatus completion_status(net::ERR_FAILED);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kEventLevelReportUrl), completion_status,
        network::mojom::URLResponseHead::New(), ""));
    // kInternalError = 1.
    histograms.ExpectUniqueSample(kStatusMetric, 1, 1);
    histograms.ExpectUniqueSample(kErrorCodeMetric, net::ERR_FAILED, 1);
    histograms.ExpectTotalCount(kReportSizeMetric, 1);
  }
  // External error
  {
    base::HistogramTester histograms;
    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/false,
                                base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kEventLevelReportUrl, "", net::HTTP_UNAUTHORIZED));
    // kExternalError = 2.
    histograms.ExpectUniqueSample(kStatusMetric, 2, 1);
    histograms.ExpectUniqueSample(kErrorCodeMetric, net::HTTP_UNAUTHORIZED, 1);
    histograms.ExpectTotalCount(kReportSizeMetric, 1);
  }
  // Retried network change error
  {
    base::HistogramTester histograms;
    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/false,
                                base::DoNothing());

    ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kEventLevelReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), ""));

    ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kEventLevelReportUrl, ""));

    histograms.ExpectUniqueSample("Conversions.ReportRetrySucceedEventLevel",
                                  true, 1);
    histograms.ExpectTotalCount(kReportSizeMetric, 1);
  }
}

TEST_F(AttributionReportNetworkSenderTest,
       EventLevelReportSent_DebugMetricsRecorded) {
  // All OK
  {
    base::HistogramTester histograms;
    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/true,
                                base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kDebugEventLevelReportUrl, ""));
    // kOk = 0.
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.ReportStatusEventLevel", 0, 1);
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.HttpResponseOrNetErrorCodeEventLevel",
        net::HTTP_OK, 1);
  }

  // Internal error
  {
    base::HistogramTester histograms;
    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/true,
                                base::DoNothing());
    network::URLLoaderCompletionStatus completion_status(net::ERR_FAILED);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kDebugEventLevelReportUrl), completion_status,
        network::mojom::URLResponseHead::New(), ""));
    // kInternalError = 1.
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.ReportStatusEventLevel", 1, 1);
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.HttpResponseOrNetErrorCodeEventLevel",
        net::ERR_FAILED, 1);
  }
  // External error
  {
    base::HistogramTester histograms;
    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/true,
                                base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kDebugEventLevelReportUrl, "", net::HTTP_UNAUTHORIZED));
    // kExternalError = 2.
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.ReportStatusEventLevel", 2, 1);
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.HttpResponseOrNetErrorCodeEventLevel",
        net::HTTP_UNAUTHORIZED, 1);
  }
  // Retried network change error
  {
    base::HistogramTester histograms;
    auto report = DefaultEventLevelReport();
    network_sender_->SendReport(report, /*is_debug_report=*/true,
                                base::DoNothing());

    ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kDebugEventLevelReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), ""));

    ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kDebugEventLevelReportUrl, ""));

    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.ReportRetrySucceedEventLevel", true, 1);
  }
}

TEST_F(AttributionReportNetworkSenderTest,
       AggregatableReportSent_MetricsRecorded) {
  const auto verify_histogram = [](base::HistogramTester& histograms,
                                   std::string_view suffix,
                                   bool has_trigger_context_id,
                                   base::HistogramBase::Sample sample,
                                   base::HistogramBase::Count count) {
    histograms.ExpectUniqueSample(base::StrCat({"Conversions.", suffix}),
                                  sample, count);
    if (has_trigger_context_id) {
      histograms.ExpectUniqueSample(
          base::StrCat({"Conversions.ContextID.", suffix}), sample, count);
    } else {
      histograms.ExpectUniqueSample(
          base::StrCat({"Conversions.NoContextID.", suffix}), sample, count);
    }
  };

  static constexpr char kStatusMetric[] = "ReportStatusAggregatable";
  static constexpr char kErrorCodeMetric[] =
      "HttpResponseOrNetErrorCodeAggregatable";
  static constexpr char kReportSizeMetric[] =
      "Conversions.AggregatableReport.ReportBodySize";

  for (const bool has_trigger_context_id : {false, true}) {
    SCOPED_TRACE(has_trigger_context_id);

    std::optional<std::string> trigger_context_id;
    if (has_trigger_context_id) {
      trigger_context_id.emplace();
    }

    // All OK
    {
      base::HistogramTester histograms;
      auto report = DefaultAggregatableReport(trigger_context_id);
      network_sender_->SendReport(report, /*is_debug_report=*/false,
                                  base::DoNothing());
      EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
          kAggregatableReportUrl, ""));
      // kOk = 0.
      verify_histogram(histograms, kStatusMetric, has_trigger_context_id, 0, 1);
      verify_histogram(histograms, kErrorCodeMetric, has_trigger_context_id,
                       net::HTTP_OK, 1);
      histograms.ExpectTotalCount(kReportSizeMetric, 1);
    }

    // Internal error
    {
      base::HistogramTester histograms;
      auto report = DefaultAggregatableReport(trigger_context_id);
      network_sender_->SendReport(report, /*is_debug_report=*/false,
                                  base::DoNothing());
      network::URLLoaderCompletionStatus completion_status(net::ERR_FAILED);
      EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
          GURL(kAggregatableReportUrl), completion_status,
          network::mojom::URLResponseHead::New(), ""));
      // kInternalError = 1.
      verify_histogram(histograms, kStatusMetric, has_trigger_context_id, 1, 1);
      verify_histogram(histograms, kErrorCodeMetric, has_trigger_context_id,
                       net::ERR_FAILED, 1);
      histograms.ExpectTotalCount(kReportSizeMetric, 1);
    }
    // External error
    {
      base::HistogramTester histograms;
      auto report = DefaultAggregatableReport(trigger_context_id);
      network_sender_->SendReport(report, /*is_debug_report=*/false,
                                  base::DoNothing());
      EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
          kAggregatableReportUrl, "", net::HTTP_UNAUTHORIZED));
      // kExternalError = 2.
      verify_histogram(histograms, kStatusMetric, has_trigger_context_id, 2, 1);
      verify_histogram(histograms, kErrorCodeMetric, has_trigger_context_id,
                       net::HTTP_UNAUTHORIZED, 1);
      histograms.ExpectTotalCount(kReportSizeMetric, 1);
    }
    // Retried network change error
    {
      base::HistogramTester histograms;
      auto report = DefaultAggregatableReport(trigger_context_id);
      network_sender_->SendReport(report, /*is_debug_report=*/false,
                                  base::DoNothing());

      ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
          GURL(kAggregatableReportUrl),
          network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
          network::mojom::URLResponseHead::New(), ""));

      ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
          kAggregatableReportUrl, ""));

      verify_histogram(histograms, "ReportRetrySucceedAggregatable",
                       has_trigger_context_id, 1, 1);
      histograms.ExpectTotalCount(kReportSizeMetric, 1);
    }
  }
}

TEST_F(AttributionReportNetworkSenderTest,
       AggregatableReportSent_DebugMetricsRecorded) {
  // All OK
  {
    base::HistogramTester histograms;
    auto report = DefaultAggregatableReport();
    network_sender_->SendReport(report, /*is_debug_report=*/true,
                                base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kDebugAggregatableReportUrl, ""));
    // kOk = 0.
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.ReportStatusAggregatable", 0, 1);
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.HttpResponseOrNetErrorCodeAggregatable",
        net::HTTP_OK, 1);
  }

  // Internal error
  {
    base::HistogramTester histograms;
    auto report = DefaultAggregatableReport();
    network_sender_->SendReport(report, /*is_debug_report=*/true,
                                base::DoNothing());
    network::URLLoaderCompletionStatus completion_status(net::ERR_FAILED);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kDebugAggregatableReportUrl), completion_status,
        network::mojom::URLResponseHead::New(), ""));
    // kInternalError = 1.
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.ReportStatusAggregatable", 1, 1);
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.HttpResponseOrNetErrorCodeAggregatable",
        net::ERR_FAILED, 1);
  }
  // External error
  {
    base::HistogramTester histograms;
    auto report = DefaultAggregatableReport();
    network_sender_->SendReport(report, /*is_debug_report=*/true,
                                base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kDebugAggregatableReportUrl, "", net::HTTP_UNAUTHORIZED));
    // kExternalError = 2.
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.ReportStatusAggregatable", 2, 1);
    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.HttpResponseOrNetErrorCodeAggregatable",
        net::HTTP_UNAUTHORIZED, 1);
  }
  // Retried network change error
  {
    base::HistogramTester histograms;
    auto report = DefaultAggregatableReport();
    network_sender_->SendReport(report, /*is_debug_report=*/true,
                                base::DoNothing());

    ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kDebugAggregatableReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), ""));

    ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kDebugAggregatableReportUrl, ""));

    histograms.ExpectUniqueSample(
        "Conversions.DebugReport.ReportRetrySucceedAggregatable", true, 1);
  }
}

TEST_F(AttributionReportNetworkSenderTest,
       ErrorReportSent_ReportBodySetCorrectly) {
  base::HistogramTester histograms;

  static constexpr char kExpectedReportBody[] =
      R"([{)"
      R"("body":{)"
      R"("attribution_destination":"https://conversion.test",)"
      R"("limit":"3",)"
      R"("source_event_id":"123",)"
      R"("source_site":"https://impression.test"},)"
      R"("type":"source-destination-limit")"
      R"(}])";

  static constexpr char kErrorReportUrl[] =
      "https://report.test/.well-known/attribution-reporting/debug/verbose";

  std::optional<AttributionDebugReport> report = AttributionDebugReport::Create(
      /*is_operation_allowed=*/[]() { return true; },
      StoreSourceResult(
          SourceBuilder()
              .SetDebugReporting(true)
              .SetDebugCookieSet(true)
              .Build(),
          /*is_noised=*/false, /*source_time=*/base::Time::Now(),
          /*destination_limit=*/std::nullopt,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(3)));
  ASSERT_TRUE(report);

  base::MockCallback<AttributionReportSender::DebugReportSentCallback> callback;
  EXPECT_CALL(callback, Run(_, 200));

  network_sender_->SendReport(*std::move(report), callback.Get());

  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(
      test_url_loader_factory_.IsPending(kErrorReportUrl, &pending_request));
  EXPECT_EQ(kExpectedReportBody, network::GetUploadData(*pending_request));
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kErrorReportUrl, ""));

  histograms.ExpectUniqueSample(kVerboseDebugReportMetricName,
                                net::HttpStatusCode::HTTP_OK, 1);
}

TEST_F(AttributionReportNetworkSenderTest,
       ErrorReportSent_CallbackInvokedWithNetworkError) {
  base::HistogramTester histograms;

  static constexpr char kErrorReportUrl[] =
      "https://report.test/.well-known/attribution-reporting/debug/verbose";

  std::optional<AttributionDebugReport> report = AttributionDebugReport::Create(
      /*is_operation_allowed=*/[]() { return true; },
      StoreSourceResult(
          SourceBuilder()
              .SetDebugReporting(true)
              .SetDebugCookieSet(true)
              .Build(),
          /*is_noised=*/false, /*source_time=*/base::Time::Now(),
          /*destination_limit=*/std::nullopt,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(3)));
  ASSERT_TRUE(report);

  base::MockCallback<AttributionReportSender::DebugReportSentCallback> callback;
  EXPECT_CALL(callback, Run(_, net::ERR_CONNECTION_ABORTED));

  network_sender_->SendReport(*std::move(report), callback.Get());

  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(
      test_url_loader_factory_.IsPending(kErrorReportUrl, &pending_request));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kErrorReportUrl),
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_ABORTED),
      network::mojom::URLResponseHead::New(), "");

  histograms.ExpectUniqueSample(kVerboseDebugReportMetricName,
                                net::ERR_CONNECTION_ABORTED, 1);
}

TEST_F(AttributionReportNetworkSenderTest, AggregatableDebugReportSent) {
  base::HistogramTester histograms;

  base::MockCallback<
      AttributionReportSender::AggregatableDebugReportSentCallback>
      callback;
  EXPECT_CALL(callback, Run(_, _, 200));

  network_sender_->SendReport(
      AggregatableDebugReport::CreateForTesting(
          /*contributions=*/{},
          /*context_site=*/net::SchemefulSite::Deserialize("https://c.test"),
          /*reporting_origin=*/
          *SuitableOrigin::Deserialize("https://report.test"),
          /*effective_destination=*/
          net::SchemefulSite::Deserialize("https://d.test"),
          /*aggregation_coordinator_origin=*/std::nullopt,
          /*scheduled_report_time=*/base::Time::Now()),
      /*report_body=*/base::Value::Dict(), callback.Get());

  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kAggregatableDebugReportUrl,
                                                 &pending_request));
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kAggregatableDebugReportUrl, ""));

  histograms.ExpectUniqueSample(kAggregatableDebugReportMetricName,
                                net::HttpStatusCode::HTTP_OK, 1);
}

TEST_F(AttributionReportNetworkSenderTest,
       AggregatableDebugReportSent_CallbackInvokedWithNetworkError) {
  base::HistogramTester histograms;

  base::MockCallback<
      AttributionReportSender::AggregatableDebugReportSentCallback>
      callback;
  EXPECT_CALL(callback, Run(_, _, net::ERR_CONNECTION_ABORTED));

  network_sender_->SendReport(
      AggregatableDebugReport::CreateForTesting(
          /*contributions=*/{},
          /*context_site=*/net::SchemefulSite::Deserialize("https://c.test"),
          /*reporting_origin=*/
          *SuitableOrigin::Deserialize("https://report.test"),
          /*effective_destination=*/
          net::SchemefulSite::Deserialize("https://d.test"),
          /*aggregation_coordinator_origin=*/std::nullopt,
          /*scheduled_report_time=*/base::Time::Now()),
      /*report_body=*/base::Value::Dict(), callback.Get());

  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kAggregatableDebugReportUrl,
                                                 &pending_request));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kAggregatableDebugReportUrl),
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_ABORTED),
      network::mojom::URLResponseHead::New(), "");

  histograms.ExpectUniqueSample(kAggregatableDebugReportMetricName,
                                net::ERR_CONNECTION_ABORTED, 1);
}

TEST_F(AttributionReportNetworkSenderTest,
       NullAggregatableReportSent_MetricsRecorded) {
  const auto verify_histogram = [](base::HistogramTester& histograms,
                                   std::string_view suffix,
                                   bool has_trigger_context_id,
                                   base::HistogramBase::Sample sample,
                                   base::HistogramBase::Count count) {
    histograms.ExpectUniqueSample(base::StrCat({"Conversions.", suffix}),
                                  sample, count);
    if (has_trigger_context_id) {
      histograms.ExpectUniqueSample(
          base::StrCat({"Conversions.ContextID.", suffix}), sample, count);
    } else {
      histograms.ExpectUniqueSample(
          base::StrCat({"Conversions.NoContextID.", suffix}), sample, count);
    }
  };

  for (const bool has_trigger_context_id : {false, true}) {
    SCOPED_TRACE(has_trigger_context_id);

    std::optional<std::string> trigger_context_id;
    if (has_trigger_context_id) {
      trigger_context_id.emplace();
    }

    base::HistogramTester histograms;
    network_sender_->SendReport(
        DefaultAggregatableReport(trigger_context_id,
                                  /*is_null_report=*/true),
        /*is_debug_report=*/false, base::DoNothing());

    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kAggregatableReportUrl, ""));
    // kOk = 0.
    verify_histogram(histograms, "ReportStatusAggregatableNull",
                     has_trigger_context_id, 0, 1);
    verify_histogram(histograms, "HttpResponseOrNetErrorCodeAggregatableNull",
                     has_trigger_context_id, net::HTTP_OK, 1);
    histograms.ExpectTotalCount("Conversions.AggregatableReport.ReportBodySize",
                                1);
  }
}

}  // namespace content
