// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/user_policy/user_policy_prompt_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy/user_policy_prompt_coordinator_delegate.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy/user_policy_prompt_mediator.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy/user_policy_prompt_presenter.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy/user_policy_prompt_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"


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
// Returns an empty string if the account isn't managed.
- (NSString*)managedDomain {
  return base::SysUTF16ToNSString(HostedDomainForPrimaryAccount(self.browser));
}

// Returns the AuthenticationService of the browser.
- (AuthenticationService*)authService {
  ProfileIOS* profile = self.browser->GetProfile();
  DCHECK(profile);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
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
  [self.delegate didCompletePresentationAndShowLearnMoreAfterward:NO];
}

- (void)stopPresentingAndShowLearnMoreAfterward {
  [self.delegate didCompletePresentationAndShowLearnMoreAfterward:YES];
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
