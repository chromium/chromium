// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_tab_picker_view_controller.h"

#import "ios/chrome/browser/composebox/ui/composebox_tab_picker_mutator.h"
#import "ios/chrome/browser/shared/public/commands/composebox_tab_picker_commands.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/ui/base_grid_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation ComposeboxTabPickerViewController {
  /// The done button that confirm user's tabs selection.
  UIBarButtonItem* _doneButton;
  /// Current selected tabs count.
  NSUInteger _tabsCount;
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
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  UIView* gridView = _gridViewController.view;
  gridView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_gridViewController];
  [self.view addSubview:gridView];
  [_gridViewController didMoveToParentViewController:self];

  [self configureNavigationBarIfNeeded];

  AddSameConstraints(gridView, self.view);
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  _gridViewController.contentInsets = self.view.safeAreaInsets;
}

#pragma mark - ComposeboxTabPickerConsumer

- (void)setSelectedTabsCount:(NSUInteger)tabsCount {
  _tabsCount = tabsCount;
  self.navigationItem.title =
      _tabsCount > 0 ? l10n_util::GetPluralNSStringF(
                           IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE, _tabsCount)
                     : l10n_util::GetNSString(
                           IDS_IOS_COMPOSEBOX_TAB_PICKER_ADD_TABS_TITLE);
}

- (void)setDoneButtonEnabled:(BOOL)enabled {
  if (!_doneButton) {
    [self configureNavigationBarIfNeeded];
  }
  _doneButton.enabled = enabled;
}

#pragma mark - Private helpers

/// Performs action when the button to add the selected tabs has been pressed.
- (void)attachSelectedTabsButtonTapped {
  [self.mutator attachSelectedTabs];
  [self.composeboxTabPickerHandler hideComposeboxTabPicker];
}

/// Dismisses the view.
- (void)cancelButtonTapped {
  [self.composeboxTabPickerHandler hideComposeboxTabPicker];
}

/// Creates the navigation bar.
- (void)configureNavigationBarIfNeeded {
  if (_doneButton) {
    return;
  }

  _doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(attachSelectedTabsButtonTapped)];
  _doneButton.enabled = NO;

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];

  [self.navigationItem setLeftBarButtonItem:cancelButton];
  [self.navigationItem setRightBarButtonItem:_doneButton];

  // Configure the navigation bar title and buttons enabled or disabled state.
  [self setSelectedTabsCount:_tabsCount];
}

@end
