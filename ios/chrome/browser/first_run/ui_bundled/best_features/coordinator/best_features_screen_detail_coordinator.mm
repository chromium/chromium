// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_detail_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/feature_highlight_screenshot_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/metrics_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/instruction_view/instructions_half_sheet_coordinator.h"

@interface BestFeaturesScreenDetailCoordinator () <
    ConfirmationAlertActionHandler>

@end

@implementation BestFeaturesScreenDetailCoordinator {
  // The BestFeaturesItem the coordinator should handle.
  BestFeaturesItem* _bestFeaturesItem;
  // The View Controller for this coordinator.
  FeatureHighlightScreenshotViewController* _viewController;
  // The half sheet coordinator presented when the primary action button is
  // pressed.
  InstructionsHalfSheetCoordinator* _halfSheetCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationViewController:
                    (UINavigationController*)navigationController
                                             browser:(Browser*)browser
                                    bestFeaturesItem:
                                        (BestFeaturesItem*)bestFeaturesItem {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _bestFeaturesItem = bestFeaturesItem;
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  _viewController = [[FeatureHighlightScreenshotViewController alloc]
      initWithFeatureHighlightItem:_bestFeaturesItem
                     actionHandler:self];
  _baseNavigationController.delegate = _viewController;
  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  _viewController = nil;
  _halfSheetCoordinator = nil;
  self.delegate = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  base::UmaHistogramEnumeration(
      BestFeaturesActionHistogramForItemType(_bestFeaturesItem.type),
      BestFeaturesDetailScreenActionType::kShowMeHow);
  _halfSheetCoordinator = [[InstructionsHalfSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                instructionsList:_bestFeaturesItem.instructionSteps];
  [_halfSheetCoordinator start];
}

- (void)confirmationAlertSecondaryAction {
  base::UmaHistogramEnumeration(
      BestFeaturesActionHistogramForItemType(_bestFeaturesItem.type),
      BestFeaturesDetailScreenActionType::kContinueInFRESequence);
  base::UmaHistogramEnumeration(
      first_run::kFirstRunStageHistogram,
      first_run::kBestFeaturesExperienceCompletionThroughDetailScreen);
  [self.delegate screenWillFinishPresenting];
}

@end
