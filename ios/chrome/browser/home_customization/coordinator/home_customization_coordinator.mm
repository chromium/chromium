// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_coordinator.h"

#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

@interface HomeCustomizationCoordinator () <
    UISheetPresentationControllerDelegate>

// The main page of the customization menu.
@property(nonatomic, strong)
    HomeCustomizationMainViewController* mainViewController;

// The mediator for the Home customization menu.
@property(nonatomic, strong) HomeCustomizationMediator* mediator;

// The navigation controller to allow for navigations between the submenus.
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation HomeCustomizationCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  _mainViewController = [[HomeCustomizationMainViewController alloc] init];
  _mediator = [[HomeCustomizationMediator alloc]
      initWithPrefService:ChromeBrowserState::FromBrowserState(
                              self.browser->GetBrowserState())
                              ->GetPrefs()];
  _mainViewController.mutator = _mediator;
  _mediator.mainPageConsumer = _mainViewController;

  [super start];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:NO completion:nil];

  _mainViewController = nil;
  _mediator = nil;

  [super stop];
}

#pragma mark - Public

- (void)presentCustomizationMenuAtPage:(CustomizationMenuPage)page {
  [self.mediator configureMainPageData];

  // Configure the navigation controller.
  self.navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.mainViewController];
  self.navigationController.modalPresentationStyle =
      UIModalPresentationPageSheet;
  self.navigationController.presentationController.delegate = self;

  // Configure the presentation controller with a custom initial detent.
  UISheetPresentationController* presentationController =
      self.navigationController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;

  // TODO(crbug.com/350990359): Dynamically calculate height.
  CGFloat bottomSheetHeight = 200;
  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return bottomSheetHeight;
  };
  UISheetPresentationControllerDetent* initialDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kBottomSheetDetentIdentifier
                            resolver:detentResolver];
  presentationController.detents = @[
    initialDetent,
    UISheetPresentationControllerDetent.mediumDetent,
  ];
  presentationController.selectedDetentIdentifier =
      kBottomSheetDetentIdentifier;

  // Present the navigation controller.
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  // Handle navigation if the initial page isn't the main one.
  if (page != CustomizationMenuPage::kMain) {
    [self navigateToPage:page];
  }
}

#pragma mark - Private

// Navigates to a given page within the customization menu.
- (void)navigateToPage:(CustomizationMenuPage)page {
  switch (page) {
    case CustomizationMenuPage::kMain:
      [self.navigationController pushViewController:self.mainViewController
                                           animated:YES];
      break;
    case CustomizationMenuPage::kMagicStack:
      // TODO(crbug.com/350990359): Push Magic Stack view controller.
      [self.navigationController pushViewController:self.mainViewController
                                           animated:YES];
      break;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate handleCustomizationMenuDismissed:self];
}

@end
