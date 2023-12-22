// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_mediator.h"

#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";

}  // namespace

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
    web_state_ = std::make_unique<web::FakeWebState>();
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
