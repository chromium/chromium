// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_delivery_agent.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "net/base/backoff_entry.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
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

class ReportingDeliveryAgentTest : public ReportingTestBase {
 protected:
  ReportingDeliveryAgentTest() {
    // This is a private API of the reporting service, so no need to test the
    // case kPartitionNelAndReportingByNetworkIsolationKey is disabled - the
    // feature is only applied at the entry points of the service.
    feature_list_.InitAndEnableFeature(
        features::kPartitionNelAndReportingByNetworkIsolationKey);

    ReportingPolicy policy;
    policy.endpoint_backoff_policy.num_errors_to_ignore = 0;
    policy.endpoint_backoff_policy.initial_delay_ms = 60000;
    policy.endpoint_backoff_policy.multiply_factor = 2.0;
    policy.endpoint_backoff_policy.jitter_factor = 0.0;
    policy.endpoint_backoff_policy.maximum_backoff_ms = -1;
    policy.endpoint_backoff_policy.entry_lifetime_ms = 0;
    policy.endpoint_backoff_policy.always_use_initial_delay = false;
    UsePolicy(policy);
    report_body_.SetStringKey("key", "value");
  }

  void AddReport(const NetworkIsolationKey& network_isolation_key,
                 const GURL& url,
                 const std::string& group) {
    cache()->AddReport(network_isolation_key, url, kUserAgent_, group, kType_,
                       std::make_unique<base::Value>(report_body_.Clone()),
                       0 /* depth */, tick_clock()->NowTicks() /* queued */,
                       0 /* attempts */);
  }

  // The first report added to the cache is uploaded immediately, and a timer is
  // started for all subsequent reports (which may then be batched). To test
  // behavior involving batching multiple reports, we need to add, upload, and
  // immediately resolve a dummy report to prime the delivery timer.
  void UploadFirstReportAndStartTimer() {
    ReportingEndpointGroupKey dummy_group(
        NetworkIsolationKey(), url::Origin::Create(GURL("https://dummy.test")),
        "dummy");
    ASSERT_TRUE(SetEndpointInCache(
        dummy_group, GURL("https://dummy.test/upload"), kExpires_));
    AddReport(dummy_group.network_isolation_key, dummy_group.origin.GetURL(),
              dummy_group.group_name);

    ASSERT_EQ(1u, pending_uploads().size());
    pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
    EXPECT_EQ(0u, pending_uploads().size());
    EXPECT_TRUE(delivery_timer()->IsRunning());
  }

  base::test::ScopedFeatureList feature_list_;

  base::Value report_body_{base::Value::Type::DICTIONARY};
  const GURL kUrl_ = GURL("https://origin/path");
  const GURL kOtherUrl_ = GURL("https://other-origin/path");
  const GURL kSubdomainUrl_ = GURL("https://sub.origin/path");
  const url::Origin kOrigin_ = url::Origin::Create(GURL("https://origin/"));
  const url::Origin kOtherOrigin_ =
      url::Origin::Create(GURL("https://other-origin/"));
  const NetworkIsolationKey kNik_ =
      NetworkIsolationKey(SchemefulSite(kOrigin_), SchemefulSite(kOrigin_));
  const NetworkIsolationKey kOtherNik_ =
      NetworkIsolationKey(SchemefulSite(kOtherOrigin_),
                          SchemefulSite(kOtherOrigin_));
  const GURL kEndpoint_ = GURL("https://endpoint/");
  const std::string kUserAgent_ = "Mozilla/1.0";
  const std::string kGroup_ = "group";
  const std::string kType_ = "type";
  const base::Time kExpires_ = base::Time::Now() + base::TimeDelta::FromDays(7);
  const ReportingEndpointGroupKey kGroupKey_ =
      ReportingEndpointGroupKey(kNik_, kOrigin_, kGroup_);
};

TEST_F(ReportingDeliveryAgentTest, SuccessfulImmediateUpload) {
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kNik_, kUrl_, kGroup_);

  // Upload is automatically started when cache is modified.

  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kEndpoint_, pending_uploads()[0]->url());
  {
    auto value = pending_uploads()[0]->GetValue();

    base::ListValue* list;
    ASSERT_TRUE(value->GetAsList(&list));
    EXPECT_EQ(1u, list->GetSize());

    base::DictionaryValue* report;
    ASSERT_TRUE(list->GetDictionary(0, &report));
    EXPECT_EQ(5u, report->size());

    ExpectDictIntegerValue(0, *report, "age");
    ExpectDictStringValue(kType_, *report, "type");
    ExpectDictStringValue(kUrl_.spec(), *report, "url");
    ExpectDictStringValue(kUserAgent_, *report, "user_agent");
    base::Value* body = report->FindDictKey("body");
    EXPECT_EQ("value", *body->FindStringKey("key"));
  }
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Successful upload should remove delivered reports.
  std::vector<const ReportingReport*> reports;
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

TEST_F(ReportingDeliveryAgentTest, SuccessfulImmediateSubdomainUpload) {
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_,
                                 OriginSubdomains::INCLUDE));
  AddReport(kNik_, kSubdomainUrl_, kGroup_);

  // Upload is automatically started when cache is modified.

  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kEndpoint_, pending_uploads()[0]->url());
  {
    auto value = pending_uploads()[0]->GetValue();

    base::ListValue* list;
    ASSERT_TRUE(value->GetAsList(&list));
    EXPECT_EQ(1u, list->GetSize());

    base::DictionaryValue* report;
    ASSERT_TRUE(list->GetDictionary(0, &report));
    EXPECT_EQ(5u, report->size());

    ExpectDictIntegerValue(0, *report, "age");
    ExpectDictStringValue(kType_, *report, "type");
    ExpectDictStringValue(kSubdomainUrl_.spec(), *report, "url");
    ExpectDictStringValue(kUserAgent_, *report, "user_agent");
    base::Value* body = report->FindDictKey("body");
    EXPECT_EQ("value", *body->FindStringKey("key"));
  }
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Successful upload should remove delivered reports.
  std::vector<const ReportingReport*> reports;
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
  AddReport(kNik_, kSubdomainUrl_, kGroup_);

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
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_F(ReportingDeliveryAgentTest, SuccessfulDelayedUpload) {
  // Trigger and complete an upload to start the delivery timer.
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kNik_, kUrl_, kGroup_);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Add another report to upload after a delay.
  AddReport(kNik_, kUrl_, kGroup_);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kEndpoint_, pending_uploads()[0]->url());
  {
    auto value = pending_uploads()[0]->GetValue();

    base::ListValue* list;
    ASSERT_TRUE(value->GetAsList(&list));
    EXPECT_EQ(1u, list->GetSize());

    base::DictionaryValue* report;
    ASSERT_TRUE(list->GetDictionary(0, &report));
    EXPECT_EQ(5u, report->size());

    ExpectDictIntegerValue(0, *report, "age");
    ExpectDictStringValue(kType_, *report, "type");
    ExpectDictStringValue(kUrl_.spec(), *report, "url");
    ExpectDictStringValue(kUserAgent_, *report, "user_agent");
    base::Value* body = report->FindDictKey("body");
    EXPECT_EQ("value", *body->FindStringKey("key"));
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
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());

  // TODO(juliatuttle): Check that BackoffEntry was informed of success.
}

TEST_F(ReportingDeliveryAgentTest, FailedUpload) {
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kNik_, kUrl_, kGroup_);

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
  std::vector<const ReportingReport*> reports;
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

  base::DictionaryValue body;
  body.SetString("key", "value");

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  AddReport(kNik_, kUrl_, kGroup_);

  tick_clock()->Advance(base::TimeDelta::FromMilliseconds(kAgeMillis));

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
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());
}

TEST_F(ReportingDeliveryAgentTest, RemoveEndpointUpload) {
  static const ReportingEndpointGroupKey kOtherGroupKey(kNik_, kOtherOrigin_,
                                                        kGroup_);

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  ASSERT_TRUE(SetEndpointInCache(kOtherGroupKey, kEndpoint_, kExpires_));

  AddReport(kNik_, kUrl_, kGroup_);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::REMOVE_ENDPOINT);

  // "Remove endpoint" upload should remove endpoint from *all* origins and
  // increment reports' attempts.
  std::vector<const ReportingReport*> reports;
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
  AddReport(kNik_, kUrl_, kGroup_);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());

  // Remove the report while the upload is running.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());

  const ReportingReport* report = reports[0];
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));

  // Report should appear removed, even though the cache has doomed it.
  cache()->RemoveReports(reports, ReportingReport::Outcome::UNKNOWN);
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
  AddReport(kNik_, kUrl_, kGroup_);

  ASSERT_TRUE(context()->test_delegate()->PermissionsCheckPaused());

  // Remove the report while the upload is running.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());

  const ReportingReport* report = reports[0];
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));

  // Report should appear removed, even though the cache has doomed it.
  cache()->RemoveReports(reports, ReportingReport::Outcome::UNKNOWN);
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

// Reports uploaded together must share a NIK and origin.
// Test that the agent will not combine reports destined for the same endpoint
// if the reports are from different origins or NIKs, but does combine all
// reports for the same (NIK, origin).
TEST_F(ReportingDeliveryAgentTest, OnlyBatchSameNikAndOrigin) {
  const ReportingEndpointGroupKey kGroupKeys[] = {
      ReportingEndpointGroupKey(kNik_, kOrigin_, kGroup_),
      ReportingEndpointGroupKey(kNik_, kOtherOrigin_, kGroup_),
      ReportingEndpointGroupKey(kOtherNik_, kOrigin_, kGroup_),
      ReportingEndpointGroupKey(kOtherNik_, kOtherOrigin_, kGroup_),
  };
  for (const ReportingEndpointGroupKey& group_key : kGroupKeys) {
    ASSERT_TRUE(SetEndpointInCache(group_key, kEndpoint_, kExpires_));
  }

  // Trigger and complete an upload to start the delivery timer.
  UploadFirstReportAndStartTimer();

  // Now that the delivery timer is running, these reports won't be immediately
  // uploaded.
  AddReport(kNik_, kUrl_, kGroup_);
  AddReport(kNik_, kOtherUrl_, kGroup_);
  AddReport(kNik_, kOtherUrl_, kGroup_);
  AddReport(kOtherNik_, kUrl_, kGroup_);
  AddReport(kOtherNik_, kUrl_, kGroup_);
  AddReport(kOtherNik_, kUrl_, kGroup_);
  AddReport(kOtherNik_, kOtherUrl_, kGroup_);
  AddReport(kOtherNik_, kOtherUrl_, kGroup_);
  AddReport(kOtherNik_, kOtherUrl_, kGroup_);
  AddReport(kOtherNik_, kOtherUrl_, kGroup_);
  EXPECT_EQ(0u, pending_uploads().size());

  // There should be one upload per (NIK, origin).
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

// Test that the agent won't start a second upload for a (NIK, origin, group)
// while one is pending, even if a different endpoint is available, but will
// once the original delivery is complete and the (NIK, origin, group) is no
// longer pending.
TEST_F(ReportingDeliveryAgentTest, SerializeUploadsToGroup) {
  static const GURL kDifferentEndpoint("https://endpoint2/");

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kDifferentEndpoint, kExpires_));

  // Trigger and complete an upload to start the delivery timer.
  UploadFirstReportAndStartTimer();

  // First upload causes this group key to become pending.
  AddReport(kNik_, kUrl_, kGroup_);
  EXPECT_EQ(0u, pending_uploads().size());
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  EXPECT_EQ(1u, pending_uploads().size());

  // Second upload isn't started because the group is pending.
  AddReport(kNik_, kUrl_, kGroup_);
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
// the same (NIK, origin) to endpoints with different URLs.
TEST_F(ReportingDeliveryAgentTest, ParallelizeUploadsAcrossGroups) {
  static const GURL kDifferentEndpoint("https://endpoint2/");
  static const std::string kDifferentGroup("group2");
  const ReportingEndpointGroupKey kDifferentGroupKey(kNik_, kOrigin_,
                                                     kDifferentGroup);

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  ASSERT_TRUE(
      SetEndpointInCache(kDifferentGroupKey, kDifferentEndpoint, kExpires_));

  // Trigger and complete an upload to start the delivery timer.
  UploadFirstReportAndStartTimer();

  AddReport(kNik_, kUrl_, kGroup_);
  AddReport(kNik_, kUrl_, kDifferentGroup);

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
// (NIK, origin) in the same upload if they are destined for the same endpoint
// URL.
TEST_F(ReportingDeliveryAgentTest, BatchReportsAcrossGroups) {
  static const std::string kDifferentGroup("group2");
  const ReportingEndpointGroupKey kDifferentGroupKey(kNik_, kOrigin_,
                                                     kDifferentGroup);

  ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_, kExpires_));
  ASSERT_TRUE(SetEndpointInCache(kDifferentGroupKey, kEndpoint_, kExpires_));

  UploadFirstReportAndStartTimer();

  AddReport(kNik_, kUrl_, kGroup_);
  AddReport(kNik_, kUrl_, kDifferentGroup);

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

}  // namespace
}  // namespace net
