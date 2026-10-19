// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager.h"

#import <memory>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/json/json_reader.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/policy/core/common/policy_types.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager_factory.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/download_protection_metrics.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kDownloadUrl[] = "https://example.com/download";

constexpr char kNonBlockingAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ],
    "block_until_verdict": 0
  }
])";

}  // namespace

// Unit tests for BackgroundCloudScannerManager, focusing on the lifetime of the
// scanners it owns: they must be unregistered as soon as they complete,
// destroyed outside of their own completion callback, and never outlive the
// profile.
class BackgroundCloudScannerManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    test_file_path_ =
        scoped_temp_dir_.GetPath().Append(FILE_PATH_LITERAL("test_file.txt"));
    ASSERT_TRUE(base::WriteFile(test_file_path_, "test content"));

    TestProfileIOS::Builder profile_builder;
    profile_ = std::move(profile_builder).Build();
    web_state_.SetBrowserState(profile_.get());
    manager_ =
        BackgroundCloudScannerManagerFactory::GetForProfile(profile_.get());
    ASSERT_NE(manager_, nullptr);
  }

  void TearDown() override {
    // Release the pointer to the KeyedService before the profile destroys it.
    manager_ = nullptr;
    PlatformTest::TearDown();
  }

  std::unique_ptr<ContentAnalysisInfo> CreateContentAnalysisInfo() {
    std::optional<AnalysisSettings> settings;
    if (ConnectorsService* connectors_service =
            ConnectorsServiceFactory::GetForProfile(profile_.get())) {
      settings = connectors_service->GetAnalysisSettings(
          GURL(kDownloadUrl), AnalysisConnector::FILE_DOWNLOADED);
    }
    return std::make_unique<ContentAnalysisInfo>(
        GURL(kDownloadUrl), std::move(settings).value_or(AnalysisSettings()),
        ContentAnalysisRequest::NORMAL_DOWNLOAD, web_state_);
  }

  // Starts a scan that completes synchronously: without connector policies
  // enabled, FilesRequestHandlerIOS runs its completion callback synchronously
  // upon `StartScanner()`.
  void StartSynchronouslyCompletingScan(
      base::test::TestFuture<bool>& download_proceed) {
    manager_->StartScanner(CreateContentAnalysisInfo(), GURL(kDownloadUrl),
                           test_file_path_, download_proceed.GetCallback());
  }

  // Runs the task queue until the task posted by the manager to delete the
  // completed scanners has run. Posting the quit closure after that task
  // guarantees it has run once the run loop stops.
  void RunUntilPendingScannersAreDeleted() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void SetUpAnalysisConnectorPolicy() {
    fake_browser_dm_token_storage_.SetDMToken("fake_dm_token");
    fake_browser_dm_token_storage_.SetClientId("fake_client_id");

    profile_->GetPrefs()->Set(
        enterprise_connectors::AnalysisConnectorPref(
            enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED),
        *base::JSONReader::Read(kNonBlockingAnalysisSettingsPref,
                                base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    profile_->GetPrefs()->SetInteger(
        enterprise_connectors::AnalysisConnectorScopePref(
            enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED),
        policy::POLICY_SCOPE_MACHINE);
  }

  web::WebTaskEnvironment task_environment_;
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage_;
  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath test_file_path_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  raw_ptr<BackgroundCloudScannerManager> manager_;
};

// Tests that a completed scanner is unregistered immediately but only destroyed
// once the completion callback it is running has returned.
TEST_F(BackgroundCloudScannerManagerTest, CompletedScannerIsDeletedAsync) {
  base::test::TestFuture<bool> download_proceed;
  StartSynchronouslyCompletingScan(download_proceed);
  EXPECT_TRUE(download_proceed.Get());

  histogram_tester_.ExpectUniqueSample(
      kIOSDownloadProtectionScanTriggeredClonefileSuccessHistogram, true, 1);

  // The scan is done, so the scanner is no longer active, but its destruction
  // is deferred to a posted task.
  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 1u);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetPendingDeletionCountForTesting() == 0u; }));

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester_.GetBucketCount(
               kIOSDownloadProtectionScanTriggeredTempDirectoryDeleteSuccessHistogram,
               true) == 1;
  }));
}

// Tests that several scanners completing before the deletion task runs are all
// destroyed.
TEST_F(BackgroundCloudScannerManagerTest, MultipleCompletedScannersAreDeleted) {
  base::test::TestFuture<bool> download_proceed_1;
  StartSynchronouslyCompletingScan(download_proceed_1);
  base::test::TestFuture<bool> download_proceed_2;
  StartSynchronouslyCompletingScan(download_proceed_2);

  EXPECT_TRUE(download_proceed_1.Get());
  EXPECT_TRUE(download_proceed_2.Get());

  histogram_tester_.ExpectUniqueSample(
      kIOSDownloadProtectionScanTriggeredClonefileSuccessHistogram, true, 2);

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);

  // Depending on whether the scanner for `download_proceed_1` is cleaned up
  // before we successfully cloned the temporary file for `download_proceed_2`,
  // we would either have 1 or 2 pending deletion count.
  EXPECT_GE(manager_->GetPendingDeletionCountForTesting(), 1u);
  EXPECT_LE(manager_->GetPendingDeletionCountForTesting(), 2u);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetPendingDeletionCountForTesting() == 0u; }));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester_.GetBucketCount(
               kIOSDownloadProtectionScanTriggeredTempDirectoryDeleteSuccessHistogram,
               true) == 2;
  }));
}

// Tests that active in-flight scanners in `scanners_` are directly cleared and
// destroyed by Shutdown() without crashing when reporting cancellation.
TEST_F(BackgroundCloudScannerManagerTest, ShutdownDeletesActiveScanners) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kEnableCancelUploadOnContentAnalysis);
  SetUpAnalysisConnectorPolicy();

  base::test::TestFuture<bool> download_proceed;
  manager_->StartScanner(CreateContentAnalysisInfo(), GURL(kDownloadUrl),
                         test_file_path_, download_proceed.GetCallback());
  EXPECT_TRUE(download_proceed.Get());

  histogram_tester_.ExpectUniqueSample(
      kIOSDownloadProtectionScanTriggeredClonefileSuccessHistogram, true, 1);

  ASSERT_EQ(manager_->GetScannerCountForTesting(), 1u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);

  manager_->Shutdown();

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester_.GetBucketCount(
               kIOSDownloadProtectionScanTriggeredTempDirectoryDeleteSuccessHistogram,
               true) == 1;
  }));
}

// Tests that scanners waiting to be deleted are destroyed by Shutdown() when
// the profile goes away before the deletion task runs, and that the pending
// task is then harmless.
TEST_F(BackgroundCloudScannerManagerTest, ShutdownDeletesPendingScanners) {
  base::test::TestFuture<bool> download_proceed;
  StartSynchronouslyCompletingScan(download_proceed);
  EXPECT_TRUE(download_proceed.Get());

  histogram_tester_.ExpectUniqueSample(
      kIOSDownloadProtectionScanTriggeredClonefileSuccessHistogram, true, 1);

  ASSERT_EQ(manager_->GetPendingDeletionCountForTesting(), 1u);

  manager_->Shutdown();

  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);

  // The already posted DeletePendingScanners() task must still run cleanly.
  RunUntilPendingScannersAreDeleted();

  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester_.GetBucketCount(
               kIOSDownloadProtectionScanTriggeredTempDirectoryDeleteSuccessHistogram,
               true) == 1;
  }));
}

// Tests that no scan is started once the profile is being torn down.
TEST_F(BackgroundCloudScannerManagerTest, StartScannerAfterShutdownIsIgnored) {
  manager_->Shutdown();

  base::test::TestFuture<bool> download_proceed;
  StartSynchronouslyCompletingScan(download_proceed);
  EXPECT_TRUE(download_proceed.Get());

  histogram_tester_.ExpectTotalCount(
      kIOSDownloadProtectionScanTriggeredClonefileSuccessHistogram, 0);

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);
  histogram_tester_.ExpectTotalCount(
      kIOSDownloadProtectionScanTriggeredTempDirectoryDeleteSuccessHistogram,
      0);
}

// Tests that when `kEnableDLPClonefileScanUploadIOS` is disabled, the scanner
// still starts using the original file path and does not delete it on cleanup.
TEST_F(BackgroundCloudScannerManagerTest,
       CloneDisabledFallsBackToOriginalFile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEnableDLPClonefileScanUploadIOS);

  base::test::TestFuture<bool> download_proceed;
  StartSynchronouslyCompletingScan(download_proceed);
  EXPECT_TRUE(download_proceed.Get());

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 1u);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetPendingDeletionCountForTesting() == 0u; }));

  EXPECT_TRUE(base::PathExists(test_file_path_));
}

// Tests that when `clonefile` fails (e.g. source file does not exist), the
// scanner still starts with the original path instead of dropping the scan.
TEST_F(BackgroundCloudScannerManagerTest, CloneFailureFallsBackToOriginalFile) {
  base::FilePath missing_path =
      scoped_temp_dir_.GetPath().Append(FILE_PATH_LITERAL("missing_file.txt"));
  base::test::TestFuture<bool> download_proceed;
  manager_->StartScanner(CreateContentAnalysisInfo(), GURL(kDownloadUrl),
                         missing_path, download_proceed.GetCallback());
  EXPECT_TRUE(download_proceed.Get());

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 1u);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetPendingDeletionCountForTesting() == 0u; }));
}

}  // namespace enterprise_connectors
