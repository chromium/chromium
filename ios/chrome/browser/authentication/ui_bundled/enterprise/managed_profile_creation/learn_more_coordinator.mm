// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/learn_more_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/learn_more_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface LearnMoreCoordinator () <
    LearnMoreTableViewControllerPresentationDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation LearnMoreCoordinator {
  NSString* _userEmail;
  NSString* _hostedDomain;
  LearnMoreTableViewController* _viewController;
  UINavigationController* _navigationController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 userEmail:(NSString*)userEmail
                              hostedDomain:(NSString*)hostedDomain {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _userEmail = userEmail;
    _hostedDomain = hostedDomain;
  }
  return self;
}

- (void)start {
  [super start];
  // Creates the view controller.
  _viewController =
      [[LearnMoreTableViewController alloc] initWithUserEmail:_userEmail
                                                 hostedDomain:_hostedDomain];
  _viewController.presentationDelegate = self;
  // Creates the navigation controller and present.
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;
  UISheetPresentationController* presentationController =
      _navigationController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController dismissViewControllerAnimated:YES completion:nil];
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate removeLearnMoreCoordinator:self];
}

#pragma mark - LearnMoreTableViewControllerPresentationDelegate

- (void)dismissLearnMoreTableViewController:
    (LearnMoreTableViewController*)viewController {
  CHECK_EQ(_viewController, viewController);
  [self.delegate removeLearnMoreCoordinator:self];
}

@end
