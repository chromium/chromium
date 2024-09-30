// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_coordinator.h"

#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_discover_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_magic_stack_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

namespace {

// The height of the menu's initial detent, which roughly represents a header
// and 3 cells.
const CGFloat kInitialDetentHeight = 350;

// The corner radius of the customization menu sheet.
CGFloat const kSheetCornerRadius = 30;

}  // namespace

@interface HomeCustomizationCoordinator () <
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

// This menu consists of several sheets that can be overlayed on top of each
// other, each representing a submenu.
// This property points to the view controller that is at the base of the stack.
@property(nonatomic, weak) UIViewController* firstPageViewController;
// This property points to the view controller that is at the top of the stack.
@property(nonatomic, weak) UIViewController* currentPageViewController;

@end

@implementation HomeCustomizationCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator = [[HomeCustomizationMediator alloc]
      initWithPrefService:ProfileIOS::FromBrowserState(
                              self.browser->GetProfile())
                              ->GetPrefs()];
  _mediator.navigationDelegate = self;

  _currentPageViewController = self.baseViewController;

  [super start];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:NO completion:nil];

  _mainViewController = nil;
  _magicStackViewController = nil;
  _discoverViewController = nil;
  _mediator = nil;

  [super stop];
}

#pragma mark - Public

- (void)updateMenuData {
  if (self.mainViewController) {
    [self.mediator configureMainPageData];
  }

  if (self.magicStackViewController) {
    [self.mediator configureMagicStackPageData];
  }

  if (self.discoverViewController) {
    [self.mediator configureDiscoverPageData];
  }
}

#pragma mark - HomeCustomizationNavigationDelegate

- (void)presentCustomizationMenuPage:(CustomizationMenuPage)page {
  UIViewController* menuPage = [self createMenuPage:page];

  // If this is the first page being presented, set a reference to it in
  // `firstPageViewController`.
  if (self.baseViewController == self.currentPageViewController) {
    self.firstPageViewController = menuPage;
  }

  [self.currentPageViewController
      presentViewController:menuPage
                   animated:YES
                 completion:^{
                   UIAccessibilityPostNotification(
                       UIAccessibilityScreenChangedNotification, menuPage);
                 }];

  self.currentPageViewController.view.accessibilityElementsHidden = YES;
  self.currentPageViewController = menuPage;
}

- (void)dismissMenuPage {
  // If the page being dismissed is the first page of the stack, then the entire
  // menu should be dismissed. Otherwise, dismiss the topmost page and update
  // the currently visible page.
  if (self.currentPageViewController == self.firstPageViewController) {
    [self.delegate dismissCustomizationMenu];
  } else {
    [self.currentPageViewController dismissViewControllerAnimated:YES
                                                       completion:nil];
    self.currentPageViewController =
        self.currentPageViewController.presentingViewController;
    self.currentPageViewController.view.accessibilityElementsHidden = NO;
  }
}

- (void)navigateToURL:(GURL)URL {
  UrlLoadingBrowserAgent::FromBrowser(self.browser)
      ->Load(UrlLoadParams::InCurrentTab(URL));
  [self.delegate dismissCustomizationMenu];
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  [self dismissMenuPage];
}

#pragma mark - Private

// Creates a view controller for a page in the menu.
- (UIViewController*)createMenuPage:(CustomizationMenuPage)page {
  UIViewController* menuPage;

  // Create view controller for the `page` and configure it with the mediator.
  switch (page) {
    case CustomizationMenuPage::kMain: {
      self.mainViewController =
          [[HomeCustomizationMainViewController alloc] init];
      self.mainViewController.mutator = _mediator;
      self.mediator.mainPageConsumer = self.mainViewController;
      [self.mediator configureMainPageData];
      menuPage = self.mainViewController;
      break;
    }
    case CustomizationMenuPage::kMagicStack: {
      self.magicStackViewController =
          [[HomeCustomizationMagicStackViewController alloc] init];
      self.magicStackViewController.mutator = _mediator;
      self.mediator.magicStackPageConsumer = self.magicStackViewController;
      [self.mediator configureMagicStackPageData];
      menuPage = self.magicStackViewController;
      break;
    }
    case CustomizationMenuPage::kDiscover: {
      self.discoverViewController =
          [[HomeCustomizationDiscoverViewController alloc] init];
      self.discoverViewController.mutator = _mediator;
      self.mediator.discoverPageConsumer = self.discoverViewController;
      [self.mediator configureDiscoverPageData];
      menuPage = self.discoverViewController;
      break;
    }
    case CustomizationMenuPage::kUnknown:
      NOTREACHED_NORETURN();
  }

  // Configure the navigation controller.
  UINavigationController* navigationController =
      [[UINavigationController alloc] initWithRootViewController:menuPage];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

  // Configure the presentation controller with a custom initial detent.
  UISheetPresentationController* presentationController =
      navigationController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.preferredCornerRadius = kSheetCornerRadius;
  presentationController.delegate = self;

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
  ];
  presentationController.selectedDetentIdentifier =
      kBottomSheetDetentIdentifier;
  presentationController.largestUndimmedDetentIdentifier =
      kBottomSheetDetentIdentifier;

  return navigationController;
}

@end
