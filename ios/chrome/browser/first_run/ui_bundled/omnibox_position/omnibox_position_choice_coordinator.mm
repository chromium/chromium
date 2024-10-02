// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_coordinator.h"

#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_mediator.h"
#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_view_controller.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"

@interface OmniboxPositionChoiceCoordinator () <
    PromoStyleViewControllerDelegate>
@end

@implementation OmniboxPositionChoiceCoordinator {
  /// View controller of the omnibox position choice screen.
  OmniboxPositionChoiceViewController* _viewController;
  /// Mediator of the omnibox position choice screen.
  OmniboxPositionChoiceMediator* _mediator;
  /// Time when the choice screen was shown.
  base::ElapsedTimer _startTime;
}

- (void)start {
  [super start];

  _mediator = [[OmniboxPositionChoiceMediator alloc] init];
  if (!self.browser->GetProfile()->IsOffTheRecord()) {
    _mediator.deviceSwitcherResultDispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForProfile(self.browser->GetProfile());
  }

  _viewController = [[OmniboxPositionChoiceViewController alloc] init];
  _viewController.modalInPresentation = YES;
  _viewController.delegate = self;
  _viewController.mutator = _mediator;

  _mediator.consumer = _viewController;

  _viewController.modalInPresentation = NO;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kScreenDisplayed);
  _startTime = base::ElapsedTimer();

  if (IsSegmentationTipsManagerEnabled()) {
    [self recordScreenDisplayed];
  }
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];

  _viewController = nil;
  _mediator = nil;
  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [_mediator saveSelectedPosition];
  [self dismissScreen];
}

- (void)didTapSecondaryActionButton {
  [_mediator discardSelectedPosition];
  [self dismissScreen];
}

- (void)didDismissViewController {
  [_mediator discardSelectedPosition];
  [self dismissScreen];
}

#pragma mark - Private

// Records that the omnibox position choice screen was displayed.
// This notifies the Tips Manager to potentially trigger related tips.
- (void)recordScreenDisplayed {
  CHECK(IsSegmentationTipsManagerEnabled());

  if (!self.browser) {
    return;
  }

  TipsManagerIOS* tipsManager =
      TipsManagerIOSFactory::GetForProfile(self.browser->GetProfile());

  tipsManager->NotifySignal(segmentation_platform::tips_manager::signals::
                                kAddressBarPositionChoiceScreenDisplayed);
}

/// Dismisses the omnibox position choice view controller.
- (void)dismissScreen {
  id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [handler dismissOmniboxPositionChoice];

  RecordTimeOpen(_startTime.Elapsed());
}

@end
