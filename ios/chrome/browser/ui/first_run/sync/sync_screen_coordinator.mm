// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/consent_auditor_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_view_controller.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SyncScreenCoordinator () <SyncScreenViewControllerDelegate>

// Sync screen view controller.
@property(nonatomic, strong) SyncScreenViewController* viewController;
// Sync screen mediator.
@property(nonatomic, strong) SyncScreenMediator* mediator;

@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;

@end

@implementation SyncScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);

  if (!authenticationService->GetAuthenticatedIdentity()) {
    // Don't show sync screen if no logged-in user account.
    return [self.delegate willFinishPresenting];
  }

  self.viewController = [[SyncScreenViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.unifiedButtonStyle = self.isMinorMode;

  // Setup mediator.
  self.mediator = [[SyncScreenMediator alloc]
      initWithAuthenticationService:authenticationService
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        browserState)
                     consentAuditor:ConsentAuditorFactory::GetForBrowserState(
                                        browserState)
              unifiedConsentService:UnifiedConsentServiceFactory::
                                        GetForBrowserState(browserState)
                   syncSetupService:SyncSetupServiceFactory::GetForBrowserState(
                                        browserState)];

  base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kSyncScreenStart);

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
  if (@available(iOS 13, *)) {
    self.viewController.modalInPresentation = YES;
  }
}

- (void)stop {
  self.delegate = nil;
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - SyncScreenViewControllerDelegate

- (void)didTapPrimaryActionButton {
  base::UmaHistogramEnumeration("FirstRun.Stage",
                                first_run::kSyncScreenCompletionWithSync);
  [self.mediator startSync];
  [self.delegate willFinishPresenting];
}

- (void)didTapSecondaryActionButton {
  base::UmaHistogramEnumeration("FirstRun.Stage",
                                first_run::kSyncScreenCompletionWithoutSync);
  [self.delegate willFinishPresenting];
}

- (void)showSyncSettings {
  [self.delegate skipAllAndShowSyncSettings];
}

#pragma mark - Private

// Returns whether a minor mode UI needs to be shown.
- (BOOL)isMinorMode {
  // TODO(crbug.com/1205783): check if the user account is a minor/family link
  // account
  return NO;
}

@end
