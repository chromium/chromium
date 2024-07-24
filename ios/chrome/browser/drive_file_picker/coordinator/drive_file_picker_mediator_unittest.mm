// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

// Test fixture for testing DriveFilePickerMediator class.
class DriveFilePickerMediatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    web_state_ = std::make_unique<web::FakeWebState>();
    mediator_ =
        [[DriveFilePickerMediator alloc] initWithWebState:web_state_.get()];
    // Start file selection in `web_state_`.
    choose_file_tab_helper_ =
        ChooseFileTabHelper::GetOrCreateForWebState(web_state_.get());
    auto controller = std::make_unique<FakeChooseFileController>(
        ChooseFileEvent(false, std::vector<std::string>{},
                        std::vector<std::string>{}, web_state_.get()));
    choose_file_tab_helper_->StartChoosingFiles(std::move(controller));
  }

  void TearDown() final {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  DriveFilePickerMediator* mediator_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<ChooseFileTabHelper> choose_file_tab_helper_;
};

// Tests that disconnecting the mediator stops the file selection.
TEST_F(DriveFilePickerMediatorTest, StopsChoosingFiles) {
  EXPECT_TRUE(choose_file_tab_helper_->IsChoosingFiles());
  // Disconnect the mediator.
  [mediator_ disconnect];
  mediator_ = nil;
  EXPECT_FALSE(choose_file_tab_helper_->IsChoosingFiles());
}
