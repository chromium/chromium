// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_coordinator.h"

#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_navigation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_discover_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_magic_stack_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

namespace {

// The height of the menu's initial detent, which roughly represents a header
// and 3 cells.
const CGFloat kInitialDetentHeight = 350;

}  // namespace

@interface HomeCustomizationCoordinator () <
    HomeCustomizationNavigationDelegate,
    UISheetPresentationControllerDelegate>

// The main page of the customization menu.
@property(nonatomic, strong)
    HomeCustomizationMainViewController* mainViewController;

// The Magic Stack page of the customization menu.
@property(nonatomic, strong)
    HomeCustomizationMagicStackViewController* magicStackViewController;

// The Discover page of the customization menu.
@property(nonatomic, strong)
    HomeCustomizationDiscoverViewController* discoverViewController;

// The mediator for the Home customization menu.
@property(nonatomic, strong) HomeCustomizationMediator* mediator;

// The navigation controller to allow for navigations between the submenus.
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation HomeCustomizationCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  _mainViewController = [[HomeCustomizationMainViewController alloc] init];
  _magicStackViewController =
      [[HomeCustomizationMagicStackViewController alloc] init];
  _discoverViewController =
      [[HomeCustomizationDiscoverViewController alloc] init];
  _mediator = [[HomeCustomizationMediator alloc]
      initWithPrefService:ChromeBrowserState::FromBrowserState(
                              self.browser->GetBrowserState())
                              ->GetPrefs()];

  _mainViewController.mutator = _mediator;
  _discoverViewController.mutator = _mediator;
  _magicStackViewController.mutator = _mediator;

  _mediator.mainPageConsumer = _mainViewController;
  _mediator.discoverPageConsumer = _discoverViewController;
  _mediator.magicStackPageConsumer = _magicStackViewController;
  _mediator.navigationDelegate = self;

  [super start];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:NO completion:nil];

  _mainViewController = nil;
  _mediator = nil;

  [super stop];
}

#pragma mark - Public

- (void)presentCustomizationMenuAtPage:(CustomizationMenuPage)page
                              animated:(BOOL)animated {
  [self.mediator configureMainPageData];

  // Configure the navigation controller.
  self.navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.mainViewController];
  self.navigationController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.navigationController.presentationController.delegate = self;

  // Configure the presentation controller with a custom initial detent.
  UISheetPresentationController* presentationController =
      self.navigationController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;

  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return kInitialDetentHeight;
  };
  UISheetPresentationControllerDetent* initialDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kBottomSheetDetentIdentifier
                            resolver:detentResolver];
  presentationController.detents = @[
    initialDetent,
    UISheetPresentationControllerDetent.largeDetent,
  ];
  presentationController.selectedDetentIdentifier =
      kBottomSheetDetentIdentifier;

  // Present the navigation controller.
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  // Handle navigation if the initial page isn't the main one.
  if (page != CustomizationMenuPage::kMain) {
    [self navigateToPage:page animated:animated];
  }
}

#pragma mark - HomeCustomizationNavigationDelegate

- (void)navigateToPage:(CustomizationMenuPage)page animated:(BOOL)animated {
  switch (page) {
    case CustomizationMenuPage::kMain:
      [self.navigationController pushViewController:self.mainViewController
                                           animated:animated];
      break;
    case CustomizationMenuPage::kMagicStack:
      [self.navigationController
          pushViewController:self.magicStackViewController
                    animated:animated];
      [self.mediator configureMagicStackPageData];
      break;
    case CustomizationMenuPage::kDiscover:
      [self expandMenu];
      [self.navigationController pushViewController:self.discoverViewController
                                           animated:animated];
      [self.mediator configureDiscoverPageData];
      break;
    case CustomizationMenuPage::kUnknown:
      NOTREACHED_NORETURN();
  }
}

- (void)navigateToURL:(GURL)URL {
  UrlLoadingBrowserAgent::FromBrowser(self.browser)
      ->Load(UrlLoadParams::InCurrentTab(URL));
  [self.mainViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate handleCustomizationMenuDismissed:self];
}

#pragma mark - Private

// Expands the menu to a large detent.
- (void)expandMenu {
  UISheetPresentationController* presentationController =
      self.navigationController.sheetPresentationController;
  [presentationController animateChanges:^{
    presentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  }];
}

@end
