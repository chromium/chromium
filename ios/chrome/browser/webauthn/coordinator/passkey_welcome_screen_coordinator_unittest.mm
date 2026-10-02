// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/passkey_welcome_screen_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using webauthn::PasskeyWelcomeScreenPurpose;

namespace {

// Returns whether `view` or any of its descendants is a label displaying
// `text`.
bool HasLabelWithText(UIView* view, NSString* text) {
  UILabel* label = base::apple::ObjCCast<UILabel>(view);
  if ([label.text isEqualToString:text]) {
    return true;
  }
  for (UIView* subview in view.subviews) {
    if (HasLabelWithText(subview, text)) {
      return true;
    }
  }
  return false;
}

}  // namespace

// Test fixture for `PasskeyWelcomeScreenCoordinator`. An identity is signed in
// to the original profile so that the welcome screen has an email to display.
class PasskeyWelcomeScreenCoordinatorTest : public PlatformTest {
 protected:
  PasskeyWelcomeScreenCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_);
    authentication_service->SignIn(identity_,
                                   signin_metrics::AccessPoint::kStartPage);

    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  ~PasskeyWelcomeScreenCoordinatorTest() override { [coordinator_ stop]; }

  // Creates and starts a coordinator presenting the enrollment welcome screen,
  // which displays the user's email, in a browser for `profile`.
  void StartEnrollmentCoordinator(ProfileIOS* profile) {
    browser_ = std::make_unique<TestBrowser>(profile);
    coordinator_ = [[PasskeyWelcomeScreenCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                           purpose:PasskeyWelcomeScreenPurpose::kEnroll
                        completion:^(UINavigationController*){
                        }];
    [coordinator_ start];
  }

  // Returns the presented welcome screen, or nil if it is not presented.
  PasskeyWelcomeScreenViewController* PresentedWelcomeScreen() {
    UINavigationController* navigation_controller =
        base::apple::ObjCCast<UINavigationController>(
            base_view_controller_.presentedViewController);
    return base::apple::ObjCCast<PasskeyWelcomeScreenViewController>(
        navigation_controller.topViewController);
  }

  // Returns the enrollment footer expected for the signed-in identity.
  NSString* ExpectedEnrollmentFooter() {
    return l10n_util::GetNSStringF(
        IDS_IOS_PASSKEY_ENROLLMENT_FOOTER_MESSAGE,
        base::SysNSStringToUTF16(identity_.userEmail));
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  FakeSystemIdentity* identity_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  PasskeyWelcomeScreenCoordinator* coordinator_;
};

// Tests that the welcome screen displays the primary account email when shown
// in a regular browser.
TEST_F(PasskeyWelcomeScreenCoordinatorTest, ShowsPrimaryAccountEmail) {
  StartEnrollmentCoordinator(profile_);

  PasskeyWelcomeScreenViewController* welcome_screen = PresentedWelcomeScreen();
  ASSERT_TRUE(welcome_screen);
  EXPECT_TRUE(
      HasLabelWithText(welcome_screen.view, ExpectedEnrollmentFooter()));
}

// Tests that the welcome screen displays the original profile's primary account
// email when shown in an incognito browser, as happens for passkey requests
// made from incognito tabs. No `AuthenticationService` exists for the
// off-the-record profile.
TEST_F(PasskeyWelcomeScreenCoordinatorTest,
       ShowsPrimaryAccountEmailInIncognito) {
  StartEnrollmentCoordinator(profile_->GetOffTheRecordProfile());

  PasskeyWelcomeScreenViewController* welcome_screen = PresentedWelcomeScreen();
  ASSERT_TRUE(welcome_screen);
  EXPECT_TRUE(
      HasLabelWithText(welcome_screen.view, ExpectedEnrollmentFooter()));
}
