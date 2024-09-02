// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import "base/test/task_environment.h"
#import "components/image_fetcher/core/cached_image_fetcher.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

// Test fixture for testing DriveFilePickerMediator class.
class DriveFilePickerMediatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    browser_state_ = TestChromeBrowserState::Builder().Build();
    drive_service_ =
        drive::DriveServiceFactory::GetForBrowserState(browser_state_.get());
    _accountManagerService =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            browser_state_.get());
    image_fetcher_ = std::make_unique<image_fetcher::ImageDataFetcher>(
        browser_state_.get()->GetSharedURLLoaderFactory());
    web_state_ = std::make_unique<web::FakeWebState>();
    mediator_ = [[DriveFilePickerMediator alloc]
             initWithWebState:web_state_.get()
                     identity:[FakeSystemIdentity fakeIdentity1]
                driveFolderID:nil
                 driveService:drive_service_
        accountManagerService:_accountManagerService
                 imageFetcher:std::move(image_fetcher_)];
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
  base::test::TaskEnvironment task_environment_;
  DriveFilePickerMediator* mediator_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<ChooseFileTabHelper> choose_file_tab_helper_;
  raw_ptr<drive::DriveService> drive_service_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  std::unique_ptr<image_fetcher::ImageDataFetcher> image_fetcher_;
};

// Tests that disconnecting the mediator stops the file selection.
TEST_F(DriveFilePickerMediatorTest, StopsChoosingFiles) {
  EXPECT_TRUE(choose_file_tab_helper_->IsChoosingFiles());
  // Disconnect the mediator.
  [mediator_ disconnect];
  mediator_ = nil;
  EXPECT_FALSE(choose_file_tab_helper_->IsChoosingFiles());
}
