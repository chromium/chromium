// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_mediator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UnifiedConsentCoordinator ()<IdentityChooserCoordinatorDelegate,
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
// YES if the user choose an identity (or accept the default identity selected).
@property(nonatomic, assign) BOOL identitySelectedByUser;

@end

@implementation UnifiedConsentCoordinator

@synthesize delegate = _delegate;
@synthesize unifiedConsentMediator = _unifiedConsentMediator;
@synthesize unifiedConsentViewController = _unifiedConsentViewController;
@synthesize settingsLinkWasTapped = _settingsLinkWasTapped;
@synthesize interactable = _interactable;
@synthesize identityChooserCoordinator = _identityChooserCoordinator;
@synthesize identitySelectedByUser = _identitySelectedByUser;

- (instancetype)init {
  self = [super init];
  if (self) {
    _unifiedConsentViewController = [[UnifiedConsentViewController alloc] init];
    _unifiedConsentViewController.delegate = self;
    _unifiedConsentMediator = [[UnifiedConsentMediator alloc]
        initWithUnifiedConsentViewController:_unifiedConsentViewController];
  }
  return self;
}

- (void)start {
  self.unifiedConsentViewController.interactable = self.interactable;
  [self.unifiedConsentMediator start];
}

- (ChromeIdentity*)selectedIdentity {
  return self.unifiedConsentMediator.selectedIdentity;
}

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  self.identitySelectedByUser = YES;
  self.unifiedConsentMediator.selectedIdentity = selectedIdentity;
}

- (UIViewController*)viewController {
  return self.unifiedConsentViewController;
}

- (int)openSettingsStringId {
  return self.unifiedConsentViewController.openSettingsStringId;
}

- (const std::vector<int>&)consentStringIds {
  return [self.unifiedConsentViewController consentStringIds];
}

- (void)scrollToBottom {
  [self.unifiedConsentViewController scrollToBottom];
}

- (BOOL)isScrolledToBottom {
  return self.unifiedConsentViewController.isScrolledToBottom;
}

#pragma mark - Private

// Opens the identity chooser dialog with an animation from |point|.
- (void)showIdentityChooserDialogWithPoint:(CGPoint)point {
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.unifiedConsentViewController];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
  self.identityChooserCoordinator.selectedIdentity = self.selectedIdentity;
}

#pragma mark - UnifiedConsentViewControllerDelegate

- (void)unifiedConsentViewControllerViewDidAppear:
    (UnifiedConsentViewController*)controller {
  // Only opens automatically the identity chooser dialog if the user didn't
  // select an identity and the dialog is interactable.
  if (self.identitySelectedByUser || !self.interactable)
    return;
  CGFloat midX = CGRectGetMidX(self.unifiedConsentViewController.view.bounds);
  CGFloat midY = CGRectGetMidY(self.unifiedConsentViewController.view.bounds);
  CGPoint point = CGPointMake(midX, midY);
  [self showIdentityChooserDialogWithPoint:point];
}

- (void)unifiedConsentViewControllerDidTapSettingsLink:
    (UnifiedConsentViewController*)controller {
  DCHECK_EQ(self.unifiedConsentViewController, controller);
  DCHECK(!self.settingsLinkWasTapped);
  self.settingsLinkWasTapped = YES;
  [self.delegate unifiedConsentCoordinatorDidTapSettingsLink:self];
}

- (void)unifiedConsentViewControllerDidTapIdentityPickerView:
            (UnifiedConsentViewController*)controller
                                                     atPoint:(CGPoint)point {
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
  self.identityChooserCoordinator.delegate = nil;
  self.identityChooserCoordinator = nil;
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  [self.delegate unifiedConsentCoordinatorDidTapOnAddAccount:self];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(ChromeIdentity*)identity {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  self.selectedIdentity = identity;
}

@end
