// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_view_controller.h"

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_mutator.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/ui/base_grid_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AimPrototypeTabPickerViewController {
  /// The select tabs button.
  UIBarButtonItem* _selectTabsButton;
  /// The header title of the tab picker.
  UINavigationItem* _headerTitle;
}

- (instancetype)init {
  self = [super init];

  if (self) {
    _gridViewController = [[BaseGridViewController alloc] init];
    _gridViewController.theme = GridThemeLight;
    [_gridViewController setTabGridMode:TabGridMode::kSelection];
  }

  return self;
}

- (void)viewDidLoad {
  UIView* gridView = _gridViewController.view;
  gridView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_gridViewController];
  [self.view addSubview:gridView];
  [_gridViewController didMoveToParentViewController:self];

  UIToolbar* bottomBar = [[UIToolbar alloc] init];
  bottomBar.barStyle = UIBarStyleDefault;
  bottomBar.translucent = YES;
  bottomBar.tintColor = UIColor.blackColor;
  bottomBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:bottomBar];

  // TODO(crbug.com/40280872): Localize this string.
  _headerTitle = [[UINavigationItem alloc] initWithTitle:@"Select tabs"];

  UINavigationBar* navigationBar = [[UINavigationBar alloc] init];
  navigationBar.items = @[ _headerTitle ];
  navigationBar.barStyle = UIBarStyleDefault;
  navigationBar.translucent = YES;
  navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:navigationBar];

  __weak __typeof(self) weakSelf = self;
  UIAction* attachSelectedTabsAction =
      // TODO(crbug.com/40280872): Localize this string.
      [UIAction actionWithTitle:@"Attach selected Tabs"
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf attachSelectedTabsButtonTapped];
                        }];

  UIBarButtonItem* flexibleSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  _selectTabsButton =
      [[UIBarButtonItem alloc] initWithPrimaryAction:attachSelectedTabsAction];
  _selectTabsButton.enabled = NO;
  bottomBar.items = @[ flexibleSpace, _selectTabsButton, flexibleSpace ];

  AddSameConstraintsToSides(
      navigationBar, self.view.safeAreaLayoutGuide,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(
      bottomBar, self.view.safeAreaLayoutGuide,
      LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(
      gridView, self.view,
      LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);

  [NSLayoutConstraint activateConstraints:@[
    [gridView.topAnchor constraintEqualToAnchor:navigationBar.bottomAnchor],
  ]];

  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
}

#pragma mark - AimPrototypeTabPickerConsumer

- (void)setSelectedTabsCount:(NSUInteger)tabsCount {
  if (tabsCount > 0) {
    _headerTitle.title = l10n_util::GetPluralNSStringF(
        IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE, tabsCount);
    _selectTabsButton.enabled = YES;
    return;
  }

  // TODO(crbug.com/40280872): Localize this string.
  _headerTitle.title = @"Select Tabs";
  _selectTabsButton.enabled = NO;
}

#pragma mark - private

- (void)attachSelectedTabsButtonTapped {
  if (_selectTabsButton.enabled) {
    [self.mutator attachSelectedTabs];
  }
}

@end
