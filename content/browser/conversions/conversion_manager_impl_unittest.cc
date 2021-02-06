// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_manager_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr base::TimeDelta kExpiredReportOffset =
    base::TimeDelta::FromMinutes(2);

class ConstantStartupDelayPolicy : public ConversionPolicy {
 public:
  ConstantStartupDelayPolicy() = default;
  ~ConstantStartupDelayPolicy() override = default;

  base::Time GetReportTimeForExpiredReportAtStartup(
      base::Time now) const override {
    return now + kExpiredReportOffset;
  }
};

// Mock reporter that tracks reports being queued by the ConversionManager.
class TestConversionReporter
    : public ConversionManagerImpl::ConversionReporter {
 public:
  TestConversionReporter() = default;
  ~TestConversionReporter() override = default;

  // ConversionManagerImpl::ConversionReporter
  void AddReportsToQueue(
      std::vector<ConversionReport> reports,
      base::RepeatingCallback<void(int64_t)> report_sent_callback) override {
    num_reports_ += reports.size();
    last_conversion_id_ = *reports.back().conversion_id;
    last_report_time_ = reports.back().report_time;

    if (should_run_report_sent_callbacks_) {
      for (const auto& report : reports) {
        report_sent_callback.Run(*report.conversion_id);
      }
    }

    if (quit_closure_ && num_reports_ >= expected_num_reports_)
      std::move(quit_closure_).Run();
  }

  void ShouldRunReportSentCallbacks(bool should_run_report_sent_callbacks) {
    should_run_report_sent_callbacks_ = should_run_report_sent_callbacks;
  }

  size_t num_reports() { return num_reports_; }

  int64_t last_conversion_id() { return last_conversion_id_; }

  base::Time last_report_time() { return last_report_time_; }

  void WaitForNumReports(size_t expected_num_reports) {
    if (num_reports_ >= expected_num_reports)
      return;

    expected_num_reports_ = expected_num_reports;
    base::RunLoop wait_loop;
    quit_closure_ = wait_loop.QuitClosure();
    wait_loop.Run();
  }

 private:
  bool should_run_report_sent_callbacks_ = false;
  size_t expected_num_reports_ = 0u;
  size_t num_reports_ = 0u;
  int64_t last_conversion_id_ = 0UL;
  base::Time last_report_time_;
  base::OnceClosure quit_closure_;
};

// Time after impression that a conversion can first be sent. See
// ConversionStorageDelegateImpl::GetReportTimeForConversion().
constexpr base::TimeDelta kFirstReportingWindow = base::TimeDelta::FromDays(2);

// Give impressions a sufficiently long expiry.
constexpr base::TimeDelta kImpressionExpiry = base::TimeDelta::FromDays(30);

}  // namespace

class ConversionManagerImplTest : public testing::Test {
 public:
  ConversionManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        mock_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    CreateManager();
  }

  void CreateManager() {
    auto reporter = std::make_unique<TestConversionReporter>();
    test_reporter_ = reporter.get();
    conversion_manager_ = ConversionManagerImpl::CreateForTesting(
        std::move(reporter), std::make_unique<ConstantStartupDelayPolicy>(),
        task_environment_.GetMockClock(), dir_.GetPath(), mock_storage_policy_);
  }

  void ExpectNumStoredImpressions(size_t expected_num_impressions) {
    // There should be one impression and one conversion.
    base::RunLoop impression_loop;
    auto get_impressions_callback = base::BindLambdaForTesting(
        [&](std::vector<StorableImpression> impressions) {
          EXPECT_EQ(expected_num_impressions, impressions.size());
          impression_loop.Quit();
        });
    conversion_manager_->GetActiveImpressionsForWebUI(
        std::move(get_impressions_callback));
    impression_loop.Run();
  }

  void ExpectNumStoredReports(size_t expected_num_reports) {
    base::RunLoop report_loop;
    auto reports_callback =
        base::BindLambdaForTesting([&](std::vector<ConversionReport> reports) {
          EXPECT_EQ(expected_num_reports, reports.size());
          report_loop.Quit();
        });
    conversion_manager_->GetReportsForWebUI(std::move(reports_callback),
                                            base::Time::Max());
    report_loop.Run();
  }

  const base::Clock& clock() { return *task_environment_.GetMockClock(); }

 protected:
  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ConversionManagerImpl> conversion_manager_;
  TestConversionReporter* test_reporter_ = nullptr;
  scoped_refptr<storage::MockSpecialStoragePolicy> mock_storage_policy_;
};

TEST_F(ConversionManagerImplTest, ImpressionRegistered_ReturnedToWebUI) {
  auto impression = ImpressionBuilder(clock().Now())
                        .SetExpiry(kImpressionExpiry)
                        .SetData("100")
                        .Build();
  conversion_manager_->HandleImpression(impression);

  base::RunLoop run_loop;
  auto get_impressions_callback = base::BindLambdaForTesting(
      [&](std::vector<StorableImpression> impressions) {
        EXPECT_EQ(1u, impressions.size());
        EXPECT_TRUE(ImpressionsEqual(impression, impressions.back()));
        run_loop.Quit();
      });
  conversion_manager_->GetActiveImpressionsForWebUI(
      std::move(get_impressions_callback));
  run_loop.Run();
}

TEST_F(ConversionManagerImplTest, ExpiredImpression_NotReturnedToWebUI) {
  conversion_manager_->HandleImpression(ImpressionBuilder(clock().Now())
                                            .SetExpiry(kImpressionExpiry)
                                            .SetData("100")
                                            .Build());
  task_environment_.FastForwardBy(2 * kImpressionExpiry);

  base::RunLoop run_loop;
  auto get_impressions_callback = base::BindLambdaForTesting(
      [&](std::vector<StorableImpression> impressions) {
        EXPECT_TRUE(impressions.empty());
        run_loop.Quit();
      });
  conversion_manager_->GetActiveImpressionsForWebUI(
      std::move(get_impressions_callback));
  run_loop.Run();
}

TEST_F(ConversionManagerImplTest, ImpressionConverted_ReportReturnedToWebUI) {
  auto impression = ImpressionBuilder(clock().Now())
                        .SetExpiry(kImpressionExpiry)
                        .SetData("100")
                        .Build();
  conversion_manager_->HandleImpression(impression);

  auto conversion = DefaultConversion();
  conversion_manager_->HandleConversion(conversion);

  ConversionReport expected_report(
      impression, conversion.conversion_data(),
      /*conversion_time=*/clock().Now(),
      /*report_time=*/clock().Now() + kFirstReportingWindow,
      base::nullopt /* conversion_id */);
  expected_report.attribution_credit = 100;

  base::RunLoop run_loop;
  auto reports_callback =
      base::BindLambdaForTesting([&](std::vector<ConversionReport> reports) {
        EXPECT_EQ(1u, reports.size());
        EXPECT_TRUE(ReportsEqual({expected_report}, reports));
        run_loop.Quit();
      });
  conversion_manager_->GetReportsForWebUI(std::move(reports_callback),
                                          base::Time::Max());
  run_loop.Run();
}

TEST_F(ConversionManagerImplTest, ImpressionConverted_ReportQueued) {
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());

  // Reports are queued in intervals ahead of when they should be
  // sent. Make sure the report is not queued earlier than this.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kConversionManagerQueueReportsInterval -
                                  base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(0u, test_reporter_->num_reports());

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1u, test_reporter_->num_reports());
}

TEST_F(ConversionManagerImplTest, QueuedReportNotSent_QueuedAgain) {
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kConversionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // If the report is not sent, it should be added to the queue again.
  task_environment_.FastForwardBy(kConversionManagerQueueReportsInterval);
  EXPECT_EQ(2u, test_reporter_->num_reports());
}

TEST_F(ConversionManagerImplTest, QueuedReportSent_NotQueuedAgain) {
  test_reporter_->ShouldRunReportSentCallbacks(true);
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  kConversionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // The report should not be added to the queue again.
  task_environment_.FastForwardBy(kConversionManagerQueueReportsInterval);
  EXPECT_EQ(1u, test_reporter_->num_reports());
}

// Add a conversion to storage and reset the manager to mimic a report being
// available at startup.
TEST_F(ConversionManagerImplTest, ExpiredReportsAtStartup_Queued) {
  // Create a report that will be reported at t= 2 days.
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());

  // Create another conversion that will be reported at t=
  // (kFirstReportingWindow + 2 * kConversionManagerQueueReportsInterval).
  task_environment_.FastForwardBy(2 * kConversionManagerQueueReportsInterval);
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());

  EXPECT_EQ(0u, test_reporter_->num_reports());

  // Reset the manager to simulate shutdown.
  conversion_manager_.reset();

  // Fast forward past the expected report time of the first conversion, t =
  // (kFirstReportingWindow+ 1 minute).
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  (2 * kConversionManagerQueueReportsInterval) +
                                  base::TimeDelta::FromMinutes(1));

  // Create the manager and check that the first report is queued immediately.
  CreateManager();
  test_reporter_->ShouldRunReportSentCallbacks(true);
  test_reporter_->WaitForNumReports(1);
  EXPECT_EQ(1u, test_reporter_->num_reports());

  // The second report is still queued at the correct time.
  task_environment_.FastForwardBy(kConversionManagerQueueReportsInterval);
  EXPECT_EQ(2u, test_reporter_->num_reports());
}

// This functionality is tested more thoroughly in the ConversionStorageSql
// unit tests. Here, just test to make sure the basic control flow is working.
TEST_F(ConversionManagerImplTest, ClearData) {
  for (bool match_url : {true, false}) {
    base::Time start = clock().Now();
    conversion_manager_->HandleImpression(
        ImpressionBuilder(start).SetExpiry(kImpressionExpiry).Build());
    conversion_manager_->HandleConversion(DefaultConversion());

    base::RunLoop run_loop;
    conversion_manager_->ClearData(
        start, start + base::TimeDelta::FromMinutes(1),
        base::BindLambdaForTesting(
            [match_url](const url::Origin& _) { return match_url; }),
        run_loop.QuitClosure());
    run_loop.Run();

    task_environment_.FastForwardBy(kFirstReportingWindow -
                                    kConversionManagerQueueReportsInterval);
    size_t expected_reports = match_url ? 0u : 1u;
    EXPECT_EQ(expected_reports, test_reporter_->num_reports());
  }
}

TEST_F(ConversionManagerImplTest, ConversionsSentFromUI_ReportedImmediately) {
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());
  EXPECT_EQ(0u, test_reporter_->num_reports());

  conversion_manager_->SendReportsForWebUI(base::DoNothing());
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(2u, test_reporter_->num_reports());
}

// TODO(crbug.com/1088449): Flaky on Linux and Android.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#define MAYBE_ExpiredReportsAtStartup_Delayed \
  DISABLED_ExpiredReportsAtStartup_Delayed
#else
#define MAYBE_ExpiredReportsAtStartup_Delayed ExpiredReportsAtStartup_Delayed
#endif
TEST_F(ConversionManagerImplTest, MAYBE_ExpiredReportsAtStartup_Delayed) {
  // Create a report that will be reported at t= 2 days.
  base::Time start_time = clock().Now();
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());
  EXPECT_EQ(0u, test_reporter_->num_reports());

  // Reset the manager to simulate shutdown.
  conversion_manager_.reset();

  // Fast forward past the expected report time of the first conversion, t =
  // (kFirstReportingWindow+ 1 minute).
  task_environment_.FastForwardBy(kFirstReportingWindow +
                                  base::TimeDelta::FromMinutes(1));

  CreateManager();
  test_reporter_->WaitForNumReports(1);

  // Ensure that the expired report is delayed based on the time the browser
  // started.
  EXPECT_EQ(start_time + kFirstReportingWindow +
                base::TimeDelta::FromMinutes(1) + kExpiredReportOffset,
            test_reporter_->last_report_time());
}

TEST_F(ConversionManagerImplTest, NonExpiredReportsQueuedAtStartup_NotDelayed) {
  // Create a report that will be reported at t= 2 days.
  base::Time start_time = clock().Now();
  conversion_manager_->HandleImpression(
      ImpressionBuilder(clock().Now()).SetExpiry(kImpressionExpiry).Build());
  conversion_manager_->HandleConversion(DefaultConversion());
  EXPECT_EQ(0u, test_reporter_->num_reports());

  // Reset the manager to simulate shutdown.
  conversion_manager_.reset();

  // Fast forward just before the expected report time.
  task_environment_.FastForwardBy(kFirstReportingWindow -
                                  base::TimeDelta::FromMinutes(1));

  // Ensure that this report does not receive additional delay.
  CreateManager();
  test_reporter_->WaitForNumReports(1);
  EXPECT_EQ(1u, test_reporter_->num_reports());
  EXPECT_EQ(start_time + kFirstReportingWindow,
            test_reporter_->last_report_time());
}

TEST_F(ConversionManagerImplTest, SessionOnlyOrigins_DataDeletedAtShutdown) {
  GURL session_only_origin("https://sessiononly.example");
  auto impression =
      ImpressionBuilder(clock().Now())
          .SetImpressionOrigin(url::Origin::Create(session_only_origin))
          .Build();

  mock_storage_policy_->AddSessionOnly(session_only_origin);

  conversion_manager_->HandleImpression(impression);
  conversion_manager_->HandleConversion(DefaultConversion());

  ExpectNumStoredImpressions(1u);
  ExpectNumStoredReports(1u);

  // Reset the manager to simulate shutdown.
  conversion_manager_.reset();
  CreateManager();

  ExpectNumStoredImpressions(0u);
  ExpectNumStoredReports(0u);
}

TEST_F(ConversionManagerImplTest,
       SessionOnlyOrigins_DeletedIfAnyOriginMatches) {
  url::Origin session_only_origin =
      url::Origin::Create(GURL("https://sessiononly.example"));
  // Create impressions which each have the session only origin as one of
  // impression/conversion/reporting origin.
  auto impression1 = ImpressionBuilder(clock().Now())
                         .SetImpressionOrigin(session_only_origin)
                         .Build();
  auto impression2 = ImpressionBuilder(clock().Now())
                         .SetReportingOrigin(session_only_origin)
                         .Build();
  auto impression3 = ImpressionBuilder(clock().Now())
                         .SetConversionOrigin(session_only_origin)
                         .Build();

  // Create one  impression which is not session only.
  auto impression4 = ImpressionBuilder(clock().Now()).Build();

  mock_storage_policy_->AddSessionOnly(session_only_origin.GetURL());

  conversion_manager_->HandleImpression(impression1);
  conversion_manager_->HandleImpression(impression2);
  conversion_manager_->HandleImpression(impression3);
  conversion_manager_->HandleImpression(impression4);

  ExpectNumStoredImpressions(4u);

  // Reset the manager to simulate shutdown.
  conversion_manager_.reset();
  CreateManager();

  // All session-only impressions should be deleted.
  ExpectNumStoredImpressions(1u);
}

}  // namespace content
