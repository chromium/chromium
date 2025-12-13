// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_collection.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_image_fetcher.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_metrics_helper.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/fake_drive_file_picker_handler.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/platform_test.h"

// Test fixture for testing `BrowseDriveFilePickerCoordinator` class.
class BrowseDriveFilePickerCoordinatorTest : public PlatformTest {
 public:
  BrowseDriveFilePickerCoordinatorTest()
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kIOSChooseFromDrive);
    root_view_controller_ = [[UIViewController alloc] init];
    navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:root_view_controller_];
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    handler_ = [[FakeDriveFilePickerHandler alloc] init];
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:handler_
                             forProtocol:@protocol(DriveFilePickerCommands)];
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    ChooseFileTabHelper::CreateForWebState(fake_web_state_.get());
    metrics_helper_ = [[DriveFilePickerMetricsHelper alloc] init];
    image_fetcher_ =
        std::make_unique<DriveFilePickerImageFetcher>(shared_factory_);
    id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
    std::unique_ptr<DriveFilePickerCollection> collection =
        DriveFilePickerCollection::GetRoot(identity)->GetFolder(
            @"Collection title", nil);
    coordinator_ = [[BrowseDriveFilePickerCoordinator alloc]
        initWithBaseNavigationViewController:navigation_controller_
                                     browser:browser_.get()
                                    webState:fake_web_state_->GetWeakPtr()
                                  collection:std::move(collection)
                                imageFetcher:image_fetcher_.get()
                                     options:DriveFilePickerOptions::Default()
                               metricsHelper:metrics_helper_];
    StartChoosingFiles();
  }

  // Starts file selection in the WebState.
  void StartChoosingFiles() {
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::FromWebState(fake_web_state_.get());
    auto controller = std::make_unique<FakeChooseFileController>(
        ChooseFileEvent::Builder()
            .SetAllowMultipleFiles(false)
            .SetHasSelectedFile(false)
            .SetWebState(fake_web_state_.get())
            .Build());
    tab_helper->StartChoosingFiles(std::move(controller));
  }

  void TearDown() final {
    [coordinator_ stop];
    coordinator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  UIViewController* root_view_controller_;
  UINavigationController* navigation_controller_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  FakeDriveFilePickerHandler* handler_;
  DriveFilePickerMetricsHelper* metrics_helper_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<DriveFilePickerImageFetcher> image_fetcher_;
  BrowseDriveFilePickerCoordinator* coordinator_;
};

TEST_F(BrowseDriveFilePickerCoordinatorTest, StartCoordinator) {
  [coordinator_ start];
}
