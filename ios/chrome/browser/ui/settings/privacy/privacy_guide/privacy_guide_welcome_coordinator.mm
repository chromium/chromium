// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface PrivacyGuideWelcomeCoordinator () <PromoStyleViewControllerDelegate>
@end

@implementation PrivacyGuideWelcomeCoordinator {
  PrivacyGuideWelcomeViewController* _viewController;
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
  _viewController = [[PrivacyGuideWelcomeViewController alloc] init];
  _viewController.delegate = self;

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  id<PrivacyGuideCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PrivacyGuideCommands);
  [handler showNextStep];
}

- (void)didTapSecondaryActionButton {
  id<PrivacyGuideCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PrivacyGuideCommands);
  [handler dismissGuide];
}

@end
