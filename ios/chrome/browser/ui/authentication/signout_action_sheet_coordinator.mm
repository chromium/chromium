// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"

#import "base/check.h"
#import "base/format_macros.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Enum to describe all 5 cases for a user being signed-in. This enum is used
// internaly by SignoutActionSheetCoordinator().
typedef NS_ENUM(NSUInteger, SignedInUserState) {
  // Sign-in with a managed account and sync is turned on.
  SignedInUserStateWithManagedAccountAndSyncing,
  // Sign-in with a managed account and sync is turned off.
  SignedInUserStateWithManagedAccountAndNotSyncing,
  // Sign-in with a regular account and sync is turned on.
  SignedInUserStateWithNonManagedAccountAndSyncing,
  // Sign-in with a regular account and sync is turned off.
  SignedInUserStateWithNoneManagedAccountAndNotSyncing,
  // Sign-in with a requirement to give more contextual information when the
  // forced sign-in policy is enabled.
  SignedInUserStateWithForcedSigninInfoRequired
};

@interface SignoutActionSheetCoordinator () {
  // Rectangle for the popover alert.
  CGRect _rect;
  // View for the popovert alert.
  __weak UIView* _view;
  // Source of the sign-out action. For histogram if the sign-out occurs.
  signin_metrics::ProfileSignout _signout_source_metric;
}

// Service for managing identity authentication.
@property(nonatomic, assign, readonly)
    AuthenticationService* authenticationService;
// Action sheet to display sign-out actions.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;
// YES if the user has confirmed that they want to signout.
@property(nonatomic, assign) BOOL confirmSignOut;
// YES if sign-in is forced by enterprise policy.
@property(nonatomic, assign, readonly) BOOL isForceSigninEnabled;

@end

@implementation SignoutActionSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      rect:(CGRect)rect
                                      view:(UIView*)view
                                withSource:(signin_metrics::ProfileSignout)
                                               signout_source_metric {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _rect = rect;
    _view = view;
    _signout_source_metric = signout_source_metric;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.completion);
  DCHECK(self.authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  [self startActionSheetCoordinatorForSignout];
}

- (void)stop {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

#pragma mark - ActionSheetCoordinator properties

- (NSString*)title {
  return self.actionSheetCoordinator.title;
}

- (NSString*)message {
  return self.actionSheetCoordinator.message;
}

#pragma mark - Browser-based properties

- (AuthenticationService*)authenticationService {
  return AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

// Returns the user's sign-in and syncing state.
- (SignedInUserState)signedInUserState {
  DCHECK(self.browser);
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  BOOL syncEnabled = syncSetupService->IsInitialSyncFeatureSetupComplete();

  // Need a first step to show logout contextual information about the forced
  // sign-in policy. Only return this state when sync is enabled because it is
  // already shown for sync disabled.
  if (self.isForceSigninEnabled && syncEnabled && !self.confirmSignOut) {
    return SignedInUserStateWithForcedSigninInfoRequired;
  }

  if (self.authenticationService->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSignin)) {
    return syncEnabled ? SignedInUserStateWithManagedAccountAndSyncing
                       : SignedInUserStateWithManagedAccountAndNotSyncing;
  }
  return syncEnabled ? SignedInUserStateWithNonManagedAccountAndSyncing
                     : SignedInUserStateWithNoneManagedAccountAndNotSyncing;
}

// Returns the title associated to the given user sign-in state or nil if no
// title is defined for the state.
- (NSString*)actionSheetCoordinatorTitle {
  DCHECK(self.browser);
  NSString* title = nil;
  switch (self.signedInUserState) {
    case SignedInUserStateWithManagedAccountAndSyncing: {
      std::u16string hostedDomain = HostedDomainForPrimaryAccount(self.browser);
      title = l10n_util::GetNSStringF(
          IDS_IOS_SIGNOUT_DIALOG_TITLE_WITH_SYNCING_MANAGED_ACCOUNT,
          hostedDomain);
      break;
    }
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      title = l10n_util::GetNSString(
          IDS_IOS_SIGNOUT_DIALOG_TITLE_WITH_SYNCING_ACCOUNT);
      break;
    }
    case SignedInUserStateWithForcedSigninInfoRequired:
    case SignedInUserStateWithManagedAccountAndNotSyncing:
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing: {
      if (self.isForceSigninEnabled) {
        title = l10n_util::GetNSString(
            IDS_IOS_ENTERPRISE_FORCED_SIGNIN_SIGNOUT_DIALOG_TITLE);
      } else if (self.showUnavailableFeatureDialogHeader) {
        title = l10n_util::GetNSString(
            IDS_IOS_SIGNOUT_DIALOG_TITLE_WITHOUT_SYNCING_ACCOUNT);
      }
      break;
    }
  }

  return title;
}

// Returns the message associated to the given user sign-in state or nil if no
// message is defined for the state.
- (NSString*)actionSheetCoordinatorMessage {
  switch (self.signedInUserState) {
    case SignedInUserStateWithForcedSigninInfoRequired:
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing:
    case SignedInUserStateWithManagedAccountAndNotSyncing: {
      if (self.isForceSigninEnabled) {
        return l10n_util::GetNSString(IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE);
      }
      return nil;
    }
    case SignedInUserStateWithManagedAccountAndSyncing:
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      return nil;
    }
  }
}

#pragma mark - Properties

- (BOOL)isForceSigninEnabled {
  return self.authenticationService->GetServiceStatus() ==
         AuthenticationService::ServiceStatus::SigninForcedByPolicy;
}

#pragma mark - Private

// Starts the signout action sheet for the current user state.
- (void)startActionSheetCoordinatorForSignout {
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:self.actionSheetCoordinatorTitle
                         message:self.actionSheetCoordinatorMessage
                            rect:_rect
                            view:_view];

  __weak SignoutActionSheetCoordinator* weakSelf = self;
  switch (self.signedInUserState) {
    case SignedInUserStateWithForcedSigninInfoRequired: {
      NSString* const signOutButtonTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:signOutButtonTitle
                    action:^{
                      weakSelf.confirmSignOut = YES;
                      // Stop the current action sheet coordinator and start a
                      // new one for the next step.
                      [weakSelf.actionSheetCoordinator stop];
                      weakSelf.actionSheetCoordinator = nil;
                      [weakSelf startActionSheetCoordinatorForSignout];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
    case SignedInUserStateWithManagedAccountAndSyncing: {
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:YES];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
    case SignedInUserStateWithManagedAccountAndNotSyncing: {
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:NO];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
    case SignedInUserStateWithNonManagedAccountAndSyncing: {
      NSString* const clearFromDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON);
      NSString* const keepOnDeviceTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:clearFromDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:YES];
                    }
                     style:UIAlertActionStyleDestructive];
      [self.actionSheetCoordinator
          addItemWithTitle:keepOnDeviceTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:NO];
                    }
                     style:UIAlertActionStyleDefault];
      break;
    }
    case SignedInUserStateWithNoneManagedAccountAndNotSyncing: {
      NSString* const signOutButtonTitle =
          l10n_util::GetNSString(IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON);
      [self.actionSheetCoordinator
          addItemWithTitle:signOutButtonTitle
                    action:^{
                      [weakSelf handleSignOutWithForceClearData:NO];
                    }
                     style:UIAlertActionStyleDestructive];
      break;
    }
  }
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  if (weakSelf)
                    weakSelf.completion(NO);
                }
                 style:UIAlertActionStyleCancel];
  [self.actionSheetCoordinator start];
}

// Signs the user out of the primary account and clears the data from their
// device if specified to do so.
- (void)handleSignOutWithForceClearData:(BOOL)forceClearData {
  if (!self.browser)
    return;

  if (!self.authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    self.completion(YES);
    return;
  }

  [self.delegate signoutActionSheetCoordinatorPreventUserInteraction:self];

  __weak SignoutActionSheetCoordinator* weakSelf = self;
  self.authenticationService->SignOut(_signout_source_metric, forceClearData, ^{
    __strong SignoutActionSheetCoordinator* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    [strongSelf.delegate
        signoutActionSheetCoordinatorAllowUserInteraction:strongSelf];
    strongSelf.completion(YES);
  });
  // Get UMA metrics on the usage of different options for signout available
  // for users with non-managed accounts.
  if (!self.authenticationService->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSignin)) {
    UMA_HISTOGRAM_BOOLEAN("Signin.UserRequestedWipeDataOnSignout",
                          forceClearData);
  }
  if (forceClearData) {
    base::RecordAction(base::UserMetricsAction("Signin_SignoutClearData"));
  } else {
    base::RecordAction(base::UserMetricsAction("Signin_Signout"));
  }
}

@end
