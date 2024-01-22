// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_mediator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/drive/model/upload_task.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";

}  // namespace

#pragma mark - FakeDownloadManagerTabHelper

// Fake `DownloadManagerTabHelper` to override `StartDownload()`.
class FakeDownloadManagerTabHelper final : public DownloadManagerTabHelper {
 public:
  explicit FakeDownloadManagerTabHelper(web::WebState* web_state)
      : DownloadManagerTabHelper(web_state) {}

  static void CreateForWebState(web::WebState* web_state) {
    web_state->SetUserData(
        UserDataKey(),
        std::make_unique<FakeDownloadManagerTabHelper>(web_state));
  }

  void StartDownload(web::DownloadTask* task) override {
    download_task_started_ = task;
  }

  raw_ptr<web::DownloadTask> download_task_started_ = nullptr;
};

#pragma mark - FakeSaveToDriveCommandsHandler

@interface FakeSaveToDriveCommandsHandler : NSObject <SaveToDriveCommands>

// Current task presented in the Save to Drive UI.
@property(nonatomic, assign) raw_ptr<web::DownloadTask> presentedDownloadTask;

// SaveToDriveCommands methods.
- (void)showSaveToDriveForDownload:(web::DownloadTask*)downloadTask;
- (void)hideSaveToDrive;

@end

@implementation FakeSaveToDriveCommandsHandler

- (void)showSaveToDriveForDownload:(web::DownloadTask*)downloadTask {
  self.presentedDownloadTask = downloadTask;
}

- (void)hideSaveToDrive {
  self.presentedDownloadTask = nullptr;
}

@end

#pragma mark - SaveToDriveMediatorTest

// Test fixture for testing SaveToDriveMediator class.
class SaveToDriveMediatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    browser_state_ = TestChromeBrowserState::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(browser_state_.get());
    DriveTabHelper::CreateForWebState(web_state_.get());
    FakeDownloadManagerTabHelper::CreateForWebState(web_state_.get());
    download_task_ =
        std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
    download_task_->SetWebState(web_state_.get());
    save_to_drive_commands_handler_ =
        [[FakeSaveToDriveCommandsHandler alloc] init];
    [save_to_drive_commands_handler_
        showSaveToDriveForDownload:download_task_.get()];
    mediator_ = [[SaveToDriveMediator alloc]
              initWithDownloadTask:download_task_.get()
        saveToDriveCommandsHandler:save_to_drive_commands_handler_];
  }

  void TearDown() final {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  DriveTabHelper* GetDriveTabHelper() const {
    return DriveTabHelper::FromWebState(web_state_.get());
  }

  FakeDownloadManagerTabHelper* GetDownloadManagerTabHelper() const {
    return static_cast<FakeDownloadManagerTabHelper*>(
        DownloadManagerTabHelper::FromWebState(web_state_.get()));
  }

  base::test::TaskEnvironment task_environment;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<web::FakeDownloadTask> download_task_;
  FakeSaveToDriveCommandsHandler* save_to_drive_commands_handler_;
  SaveToDriveMediator* mediator_;
};

// Tests that the Save to Drive UI is hidden when the `DownloadTask` is
// destroyed.
TEST_F(SaveToDriveMediatorTest, HidesSaveToDriveOnDownloadTaskDestroyed) {
  EXPECT_EQ(download_task_.get(),
            save_to_drive_commands_handler_.presentedDownloadTask);
  download_task_.reset();
  EXPECT_EQ(nullptr, save_to_drive_commands_handler_.presentedDownloadTask);
}

// Tests that the Save to Drive UI is hidden when the `WebState` is destroyed.
TEST_F(SaveToDriveMediatorTest, HidesSaveToDriveOnWebStateDestroyed) {
  EXPECT_EQ(download_task_.get(),
            save_to_drive_commands_handler_.presentedDownloadTask);
  web_state_.reset();
  EXPECT_EQ(nullptr, save_to_drive_commands_handler_.presentedDownloadTask);
}

// Tests that the Save to Drive UI is hidden when the `WebState` is hidden.
TEST_F(SaveToDriveMediatorTest, HidesSaveToDriveOnWebStateHidden) {
  EXPECT_EQ(download_task_.get(),
            save_to_drive_commands_handler_.presentedDownloadTask);
  web_state_->WasHidden();
  EXPECT_EQ(nullptr, save_to_drive_commands_handler_.presentedDownloadTask);
}

// Tests that the `DownloadManagerTabHelper` is informed and that the
// `DownloadTask` and the selected identity are not sent to the `DriveTabHelper`
// when `startDownloadWithIdentity:` is invoked if the selected destination is
// `FileDestination::kFiles`.
TEST_F(SaveToDriveMediatorTest, DoesNotSaveToDriveIfDestinationIsFiles) {
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
  FakeDownloadManagerTabHelper* download_helper = GetDownloadManagerTabHelper();
  DriveTabHelper* drive_helper = GetDriveTabHelper();
  EXPECT_EQ(nullptr, download_helper->download_task_started_);
  EXPECT_EQ(nullptr,
            drive_helper->GetUploadTaskForDownload(download_task_.get()));
  [mediator_ fileDestinationPicker:nil
              didSelectDestination:FileDestination::kFiles];
  [mediator_ startDownloadWithIdentity:identity];
  EXPECT_EQ(download_task_.get(), download_helper->download_task_started_);
  UploadTask* upload_task =
      drive_helper->GetUploadTaskForDownload(download_task_.get());
  ASSERT_EQ(nullptr, upload_task);
}

// Tests that the `DownloadManagerTabHelper` is informed and that the
// `DownloadTask` and the selected identity are sent to the `DriveTabHelper`
// when `startDownloadWithIdentity:` is invoked if the selected destination is
// `FileDestination::kDrive`.
TEST_F(SaveToDriveMediatorTest, SavesToDriveIfDestinationIsDrive) {
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
  FakeDownloadManagerTabHelper* download_helper = GetDownloadManagerTabHelper();
  DriveTabHelper* drive_helper = GetDriveTabHelper();
  EXPECT_EQ(nullptr, download_helper->download_task_started_);
  EXPECT_EQ(nullptr,
            drive_helper->GetUploadTaskForDownload(download_task_.get()));
  [mediator_ fileDestinationPicker:nil
              didSelectDestination:FileDestination::kDrive];
  [mediator_ startDownloadWithIdentity:identity];
  EXPECT_EQ(download_task_.get(), download_helper->download_task_started_);
  UploadTask* upload_task =
      drive_helper->GetUploadTaskForDownload(download_task_.get());
  ASSERT_NE(nullptr, upload_task);
  EXPECT_EQ(identity, upload_task->GetIdentity());
}
