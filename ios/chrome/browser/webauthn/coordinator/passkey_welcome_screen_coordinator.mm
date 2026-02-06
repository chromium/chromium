// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/passkey_welcome_screen_coordinator.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/webauthn/public/passkey_welcome_screen_util.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_view_controller.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PasskeyWelcomeScreenCoordinator () <
    PasskeyWelcomeScreenViewControllerDelegate>
@end

@implementation PasskeyWelcomeScreenCoordinator {
  // Purpose of the welcome screen.
  webauthn::PasskeyWelcomeScreenPurpose _purpose;

  // Completion block.
  webauthn::PasskeyWelcomeScreenAction _completion;

  // Navigation controller to present the welcome screen.
  UINavigationController* _navigationController;

  // The welcome screen view controller.
  PasskeyWelcomeScreenViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                       purpose:(webauthn::PasskeyWelcomeScreenPurpose)purpose
                    completion:
                        (webauthn::PasskeyWelcomeScreenAction)completion {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _purpose = purpose;
    _completion = completion;
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  std::string userEmail = base::SysNSStringToUTF8(identity.userEmail);
  UIView* passkeyNavigationTitleView =
      password_manager::CreatePasswordManagerTitleView(
          l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER));
  PasskeyWelcomeScreenStrings* strings =
      GetPasskeyWelcomeScreenStrings(_purpose, std::move(userEmail));

  _viewController = [[PasskeyWelcomeScreenViewController alloc]
               initForPurpose:_purpose
      navigationItemTitleView:passkeyNavigationTitleView
                     delegate:self
          primaryButtonAction:_completion
                      strings:strings];
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

#pragma mark - Public

- (void)stopWithCompletion:(ProceduralBlock)completion {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completion];
  _viewController = nil;
  _navigationController = nil;
  _completion = nil;
}

#pragma mark - PasskeyWelcomeScreenViewControllerDelegate

- (void)passkeyWelcomeScreenViewControllerShouldBeDismissed:
    (PasskeyWelcomeScreenViewController*)passkeyWelcomeScreenViewController {
  [self.delegate passkeyWelcomeScreenCoordinatorWantsToBeDismissed:self];
}

@end
