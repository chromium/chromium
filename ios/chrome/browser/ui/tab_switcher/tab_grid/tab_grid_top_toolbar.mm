// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_top_toolbar.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The space after the new tab toolbar button item. Calculated to have
// approximately 33 pts between the plus button and the done button.
const int kNewTabButtonTrailingSpace = 20;
}

@interface TabGridTopToolbar () <UIToolbarDelegate>
@end

@implementation TabGridTopToolbar {
  UIBarButtonItem* _leadingButton;
  UIBarButtonItem* _spaceItem;
  UIBarButtonItem* _newTabButton;
  UIBarButtonItem* _fixedTrailingSpaceItem;
  UIBarButtonItem* _selectTabsButton;
  UIBarButtonItem* _selectAllButton;
  UIBarButtonItem* _selectedTabsItem;
  UIBarButtonItem* _doneButton;
  UIBarButtonItem* _closeAllOrUndoButton;
  UIBarButtonItem* _pageControlItem;
  UILabel* _selectedTabsLabel;
}

- (UIBarButtonItem*)anchorItem {
  return _leadingButton;
}

- (void)setPage:(TabGridPage)page {
  _page = page;
  [self setItemsForTraitCollection:self.traitCollection];
}

- (void)setMode:(TabGridMode)mode {
  if (_mode == mode)
    return;
  _mode = mode;
  [self setItemsForTraitCollection:self.traitCollection];
}

- (void)setSelectedTabsCount:(int)count {
  _selectedTabsCount = count;
  if (_selectedTabsCount == 0) {
    _selectedTabsLabel.text =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_TABS_TITLE);

  } else {
    _selectedTabsLabel.text = l10n_util::GetPluralNSStringF(
        IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE, _selectedTabsCount);
  }
}

- (void)setNewTabButtonTarget:(id)target action:(SEL)action {
  _newTabButton.target = target;
  _newTabButton.action = action;
}

- (void)setSelectTabButtonTarget:(id)target action:(SEL)action {
  _selectTabsButton.target = target;
  _selectTabsButton.action = action;
}

- (void)setDoneButtonTarget:(id)target action:(SEL)action {
  _doneButton.target = target;
  _doneButton.action = action;
}

- (void)setNewTabButtonEnabled:(BOOL)enabled {
  _newTabButton.enabled = enabled;
}

- (void)setSelectTabsButtonEnabled:(BOOL)enabled {
  _selectTabsButton.enabled = enabled;
}

- (void)setCloseAllButtonTarget:(id)target action:(SEL)action {
  _closeAllOrUndoButton.target = target;
  _closeAllOrUndoButton.action = action;
}

- (void)setCloseAllButtonEnabled:(BOOL)enabled {
  _closeAllOrUndoButton.enabled = enabled;
}

- (void)setDoneButtonEnabled:(BOOL)enabled {
  _doneButton.enabled = enabled;
}

- (void)useUndoCloseAll:(BOOL)useUndo {
  _closeAllOrUndoButton.enabled = YES;
  if (useUndo) {
    _closeAllOrUndoButton.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_UNDO_CLOSE_ALL_BUTTON);
    // Setting the |accessibilityIdentifier| seems to trigger layout, which
    // causes an infinite loop.
    if (_closeAllOrUndoButton.accessibilityIdentifier !=
        kTabGridUndoCloseAllButtonIdentifier) {
      _closeAllOrUndoButton.accessibilityIdentifier =
          kTabGridUndoCloseAllButtonIdentifier;
    }
  } else {
    _closeAllOrUndoButton.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_CLOSE_ALL_BUTTON);
    // Setting the |accessibilityIdentifier| seems to trigger layout, which
    // causes an infinite loop.
    if (_closeAllOrUndoButton.accessibilityIdentifier !=
        kTabGridCloseAllButtonIdentifier) {
      _closeAllOrUndoButton.accessibilityIdentifier =
          kTabGridCloseAllButtonIdentifier;
    }
  }
}

- (void)hide {
  self.backgroundColor = UIColor.blackColor;
  self.pageControl.alpha = 0.0;
}

- (void)show {
  self.backgroundColor = UIColor.clearColor;
  self.pageControl.alpha = 1.0;
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kTabGridTopToolbarHeight);
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // The first time this moves to a superview, perform the view setup.
  if (newSuperview && self.subviews.count == 0) {
    [self setupViews];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self setItemsForTraitCollection:self.traitCollection];
}

#pragma mark - UIBarPositioningDelegate

// Returns UIBarPositionTopAttached, otherwise the toolbar's translucent
// background won't extend below the status bar.
- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  return UIBarPositionTopAttached;
}

#pragma mark - Private

- (BOOL)requiresPreTrailingButtonForTraitCollection:
    (UITraitCollection*)traitCollection {
  return (IsTabsBulkActionsEnabled() && _mode == TabGridModeNormal &&
          self.page != TabGridPageRemoteTabs) ||
         ShowThumbStripInTraitCollection(traitCollection);
}

- (void)setItemsForTraitCollection:(UITraitCollection*)traitCollection {
  UIBarButtonItem* centralItem = _pageControlItem;
  UIBarButtonItem* trailingButton = _doneButton;

  if (traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
      traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact) {
    if (IsTabsBulkActionsEnabled() && self.page != TabGridPageRemoteTabs) {
      if (_mode == TabGridModeSelection) {
        centralItem = _selectedTabsItem;
        _leadingButton = _selectAllButton;
      } else {
        trailingButton = _selectTabsButton;
        _leadingButton = _spaceItem;
      }
      [self setItems:@[
        _leadingButton, _spaceItem, centralItem, _spaceItem, trailingButton
      ]];
    } else {
      [self setItems:@[ _spaceItem, centralItem, _spaceItem ]];
    }
    return;
  }
  // In Landscape normal mode trailig button is always "done".
  trailingButton = _doneButton;
  // In Landscape normal mode leading button is always "closeAll".
  _leadingButton = _closeAllOrUndoButton;

  if ([self requiresPreTrailingButtonForTraitCollection:traitCollection]) {
    UIBarButtonItem* preTrailingButton;

    // The new tab button is only used if the thumb strip is enabled. In other
    // cases, there is a floating new tab button on the bottom.
    if (ShowThumbStripInTraitCollection(traitCollection))
      preTrailingButton = _newTabButton;
    else
      preTrailingButton = _selectTabsButton;

    [self setItems:@[
      _leadingButton, _spaceItem, centralItem, _spaceItem, preTrailingButton,
      _fixedTrailingSpaceItem, trailingButton
    ]];
    return;
  }

  if (IsTabsBulkActionsEnabled() && _mode == TabGridModeSelection) {
    centralItem = _selectedTabsItem;
    _leadingButton = _selectAllButton;
  }

  [self setItems:@[
    _leadingButton, _spaceItem, centralItem, _spaceItem, trailingButton
  ]];
}

- (void)setupViews {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.barStyle = UIBarStyleBlack;
  self.translucent = YES;
  self.delegate = self;
  [self setShadowImage:[[UIImage alloc] init]
      forToolbarPosition:UIBarPositionAny];

  _closeAllOrUndoButton = [[UIBarButtonItem alloc] init];
  _closeAllOrUndoButton.tintColor =
      UIColorFromRGB(kTabGridToolbarTextButtonColor);
  [self useUndoCloseAll:NO];

  _selectAllButton = [[UIBarButtonItem alloc] init];
  _selectAllButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _selectAllButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_ALL_BUTTON);

  _selectTabsButton = [[UIBarButtonItem alloc] init];
  _selectTabsButton.image = [UIImage imageNamed:@"select_tabs_toolbar_button"];
  _selectTabsButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

  // The segmented control has an intrinsic size.
  _pageControl = [[TabGridPageControl alloc] init];
  _pageControl.translatesAutoresizingMaskIntoConstraints = NO;
  _pageControlItem = [[UIBarButtonItem alloc] initWithCustomView:_pageControl];

  _doneButton = [[UIBarButtonItem alloc] init];
  _doneButton.style = UIBarButtonItemStyleDone;
  _doneButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _doneButton.accessibilityIdentifier = kTabGridDoneButtonIdentifier;
  _doneButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON);

  _selectedTabsLabel = [[UILabel alloc] init];
  _selectedTabsLabel.text =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_TABS_TITLE);
  _selectedTabsLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _selectedTabsLabel.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

  _selectedTabsItem =
      [[UIBarButtonItem alloc] initWithCustomView:_selectedTabsLabel];

  _newTabButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
                           target:nil
                           action:nil];
  _newTabButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

  _fixedTrailingSpaceItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace
                           target:nil
                           action:nil];
  _fixedTrailingSpaceItem.width = kNewTabButtonTrailingSpace;

  _spaceItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  [self setItemsForTraitCollection:self.traitCollection];
}

@end
