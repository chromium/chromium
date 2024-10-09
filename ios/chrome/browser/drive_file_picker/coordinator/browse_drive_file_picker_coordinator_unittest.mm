// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
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
#import "testing/platform_test.h"

// Test fixture for testing `BrowseDriveFilePickerCoordinator` class.
class BrowseDriveFilePickerCoordinatorTest : public PlatformTest {
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
    images_pending_ = [NSMutableSet set];
    image_cache_ = [[NSCache alloc] init];
    coordinator_ = [[BrowseDriveFilePickerCoordinator alloc]
        initWithBaseNavigationViewController:navigation_controller_
                                     browser:browser_.get()
                                    webState:fake_web_state_->GetWeakPtr()
                                       title:@"Collection title"
                               imagesPending:images_pending_
                                  imageCache:image_cache_
                              collectionType:DriveFilePickerCollectionType::
                                                 kFolder
                            folderIdentifier:nil
                                      filter:DriveFilePickerFilter::
                                                 kShowAllFiles
                         ignoreAcceptedTypes:NO
                             sortingCriteria:DriveItemsSortingType::kName
                            sortingDirection:DriveItemsSortingOrder::kAscending
                                    identity:[FakeSystemIdentity
                                                 fakeIdentity1]];
    StartChoosingFiles();
  }

  // Starts file selection in the WebState.
  void StartChoosingFiles() {
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::GetOrCreateForWebState(fake_web_state_.get());
    auto controller = std::make_unique<FakeChooseFileController>(
        ChooseFileEvent(false, std::vector<std::string>{},
                        std::vector<std::string>{}, fake_web_state_.get()));
    tab_helper->StartChoosingFiles(std::move(controller));
  }

  void TearDown() final {
    [coordinator_ stop];
    coordinator_ = nil;
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  UIViewController* root_view_controller_;
  UINavigationController* navigation_controller_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  FakeDriveFilePickerHandler* handler_;
  NSMutableSet<NSString*>* images_pending_;
  NSCache<NSString*, UIImage*>* image_cache_;
  BrowseDriveFilePickerCoordinator* coordinator_;
};

TEST_F(BrowseDriveFilePickerCoordinatorTest, StartCoordinator) {
  [coordinator_ start];
}
