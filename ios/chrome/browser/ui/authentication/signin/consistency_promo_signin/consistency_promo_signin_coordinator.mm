// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_coordinator.h"

#import "components/signin/public/base/account_consistency_method.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencyPromoSigninCoordinator () <
    IdentityChooserCoordinatorDelegate>

// Identity chooser coordinator.
@property(nonatomic, strong)
    IdentityChooserCoordinator* identityChooserCoordinator;

@end

@implementation ConsistencyPromoSigninCoordinator

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  return [super initWithBaseViewController:viewController browser:browser];
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator.delegate = nil;
  self.identityChooserCoordinator = nil;

  __weak ConsistencyPromoSigninCoordinator* weakSelf = self;
  ProceduralBlock interruptCompletion = ^{
    [weakSelf
        runCompletionCallbackWithSigninResult:SigninCoordinatorResultInterrupted
                                     identity:nil
                   showAdvancedSettingsSignin:NO];
    if (completion) {
      completion();
    }
  };

  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss: {
      interruptCompletion();
      break;
    }
    case SigninCoordinatorInterruptActionDismissWithoutAnimation: {
      [self.baseViewController
          dismissViewControllerAnimated:NO
                             completion:interruptCompletion];
      break;
    }
    case SigninCoordinatorInterruptActionDismissWithAnimation: {
      [self.baseViewController
          dismissViewControllerAnimated:YES
                             completion:interruptCompletion];
      break;
    }
  }
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  DCHECK(!authenticationService->IsAuthenticated());
  DCHECK(base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency));
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  self.identityChooserCoordinator.delegate = self;
  [self.identityChooserCoordinator start];
}

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator.delegate = nil;
  self.identityChooserCoordinator = nil;

  [self runCompletionCallbackWithSigninResult:SigninCoordinatorResultSuccess
                                     identity:nil
                   showAdvancedSettingsSignin:NO];
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  // TODO(crbug.com/1125631): Add action for tapping on "Add account" button.
  NOTIMPLEMENTED();
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(ChromeIdentity*)identity {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  // TODO(crbug.com/1125631): Add sign-in action for tapping on an identity.
  NOTIMPLEMENTED();
}

@end
