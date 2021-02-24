// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_network_sender_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Gets a report url which matches reports created by GetReport().
std::string GetReportUrl(std::string impression_data) {
  return base::StrCat(
      {"https://report.test/.well-known/register-conversion?impression-data=",
       impression_data, "&conversion-data=", impression_data, "&credit=0"});
}

// Create a simple report where impression data/conversion data/conversion id
// are all the same.
ConversionReport GetReport(int64_t conversion_id) {
  return ConversionReport(
      ImpressionBuilder(base::Time())
          .SetData(base::NumberToString(conversion_id))
          .Build(),
      /*conversion_data=*/base::NumberToString(conversion_id),
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
  size_t num_reports_sent_ = 0u;

  // |task_enviorment_| must be initialized first.
  content::BrowserTaskEnvironment task_environment_;

  // Unique ptr so it can be reset during testing.
  std::unique_ptr<ConversionNetworkSenderImpl> network_sender_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  void OnReportSent() { num_reports_sent_++; }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(ConversionNetworkSenderTest,
       ConversionReportReceived_NetworkRequestMade) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(&report, std::move(base::DoNothing()));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetReportUrl("1"), ""));
}

TEST_F(ConversionNetworkSenderTest, ReportSent_CallbackFired) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(&report, GetSentCallback());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetReportUrl("1"), ""));
  EXPECT_EQ(1u, num_reports_sent_);
}

TEST_F(ConversionNetworkSenderTest, SenderDeletedDuringRequest_NoCrash) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(&report, GetSentCallback());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  network_sender_.reset();
  EXPECT_FALSE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetReportUrl("1"), ""));
  EXPECT_EQ(0u, num_reports_sent_);
}

TEST_F(ConversionNetworkSenderTest, ReportRequestHangs_TimesOut) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(&report, GetSentCallback());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // The request should time out after 30 seconds.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(30));

  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Also verify that the sent callback runs if the request times out.
  EXPECT_EQ(1u, num_reports_sent_);
}

TEST_F(ConversionNetworkSenderTest,
       ReportRequesFailsDueToNetworkChange_Retries) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(&report, GetSentCallback());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // Simulate the request failing due to network change.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(GetReportUrl("1")),
      network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
      network::mojom::URLResponseHead::New(), std::string());

  // The sender should automatically retry.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // Simulate a second request failure due to network change.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(GetReportUrl("1")),
      network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
      network::mojom::URLResponseHead::New(), std::string());

  // We should not retry again. Verify the report sent callback only gets fired once.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(1u, num_reports_sent_);
}

TEST_F(ConversionNetworkSenderTest, ReportSent_QueryParamsSetCorrectly) {
  auto impression =
      ImpressionBuilder(base::Time())
          .SetData("impression")
          .SetReportingOrigin(url::Origin::Create(GURL("https://a.com")))
          .Build();
  ConversionReport report(impression,
                          /*conversion_data=*/"conversion",
                          /*conversion_time=*/base::Time(),
                          /*report_time=*/base::Time(),
                          /*conversion_id=*/1);
  report.attribution_credit = 50;
  network_sender_->SendReport(&report, base::DoNothing());

  std::string expected_report_url(
      "https://a.com/.well-known/"
      "register-conversion?impression-data=impression&conversion-data="
      "conversion&credit=50");
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      expected_report_url, ""));
}

TEST_F(ConversionNetworkSenderTest, ReportSent_RequestAttributesSet) {
  auto impression =
      ImpressionBuilder(base::Time())
          .SetData("1")
          .SetReportingOrigin(url::Origin::Create(GURL("https://a.com")))
          .SetConversionOrigin(url::Origin::Create(GURL("https://sub.b.com")))
          .Build();
  ConversionReport report(impression,
                          /*conversion_data=*/"1",
                          /*conversion_time=*/base::Time(),
                          /*report_time=*/base::Time(),
                          /*conversion_id=*/1);
  network_sender_->SendReport(&report, base::DoNothing());

  const network::ResourceRequest* pending_request;
  std::string expected_report_url(
      "https://a.com/.well-known/"
      "register-conversion?impression-data=1&conversion-data=1&credit=0");
  EXPECT_TRUE(test_url_loader_factory_.IsPending(expected_report_url,
                                                 &pending_request));

  // Ensure that the request is sent with no credentials.
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            pending_request->credentials_mode);
  EXPECT_EQ("POST", pending_request->method);

  // Make sure the domain is used as the referrer.
  EXPECT_EQ(GURL("https://b.com"), pending_request->referrer);
}

TEST_F(ConversionNetworkSenderTest, ReportResultsInHttpError_SentCallbackRuns) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(&report, GetSentCallback());
  EXPECT_EQ(0u, num_reports_sent_);

  // We should run the sent callback even if there is an http error.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetReportUrl("1"), "", net::HttpStatusCode::HTTP_BAD_REQUEST));
  EXPECT_EQ(1u, num_reports_sent_);
}

TEST_F(ConversionNetworkSenderTest, ManyReports_AllSentSuccessfully) {
  for (int i = 0; i < 10; i++) {
    auto report = GetReport(/*conversion_id=*/i);
    network_sender_->SendReport(&report, GetSentCallback());
  }
  EXPECT_EQ(10, test_url_loader_factory_.NumPending());

  // Send reports out of order to guarantee that callback conversion_ids are
  // properly handled.
  for (int i = 9; i >= 0; i--) {
    std::string report_id = base::NumberToString(i);

    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetReportUrl(report_id), ""));
  }
  EXPECT_EQ(10u, num_reports_sent_);
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(ConversionNetworkSenderTest, LoadFlags) {
  auto report = GetReport(/*conversion_id=*/1);
  network_sender_->SendReport(&report, GetSentCallback());
  int load_flags =
      test_url_loader_factory_.GetPendingRequest(0)->request.load_flags;
  EXPECT_TRUE(load_flags & net::LOAD_BYPASS_CACHE);
  EXPECT_TRUE(load_flags & net::LOAD_DISABLE_CACHE);
}

TEST_F(ConversionNetworkSenderTest, ErrorHistogram) {
  // All OK.
  {
    base::HistogramTester histograms;
    auto report = GetReport(/*conversion_id=*/1);
    network_sender_->SendReport(&report, GetSentCallback());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetReportUrl("1"), ""));
    // kOk = 0.
    histograms.ExpectUniqueSample("Conversions.ReportStatus", 0, 1);
  }
  // Internal error.
  {
    base::HistogramTester histograms;
    auto report = GetReport(/*conversion_id=*/2);
    network_sender_->SendReport(&report, GetSentCallback());
    network::URLLoaderCompletionStatus completion_status(net::ERR_FAILED);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(GetReportUrl("2")), completion_status,
        network::mojom::URLResponseHead::New(), std::string()));
    // kInternalError = 1.
    histograms.ExpectUniqueSample("Conversions.ReportStatus", 1, 1);
  }
  {
    base::HistogramTester histograms;
    auto report = GetReport(/*conversion_id=*/3);
    network_sender_->SendReport(&report, GetSentCallback());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetReportUrl("3"), std::string(), net::HTTP_UNAUTHORIZED));
    // kExternalError = 2.
    histograms.ExpectUniqueSample("Conversions.ReportStatus", 2, 1);
  }
}

TEST_F(ConversionNetworkSenderTest, TimeFromConversionToReportSendHistogram) {
  base::HistogramTester histograms;
  auto report = GetReport(/*conversion_id=*/1);
  report.report_time = base::Time() + base::TimeDelta::FromHours(5);
  network_sender_->SendReport(&report, GetSentCallback());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetReportUrl("1"), ""));
  histograms.ExpectUniqueSample("Conversions.TimeFromConversionToReportSend", 5,
                                1);
}

}  // namespace content
