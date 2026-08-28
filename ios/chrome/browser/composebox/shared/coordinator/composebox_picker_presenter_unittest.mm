// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"

#import <UIKit/UIKit.h>

#import "base/test/scoped_feature_list.h"
#import "components/contextual_search/input_state_model.h"
#import "components/contextual_search/pref_names.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
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
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#pragma mark - FakePresenterDriveFilePickerHandler

@interface FakePresenterDriveFilePickerHandler
    : NSObject <DriveFilePickerCommands>

@property(nonatomic, assign) BOOL drivePickerShown;

@end

@implementation FakePresenterDriveFilePickerHandler

- (void)showDriveFilePicker {
}

- (void)hideDriveFilePicker {
}

- (void)setDriveFilePickerSelectedIdentity:
    (id<SystemIdentity>)selectedIdentity {
}

- (void)showDriveFilePickerWithComposeboxDelegate:
            (id<ComposeboxPickerPresenterDelegate>)composeboxDelegate
                               baseViewController:
                                   (UIViewController*)baseViewController {
  self.drivePickerShown = YES;
}

@end

#pragma mark - ComposeboxPickerPresenterTest

// Test fixture for ComposeboxPickerPresenter.
class ComposeboxPickerPresenterTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    base_view_controller_ = [[UIViewController alloc] init];

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    handler_ = [[FakePresenterDriveFilePickerHandler alloc] init];
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:handler_
                             forProtocol:@protocol(DriveFilePickerCommands)];

    presenter_ = [[ComposeboxPickerPresenter alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
  }

  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);

    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    auth_service->SignIn(fake_identity,
                         signin_metrics::AccessPoint::kStartPage);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  UIViewController* base_view_controller_ = nil;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakePresenterDriveFilePickerHandler* handler_ = nil;
  ComposeboxPickerPresenter* presenter_ = nil;
};

// Tests that when the disclaimer feature is disabled and user is signed in, the
// Drive picker is presented directly.
TEST_F(ComposeboxPickerPresenterTest,
       TestPresentDriveFilePicker_DisclaimerDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  SignIn();

  [presenter_ presentDriveFilePicker];

  EXPECT_TRUE(handler_.drivePickerShown);
}

// Tests that when the user has already consented, the Drive picker is
// presented directly even if the disclaimer feature is enabled.
TEST_F(ComposeboxPickerPresenterTest, TestPresentDriveFilePicker_PreConsented) {
  scoped_feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kConsent));

  SignIn();

  [presenter_ presentDriveFilePicker];

  EXPECT_TRUE(handler_.drivePickerShown);
}

// Tests that when kForceDriveDisclaimerAccepted is enabled, the Drive picker is
// presented directly without needing consent.
TEST_F(ComposeboxPickerPresenterTest,
       TestPresentDriveFilePicker_ForceAccepted) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{omnibox::
                                kComposeboxDriveContextMenuOptionDisclaimer,
                            omnibox::kForceDriveDisclaimerAccepted},
      /*disabled_features=*/{});

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kNotConsent));

  SignIn();

  [presenter_ presentDriveFilePicker];

  EXPECT_TRUE(handler_.drivePickerShown);
}

// Tests that attempting to present the Drive file picker when no identity is
// signed in causes a CHECK failure.
TEST_F(ComposeboxPickerPresenterTest, TestPresentDriveFilePicker_NoIdentity) {
  scoped_feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kNotConsent));

  EXPECT_DEATH_IF_SUPPORTED([presenter_ presentDriveFilePicker], "");
}

// Tests that when consent is needed and the upstream provider stubs out
// ConsentKit (returns false), the Drive picker is not shown.
TEST_F(ComposeboxPickerPresenterTest,
       TestPresentDriveFilePicker_NeedsConsent_UpstreamStub) {
  scoped_feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kNotConsent));

  SignIn();

  [presenter_ presentDriveFilePicker];

  EXPECT_FALSE(handler_.drivePickerShown);
}
