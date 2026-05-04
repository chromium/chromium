// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/best_features/coordinator/best_features_screen_detail_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/first_run/best_features/ui/best_features_carousel_view_controller.h"
#import "ios/chrome/browser/first_run/best_features/ui/feature_highlight_screenshot_view_controller.h"
#import "ios/chrome/browser/first_run/best_features/ui/metrics_util.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/instructions_bottom_sheet/ui/instructions_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface BestFeaturesScreenDetailCoordinator () <
    ConfirmationAlertActionHandler,
    PromoStyleViewControllerDelegate>

@end

@implementation BestFeaturesScreenDetailCoordinator {
  // The BestFeaturesItem objects the coordinator should handle.
  NSArray<BestFeaturesItem*>* _bestFeaturesItems;
  // The single item focused on for metrics (based on entry or tap).
  BestFeaturesItem* _bestFeaturesItem;
  // The starting index for the carousel.
  int _startIndex;
  // The View Controller for this coordinator.
  UIViewController* _viewController;
  // The half sheet coordinator presented when the primary action button is
  // pressed.
  InstructionsBottomSheetCoordinator* _halfSheetCoordinator;
  // The source from which this coordinator was started.
  DetailScreenPresentationSource _source;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationViewController:
        (UINavigationController*)navigationController
                                 browser:(Browser*)browser
                        bestFeaturesItem:(BestFeaturesItem*)bestFeaturesItem
                                  source:
                                      (DetailScreenPresentationSource)source {
  return [self initWithBaseNavigationViewController:navigationController
                                            browser:browser
                                  bestFeaturesItems:@[ bestFeaturesItem ]
                                         startIndex:0
                                             source:source];
}

- (instancetype)
    initWithBaseNavigationViewController:
        (UINavigationController*)navigationController
                                 browser:(Browser*)browser
                       bestFeaturesItems:
                           (NSArray<BestFeaturesItem*>*)bestFeaturesItems
                              startIndex:(int)startIndex
                                  source:
                                      (DetailScreenPresentationSource)source {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _bestFeaturesItems = bestFeaturesItems;
    _startIndex = startIndex;
    _bestFeaturesItem = bestFeaturesItems[startIndex];
    _baseNavigationController = navigationController;
    _source = source;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  if (_source == DetailScreenPresentationSource::kBestOfAppFRE) {
    BestFeaturesCarouselViewController* carouselViewController =
        [[BestFeaturesCarouselViewController alloc]
            initWithBestFeaturesItems:_bestFeaturesItems
                           startIndex:_startIndex];
    carouselViewController.delegate = self;
    _viewController = carouselViewController;
  } else {
    FeatureHighlightScreenshotViewController* screenshotViewController =
        [[FeatureHighlightScreenshotViewController alloc]
            initWithFeatureHighlightItem:_bestFeaturesItem];
    screenshotViewController.actionHandler = self;
    _viewController = screenshotViewController;

    _baseNavigationController.navigationBarHidden = NO;
    UIBarButtonItem* backButton = [[UIBarButtonItem alloc]
        initWithImage:DefaultSymbolWithPointSize(kChevronBackwardSymbol,
                                                 kSymbolActionPointSize)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(dismiss)];
    _viewController.navigationItem.leftBarButtonItem = backButton;
  }
  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [_halfSheetCoordinator stop];
  _halfSheetCoordinator = nil;

  if (_baseNavigationController.topViewController == _viewController) {
    [_baseNavigationController popViewControllerAnimated:NO];
  }

  _viewController = nil;
  self.delegate = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

// Handles taps on the primary action of
// the`FeatureHighlightScreenshotViewController` that is presented when the
// DetailScreenPresentationSource is kBestFeaturesFRE or kWelcomeBack.
- (void)confirmationAlertPrimaryAction {
  base::UmaHistogramEnumeration(
      BestFeaturesActionHistogramForItemType(_bestFeaturesItem.type),
      BestFeaturesDetailScreenActionType::kShowMeHow);
  _halfSheetCoordinator = [[InstructionsBottomSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                           title:_bestFeaturesItem.title
                           steps:_bestFeaturesItem.instructionSteps];
  [_halfSheetCoordinator start];
}

// Handles taps on the secondary action of
// the`FeatureHighlightScreenshotViewController` that is presented when the
// DetailScreenPresentationSource is kBestFeaturesFRE or kWelcomeBack.
- (void)confirmationAlertSecondaryAction {
  [self continueFRESequence];
}

#pragma mark - PromoStyleViewControllerDelegate

// Handles taps on the primary action of the
// `FeatureHighlightAnimatedViewController` that is presented when the
// DetailScreenPresentationSource is kBestOfAppFRE.
- (void)didTapPrimaryActionButton {
  // Update the _bestFeaturesItem before metrics logging so the correct
  // interaction is logged.
  BestFeaturesCarouselViewController* carouselViewController =
      base::apple::ObjCCastStrict<BestFeaturesCarouselViewController>(
          _viewController);
  int index = carouselViewController.currentIndex;
  _bestFeaturesItem = _bestFeaturesItems[index];

  [self continueFRESequence];
}

#pragma mark - Private

- (void)dismiss {
  [_baseNavigationController popViewControllerAnimated:NO];
}

- (void)continueFRESequence {
  base::UmaHistogramEnumeration(
      BestFeaturesActionHistogramForItemType(_bestFeaturesItem.type),
      BestFeaturesDetailScreenActionType::kContinueInFRESequence);
  base::UmaHistogramEnumeration(
      first_run::kFirstRunStageHistogram,
      first_run::kBestFeaturesExperienceCompletionThroughDetailScreen);
  [self.delegate screenWillFinishPresenting];
}

@end
