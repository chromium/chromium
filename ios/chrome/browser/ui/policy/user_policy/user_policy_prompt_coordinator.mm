// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/policy/user_policy/user_policy_prompt_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/ui/policy/user_policy/user_policy_prompt_coordinator_delegate.h"
#import "ios/chrome/browser/ui/policy/user_policy/user_policy_prompt_mediator.h"
#import "ios/chrome/browser/ui/policy/user_policy/user_policy_prompt_presenter.h"
#import "ios/chrome/browser/ui/policy/user_policy/user_policy_prompt_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ChromeBrowserState;

namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@interface UserPolicyPromptCoordinator () <UserPolicyPromptPresenter>

// View controller for the User Policy prompt.
@property(nonatomic, strong)
    UserPolicyPromptViewController* presentedViewController;

// Mediator for the User Policy prompt.
@property(nonatomic, strong) UserPolicyPromptMediator* mediator;

// Child coordinator than handles the activity overlay shown on top of the view
// when there is an ongoing activity.
@property(nonatomic, strong)
    ActivityOverlayCoordinator* activityOverlayCoordinator;

@end

@implementation UserPolicyPromptCoordinator

#pragma mark - Internal

// Returns the domain of the administrator hosting the primary account.
// Returns an empty string if the account isn't managed OR isn't syncing.
- (NSString*)managedDomain {
  return base::SysUTF16ToNSString(HostedDomainForPrimaryAccount(self.browser));
}

// Returns the AuthenticationService of the browser.
- (AuthenticationService*)authService {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  DCHECK(browserState);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  return authService;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.mediator =
      [[UserPolicyPromptMediator alloc] initWithPresenter:self
                                              authService:[self authService]];

  self.presentedViewController = [[UserPolicyPromptViewController alloc]
      initWithManagedDomain:[self managedDomain]];
  self.presentedViewController.actionHandler = self.mediator;
  self.presentedViewController.presentationController.delegate = self.mediator;

  if (@available(iOS 15, *)) {
    self.presentedViewController.modalPresentationStyle =
        UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        self.presentedViewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
    self.presentedViewController.modalPresentationStyle =
        UIModalPresentationFormSheet;
  }

  self.presentedViewController.modalInPresentation = YES;

  [self.baseViewController presentViewController:self.presentedViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self hideActivityOverlay];
  if (self.presentedViewController) {
    [self.baseViewController.presentedViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    self.presentedViewController = nil;
  }
}

#pragma mark - UserPolicyPromptPresenter

- (void)stopPresenting {
  [self.delegate didCompletePresentation:self];
}

- (void)showActivityOverlay {
  self.activityOverlayCoordinator = [[ActivityOverlayCoordinator alloc]
      initWithBaseViewController:self.presentedViewController
                         browser:self.browser];
  [self.activityOverlayCoordinator start];
}

- (void)hideActivityOverlay {
  if (self.activityOverlayCoordinator) {
    [self.activityOverlayCoordinator stop];
    self.activityOverlayCoordinator = nil;
  }
}

@end
