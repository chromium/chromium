// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/deeplink_signin/deeplink_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/notreached.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_screen_provider.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"

@interface DeeplinkSigninCoordinator () <FullscreenSigninCoordinatorDelegate>
@end

@implementation DeeplinkSigninCoordinator {
  NSString* _selectedAccountEmail;
  ChangeProfileContinuationProvider _changeProfileContinuationProvider;
  id<SystemIdentity> _selectedIdentity;
  ChromeCoordinator* _childCoordinator;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
}

- (instancetype)
           initWithBaseViewController:(UIViewController*)viewController
                              browser:(Browser*)browser
                 selectedAccountEmail:(NSString*)selectedAccountEmail
    changeProfileContinuationProvider:(const ChangeProfileContinuationProvider&)
                                          changeProfileContinuationProvider {
  CHECK(selectedAccountEmail.length);
  DCHECK_EQ(browser->type(), Browser::Type::kRegular);

  self = [super
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:SigninContextStyle::kDeeplinkSignin
                     accessPoint:signin_metrics::AccessPoint::kDeepLinkDefault];

  if (self) {
    CHECK_EQ(browser->type(), Browser::Type::kRegular);
    CHECK(changeProfileContinuationProvider);
    _selectedAccountEmail = selectedAccountEmail;
    _changeProfileContinuationProvider = changeProfileContinuationProvider;
  }
  return self;
}

- (void)dealloc {
  CHECK(!_changeProfileContinuationProvider);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  _accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(self.profile);
  _selectedIdentity = _accountManagerService->GetIdentityOnDeviceWithEmail(
      _selectedAccountEmail);
  if (!_selectedIdentity) {
    // Provided email doesn't exist on device.
    [self startAddAccountFlow];
  } else {
    [self startSigninFlow];
  }
}

- (void)stopAnimated:(BOOL)animated {
  [self stopChildCoordinator];
  _changeProfileContinuationProvider.Reset();
  _accountManagerService = nullptr;
  [super stopAnimated:animated];
}

#pragma mark - Private

- (void)startAddAccountFlow {
  CHECK(!_childCoordinator);

  SigninCoordinator* addAccountCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.baseViewController
                                          browser:self.browser
                                     contextStyle:self.contextStyle
                                      accessPoint:self.accessPoint
                                   prefilledEmail:_selectedAccountEmail
                             continuationProvider:
                                 _changeProfileContinuationProvider];
  __weak __typeof(self) weakSelf = self;
  addAccountCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> resultIdentity) {
        [weakSelf addAccountDoneWithCoordinator:coordinator
                                         result:result
                                 resultIdentity:resultIdentity];
      };
  _childCoordinator = addAccountCoordinator;
  [_childCoordinator start];
}

- (void)startSigninFlow {
  CHECK(_selectedIdentity);
  [self stopChildCoordinator];

  FullscreenSigninCoordinator* coordinator =
      [[FullscreenSigninCoordinator alloc]
                 initWithBaseViewController:self.baseViewController
                                    browser:self.browser
                             screenProvider:[[SigninScreenProvider alloc] init]
                               contextStyle:self.contextStyle
                                accessPoint:self.accessPoint
          changeProfileContinuationProvider:_changeProfileContinuationProvider];
  coordinator.delegate = self;
  coordinator.identity = _selectedIdentity;
  coordinator.canSwitchAccount = YES;
  _childCoordinator = coordinator;
  [_childCoordinator start];
}

- (void)addAccountDoneWithCoordinator:(SigninCoordinator*)coordinator
                               result:(SigninCoordinatorResult)result
                       resultIdentity:(id<SystemIdentity>)resultIdentity {
  CHECK_EQ(_childCoordinator, coordinator, base::NotFatalUntil::M155);
  [self stopChildCoordinator];

  if (result == SigninCoordinatorResultSuccess &&
      _accountManagerService->IsValidIdentity(resultIdentity.gaiaId)) {
    _selectedIdentity = resultIdentity;
    [self startSigninFlow];
  } else {
    [self runCompletionWithSigninResult:result
                     completionIdentity:resultIdentity];
  }
}

- (void)stopChildCoordinator {
  [_childCoordinator stop];
  _childCoordinator = nil;
}

#pragma mark - FullscreenSigninCoordinatorDelegate

- (void)fullscreenSigninCoordinatorWantsToBeStopped:
            (FullscreenSigninCoordinator*)coordinator
                                             result:(SigninCoordinatorResult)
                                                        result {
  CHECK_EQ(_childCoordinator, coordinator);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  id<SystemIdentity> identity = authService->GetPrimaryIdentity();
  [self runCompletionWithSigninResult:result completionIdentity:identity];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString stringWithFormat:@"<%@: %p, childCoordinator: %p>",
                                    self.class.description, self,
                                    _childCoordinator];
}

@end
