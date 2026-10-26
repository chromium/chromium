// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_page_view_controller.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

namespace {

// The width of the sheet in landscape.
const CGFloat kLandscapeSheetWidth = 400.0f;

// The estimated navigation item spacing.
const CGFloat kEstimatedNavigationItemSpacing = 56.0;

}  // namespace

@interface ComposeboxMenuViewController () <
    UINavigationControllerDelegate,
    ComposeboxMenuPageViewControllerDelegate>
@end

@implementation ComposeboxMenuViewController {
  // The menu pages displayed by the controller.
  ComposeboxMenuPageViewController* _mainMenuVC;
  ComposeboxMenuPageViewController* _secondaryMenuVC;
}

- (instancetype)init {
  ComposeboxMenuPageType pageType = IsPlusButtonMenuMoreOptionsSubmenu()
                                        ? ComposeboxMenuPageType::kMainPage
                                        : ComposeboxMenuPageType::kFullMenu;
  ComposeboxMenuPageViewController* mainMenuVC =
      [[ComposeboxMenuPageViewController alloc] initWithPageType:pageType];
  self = [super initWithRootViewController:mainMenuVC];
  if (self) {
    _mainMenuVC = mainMenuVC;
    _mainMenuVC.delegate = self;
    _secondaryMenuVC = [[ComposeboxMenuPageViewController alloc]
        initWithPageType:ComposeboxMenuPageType::kSecondaryPage];
    _secondaryMenuVC.delegate = self;
    self.delegate = self;
  }

  return self;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [_mainMenuVC.view layoutIfNeeded];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  [self.menuDelegate composeboxMenuViewControllerDidChangeMenuPage:self];
}

#pragma mark - ComposeboxMenuPageViewControllerDelegate

- (void)composeboxMenuPageViewControllerDidRequestSecondaryMenu:
    (ComposeboxMenuPageViewController*)composeboxMenuPageViewController {
  [self pushViewController:_secondaryMenuVC animated:YES];
  [_secondaryMenuVC.view layoutIfNeeded];
  [self.menuDelegate composeboxMenuViewControllerDidChangeMenuPage:self];
}

- (void)setUIInputState:(ComposeboxUIInputState*)state {
  [_mainMenuVC setUIInputState:state];
  [_secondaryMenuVC setUIInputState:state];
}

- (void)setMutator:(id<ComposeboxMenuMutator>)mutator {
  _mutator = mutator;
  _mainMenuVC.mutator = mutator;
  _secondaryMenuVC.mutator = mutator;
}

- (CGSize)preferredContentSize {
  ComposeboxMenuPageViewController* topViewController =
      base::apple::ObjCCast<ComposeboxMenuPageViewController>(
          self.topViewController);
  BOOL isFirstPage =
      topViewController.menuPageType != ComposeboxMenuPageType::kSecondaryPage;
  if (isFirstPage && self.isViewLoaded && self.view.window) {
    [self.view layoutIfNeeded];
  }
  CGSize size =
      [self.view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];

  // The secondary menu page displays navigation items on top, so the safe area
  // needs to be taken into account.
  // As the VC might not yet be in the hierarchy by the time this is called,
  // use an estimated value and the size of the view controller.
  if (topViewController.menuPageType ==
      ComposeboxMenuPageType::kSecondaryPage) {
    CGFloat extraTopPadding = topViewController.view.superview
                                  ? topViewController.view.safeAreaInsets.top
                                  : kEstimatedNavigationItemSpacing;
    size.height = topViewController.contentHeightToFit + extraTopPadding;
  } else {
    size.height = topViewController.contentHeightToFit;
  }

  BOOL isLandscape =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  if (isLandscape) {
    CGFloat baseWidth = size.width > 0 ? size.width : kLandscapeSheetWidth;
    size.width =
        baseWidth < kLandscapeSheetWidth ? baseWidth : kLandscapeSheetWidth;
  } else {
    // Portrait fallback: use full width if not specified.
    if (size.width == 0) {
      size.width = self.view.bounds.size.width;
    }
  }
  return size;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self.menuDelegate composeboxMenuViewControllerDidRequestClose:self];
}

@end
