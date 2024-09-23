// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_history_sync_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_history_sync_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_view_controller_presentation_delegate.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface PrivacyGuideHistorySyncCoordinator () <
    PrivacyGuideViewControllerPresentationDelegate,
    PromoStyleViewControllerDelegate>
@end

@implementation PrivacyGuideHistorySyncCoordinator {
  PrivacyGuideHistorySyncViewController* _viewController;
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
  _viewController = [[PrivacyGuideHistorySyncViewController alloc] init];
  _viewController.delegate = self;
  _viewController.presentationDelegate = self;
  // TODO(crbug.com/41493454): Implement History Sync mediator.

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
}

#pragma mark - PrivacyGuideViewControllerPresentationDelegate

- (void)privacyGuideViewControllerDidRemove:(UIViewController*)controller {
  CHECK_EQ(controller, _viewController);
  [self.delegate privacyGuideCoordinatorDidRemove:self];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  id<PrivacyGuideCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PrivacyGuideCommands);
  [handler showNextStep];
}

@end
