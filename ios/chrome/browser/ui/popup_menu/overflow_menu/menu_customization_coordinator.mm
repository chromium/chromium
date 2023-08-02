// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/menu_customization_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/overflow_menu_customization_commands.h"
#import "ios/chrome/browser/ui/popup_menu//overflow_menu/overflow_menu_orderer.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

@interface MenuCustomizationCoordinator () <
    UISheetPresentationControllerDelegate,
    MenuCustomizationEventHandler>

@end

@implementation MenuCustomizationCoordinator {
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
      makeMenuCustomizationViewControllerWithActionModel:
          self.menuOrderer.actionCustomizationModel
                                        destinationModel:
                                            self.menuOrderer
                                                .destinationCustomizationModel
                                         uiConfiguration:_UIConfiguration
                                            eventHandler:self];

  UISheetPresentationController* sheetPresentationController =
      _viewController.sheetPresentationController;
  if (sheetPresentationController) {
    sheetPresentationController.delegate = self;
    sheetPresentationController.prefersGrabberVisible = YES;
    sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
    sheetPresentationController
        .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
    sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent largeDetent] ];
  }

  [self.baseViewController.presentedViewController
      presentViewController:_viewController
                   animated:YES
                 completion:^{
                 }];
}

- (void)stop {
  UIViewController* presentingViewController =
      self.baseViewController.presentedViewController;
  if (presentingViewController.presentedViewController == _viewController) {
    [presentingViewController dismissViewControllerAnimated:YES
                                                 completion:^{
                                                 }];
  }
  _UIConfiguration = nil;
  _viewController = nil;
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

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  // Prevent the user from dismissing the view via gesture. They can only
  // dismiss via the cancel and done buttons.
  return NO;
}

#pragma mark - MenuCustomizationEventHandler

- (void)doneWasTapped {
  [self.menuOrderer commitActionsUpdate];
  [self.menuOrderer commitDestinationsUpdate];

  id<OverflowMenuCustomizationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), OverflowMenuCustomizationCommands);
  [handler hideActionCustomization];
}

- (void)cancelWasTapped {
  [self.menuOrderer cancelActionsUpdate];
  [self.menuOrderer cancelDestinationsUpdate];

  id<OverflowMenuCustomizationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), OverflowMenuCustomizationCommands);
  [handler hideActionCustomization];
}

@end
