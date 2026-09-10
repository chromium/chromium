// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/root_drive_file_picker_coordinator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/fake_drive_file_picker_handler.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing `RootDriveFilePickerCoordinator` class.
class RootDriveFilePickerCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    base_view_controller_ = [[UIViewController alloc] init];
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_);
    handler_ = [[FakeDriveFilePickerHandler alloc] init];
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:handler_
                             forProtocol:@protocol(DriveFilePickerCommands)];
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    ChooseFileTabHelper::CreateForWebState(fake_web_state_.get());
    coordinator_ = [[RootDriveFilePickerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                          webState:fake_web_state_.get()
                     forComposebox:NO];
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

  // Signs in a fake identity.
  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_);
    auth_service->SignIn(fake_identity,
                         signin_metrics::AccessPoint::kStartPage);
  }

  void TearDown() final {
    [coordinator_ stop];
    coordinator_ = nil;
    handler_ = nil;
    base_view_controller_ = nil;
    fake_web_state_.reset();
    browser_.reset();
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  UIViewController* base_view_controller_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  FakeDriveFilePickerHandler* handler_;
  RootDriveFilePickerCoordinator* coordinator_;
};

TEST_F(RootDriveFilePickerCoordinatorTest, StartCoordinator) {
  SignIn();
  [coordinator_ start];
}

// Tests the metrics for identity change.
TEST_F(RootDriveFilePickerCoordinatorTest, IdentityChange) {
  base::HistogramTester histogram_tester;
  SignIn();
  [coordinator_ start];
  [coordinator_ setSelectedIdentity:[FakeSystemIdentity fakeIdentity2]];
  // The expected bucket is `kChangeAccount` which corresponds to enum 0
  // bucket of `IOS.FilePicker.Drive.AccountSelection` histogram.
  histogram_tester.ExpectBucketCount("IOS.FilePicker.Drive.AccountSelection", 0,
                                     1);
  histogram_tester.ExpectTotalCount("IOS.FilePicker.Drive.AccountSelection", 1);
}

@interface FakeDriveFilePickerResponseCommands
    : NSObject <DriveFilePickerResponseCommands>

@property(nonatomic, assign) BOOL didPickItemsCalled;
@property(nonatomic, assign) BOOL didCancelCalled;
@property(nonatomic, copy) NSArray<ComposeboxPickerDriveResult*>* pickedItems;

@end

@implementation FakeDriveFilePickerResponseCommands

- (void)driveFilePickerDidPickItems:
    (NSArray<ComposeboxPickerDriveResult*>*)items {
  self.didPickItemsCalled = YES;
  self.pickedItems = items;
}

- (void)driveFilePickerDidCancel {
  self.didCancelCalled = YES;
}

@end

// Tests that stopping the coordinator for composebox before picking items calls
// driveFilePickerDidCancel on the response handler.
TEST_F(RootDriveFilePickerCoordinatorTest, StopForComposeboxCallsDidCancel) {
  SignIn();
  RootDriveFilePickerCoordinator* composebox_coordinator =
      [[RootDriveFilePickerCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                            webState:fake_web_state_.get()
                       forComposebox:YES];
  FakeDriveFilePickerResponseCommands* response_handler =
      [[FakeDriveFilePickerResponseCommands alloc] init];
  composebox_coordinator.responseHandler = response_handler;

  [composebox_coordinator start];
  EXPECT_FALSE(response_handler.didCancelCalled);

  [composebox_coordinator stop];
  EXPECT_TRUE(response_handler.didCancelCalled);
}
