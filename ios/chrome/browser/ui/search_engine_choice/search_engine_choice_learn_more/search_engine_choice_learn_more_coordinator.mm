// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_learn_more/search_engine_choice_learn_more_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_learn_more/search_engine_choice_learn_more_view_controller.h"
#import "ui/base/device_form_factor.h"

@interface SearchEngineChoiceLearnMoreCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    SearchEngineChoiceLearnMoreDelegate>
@end

@implementation SearchEngineChoiceLearnMoreCoordinator {
  // The view controller displaying the information.
  SearchEngineChoiceLearnMoreViewController* _viewController;
}

- (void)start {
  [super start];
  _viewController = [[SearchEngineChoiceLearnMoreViewController alloc] init];
  _viewController.delegate = self;
  // Creates the navigation controller and presents.
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  // Need to set `modalPresentationStyle` otherwise, UIKit ignores the value.
  if (self.forcePresentationFormSheet) {
    navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  } else {
    ui::DeviceFormFactor deviceFormFactor = ui::GetDeviceFormFactor();
    if (deviceFormFactor == ui::DEVICE_FORM_FACTOR_PHONE) {
      navigationController.modalPresentationStyle =
          UIModalPresentationPageSheet;
    } else {
      navigationController.modalPresentationStyle =
          UIModalPresentationFormSheet;
      navigationController.preferredContentSize =
          CGSizeMake(kIPadSearchEngineChoiceScreenPreferredWidth,
                     kIPadSearchEngineChoiceScreenPreferredHeight);
    }
  }
  navigationController.presentationController.delegate = self;
  UISheetPresentationController* presentationController =
      navigationController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:NO completion:nil];
  _viewController.delegate = nil;
  _viewController = nil;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self viewControlerDidDismiss];
}

#pragma mark - SearchEngineChoiceLearnMoreDelegate

- (void)learnMoreDone:
    (SearchEngineChoiceLearnMoreViewController*)viewController {
  CHECK_EQ(_viewController, viewController, base::NotFatalUntil::M127);
  __weak __typeof(self) weakSelf = self;
  [_viewController dismissViewControllerAnimated:YES
                                      completion:^() {
                                        [weakSelf viewControlerDidDismiss];
                                      }];
}

#pragma mark - Private

// Called when the view controller has been dismissed.
- (void)viewControlerDidDismiss {
  _viewController.delegate = nil;
  _viewController = nil;
  [self.delegate learnMoreDidDismiss];
}

@end
