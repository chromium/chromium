// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_mediator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_view_controller_presentation_delegate.h"

@interface PrivacyGuideURLUsageCoordinator () <
    PrivacyGuideViewControllerPresentationDelegate,
    PromoStyleViewControllerDelegate>
@end

@implementation PrivacyGuideURLUsageCoordinator {
  PrivacyGuideURLUsageViewController* _viewController;
  PrivacyGuideURLUsageMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];

  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[PrivacyGuideURLUsageViewController alloc] init];
  _viewController.delegate = self;
  _viewController.presentationDelegate = self;

  _mediator = [[PrivacyGuideURLUsageMediator alloc]
      initWithUserPrefService:self.browser->GetProfile()->GetPrefs()];
  _mediator.consumer = _viewController;
  _viewController.modelDelegate = _mediator;

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;

  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - PrivacyGuideViewControllerPresentationDelegate

- (void)privacyGuideViewControllerDidRemove:(UIViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate privacyGuideCoordinatorDidRemove:self];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  id<PrivacyGuideCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PrivacyGuideCommands);
  [handler showNextStep];
}

@end
