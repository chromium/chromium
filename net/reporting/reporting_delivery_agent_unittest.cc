// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/reporting/reporting_delivery_agent.h"

#include <optional>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/backoff_entry.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_test_util.h"
#include "net/reporting/reporting_uploader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

constexpr char kReportingUploadHeaderTypeHistogram[] =
    "Net.Reporting.UploadHeaderType";

}  // namespace

class ReportingDeliveryAgentTest : public ReportingTestBase {
 protected:
  ReportingDeliveryAgentTest() {
    // This is a private API of the reporting service, so no need to test the
    // case kPartitionConnectionsByNetworkIsolationKey is disabled - the
    // feature is only applied at the entry points of the service.
    feature_list_.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);

    ReportingPolicy policy;
    policy.endpoint_backoff_policy.num_errors_to_ignore = 0;
    policy.endpoint_backoff_policy.initial_delay_ms = 60000;
    policy.endpoint_backoff_policy.multiply_factor = 2.0;
    policy.endpoint_backoff_policy.jitter_factor = 0.0;
    policy.endpoint_backoff_policy.maximum_backoff_ms = -1;
    policy.endpoint_backoff_policy.entry_lifetime_ms = 0;
    policy.endpoint_backoff_policy.always_use_initial_delay = false;
    UsePolicy(policy);
  }

  void AddReport(const std::optional<base::UnguessableToken>& reporting_source,
                 const NetworkAnonymizationKey& network_anonymization_key,
                 const GURL& url,
                 const std::string& group) {
    base::Value::Dict report_body;
    report_body.Set("key", "value");
    cache()->AddReport(reporting_source, network_anonymization_key, url,
                       kUserAgent_, group, kType_, std::move(report_body),
                       /*depth=*/0, /*queued=*/tick_clock()->NowTicks(),
                       /*attempts=*/0, ReportingTargetType::kDeveloper);
  }

  void AddEnterpriseReport(const GURL& url, const std::string& group) {
    base::Value::Dict report_body;
    report_body.Set("key", "value");
    cache()->AddReport(/*reporting_source=*/std::nullopt,
                       net::NetworkAnonymizationKey(), url, kUserAgent_, group,
                       kType_, std::move(report_body), /*depth=*/0,
                       /*queued=*/tick_clock()->NowTicks(), /*attempts=*/0,
                       ReportingTargetType::kEnterprise);
  }

  // The first report added to the cache is uploaded immediately, and a timer is
  // started for all subsequent reports (which may then be batched). To test
  // behavior involving batching multiple reports, we need to add, upload, and
  // immediately resolve a dummy report to prime the delivery timer.
  void UploadFirstReportAndStartTimer() {
    ReportingEndpointGroupKey dummy_group(
        NetworkAnonymizationKey(),
        url::Origin::Create(GURL("https://dummy.test")), "dummy",
        ReportingTargetType::kDeveloper);
    ASSERT_TRUE(SetEndpointInCache(
        dummy_group, GURL("https://dummy.test/upload"), kExpires_));
    AddReport(std::nullopt, dummy_group.network_anonymization_key,
              dummy_group.origin.value().GetURL(), dummy_group.group_name);

    ASSERT_EQ(1u, pending_uploads().size());
    pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
    EXPECT_EQ(0u, pending_uploads().size());
    EXPECT_TRUE(delivery_timer()->IsRunning());
  }

  // Prime delivery timer with a document report with a endpoint group that
  // has matching reporting_source.
  void UploadFirstDocumentReportAndStartTimer() {
    ReportingEndpointGroupKey dummy_group(
        kNak_, kDocumentReportingSource_,
        url::Origin::Create(GURL("https://dummy.test")), "dummy",
        ReportingTargetType::kDeveloper);
    SetV1EndpointInCache(dummy_group, kDocumentReportingSource_,
                         kIsolationInfo_, GURL("https://dummy.test/upload"));
    AddReport(kDocumentReportingSource_, dummy_group.network_anonymization_key,
              dummy_group.origin.value().GetURL(), dummy_group.group_name);

    ASSERT_EQ(1u, pending_uploads().size());
    pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
    EXPECT_EQ(0u, pending_uploads().size());
    EXPECT_TRUE(delivery_timer()->IsRunning());
  }

  void SendReportsForSource(base::UnguessableToken reporting_source) {
    delivery_agent()->SendReportsForSource(reporting_source);
  }

  void SendReports() { delivery_agent()->SendReportsForTesting(); }

  base::test::ScopedFeatureList feature_list_;

  const GURL kUrl_ = GURL("https://origin/path");
  const GURL kOtherUrl_ = GURL("https://other-origin/path");
  const GURL kSubdomainUrl_ = GURL("https://sub.origin/path");
  const url::Origin kOrigin_ = url::Origin::Create(GURL("https://origin/"));
  const url::Origin kOtherOrigin_ =
      url::Origin::Create(GURL("https://other-origin/"));
  const std::optional<base::UnguessableToken> kEmptyReportingSource_ =
      std::nullopt;
  const base::UnguessableToken kDocumentReportingSource_ =
      base::UnguessableToken::Create();
  const NetworkAnonymizationKey kNak_ =
      NetworkAnonymizationKey::CreateSameSite(SchemefulSite(kOrigin_));
  const NetworkAnonymizationKey kOtherNak_ =
      NetworkAnonymizationKey::CreateSameSite(SchemefulSite(kOtherOrigin_));
  const IsolationInfo kIsolationInfo_ =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                            kOrigin_,
                            kOrigin_,
                            SiteForCookies::FromOrigin(kOrigin_));
  const IsolationInfo kOtherIsolationInfo_ =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                            kOtherOrigin_,
                            kOtherOrigin_,
                            SiteForCookies::FromOrigin(kOtherOrigin_));
  const GURL kEndpoint_ = GURL("https://endpoint/");
  const std::string kUserAgent_ = "Mozilla/1.0";
  const std::string kGroup_ = "group";
  const std::string kType_ = "type";
  const base::Time kExpires_ = base::Time::Now() + base::Days(7);
  const ReportingEndpointGroupKey kGroupKey_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin_,
                                kGroup_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kDocumentGroupKey_ =
      ReportingEndpointGroupKey(kGroupKey_, kDocumentReportingSource_);
};

TEST_F(ReportingDeliveryAgentTest, SuccessfulImmediateUpload) {
  base::HistogramTester histograms;
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);

  // Upload is automatically started when cache is modified.

  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kEndpoint_, pending_uploads()[0]->url());
  {
    auto value = pending_uploads()[0]->GetValue();

    ASSERT_TRUE(value->is_list());
    ASSERT_EQ(1u, value->GetList().size());

    const base::Value& report = value->GetList()[0];
    ASSERT_TRUE(report.is_dict());
    const base::Value::Dict& report_dict = report.GetDict();
    EXPECT_EQ(5u, report_dict.size());

    ExpectDictIntegerValue(0, report_dict, "age");
    ExpectDictStringValue(kType_, report_dict, "type");
    ExpectDictStringValue(kUrl_.spec(), report_dict, "url");
    ExpectDictStringValue(kUserAgent_, report_dict, "user_agent");
    const base::Value::Dict* body = report_dict.FindDict("body");
    EXPECT_EQ("value", *body->FindString("key"));
  }
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Successful upload should remove delivered reports.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  histograms.ExpectBucketCount(
      kReportingUploadHeaderTypeHistogram,
      ReportingDeliveryAgent::ReportingUploadHeaderType::kReportTo, 1);
  histograms.ExpectBucketCount(
      kReportingUploadHeaderTypeHistogram,
      ReportingDeliveryAgent::ReportingUploadHeaderType::kReportingEndpoints,
      0);

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(1, stats.successful_reports);
  }

  // TODO(dcreager): Check that BackoffEntry was informed of success.
}

TEST_F(ReportingDeliveryAgentTest, ReportToHeaderCountedCorrectly) {
  base::HistogramTester histograms;

  // Set an endpoint with no reporting source (as if configured with the
  // Report-To header).
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));

  // Add and upload a report with an associated source.
  AddReport(kDocumentReportingSource_, kNak_, kUrl_, kGroup_);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Successful upload should count this as a Report-To delivery, even though
  // the report itself had a reporting source.
  histograms.ExpectBucketCount(
      kReportingUploadHeaderTypeHistogram,
      ReportingDeliveryAgent::ReportingUploadHeaderType::kReportTo, 1);
  histograms.ExpectBucketCount(
      kReportingUploadHeaderTypeHistogram,
      ReportingDeliveryAgent::ReportingUploadHeaderType::kReportingEndpoints,
      0);
}

TEST_F(ReportingDeliveryAgentTest, SuccessfulImmediateUploadDocumentReport) {
  base::HistogramTester histograms;

  SetV1EndpointInCache(kDocumentGroupKey_, kDocumentReportingSource_,
                       kIsolationInfo_, kEndpoint_);
  AddReport(kDocumentReportingSource_, kNak_, kUrl_, kGroup_);

  // Upload is automatically started when cache is modified.

  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kEndpoint_, pending_uploads()[0]->url());
  {
    const auto value = pending_uploads()[0]->GetValue();

    ASSERT_TRUE(value->is_list());
    ASSERT_EQ(1u, value->GetList().size());

    const base::Value& report = value->GetList()[0];
    ASSERT_TRUE(report.is_dict());
    const base::Value::Dict& report_dict = report.GetDict();

    ExpectDictIntegerValue(0, report_dict, "age");
    ExpectDictStringValue(kType_, report_dict, "type");
    ExpectDictStringValue(kUrl_.spec(), report_dict, "url");
    ExpectDictStringValue(kUserAgent_, report_dict, "user_agent");
    const base::Value::Dict* body = report_dict.FindDict("body");
    EXPECT_EQ("value", *body->FindString("key"));
  }
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Successful upload should remove delivered reports.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  histograms.ExpectBucketCount(
      kReportingUploadHeaderTypeHistogram,
      ReportingDeliveryAgent::ReportingUploadHeaderType::kReportingEndpoints,
      1);
  histograms.ExpectBucketCount(
      kReportingUploadHeaderTypeHistogram,
      ReportingDeliveryAgent::ReportingUploadHeaderType::kReportTo, 0);

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kDocumentGroupKey_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(1, stats.successful_reports);
  }
}

TEST_F(ReportingDeliveryAgentTest, UploadHeaderTypeEnumCountPerReport) {
  UploadFirstDocumentReportAndStartTimer();
  base::HistogramTester histograms;

  SetV1EndpointInCache(kDocumentGroupKey_, kDocumentReportingSource_,
                       kIsolationInfo_, kEndpoint_);
  AddReport(kDocumentReportingSource_, kNak_, kUrl_, kGroup_);
  AddReport(kDocumentReportingSource_, kNak_, kUrl_, kGroup_);

  // There should be one upload per (NAK, origin, reporting source).
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Successful upload should remove delivered reports.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  histograms.ExpectBucketCount(
      kReportingUploadHeaderTypeHistogram,
      ReportingDeliveryAgent::ReportingUploadHeaderType::kReportingEndpoints,
      2);
  histograms.ExpectBucketCount(
      kReportingUploadHeaderTypeHistogram,
      ReportingDeliveryAgent::ReportingUploadHeaderType::kReportTo, 0);
}

TEST_F(ReportingDeliveryAgentTest, SuccessfulImmediateSubdomainUpload) {
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_,
                                 OriginSubdomains::INCLUDE));
  AddReport(kEmptyReportingSource_, kNak_, kSubdomainUrl_, kGroup_);

  // Upload is automatically started when cache is modified.

  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kEndpoint_, pending_uploads()[0]->url());
  {
    auto value = pending_uploads()[0]->GetValue();

    ASSERT_TRUE(value->is_list());
    ASSERT_EQ(1u, value->GetList().size());

    const base::Value& report = value->GetList()[0];
    ASSERT_TRUE(report.is_dict());
    const base::Value::Dict& report_dict = report.GetDict();
    EXPECT_EQ(5u, report_dict.size());

    ExpectDictIntegerValue(0, report_dict, "age");
    ExpectDictStringValue(kType_, report_dict, "type");
    ExpectDictStringValue(kSubdomainUrl_.spec(), report_dict, "url");
    ExpectDictStringValue(kUserAgent_, report_dict, "user_agent");
    const base::Value::Dict* body = report_dict.FindDict("body");
    EXPECT_EQ("value", *body->FindString("key"));
  }
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Successful upload should remove delivered reports.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(1, stats.successful_reports);
  }

  // TODO(dcreager): Check that BackoffEntry was informed of success.
}

TEST_F(ReportingDeliveryAgentTest,
       SuccessfulImmediateSubdomainUploadWithOverwrittenEndpoint) {
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_,
                                 OriginSubdomains::INCLUDE));
  AddReport(kEmptyReportingSource_, kNak_, kSubdomainUrl_, kGroup_);

  // Upload is automatically started when cache is modified.

  ASSERT_EQ(1u, pending_uploads().size());
  // Change the endpoint group to exclude subdomains.
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_,
                                 OriginSubdomains::EXCLUDE));
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(1, stats.successful_reports);
  }

  // Successful upload should remove delivered reports.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_F(ReportingDeliveryAgentTest, SuccessfulDelayedUpload) {
  // Trigger and complete an upload to start the delivery timer.
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Add another report to upload after a delay.
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kEndpoint_, pending_uploads()[0]->url());
  {
    auto value = pending_uploads()[0]->GetValue();

    ASSERT_TRUE(value->is_list());
    ASSERT_EQ(1u, value->GetList().size());

    const base::Value& report = value->GetList()[0];
    ASSERT_TRUE(report.is_dict());
    const base::Value::Dict& report_dict = report.GetDict();
    EXPECT_EQ(5u, report_dict.size());

    ExpectDictIntegerValue(0, report_dict, "age");
    ExpectDictStringValue(kType_, report_dict, "type");
    ExpectDictStringValue(kUrl_.spec(), report_dict, "url");
    ExpectDictStringValue(kUserAgent_, report_dict, "user_agent");
    const base::Value::Dict* body = report_dict.FindDict("body");
    EXPECT_EQ("value", *body->FindString("key"));
  }
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    EXPECT_EQ(2, stats.attempted_uploads);
    EXPECT_EQ(2, stats.successful_uploads);
    EXPECT_EQ(2, stats.attempted_reports);
    EXPECT_EQ(2, stats.successful_reports);
  }

  // Successful upload should remove delivered reports.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());

  // TODO(juliatuttle): Check that BackoffEntry was informed of success.
}

TEST_F(ReportingDeliveryAgentTest, FailedUpload) {
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::FAILURE);

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(0, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(0, stats.successful_reports);
  }

  // Failed upload should increment reports' attempts.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(1, reports[0]->attempts);

  // Since endpoint is now failing, an upload won't be started despite a pending
  // report.
  ASSERT_TRUE(pending_uploads().empty());
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  EXPECT_TRUE(pending_uploads().empty());

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(0, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(0, stats.successful_reports);
  }
}

TEST_F(ReportingDeliveryAgentTest, DisallowedUpload) {
  // This mimics the check that is controlled by the BACKGROUND_SYNC permission
  // in a real browser profile.
  context()->test_delegate()->set_disallow_report_uploads(true);

  static const int kAgeMillis = 12345;

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);

  tick_clock()->Advance(base::Milliseconds(kAgeMillis));

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  // We should not try to upload the report, since we weren't given permission
  // for this origin.
  EXPECT_TRUE(pending_uploads().empty());

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    EXPECT_EQ(0, stats.attempted_uploads);
    EXPECT_EQ(0, stats.successful_uploads);
    EXPECT_EQ(0, stats.attempted_reports);
    EXPECT_EQ(0, stats.successful_reports);
  }

  // Disallowed reports should NOT have been removed from the cache.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());
}

TEST_F(ReportingDeliveryAgentTest, RemoveEndpointUpload) {
  static const ReportingEndpointGroupKey kOtherGroupKey(
      kNak_, kOtherOrigin_, kGroup_, ReportingTargetType::kDeveloper);

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  ASSERT_TRUE(SetEndpointInCache(kOtherGroupKey, kEndpoint_, kExpires_));

  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::REMOVE_ENDPOINT);

  // "Remove endpoint" upload should remove endpoint from *all* origins and
  // increment reports' attempts.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(1, reports[0]->attempts);

  EXPECT_FALSE(FindEndpointInCache(kGroupKey_, kEndpoint_));
  EXPECT_FALSE(FindEndpointInCache(kOtherGroupKey, kEndpoint_));

  // Since endpoint is now failing, an upload won't be started despite a pending
  // report.
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  EXPECT_TRUE(pending_uploads().empty());
}

TEST_F(ReportingDeliveryAgentTest, ConcurrentRemove) {
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());

  // Remove the report while the upload is running.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());

  const ReportingReport* report = reports[0];
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));

  // Report should appear removed, even though the cache has doomed it.
  cache()->RemoveReports(reports);
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(report));

  // Completing upload shouldn't crash, and report should still be gone.
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_F(ReportingDeliveryAgentTest, ConcurrentRemoveDuringPermissionsCheck) {
  // Pause the permissions check, so that we can try to remove some reports
  // while we're in the middle of verifying that we can upload them.  (This is
  // similar to the previous test, but removes the reports during a different
  // part of the upload process.)
  context()->test_delegate()->set_pause_permissions_check(true);

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);

  ASSERT_TRUE(context()->test_delegate()->PermissionsCheckPaused());

  // Remove the report while the upload is running.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());

  const ReportingReport* report = reports[0];
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));

  // Report should appear removed, even though the cache has doomed it.
  cache()->RemoveReports(reports);
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(report));

  // Completing upload shouldn't crash, and report should still be gone.
  context()->test_delegate()->ResumePermissionsCheck();
  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

// Reports uploaded together must share a NAK and origin.
// Test that the agent will not combine reports destined for the same endpoint
// if the reports are from different origins or NAKs, but does combine all
// reports for the same (NAK, origin).
TEST_F(ReportingDeliveryAgentTest, OnlyBatchSameNakAndOrigin) {
  const ReportingEndpointGroupKey kGroupKeys[] = {
      ReportingEndpointGroupKey(kNak_, kOrigin_, kGroup_,
                                ReportingTargetType::kDeveloper),
      ReportingEndpointGroupKey(kNak_, kOtherOrigin_, kGroup_,
                                ReportingTargetType::kDeveloper),
      ReportingEndpointGroupKey(kOtherNak_, kOrigin_, kGroup_,
                                ReportingTargetType::kDeveloper),
      ReportingEndpointGroupKey(kOtherNak_, kOtherOrigin_, kGroup_,
                                ReportingTargetType::kDeveloper),
  };
  for (const ReportingEndpointGroupKey& group_key : kGroupKeys) {
    ASSERT_TRUE(SetEndpointInCache(group_key, kEndpoint_, kExpires_));
  }

  // Trigger and complete an upload to start the delivery timer.
  UploadFirstReportAndStartTimer();

  // Now that the delivery timer is running, these reports won't be immediately
  // uploaded.
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kNak_, kOtherUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kNak_, kOtherUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kOtherNak_, kUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kOtherNak_, kUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kOtherNak_, kUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kOtherNak_, kOtherUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kOtherNak_, kOtherUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kOtherNak_, kOtherUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kOtherNak_, kOtherUrl_, kGroup_);
  EXPECT_EQ(0u, pending_uploads().size());

  // There should be one upload per (NAK, origin).
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(4u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  for (int i = 0; i < 4; ++i) {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKeys[i], kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(i + 1, stats.attempted_reports);
    EXPECT_EQ(i + 1, stats.successful_reports);
  }
}

// Test that the agent won't start a second upload for a (NAK, origin, group)
// while one is pending, even if a different endpoint is available, but will
// once the original delivery is complete and the (NAK, origin, group) is no
// longer pending.
TEST_F(ReportingDeliveryAgentTest, SerializeUploadsToGroup) {
  static const GURL kDifferentEndpoint("https://endpoint2/");

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kDifferentEndpoint, kExpires_));

  // Trigger and complete an upload to start the delivery timer.
  UploadFirstReportAndStartTimer();

  // First upload causes this group key to become pending.
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);
  EXPECT_EQ(0u, pending_uploads().size());
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  EXPECT_EQ(1u, pending_uploads().size());

  // Second upload isn't started because the group is pending.
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());

  // Resolve the first upload.
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  // Now the other upload can happen.
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  // A total of 2 reports were uploaded.
  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    ReportingEndpoint::Statistics different_stats =
        GetEndpointStatistics(kGroupKey_, kDifferentEndpoint);
    EXPECT_EQ(2, stats.attempted_uploads + different_stats.attempted_uploads);
    EXPECT_EQ(2, stats.successful_uploads + different_stats.successful_uploads);
    EXPECT_EQ(2, stats.attempted_reports + different_stats.attempted_reports);
    EXPECT_EQ(2, stats.successful_reports + different_stats.successful_reports);
  }
}

// Tests that the agent will start parallel uploads to different groups within
// the same (NAK, origin) to endpoints with different URLs.
TEST_F(ReportingDeliveryAgentTest, ParallelizeUploadsAcrossGroups) {
  static const GURL kDifferentEndpoint("https://endpoint2/");
  static const std::string kDifferentGroup("group2");
  const ReportingEndpointGroupKey kDifferentGroupKey(
      kNak_, kOrigin_, kDifferentGroup, ReportingTargetType::kDeveloper);

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  ASSERT_TRUE(
      SetEndpointInCache(kDifferentGroupKey, kDifferentEndpoint, kExpires_));

  // Trigger and complete an upload to start the delivery timer.
  UploadFirstReportAndStartTimer();

  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kDifferentGroup);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(2u, pending_uploads().size());

  pending_uploads()[1]->Complete(ReportingUploader::Outcome::SUCCESS);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(1, stats.successful_reports);
  }
  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kDifferentGroupKey, kDifferentEndpoint);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(1, stats.successful_reports);
  }
}

// Tests that the agent will include reports for different groups for the same
// (NAK, origin) in the same upload if they are destined for the same endpoint
// URL.
TEST_F(ReportingDeliveryAgentTest, BatchReportsAcrossGroups) {
  static const std::string kDifferentGroup("group2");
  const ReportingEndpointGroupKey kDifferentGroupKey(
      kNak_, kOrigin_, kDifferentGroup, ReportingTargetType::kDeveloper);

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  ASSERT_TRUE(SetEndpointInCache(kDifferentGroupKey, kEndpoint_, kExpires_));

  UploadFirstReportAndStartTimer();

  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kGroup_);
  AddReport(kEmptyReportingSource_, kNak_, kUrl_, kDifferentGroup);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kGroupKey_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(1, stats.successful_reports);
  }
  {
    ReportingEndpoint::Statistics stats =
        GetEndpointStatistics(kDifferentGroupKey, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(1, stats.successful_reports);
  }
}

// Tests that the agent can send all outstanding reports for a single source
// when necessary. This test queues two reports for the same reporting source,
// for different endpoints, another for a different source at the same URL, and
// another for a different source on a different origin.
TEST_F(ReportingDeliveryAgentTest, SendDeveloperReportsForSource) {
  static const std::string kGroup2("group2");

  // Two other reporting sources; kReportingSource2 will enqueue reports for the
  // same URL as kReportingSource_, while kReportingSource3 will be a separate
  // origin.
  const base::UnguessableToken kReportingSource1 =
      base::UnguessableToken::Create();
  const base::UnguessableToken kReportingSource2 =
      base::UnguessableToken::Create();
  const base::UnguessableToken kReportingSource3 =
      base::UnguessableToken::Create();

  const IsolationInfo kIsolationInfo1 =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin_,
                            kOrigin_, SiteForCookies::FromOrigin(kOrigin_));
  const IsolationInfo kIsolationInfo2 =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin_,
                            kOrigin_, SiteForCookies::FromOrigin(kOrigin_));
  const IsolationInfo kIsolationInfo3 = IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, kOtherOrigin_, kOtherOrigin_,
      SiteForCookies::FromOrigin(kOtherOrigin_));

  // Set up identical endpoint configuration for kReportingSource1 and
  // kReportingSource2. kReportingSource3 is independent.
  const ReportingEndpointGroupKey kGroup1Key1(kNak_, kReportingSource1,
                                              kOrigin_, kGroup_,
                                              ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroup2Key1(kNak_, kReportingSource1,
                                              kOrigin_, kGroup2,
                                              ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroup1Key2(kNak_, kReportingSource2,
                                              kOrigin_, kGroup_,
                                              ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroup2Key2(kNak_, kReportingSource2,
                                              kOrigin_, kGroup2,
                                              ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kOtherGroupKey(
      kOtherNak_, kReportingSource3, kOtherOrigin_, kGroup_,
      ReportingTargetType::kDeveloper);

  SetV1EndpointInCache(kGroup1Key1, kReportingSource1, kIsolationInfo1, kUrl_);
  SetV1EndpointInCache(kGroup2Key1, kReportingSource1, kIsolationInfo1, kUrl_);
  SetV1EndpointInCache(kGroup1Key2, kReportingSource2, kIsolationInfo2, kUrl_);
  SetV1EndpointInCache(kGroup2Key2, kReportingSource2, kIsolationInfo2, kUrl_);
  SetV1EndpointInCache(kOtherGroupKey, kReportingSource3, kIsolationInfo3,
                       kOtherUrl_);

  UploadFirstReportAndStartTimer();

  AddReport(kReportingSource1, kNak_, kUrl_, kGroup_);
  AddReport(kReportingSource1, kNak_, kUrl_, kGroup2);
  AddReport(kReportingSource2, kNak_, kUrl_, kGroup_);
  AddReport(kReportingSource3, kOtherNak_, kUrl_, kGroup_);

  // There should be four queued reports at this point.
  EXPECT_EQ(4u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
  EXPECT_EQ(0u, pending_uploads().size());
  SendReportsForSource(kReportingSource1);
  // Sending all reports for the source should only queue two, despite the fact
  // that there are other reports queued for the same origin and endpoint.
  EXPECT_EQ(2u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
  EXPECT_EQ(2u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));
  // All pending reports for the same source should be batched into a single
  // upload.
  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());
}

TEST_F(ReportingDeliveryAgentTest, SendEnterpriseReports) {
  const ReportingEndpointGroupKey kEnterpriseGroupKey(
      NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
      /*origin=*/std::nullopt, kGroup_, ReportingTargetType::kEnterprise);

  SetEnterpriseEndpointInCache(kEnterpriseGroupKey, kUrl_);

  AddEnterpriseReport(kUrl_, kGroup_);

  // Upload is automatically started when cache is modified.
  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kUrl_, pending_uploads()[0]->url());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  // Successful upload should remove delivered reports.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_F(ReportingDeliveryAgentTest, SendEnterpriseReportsBatched) {
  const ReportingEndpointGroupKey kEnterpriseGroupKey(
      NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
      /*origin=*/std::nullopt, kGroup_, ReportingTargetType::kEnterprise);

  SetEnterpriseEndpointInCache(kEnterpriseGroupKey, kUrl_);

  // Call so the reports will be batched together.
  UploadFirstReportAndStartTimer();

  AddEnterpriseReport(kUrl_, kGroup_);
  AddEnterpriseReport(kUrl_, kGroup_);

  // There should be two queued reports at this point.
  EXPECT_EQ(2u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
  EXPECT_EQ(0u, pending_uploads().size());

  SendReports();

  EXPECT_EQ(2u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));

  // All pending reports should be batched into a single upload.
  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kUrl_, pending_uploads()[0]->url());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  // Successful upload should remove delivered reports.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_F(ReportingDeliveryAgentTest, SendDeveloperAndEnterpriseReports) {
  const ReportingEndpointGroupKey kDeveloperGroupKey(
      kNak_, kDocumentReportingSource_, kOrigin_, kGroup_,
      ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kEnterpriseGroupKey(
      NetworkAnonymizationKey(),
      /*reporting_source=*/std::nullopt, /*origin=*/std::nullopt, kGroup_,
      ReportingTargetType::kEnterprise);

  SetV1EndpointInCache(kDeveloperGroupKey, kDocumentReportingSource_,
                       kIsolationInfo_, kUrl_);
  SetEnterpriseEndpointInCache(kEnterpriseGroupKey, kUrl_);

  AddReport(kDocumentReportingSource_, kNak_, kUrl_, kGroup_);
  AddEnterpriseReport(kUrl_, kGroup_);

  SendReports();

  // Web developer and enterprise pending reports should be in separate uploads.
  ASSERT_EQ(2u, pending_uploads().size());
  EXPECT_EQ(kUrl_, pending_uploads()[0]->url());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kUrl_, pending_uploads()[0]->url());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  // Successful upload should remove delivered reports.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

// Tests that the agent can send all outstanding V1 reports for multiple sources
// and that these are not batched together.
TEST_F(ReportingDeliveryAgentTest, SendReportsForMultipleSources) {
  static const std::string kGroup2("group2");

  // Two other reporting sources; kReportingSource2 will enqueue reports for the
  // same URL as kReportingSource_, while kReportingSource3 will be a separate
  // origin.
  const base::UnguessableToken kReportingSource1 =
      base::UnguessableToken::Create();
  const base::UnguessableToken kReportingSource2 =
      base::UnguessableToken::Create();
  const base::UnguessableToken kReportingSource3 =
      base::UnguessableToken::Create();

  const IsolationInfo kIsolationInfo1 =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin_,
                            kOrigin_, SiteForCookies::FromOrigin(kOrigin_));
  const IsolationInfo kIsolationInfo2 =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin_,
                            kOrigin_, SiteForCookies::FromOrigin(kOrigin_));
  const IsolationInfo kIsolationInfo3 = IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, kOtherOrigin_, kOtherOrigin_,
      SiteForCookies::FromOrigin(kOtherOrigin_));

  // Set up identical endpoint configuration for kReportingSource1 and
  // kReportingSource2. kReportingSource3 is independent.
  const ReportingEndpointGroupKey kGroup1Key1(kNak_, kReportingSource1,
                                              kOrigin_, kGroup_,
                                              ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroup2Key1(kNak_, kReportingSource1,
                                              kOrigin_, kGroup2,
                                              ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroup1Key2(kNak_, kReportingSource2,
                                              kOrigin_, kGroup_,
                                              ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroup2Key2(kNak_, kReportingSource2,
                                              kOrigin_, kGroup2,
                                              ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kOtherGroupKey(
      kOtherNak_, kReportingSource3, kOtherOrigin_, kGroup_,
      ReportingTargetType::kDeveloper);

  SetV1EndpointInCache(kGroup1Key1, kReportingSource1, kIsolationInfo1, kUrl_);
  SetV1EndpointInCache(kGroup2Key1, kReportingSource1, kIsolationInfo1, kUrl_);
  SetV1EndpointInCache(kGroup1Key2, kReportingSource2, kIsolationInfo2, kUrl_);
  SetV1EndpointInCache(kGroup2Key2, kReportingSource2, kIsolationInfo2, kUrl_);
  SetV1EndpointInCache(kOtherGroupKey, kReportingSource3, kIsolationInfo3,
                       kOtherUrl_);

  UploadFirstReportAndStartTimer();

  AddReport(kReportingSource1, kNak_, kUrl_, kGroup_);
  AddReport(kReportingSource1, kNak_, kUrl_, kGroup2);
  AddReport(kReportingSource2, kNak_, kUrl_, kGroup_);
  AddReport(kReportingSource3, kOtherNak_, kUrl_, kGroup_);

  // There should be four queued reports at this point.
  EXPECT_EQ(4u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
  EXPECT_EQ(0u, pending_uploads().size());

  // Send reports for both ReportingSource 1 and 2 at the same time. These
  // should be sent to the same endpoint, but should still not be batched
  // together.
  SendReportsForSource(kReportingSource1);
  SendReportsForSource(kReportingSource2);

  // We expect to see three pending reports, and one still queued. The pending
  // reports should be divided into two uploads.
  EXPECT_EQ(1u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
  EXPECT_EQ(3u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));
  ASSERT_EQ(2u, pending_uploads().size());
}

}  // namespace net
