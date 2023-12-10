// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/why_am_i_seeing_this/why_am_i_seeing_this_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/search_engine_choice/why_am_i_seeing_this/why_am_i_seeing_this_view_controller.h"

@interface WhyAmISeeingThisCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    WhyAmISeeingThisDelegate>
@end

@implementation WhyAmISeeingThisCoordinator {
  // The view controller displaying the information.
  WhyAmISeeingThisViewController* _viewController;
}

- (void)start {
  [super start];
  _viewController = [[WhyAmISeeingThisViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;
  // Creates the navigation controller and presents.
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  navigationController.presentationController.delegate = self;
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
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

#pragma mark - WhyAmISeeingThisDelegate

- (void)learnMoreDone:(WhyAmISeeingThisViewController*)viewController {
  CHECK_EQ(_viewController, viewController);
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
