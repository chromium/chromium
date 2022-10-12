// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_mediator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UnifiedConsentCoordinator () <IdentityChooserCoordinatorDelegate,
                                         UnifiedConsentMediatorDelegate,
                                         UnifiedConsentViewControllerDelegate>

// Unified consent mediator.
@property(nonatomic, strong) UnifiedConsentMediator* unifiedConsentMediator;
// Unified consent view controller.
@property(nonatomic, strong)
    UnifiedConsentViewController* unifiedConsentViewController;
// YES if the user tapped on the setting link.
@property(nonatomic, assign) BOOL settingsLinkWasTapped;
// Identity chooser coordinator.
@property(nonatomic, strong)
    IdentityChooserCoordinator* identityChooserCoordinator;
// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authenticationService;

@end

@implementation UnifiedConsentCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                    postRestoreSigninPromo:(BOOL)postRestoreSigninPromo {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _unifiedConsentViewController = [[UnifiedConsentViewController alloc]
        initWithPostRestoreSigninPromo:postRestoreSigninPromo];
    _unifiedConsentViewController.delegate = self;

    _authenticationService = AuthenticationServiceFactory::GetForBrowserState(
        browser->GetBrowserState());
    _unifiedConsentMediator = [[UnifiedConsentMediator alloc]
        initWithUnifiedConsentViewController:_unifiedConsentViewController
                       authenticationService:_authenticationService
                       accountManagerService:
                           ChromeAccountManagerServiceFactory::
                               GetForBrowserState(browser->GetBrowserState())];
    _unifiedConsentMediator.delegate = self;
  }
  return self;
}

- (void)start {
  [self.unifiedConsentMediator start];
}

- (void)stop {
  [self.identityChooserCoordinator stop];
  [self.unifiedConsentMediator disconnect];
  self.unifiedConsentMediator = nil;
}

- (void)scrollToBottom {
  [self.unifiedConsentViewController scrollToBottom];
}

- (void)resetSettingLinkTapped {
  self.settingsLinkWasTapped = NO;
}

#pragma mark - Properties

- (id<SystemIdentity>)selectedIdentity {
  return self.unifiedConsentMediator.selectedIdentity;
}

- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  self.unifiedConsentMediator.selectedIdentity = selectedIdentity;
}

- (UIViewController*)viewController {
  return self.unifiedConsentViewController;
}

- (const std::vector<int>&)consentStringIds {
  return [self.unifiedConsentViewController consentStringIds];
}

- (BOOL)isScrolledToBottom {
  return self.unifiedConsentViewController.isScrolledToBottom;
}

- (BOOL)hasManagedSyncDataType {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  PrefService* prefService = browserState->GetPrefs();
  return HasManagedSyncDataType(prefService);
}

- (BOOL)hasAccountRestrictions {
  return IsRestrictAccountsToPatternsEnabled();
}

#pragma mark - Private

// Opens the identity chooser dialog with an animation from `point`.
- (void)showIdentityChooserDialogWithPoint:(CGPoint)point {
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.unifiedConsentViewController
                         browser:self.browser];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
  self.identityChooserCoordinator.selectedIdentity = self.selectedIdentity;
}

#pragma mark - UnifiedConsentViewMediatorDelegate

- (void)unifiedConsentViewMediatorDelegateNeedPrimaryButtonUpdate:
    (UnifiedConsentMediator*)mediator {
  DCHECK_EQ(self.unifiedConsentMediator, mediator);
  [self.delegate unifiedConsentCoordinatorNeedPrimaryButtonUpdate:self];
}

#pragma mark - UnifiedConsentViewControllerDelegate

- (BOOL)unifiedConsentCoordinatorHasManagedSyncDataType {
  return self.hasManagedSyncDataType;
}

- (BOOL)unifiedConsentCoordinatorHasAccountRestrictions {
  return self.hasAccountRestrictions;
}

- (void)unifiedConsentViewControllerViewDidAppear:
    (UnifiedConsentViewController*)controller {
  if (!self.autoOpenIdentityPicker)
    return;

  // The identity picker should not open a second time, to avoid opening it
  // again after the merge/clear data dialog disappears.
  self.autoOpenIdentityPicker = NO;
  CGFloat midX = CGRectGetMidX(self.unifiedConsentViewController.view.bounds);
  CGFloat midY = CGRectGetMidY(self.unifiedConsentViewController.view.bounds);
  CGPoint point = CGPointMake(midX, midY);
  [self showIdentityChooserDialogWithPoint:point];
}

- (void)unifiedConsentViewControllerDidTapSettingsLink:
    (UnifiedConsentViewController*)controller {
  if (self.isUIDisabled) {
    return;
  }
  DCHECK_EQ(self.unifiedConsentViewController, controller);
  DCHECK(!self.settingsLinkWasTapped);
  self.settingsLinkWasTapped = YES;
  [self.delegate unifiedConsentCoordinatorDidTapSettingsLink:self];
}

- (void)unifiedConsentViewControllerDidTapLearnMoreLink:
    (UnifiedConsentViewController*)controller {
  [self.delegate unifiedConsentCoordinatorDidTapLearnMoreLink:self];
}

- (void)unifiedConsentViewControllerDidTapIdentityButtonControl:
            (UnifiedConsentViewController*)controller
                                                        atPoint:(CGPoint)point {
  if (self.isUIDisabled) {
    return;
  }
  DCHECK_EQ(self.unifiedConsentViewController, controller);
  [self showIdentityChooserDialogWithPoint:point];
}

- (void)unifiedConsentViewControllerDidReachBottom:
    (UnifiedConsentViewController*)controller {
  DCHECK_EQ(self.unifiedConsentViewController, controller);
  [self.delegate unifiedConsentCoordinatorDidReachBottom:self];
}

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator.delegate = nil;
  self.identityChooserCoordinator = nil;
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  [self.delegate unifiedConsentCoordinatorDidTapOnAddAccount:self];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(id<SystemIdentity>)identity {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  self.selectedIdentity = identity;
}

@end
