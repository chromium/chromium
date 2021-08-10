// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_network_sender_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/conversions/sent_report_info.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

const char kReportUrl[] =
    "https://report.test/.well-known/attribution-reporting/report-attribution";

// Create a simple report where impression data/conversion data/conversion id
// are all the same.
ConversionReport GetReport(int64_t conversion_id) {
  return ConversionReport(
      ImpressionBuilder(base::Time()).SetData(conversion_id).Build(),
      /*conversion_data=*/conversion_id,
      /*conversion_time=*/base::Time(),
      /*report_time=*/base::Time(),
      /*conversion_id=*/conversion_id);
}

}  // namespace

class ConversionNetworkSenderTest : public testing::Test {
 public:
  ConversionNetworkSenderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        network_sender_(std::make_unique<ConversionNetworkSenderImpl>(
            /*sotrage_partition=*/nullptr)),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    network_sender_->SetURLLoaderFactoryForTesting(shared_url_loader_factory_);
  }

  ConversionReporterImpl::NetworkSender::ReportSentCallback GetSentCallback() {
    return base::BindOnce(&ConversionNetworkSenderTest::OnReportSent,
                          base::Unretained(this));
  }

 protected:
  size_t num_reports_sent() const { return sent_reports_.size(); }

  base::circular_deque<SentReportInfo> sent_reports_;

  // |task_enviorment_| must be initialized first.
  content::BrowserTaskEnvironment task_environment_;

  // Unique ptr so it can be reset during testing.
  std::unique_ptr<ConversionNetworkSenderImpl> network_sender_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  void OnReportSent(SentReportInfo info) { sent_reports_.push_back(info); }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(ConversionNetworkSenderTest,
       ConversionReportReceived_NetworkRequestMade) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(report, std::move(base::DoNothing()));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, ""));
}

TEST_F(ConversionNetworkSenderTest, LoadFlags) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(report, GetSentCallback());
  int load_flags =
      test_url_loader_factory_.GetPendingRequest(0)->request.load_flags;
  EXPECT_TRUE(load_flags & net::LOAD_BYPASS_CACHE);
  EXPECT_TRUE(load_flags & net::LOAD_DISABLE_CACHE);
}

TEST_F(ConversionNetworkSenderTest, ReportSent_QueryParamsSetCorrectly) {
  auto impression =
      ImpressionBuilder(base::Time())
          .SetData(100)
          .SetReportingOrigin(url::Origin::Create(GURL("https://a.com")))
          .Build();
  ConversionReport report(impression,
                          /*conversion_data=*/5,
                          /*conversion_time=*/base::Time(),
                          /*report_time=*/base::Time(),
                          /*conversion_id=*/1);
  network_sender_->SendReport(report, base::DoNothing());

  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(test_url_loader_factory_.IsPending(
      "https://a.com/.well-known/attribution-reporting/report-attribution",
      &pending_request));
  EXPECT_EQ(R"({"source_event_id":"100","trigger_data":"5"})",
            network::GetUploadData(*pending_request));
}

TEST_F(ConversionNetworkSenderTest, ReportSent_RequestAttributesSet) {
  auto impression =
      ImpressionBuilder(base::Time())
          .SetData(1)
          .SetReportingOrigin(url::Origin::Create(GURL("https://a.com")))
          .SetConversionOrigin(url::Origin::Create(GURL("https://sub.b.com")))
          .Build();
  ConversionReport report(impression,
                          /*conversion_data=*/1,
                          /*conversion_time=*/base::Time(),
                          /*report_time=*/base::Time(),
                          /*conversion_id=*/1);
  network_sender_->SendReport(report, base::DoNothing());

  const network::ResourceRequest* pending_request;
  std::string expected_report_url(
      "https://a.com/.well-known/attribution-reporting/report-attribution");
  EXPECT_TRUE(test_url_loader_factory_.IsPending(expected_report_url,
                                                 &pending_request));

  // Ensure that the request is sent with no credentials.
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            pending_request->credentials_mode);
  EXPECT_EQ("POST", pending_request->method);

  // Make sure the domain is used as the referrer.
  EXPECT_EQ(GURL("https://b.com"), pending_request->referrer);
}

TEST_F(ConversionNetworkSenderTest, ReportSent_CallbackFired) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(report, GetSentCallback());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, ""));
  EXPECT_THAT(
      sent_reports_,
      ElementsAre(SentReportInfo(
          *report.conversion_id, report.original_report_time, GURL(kReportUrl),
          R"({"source_event_id":"1","trigger_data":"1"})",
          /*http_response_code=*/200, /*should_retry=*/false)));
}

TEST_F(ConversionNetworkSenderTest, SenderDeletedDuringRequest_NoCrash) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(report, GetSentCallback());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  network_sender_.reset();
  EXPECT_FALSE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, ""));
  EXPECT_THAT(sent_reports_, IsEmpty());
}

TEST_F(ConversionNetworkSenderTest, ReportRequestHangs_TimesOut) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(report, GetSentCallback());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // The request should time out after 30 seconds.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(30));

  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Also verify that the sent callback runs if the request times out.
  // TODO(apaseltiner): Should we propagate the timeout via the SentReportInfo
  // instead of just setting |http_response_code = 0|?
  EXPECT_THAT(
      sent_reports_,
      ElementsAre(SentReportInfo(
          *report.conversion_id, report.original_report_time, GURL(kReportUrl),
          R"({"source_event_id":"1","trigger_data":"1"})",
          /*http_response_code=*/0, /*should_retry=*/true)));
}

TEST_F(ConversionNetworkSenderTest,
       ReportRequestFailsWithTargetedError_ShouldRetrySet) {
  struct {
    int net_error;
    bool should_retry;
  } kTestCases[] = {
      {.net_error = net::ERR_INTERNET_DISCONNECTED, .should_retry = true},
      {.net_error = net::ERR_TIMED_OUT, .should_retry = true},
      {.net_error = net::ERR_CONNECTION_ABORTED, .should_retry = true},
      {.net_error = net::ERR_CONNECTION_TIMED_OUT, .should_retry = true},
      {.net_error = net::ERR_CONNECTION_REFUSED, .should_retry = false},
      {.net_error = net::ERR_CERT_DATE_INVALID, .should_retry = false},
      {.net_error = net::OK, .should_retry = false},
  };

  for (const auto& test_case : kTestCases) {
    auto report = GetReport(/*conversion_id=*/1);
    network_sender_->SendReport(report, GetSentCallback());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // By default, headers are not sent for network errors.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl),
        network::URLLoaderCompletionStatus(test_case.net_error),
        network::mojom::URLResponseHead::New(), std::string());

    EXPECT_EQ(test_case.should_retry, sent_reports_.back().should_retry);
  }
}

TEST_F(ConversionNetworkSenderTest, ReportRequestFailsWithHeaders_NotRetried) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  // Simulate a retry-able network error with headers received.
  test_url_loader_factory_.AddResponse(
      GURL(kReportUrl),
      /*head=*/std::move(head), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);

  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(report, GetSentCallback());

  // Ensure the request was replied to.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  EXPECT_EQ(1u, num_reports_sent());
  EXPECT_EQ(false, sent_reports_.back().should_retry);
}

TEST_F(ConversionNetworkSenderTest,
       ReportRequestFailsWithHttpError_ShouldRetryNotSet) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(report, GetSentCallback());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, "", net::HttpStatusCode::HTTP_BAD_REQUEST));

  EXPECT_EQ(1u, num_reports_sent());
  EXPECT_EQ(false, sent_reports_[0].should_retry);
}

TEST_F(ConversionNetworkSenderTest,
       ReportRequestFailsDueToNetworkChange_Retries) {
  // Retry fails
  {
    base::HistogramTester histograms;

    auto report = GetReport(/*conversion_id=*/1);
    network_sender_->SendReport(report, GetSentCallback());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // The sender should automatically retry.
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate a second request failure due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // We should not retry again. Verify the report sent callback only gets
    // fired once.
    EXPECT_EQ(0, test_url_loader_factory_.NumPending());
    EXPECT_EQ(1u, num_reports_sent());

    histograms.ExpectUniqueSample("Conversions.ReportRetrySucceed", false, 1);
  }

  // Retry succeeds
  {
    base::HistogramTester histograms;

    auto report = GetReport(/*conversion_id=*/2);
    network_sender_->SendReport(report, GetSentCallback());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // The sender should automatically retry.
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate a second request failure due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(kReportUrl, "");

    histograms.ExpectUniqueSample("Conversions.ReportRetrySucceed", true, 1);
  }
}

TEST_F(ConversionNetworkSenderTest, ReportResultsInHttpError_SentCallbackRuns) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(report, GetSentCallback());
  EXPECT_THAT(sent_reports_, IsEmpty());

  // We should run the sent callback even if there is an http error.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, "", net::HttpStatusCode::HTTP_BAD_REQUEST));
  EXPECT_THAT(
      sent_reports_,
      ElementsAre(SentReportInfo(
          *report.conversion_id, report.original_report_time, GURL(kReportUrl),
          R"({"source_event_id":"1","trigger_data":"1"})",
          /*http_response_code=*/net::HttpStatusCode::HTTP_BAD_REQUEST,
          /*should_retry=*/false)));
}

TEST_F(ConversionNetworkSenderTest, ManyReports_AllSentSuccessfully) {
  for (int i = 0; i < 10; i++) {
    auto report = GetReport(/*conversion_id=*/i);
    network_sender_->SendReport(report, GetSentCallback());
  }
  EXPECT_EQ(10, test_url_loader_factory_.NumPending());

  // Send reports out of order to guarantee that callback conversion_ids are
  // properly handled.
  for (int i = 9; i >= 0; i--) {
    std::string report_id = base::NumberToString(i);

    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kReportUrl, ""));
  }
  EXPECT_EQ(10u, num_reports_sent());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(ConversionNetworkSenderTest, ErrorHistogram) {
  // All OK.
  {
    base::HistogramTester histograms;
    auto report = GetReport(/*conversion_id=*/1);
    network_sender_->SendReport(report, GetSentCallback());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kReportUrl, ""));
    // kOk = 0.
    histograms.ExpectUniqueSample("Conversions.ReportStatus", 0, 1);
    histograms.ExpectUniqueSample(
        "Conversions.Report.HttpResponseOrNetErrorCode", net::HTTP_OK, 1);
  }
  // Internal error.
  {
    base::HistogramTester histograms;
    auto report = GetReport(/*conversion_id=*/2);
    network_sender_->SendReport(report, GetSentCallback());
    network::URLLoaderCompletionStatus completion_status(net::ERR_FAILED);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl), completion_status,
        network::mojom::URLResponseHead::New(), std::string()));
    // kInternalError = 1.
    histograms.ExpectUniqueSample("Conversions.ReportStatus", 1, 1);
    histograms.ExpectUniqueSample(
        "Conversions.Report.HttpResponseOrNetErrorCode", net::ERR_FAILED, 1);
  }
  {
    base::HistogramTester histograms;
    auto report = GetReport(/*conversion_id=*/3);
    network_sender_->SendReport(report, GetSentCallback());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kReportUrl, std::string(), net::HTTP_UNAUTHORIZED));
    // kExternalError = 2.
    histograms.ExpectUniqueSample("Conversions.ReportStatus", 2, 1);
    histograms.ExpectUniqueSample(
        "Conversions.Report.HttpResponseOrNetErrorCode", net::HTTP_UNAUTHORIZED,
        1);
  }
}

TEST_F(ConversionNetworkSenderTest, TimeFromConversionToReportSendHistogram) {
  base::HistogramTester histograms;
  auto report = GetReport(/*conversion_id=*/1);
  report.report_time = base::Time() + base::TimeDelta::FromHours(5);
  network_sender_->SendReport(report, GetSentCallback());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, ""));
  histograms.ExpectUniqueSample("Conversions.TimeFromConversionToReportSend", 5,
                                1);
}

}  // namespace content
