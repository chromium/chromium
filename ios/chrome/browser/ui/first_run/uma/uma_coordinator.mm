// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/uma/uma_coordinator.h"

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/first_run/uma/uma_mediator.h"
#import "ios/chrome/browser/ui/first_run/uma/uma_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UMACoordinator () <UIAdaptivePresentationControllerDelegate,
                              UMATableViewControllerPresentationDelegate>

@property(nonatomic, strong) UMATableViewController* viewController;
@property(nonatomic, strong) UMAMediator* mediator;

@end

@implementation UMACoordinator

- (void)start {
  [super start];
  // Creates the mediator and view controller.
  self.mediator = [[UMAMediator alloc] init];
  self.viewController =
      [[UMATableViewController alloc] initWithStyle:ChromeTableViewStyle()];
  self.viewController.modelDelegate = self.mediator;
  self.viewController.presentationDelegate = self;
  // Creates the navigation controller and present.
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  navigationController.presentationController.delegate = self;
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  if (@available(iOS 15, *)) {
    // TODO(crbug.com/1290848): Needs to set the presentation for iPad.
    UISheetPresentationController* presentationController =
        navigationController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
  }
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate UMACoordinatorDidRemove:self];
}

#pragma mark - UMATableViewControllerPresentationDelegate

- (void)UMATableViewControllerDidDismiss:
    (UMATableViewController*)viewController {
  DCHECK_EQ(self.viewController, viewController);
  [self.delegate UMACoordinatorDidRemove:self];
}

@end
