// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_presenting.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninInteractionController ()<
    ChromeIdentityInteractionManagerDelegate,
    ChromeSigninViewControllerDelegate> {
  Browser* browser_;
  signin_metrics::AccessPoint accessPoint_;
  signin_metrics::PromoAction promoAction_;
  BOOL isCancelling_;
  BOOL isDismissing_;
  BOOL interactionManagerDismissalIgnored_;
  SigninInteractionControllerCompletionCallback completionCallback_;
  ChromeSigninViewController* signinViewController_;
  ChromeIdentityInteractionManager* identityInteractionManager_;
  ChromeIdentity* signInIdentity_;
  BOOL identityAdded_;
}

// The dispatcher for this class.
@property(nonatomic, weak, readonly) id<ApplicationCommands> dispatcher;

// The object responsible for presenting the UI.
@property(nonatomic, weak, readonly) id<SigninInteractionPresenting> presenter;

@end

@implementation SigninInteractionController

@synthesize dispatcher = dispatcher_;
@synthesize presenter = presenter_;

- (instancetype)initWithBrowser:(Browser*)browser
           presentationProvider:(id<SigninInteractionPresenting>)presenter
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
                    promoAction:(signin_metrics::PromoAction)promoAction
                     dispatcher:(id<ApplicationCommands>)dispatcher {
  self = [super init];
  if (self) {
    DCHECK(browser);
    DCHECK(presenter);
    browser_ = browser;
    presenter_ = presenter;
    accessPoint_ = accessPoint;
    promoAction_ = promoAction;
    dispatcher_ = dispatcher;
  }
  return self;
}

- (void)cancel {
// Cancelling and dismissing the |identityInteractionManager_| may call the
// |completionCallback_| which could lead to |self| being released before the
// end of this method. |self| is retained here to prevent this from happening.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
  // Retain this object through the rest of this method in case this object's
  // owner frees this object during the execution of the completion block.
  SigninInteractionController* strongSelf = self;
#pragma clang diagnostic pop
  isCancelling_ = YES;
  [self.presenter dismissError];
  [identityInteractionManager_ cancelAndDismissAnimated:NO];
  [signinViewController_ cancel];
  isCancelling_ = NO;
}

- (void)cancelAndDismiss {
  isDismissing_ = YES;
  [self cancel];
  isDismissing_ = NO;
}

- (void)signInWithIdentity:(ChromeIdentity*)identity
                completion:
                    (SigninInteractionControllerCompletionCallback)completion {
  signin_metrics::LogSigninAccessPointStarted(accessPoint_, promoAction_);
  completionCallback_ = [completion copy];
  [self showSigninViewControllerWithIdentity:identity identityAdded:NO];
}

- (void)reAuthenticateWithCompletion:
    (SigninInteractionControllerCompletionCallback)completion {
  signin_metrics::LogSigninAccessPointStarted(accessPoint_, promoAction_);
  completionCallback_ = [completion copy];
  ios::ChromeBrowserState* browserState = browser_->GetBrowserState();
  CoreAccountInfo accountInfo =
      IdentityManagerFactory::GetForBrowserState(browserState)
          ->GetPrimaryAccountInfo();
  std::string emailToReauthenticate = accountInfo.email;
  std::string idToReauthenticate = accountInfo.gaia;
  if (emailToReauthenticate.empty() || idToReauthenticate.empty()) {
    // This corresponds to a re-authenticate request after the user was signed
    // out. This corresponds to the case where the identity was removed as a
    // result of the permissions being removed on the server or the identity
    // being removed from another app.
    //
    // Simply use the the last signed-in user email in this case and go though
    // the entire sign-in flow as sync needs to be configured.
    emailToReauthenticate =
        browserState->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
    idToReauthenticate = browserState->GetPrefs()->GetString(
        prefs::kGoogleServicesLastAccountId);
  }
  DCHECK(!emailToReauthenticate.empty());
  DCHECK(!idToReauthenticate.empty());
  identityInteractionManager_ =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->CreateChromeIdentityInteractionManager(browserState, self);
  __weak SigninInteractionController* weakSelf = self;
  [identityInteractionManager_
      reauthenticateUserWithID:base::SysUTF8ToNSString(idToReauthenticate)
                         email:base::SysUTF8ToNSString(emailToReauthenticate)
                    completion:^(ChromeIdentity* identity, NSError* error) {
                      [weakSelf handleIdentityAdded:identity
                                              error:error
                                       shouldSignIn:YES];
                    }];
}

- (void)addAccountWithCompletion:
    (SigninInteractionControllerCompletionCallback)completion {
  completionCallback_ = [completion copy];
  identityInteractionManager_ = ios::GetChromeBrowserProvider()
                                    ->GetChromeIdentityService()
                                    ->CreateChromeIdentityInteractionManager(
                                        browser_->GetBrowserState(), self);
  __weak SigninInteractionController* weakSelf = self;
  [identityInteractionManager_
      addAccountWithCompletion:^(ChromeIdentity* identity, NSError* error) {
        [weakSelf handleIdentityAdded:identity error:error shouldSignIn:NO];
      }];
}

#pragma mark - ChromeIdentityInteractionManager operations

- (void)handleIdentityAdded:(ChromeIdentity*)identity
                      error:(NSError*)error
               shouldSignIn:(BOOL)shouldSignIn {
  if (!identityInteractionManager_)
    return;

  if (error) {
    // Filter out cancel and errors handled internally by ChromeIdentity.
    if (!ShouldHandleSigninError(error)) {
      [self runCompletionCallbackWithSigninResult:SigninResultCanceled];
      return;
    }

    __weak SigninInteractionController* weakSelf = self;
    ProceduralBlock dismissAction = ^{
      [weakSelf runCompletionCallbackWithSigninResult:SigninResultCanceled];
    };
    [self.presenter presentError:error dismissAction:dismissAction];
    return;
  }
  if (shouldSignIn) {
    [self showSigninViewControllerWithIdentity:identity identityAdded:YES];
  } else {
    [self runCompletionCallbackWithSigninResult:SigninResultSuccess];
  }
}

- (void)dismissPresentedViewControllersAnimated:(BOOL)animated
                                     completion:(ProceduralBlock)completion {
  if (self.presenter.isPresenting) {
    [self.presenter dismissAllViewControllersAnimated:animated
                                           completion:completion];
  } else if (completion) {
    completion();
  }
  interactionManagerDismissalIgnored_ = NO;
}

#pragma mark - ChromeIdentityInteractionManagerDelegate

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
     presentViewController:(UIViewController*)viewController
                  animated:(BOOL)animated
                completion:(ProceduralBlock)completion {
  [self.presenter presentViewController:viewController
                               animated:animated
                             completion:completion];
}

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
    dismissViewControllerAnimated:(BOOL)animated
                       completion:(ProceduralBlock)completion {
  // Avoid awkward double transitions by not dismissing
  // identityInteractionManager_| if the signin view controller will be
  // displayed on top of it. |identityInteractionManager_| will be dismissed
  // when the signin view controller will be dismissed.
  if ([interactionManager isCanceling]) {
    [self dismissPresentedViewControllersAnimated:animated
                                       completion:completion];
  } else {
    interactionManagerDismissalIgnored_ = YES;
    if (completion) {
      completion();
    }
  }
}

#pragma mark - ChromeSigninViewController operations

- (void)showSigninViewControllerWithIdentity:(ChromeIdentity*)signInIdentity
                               identityAdded:(BOOL)identityAdded {
  signinViewController_ =
      [[ChromeSigninViewController alloc] initWithBrowser:browser_
                                              accessPoint:accessPoint_
                                              promoAction:promoAction_
                                           signInIdentity:signInIdentity
                                               dispatcher:self.dispatcher];
  signinViewController_.delegate = self;
  signInIdentity_ = signInIdentity;
  identityAdded_ = identityAdded;

  if (identityInteractionManager_) {
    // If |identityInteractionManager_| is currently displayed,
    // |signinViewController_| is presented on top of it (instead of on top of
    // |presentingViewController_|), to avoid an awkward transition (dismissing
    // |identityInteractionManager_|, followed by presenting
    // |signinViewController_|).
    [self.presenter presentTopViewController:signinViewController_
                                    animated:YES
                                  completion:nil];
  } else {
    [self.presenter presentViewController:signinViewController_
                                 animated:YES
                               completion:nil];
  }
}

- (void)dismissSigninViewControllerWithSigninResult:(SigninResult)signinResult {
  DCHECK(signinViewController_);
  if ((isCancelling_ && !isDismissing_) || !self.presenter.isPresenting) {
    [self runCompletionCallbackWithSigninResult:signinResult];
    return;
  }
  ProceduralBlock completion = ^{
    [self runCompletionCallbackWithSigninResult:signinResult];
  };
  [self dismissPresentedViewControllersAnimated:YES completion:completion];
}

#pragma mark - ChromeSigninViewControllerDelegate

- (void)willStartSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
}

- (void)willStartAddAccount:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
}

- (void)didSkipSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
  [self dismissSigninViewControllerWithSigninResult:SigninResultCanceled];
}

- (void)didSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
}

- (void)didUndoSignIn:(ChromeSigninViewController*)controller
             identity:(ChromeIdentity*)identity {
  DCHECK_EQ(controller, signinViewController_);
  if ([signInIdentity_ isEqual:identity]) {
    signInIdentity_ = nil;
    if (identityAdded_) {
      // This is best effort. If the operation fails, the account will be left
      // on the device. The user will not be warned either as this call is
      // asynchronous (but undo is not), the application might be in an unknown
      // state when the forget identity operation finishes.
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->ForgetIdentity(identity, nil);
    }
    [self dismissSigninViewControllerWithSigninResult:SigninResultCanceled];
  }
}

- (void)didFailSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(controller, signinViewController_);
  [self dismissSigninViewControllerWithSigninResult:SigninResultCanceled];
}

- (void)didAcceptSignIn:(ChromeSigninViewController*)controller
    showAccountsSettings:(BOOL)showAccountsSettings {
  DCHECK_EQ(controller, signinViewController_);
  SigninResult signinResult = showAccountsSettings
                                  ? SigninResultSignedInnAndOpennSettings
                                  : SigninResultSuccess;
  [self dismissSigninViewControllerWithSigninResult:signinResult];
}

#pragma mark - Utility methods

- (void)runCompletionCallbackWithSigninResult:(SigninResult)signinResult {
  // In order to avoid awkward double transitions, |identityInteractionManager_|
  // is not dismissed when requested (except when canceling). However, in case
  // of errors, |identityInteractionManager_| needs to be directly dismissed,
  // which is done here.
  if (interactionManagerDismissalIgnored_) {
    [self dismissPresentedViewControllersAnimated:YES completion:nil];
  }

  // Cleaning up and calling the |completionCallback_| should be done last.
  identityInteractionManager_ = nil;
  signinViewController_ = nil;
  // Ensure self is not destroyed in the callbacks.
  SigninInteractionController* strongSelf = self;
  if (completionCallback_) {
    completionCallback_(signinResult);
    completionCallback_ = nil;
  }
  strongSelf = nil;
}

@end
