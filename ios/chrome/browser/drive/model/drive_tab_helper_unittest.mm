// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/drive/model/upload_task.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/scan_decision_helper.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/enterprise_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/fakes/fake_enterprise_commands_handler.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";

}  // namespace

// DriveTabHelper unit tests.
class DriveTabHelperTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserListFactory::GetForProfile(profile_.get())
        ->AddBrowser(browser_.get());

    fake_enterprise_commands_handler_ =
        [[FakeEnterpriseCommandsHandler alloc] init];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:fake_enterprise_commands_handler_
                     forProtocol:@protocol(EnterpriseCommands)];

    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state_->SetBrowserState(profile_.get());
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));
    web_state_->WasShown();

    download_task_ =
        std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
    download_task_->SetWebState(web_state_);
    DriveTabHelper::CreateForWebState(web_state_);
    helper_ = DriveTabHelper::FromWebState(web_state_);
  }

 public:
  void MaybeUploadDownloadToDrive(web::DownloadTask* task,
                                  bool should_proceed) {
    helper_->MaybeUploadDownloadToDrive(task, should_proceed);
  }

  void OnDownloadUpdated(web::DownloadTask* task) {
    helper_->OnDownloadUpdated(task);
  }

 protected:
  web::WebTaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<web::FakeWebState> web_state_ = nullptr;
  std::unique_ptr<web::FakeDownloadTask> download_task_;
  raw_ptr<DriveTabHelper> helper_;
  FakeEnterpriseCommandsHandler* fake_enterprise_commands_handler_;
  id mock_snackbar_handler_;
};

// Tests that upon `DownloadTask` being destroyed, the `DriveTabHelper` stops
// observing it i.e. removes itself from observers and resets its state.
TEST_F(DriveTabHelperTest, StopsObservingDestroyedDownloadTask) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  helper_->AddDownloadToSaveToDrive(download_task_.get(), identity);
  UploadTask* upload_task =
      helper_->GetUploadTaskForDownload(download_task_.get());
  ASSERT_NE(nullptr, upload_task);
  EXPECT_EQ(identity, upload_task->GetIdentity());
  download_task_.reset();
  EXPECT_EQ(nullptr, helper_->GetUploadTaskForDownload(download_task_.get()));
}

// Tests that when `MaybeUploadDownloadToDrive` is called with `shouldProceed`
// as false, the download task is cancelled and the upload task is cleared.
TEST_F(DriveTabHelperTest,
       MaybeUploadDownloadToDriveCancelsTaskWhenShouldNotProceed) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  helper_->AddDownloadToSaveToDrive(download_task_.get(), identity);

  // Verify that an upload task was created.
  UploadTask* upload_task =
      helper_->GetUploadTaskForDownload(download_task_.get());
  ASSERT_NE(nullptr, upload_task);

  // Simulate a scan decision not to proceed.
  MaybeUploadDownloadToDrive(download_task_.get(), /*shouldProceed=*/false);

  // The task should be cancelled.
  EXPECT_EQ(web::DownloadTask::State::kCancelled, download_task_->GetState());

  // The upload task should be cleared.
  EXPECT_EQ(nullptr, helper_->GetUploadTaskForDownload(download_task_.get()));
}

// Tests that when the feature flag is disabled, the upload starts directly
// when the download is complete.
TEST_F(DriveTabHelperTest, UploadStartsDirectlyWhenFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      enterprise_connectors::kEnableFileDownloadConnectorIOS);

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  helper_->AddDownloadToSaveToDrive(download_task_.get(), identity);

  UploadTask* upload_task =
      helper_->GetUploadTaskForDownload(download_task_.get());
  ASSERT_NE(nullptr, upload_task);
  EXPECT_EQ(UploadTask::State::kNotStarted, upload_task->GetState());

  // Simulate completion of the download task.
  download_task_->SetDone(true);
  OnDownloadUpdated(download_task_.get());

  // Since the feature is disabled, it should have started the upload.
  EXPECT_EQ(UploadTask::State::kInProgress, upload_task->GetState());
}

// Tests that when scanning is ENABLED and result is SUCCESS, the upload
// proceeds.
TEST_F(DriveTabHelperTest, ScanningSuccessStartsUpload) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  helper_->AddDownloadToSaveToDrive(download_task_.get(), identity);

  UploadTask* upload_task =
      helper_->GetUploadTaskForDownload(download_task_.get());
  ASSERT_NE(nullptr, upload_task);

  enterprise_connectors::RequestHandlerResult result;
  result.final_result =
      enterprise_connectors::FinalContentAnalysisResult::SUCCESS;

  enterprise_connectors::HandleScanDecision(
      web_state_->GetWeakPtr(), enterprise_connectors::TriggerType::kSavePrompt,
      base::BindOnce(
          [](DriveTabHelperTest* test, web::DownloadTask* task,
             bool should_proceed) {
            test->MaybeUploadDownloadToDrive(task, should_proceed);
          },
          base::Unretained(this), download_task_.get()),
      result);

  // Upload should have started.
  EXPECT_EQ(UploadTask::State::kInProgress, upload_task->GetState());
}

// Tests that if the scanning decision is WARNING, a warning dialog is
// triggered.
TEST_F(DriveTabHelperTest, ScanningWarningTriggersDialog) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  helper_->AddDownloadToSaveToDrive(download_task_.get(), identity);

  enterprise_connectors::RequestHandlerResult result;
  result.final_result =
      enterprise_connectors::FinalContentAnalysisResult::WARNING;

  enterprise_connectors::HandleScanDecision(
      web_state_->GetWeakPtr(), enterprise_connectors::TriggerType::kSavePrompt,
      base::BindOnce(
          [](DriveTabHelperTest* test, web::DownloadTask* task,
             bool should_proceed) {
            test->MaybeUploadDownloadToDrive(task, should_proceed);
          },
          base::Unretained(this), download_task_.get()),
      result);

  // Warning dialog should be triggered.
  EXPECT_TRUE(fake_enterprise_commands_handler_->_callback);
}

// Tests that if the scanning decision is FAILURE (block), a snackbar is
// triggered and the task is cancelled.
TEST_F(DriveTabHelperTest, ScanningFailureTriggersSnackbarAndCancels) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  helper_->AddDownloadToSaveToDrive(download_task_.get(), identity);

  OCMExpect([mock_snackbar_handler_
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg any]]);

  enterprise_connectors::RequestHandlerResult result;
  result.final_result =
      enterprise_connectors::FinalContentAnalysisResult::FAILURE;

  enterprise_connectors::HandleScanDecision(
      web_state_->GetWeakPtr(), enterprise_connectors::TriggerType::kSavePrompt,
      base::BindOnce(
          [](DriveTabHelperTest* test, web::DownloadTask* task,
             bool should_proceed) {
            test->MaybeUploadDownloadToDrive(task, should_proceed);
          },
          base::Unretained(this), download_task_.get()),
      result);

  // Snackbar should be triggered.
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);

  // The task should be cancelled.
  EXPECT_EQ(web::DownloadTask::State::kCancelled, download_task_->GetState());

  // The upload task should be cleared.
  EXPECT_EQ(nullptr, helper_->GetUploadTaskForDownload(download_task_.get()));
}
