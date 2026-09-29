// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager.h"

#import <memory>

#import "base/files/file_path.h"
#import "base/json/json_reader.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/policy/core/common/policy_types.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager_factory.h"
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

  // Starts a scan that completes synchronously: an empty file path makes
  // FilesRequestHandlerIOS run its completion callback from within
  // `StartScanner()`. This is the re-entrant path the manager has to handle,
  // since the scanner can't be destroyed while it is running that callback.
  void StartSynchronouslyCompletingScan() {
    manager_->StartScanner(CreateContentAnalysisInfo(), GURL(kDownloadUrl),
                           base::FilePath());
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
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  raw_ptr<BackgroundCloudScannerManager> manager_;
};

// Tests that a completed scanner is unregistered immediately but only destroyed
// once the completion callback it is running has returned.
TEST_F(BackgroundCloudScannerManagerTest, CompletedScannerIsDeletedAsync) {
  StartSynchronouslyCompletingScan();

  // The scan is done, so the scanner is no longer active, but its destruction
  // is deferred to a posted task.
  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 1u);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetPendingDeletionCountForTesting() == 0u; }));

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
}

// Tests that several scanners completing before the deletion task runs are all
// destroyed.
TEST_F(BackgroundCloudScannerManagerTest, MultipleCompletedScannersAreDeleted) {
  StartSynchronouslyCompletingScan();
  StartSynchronouslyCompletingScan();

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 2u);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetPendingDeletionCountForTesting() == 0u; }));
}

// Tests that active in-flight scanners in `scanners_` are directly cleared and
// destroyed by Shutdown() without crashing when reporting cancellation.
TEST_F(BackgroundCloudScannerManagerTest, ShutdownDeletesActiveScanners) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kEnableCancelUploadOnContentAnalysis);
  SetUpAnalysisConnectorPolicy();

  manager_->StartScanner(CreateContentAnalysisInfo(), GURL(kDownloadUrl),
                         base::FilePath(FILE_PATH_LITERAL("/path/to/file")));
  ASSERT_EQ(manager_->GetScannerCountForTesting(), 1u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);

  manager_->Shutdown();

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);
}

// Tests that scanners waiting to be deleted are destroyed by Shutdown() when
// the profile goes away before the deletion task runs, and that the pending
// task is then harmless.
TEST_F(BackgroundCloudScannerManagerTest, ShutdownDeletesPendingScanners) {
  StartSynchronouslyCompletingScan();
  ASSERT_EQ(manager_->GetPendingDeletionCountForTesting(), 1u);

  manager_->Shutdown();

  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);

  // The already posted DeletePendingScanners() task must still run cleanly.
  RunUntilPendingScannersAreDeleted();

  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);
}

// Tests that no scan is started once the profile is being torn down.
TEST_F(BackgroundCloudScannerManagerTest, StartScannerAfterShutdownIsIgnored) {
  manager_->Shutdown();

  StartSynchronouslyCompletingScan();

  EXPECT_EQ(manager_->GetScannerCountForTesting(), 0u);
  EXPECT_EQ(manager_->GetPendingDeletionCountForTesting(), 0u);
}

}  // namespace enterprise_connectors
