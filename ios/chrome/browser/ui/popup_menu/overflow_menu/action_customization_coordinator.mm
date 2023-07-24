// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/action_customization_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/overflow_menu_customization_commands.h"
#import "ios/chrome/browser/ui/popup_menu//overflow_menu/overflow_menu_orderer.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ActionCustomizationCoordinator () <
    UISheetPresentationControllerDelegate>

@end

@implementation ActionCustomizationCoordinator {
  // UI configuration object to configure this view.
  OverflowMenuUIConfiguration* _UIConfiguration;

  // View controller for this feature.
  UIViewController* _viewController;
}

- (void)start {
  _UIConfiguration = [[OverflowMenuUIConfiguration alloc]
      initWithPresentingViewControllerHorizontalSizeClass:
          self.baseViewController.traitCollection.horizontalSizeClass
                presentingViewControllerVerticalSizeClass:
                    self.baseViewController.traitCollection.verticalSizeClass
                                     highlightDestination:-1];

  _viewController = [OverflowMenuViewProvider
      makeActionCustomizationViewControllerWithActionModel:
          self.menuOrderer.actionCustomizationModel
                                          destinationModel:
                                              self.menuOrderer
                                                  .destinationCustomizationModel
                                           uiConfiguration:_UIConfiguration];

  UISheetPresentationController* sheetPresentationController =
      _viewController.sheetPresentationController;
  if (sheetPresentationController) {
    sheetPresentationController.delegate = self;
    sheetPresentationController.prefersGrabberVisible = YES;
    sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
    sheetPresentationController
        .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

    NSArray<UISheetPresentationControllerDetent*>* regularDetents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];

    NSArray<UISheetPresentationControllerDetent*>* largeTextDetents =
        @[ [UISheetPresentationControllerDetent largeDetent] ];

    BOOL hasLargeText = UIContentSizeCategoryIsAccessibilityCategory(
        _viewController.traitCollection.preferredContentSizeCategory);
    sheetPresentationController.detents =
        hasLargeText ? largeTextDetents : regularDetents;
  }

  [self.baseViewController.presentedViewController
      presentViewController:_viewController
                   animated:YES
                 completion:^{
                 }];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.menuOrderer commitActionsUpdate];
  [self.menuOrderer commitDestinationsUpdate];

  id<OverflowMenuCustomizationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), OverflowMenuCustomizationCommands);
  [handler hideActionCustomization];
}

@end
