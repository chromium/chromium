// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_coordinator.h"

#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/metrics.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_mediator.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_view_controller.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"

@interface OmniboxPositionChoiceCoordinator () <
    PromoStyleViewControllerDelegate>
@end

@implementation OmniboxPositionChoiceCoordinator {
  /// View controller of the omnibox position choice screen.
  OmniboxPositionChoiceViewController* _viewController;
  /// Mediator of the omnibox position choice screen.
  OmniboxPositionChoiceMediator* _mediator;
  /// Whether the screen is being shown in the FRE.
  BOOL _firstRun;
  /// First run screen delegate.
  __weak id<FirstRunScreenDelegate> _firstRunDelegate;
  /// Time when the choice screen was shown.
  base::ElapsedTimer _startTime;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _firstRun = NO;
  }
  return self;
}

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [self initWithBaseViewController:navigationController browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _firstRun = YES;
    _firstRunDelegate = delegate;
  }
  return self;
}

- (void)start {
  CHECK(IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kAny));
  [super start];

  _mediator =
      [[OmniboxPositionChoiceMediator alloc] initWithFirstRun:_firstRun];
  if (!self.browser->GetBrowserState()->IsOffTheRecord()) {
    _mediator.deviceSwitcherResultDispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForBrowserState(self.browser->GetBrowserState());
  }

  _viewController =
      [[OmniboxPositionChoiceViewController alloc] initWithFirstRun:_firstRun];
  _viewController.modalInPresentation = YES;
  _viewController.delegate = self;
  _viewController.mutator = _mediator;

  _mediator.consumer = _viewController;

  if (_firstRun) {
    BOOL animated = self.baseNavigationController.topViewController != nil;
    [self.baseNavigationController setViewControllers:@[ _viewController ]
                                             animated:animated];
  } else {
    _viewController.modalInPresentation = NO;
    [self.baseViewController presentViewController:_viewController
                                          animated:YES
                                        completion:nil];
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  tracker->NotifyEvent(feature_engagement::events::kOmniboxPositionPromoShown);

  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kScreenDisplayed,
                    _firstRun);
  _startTime = base::ElapsedTimer();
}

- (void)stop {
  if (!_firstRun) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    [self.promosUIHandler promoWasDismissed];
  }
  _viewController = nil;
  _mediator = nil;
  _baseNavigationController = nil;
  _firstRunDelegate = nil;
  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [_mediator saveSelectedPosition];
  [self dismissScreen];
}

- (void)didTapSecondaryActionButton {
  if (_firstRun) {
    [_mediator skipSelection];
  } else {
    [_mediator discardSelectedPosition];
  }
  [self dismissScreen];
}

- (void)didDismissViewController {
  if (!_firstRun) {
    [_mediator discardSelectedPosition];
  }
  [self dismissScreen];
}

#pragma mark - Private

/// Dismisses the omnibox position choice view controller.
- (void)dismissScreen {
  if (_firstRun) {
    [_firstRunDelegate screenWillFinishPresenting];
  } else {
    id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
    [handler dismissOmniboxPositionChoice];
  }
  RecordTimeOpen(_startTime.Elapsed(), _firstRun);
}

@end
