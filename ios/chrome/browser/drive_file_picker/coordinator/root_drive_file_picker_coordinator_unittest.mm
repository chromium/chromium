// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/root_drive_file_picker_coordinator.h"

#import "base/test/task_environment.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/fake_drive_file_picker_handler.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
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
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    handler_ = [[FakeDriveFilePickerHandler alloc] init];
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:handler_
                             forProtocol:@protocol(DriveFilePickerCommands)];
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    coordinator_ = [[RootDriveFilePickerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                          webState:fake_web_state_.get()];
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

  // Signs in a fake identity.
  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    auth_service->SignIn(fake_identity,
                         signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  void TearDown() final {
    [coordinator_ stop];
    coordinator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  UIViewController* base_view_controller_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  FakeDriveFilePickerHandler* handler_;
  RootDriveFilePickerCoordinator* coordinator_;
};

TEST_F(RootDriveFilePickerCoordinatorTest, StartCoordinator) {
  SignIn();
  [coordinator_ start];
}
