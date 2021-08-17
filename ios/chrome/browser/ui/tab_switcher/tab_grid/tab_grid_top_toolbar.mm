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
const int kSelectionModeButtonSize = 17;
}

@interface TabGridTopToolbar () <UIToolbarDelegate>
@end

@implementation TabGridTopToolbar {
  UIBarButtonItem* _leadingButton;
  UIBarButtonItem* _spaceItem;
  UIBarButtonItem* _newTabButton;
  UIBarButtonItem* _fixedTrailingSpaceItem;
  UIBarButtonItem* _selectionModeFixedSpace;
  UIBarButtonItem* _selectAllButton;
  UIBarButtonItem* _selectedTabsItem;
  UIBarButtonItem* _doneButton;
  UIBarButtonItem* _closeAllOrUndoButton;
  UIBarButtonItem* _editButton;
  UIBarButtonItem* _pageControlItem;
  BOOL _undoActive;
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
  // Reset selected tabs count when mode changes.
  self.selectedTabsCount = 0;
  // Reset the Select All button to its default title.
  [self configureSelectAllButtonTitle];
  [self setItemsForTraitCollection:self.traitCollection];
}

- (void)setSelectedTabsCount:(int)count {
  _selectedTabsCount = count;
  if (_selectedTabsCount == 0) {
    _selectedTabsItem.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_TABS_TITLE);
  } else {
    _selectedTabsItem.title = l10n_util::GetPluralNSStringF(
        IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE, _selectedTabsCount);
  }
}

- (void)setNewTabButtonTarget:(id)target action:(SEL)action {
  _newTabButton.target = target;
  _newTabButton.action = action;
}

- (void)setSelectAllButtonTarget:(id)target action:(SEL)action {
  _selectAllButton.target = target;
  _selectAllButton.action = action;
}

- (void)setDoneButtonTarget:(id)target action:(SEL)action {
  _doneButton.target = target;
  _doneButton.action = action;
}

- (void)setNewTabButtonEnabled:(BOOL)enabled {
  _newTabButton.enabled = enabled;
}

- (void)setCloseAllButtonTarget:(id)target action:(SEL)action {
  _closeAllOrUndoButton.target = target;
  _closeAllOrUndoButton.action = action;
}

- (void)setCloseAllButtonEnabled:(BOOL)enabled {
  _closeAllOrUndoButton.enabled = enabled;
}

- (void)setSelectAllButtonEnabled:(BOOL)enabled {
  _selectAllButton.enabled = enabled;
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
  if (_undoActive != useUndo) {
    _undoActive = useUndo;
    [self setItemsForTraitCollection:self.traitCollection];
  }
}

- (void)configureDeselectAllButtonTitle {
  _selectAllButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_DESELECT_ALL_BUTTON);
}

- (void)configureSelectAllButtonTitle {
  _selectAllButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_ALL_BUTTON);
}

- (void)hide {
  self.backgroundColor = UIColor.blackColor;
  self.pageControl.alpha = 0.0;
}

- (void)show {
  self.backgroundColor = UIColor.clearColor;
  self.pageControl.alpha = 1.0;
}

#pragma mark Edit Button

- (void)setEditButtonMenu:(UIMenu*)menu API_AVAILABLE(ios(14.0)) {
  _editButton.menu = menu;
}

- (void)setEditButtonEnabled:(BOOL)enabled {
  _editButton.enabled = enabled;
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

- (void)setItemsForTraitCollection:(UITraitCollection*)traitCollection {
  UIBarButtonItem* centralItem = _pageControlItem;
  UIBarButtonItem* trailingButton = _doneButton;
  _selectionModeFixedSpace.width = 0;
  if (traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
      traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact) {
    if (_mode == TabGridModeSelection) {
      // In the selection mode, Done button is much smaller than SelectAll
      // we need to calculate the difference on the width and use it as a
      // fixed space to make sure that the title is still centered.
      _selectionModeFixedSpace.width = [self selectionModeFixedSpaceWidth];
      [self setItems:@[
        _selectAllButton, _spaceItem, _selectedTabsItem, _spaceItem,
        _selectionModeFixedSpace, trailingButton
      ]];
    } else {
      [self setItems:@[ _spaceItem, centralItem, _spaceItem ]];
    }
    return;
  }
  // In Landscape normal mode leading button is always "closeAll", or "Edit" if
  // bulk actions feature is enabled.
  if (IsTabsBulkActionsEnabled() && !_undoActive)
    _leadingButton = _editButton;
  else
    _leadingButton = _closeAllOrUndoButton;

  if (ShowThumbStripInTraitCollection(traitCollection)) {
    // The new tab button is only used if the thumb strip is enabled. In other
    // cases, there is a floating new tab button on the bottom.
    [self setItems:@[
      _leadingButton, _spaceItem, centralItem, _spaceItem, _newTabButton,
      _fixedTrailingSpaceItem, trailingButton
    ]];
    return;
  }

  if (IsTabsBulkActionsEnabled() && _mode == TabGridModeSelection) {
    // In the selection mode, Done button is much smaller than SelectAll
    // we need to calculate the difference on the width and use it as a
    // fixed space to make sure that the title is still centered.
    _selectionModeFixedSpace.width = [self selectionModeFixedSpaceWidth];
    centralItem = _selectedTabsItem;
    _leadingButton = _selectAllButton;
  }

  [self setItems:@[
    _leadingButton, _spaceItem, centralItem, _spaceItem,
    _selectionModeFixedSpace, trailingButton
  ]];
}

// Calculates the space width to use for selection mode.
- (CGFloat)selectionModeFixedSpaceWidth {
  NSDictionary* selectAllFontAttrs = @{
    NSFontAttributeName : [UIFont systemFontOfSize:kSelectionModeButtonSize]
  };
  CGFloat selectAllTextWidth =
      [_selectAllButton.title sizeWithAttributes:selectAllFontAttrs].width;
  NSDictionary* DonefontAttr = @{
    NSFontAttributeName : [UIFont systemFontOfSize:kSelectionModeButtonSize
                                            weight:UIFontWeightSemibold]
  };
  return selectAllTextWidth -
         [_doneButton.title sizeWithAttributes:DonefontAttr].width;
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

  // The segmented control has an intrinsic size.
  _pageControl = [[TabGridPageControl alloc] init];
  _pageControl.translatesAutoresizingMaskIntoConstraints = NO;
  _pageControlItem = [[UIBarButtonItem alloc] initWithCustomView:_pageControl];

  _doneButton = [[UIBarButtonItem alloc] init];
  _doneButton.style = UIBarButtonItemStyleDone;
  _doneButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _doneButton.accessibilityIdentifier = kTabGridDoneButtonIdentifier;
  _doneButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON);

  if (IsTabsBulkActionsEnabled()) {
    _editButton = [[UIBarButtonItem alloc] init];
    _editButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
    _editButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_EDIT_BUTTON);
    _editButton.accessibilityIdentifier = kTabGridEditButtonIdentifier;

    _selectAllButton = [[UIBarButtonItem alloc] init];
    _selectAllButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
    _selectAllButton.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_ALL_BUTTON);
    _selectAllButton.accessibilityIdentifier =
        kTabGridEditSelectAllButtonIdentifier;

    _selectedTabsItem = [[UIBarButtonItem alloc] init];
    _selectedTabsItem.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_TABS_TITLE);
    _selectedTabsItem.tintColor =
        UIColorFromRGB(kTabGridToolbarTextButtonColor);
    _selectedTabsItem.action = nil;
    _selectedTabsItem.target = nil;
    _selectedTabsItem.enabled = NO;
    [_selectedTabsItem setTitleTextAttributes:@{
      NSForegroundColorAttributeName :
          UIColorFromRGB(kTabGridToolbarTextButtonColor),
      NSFontAttributeName : [UIFont systemFontOfSize:kSelectionModeButtonSize
                                              weight:UIFontWeightSemibold]

    }
                                     forState:UIControlStateDisabled];
  }

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

  _selectionModeFixedSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace
                           target:nil
                           action:nil];

  [self setItemsForTraitCollection:self.traitCollection];
}

@end
