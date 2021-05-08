// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_top_toolbar.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

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
  UIBarButtonItem* _centralItem;
  UIBarButtonItem* _spaceItem;
  UIBarButtonItem* _newTabButton;
  UIBarButtonItem* _fixedTrailingSpaceItem;
  UIBarButtonItem* _selectTabsButton;
}

- (void)hide {
  self.backgroundColor = UIColor.blackColor;
  self.pageControl.alpha = 0.0;
}

- (void)show {
  self.backgroundColor = UIColor.clearColor;
  self.pageControl.alpha = 1.0;
}

- (void)setNewTabButtonTarget:(id)target action:(SEL)action {
  _newTabButton.target = target;
  _newTabButton.action = action;
}

- (void)setNewTabButtonEnabled:(BOOL)enabled {
  _newTabButton.enabled = enabled;
}

- (void)setSelectTabButtonTarget:(id)target action:(SEL)action {
  _selectTabsButton.target = target;
  _selectTabsButton.action = action;
}

- (void)setSelectTabsButtonEnabled:(BOOL)enabled {
  _selectTabsButton.enabled = enabled;
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
  if (traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
      traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact) {
    if (IsTabsBulkActionsEnabled()) {
      [self setItems:@[
        _spaceItem, _spaceItem, _centralItem, _spaceItem, _selectTabsButton
      ]];
    } else {
      [self setItems:@[ _spaceItem, _centralItem, _spaceItem ]];
    }
    return;
  }

  // The new tab button is only used if the thumb strip is enabled. In other
  // cases, there is a floating new tab button on the bottom.
  if (ShowThumbStripInTraitCollection(traitCollection)) {
    [self setItems:@[
      _leadingButton, _spaceItem, _centralItem, _spaceItem, _newTabButton,
      _fixedTrailingSpaceItem, _trailingButton
    ]];
    return;
  }
  if (IsTabsBulkActionsEnabled()) {
    [self setItems:@[
      _leadingButton, _spaceItem, _centralItem, _spaceItem, _selectTabsButton,
      _fixedTrailingSpaceItem, _trailingButton
    ]];
    return;
  }
  [self setItems:@[
    _leadingButton, _spaceItem, _centralItem, _spaceItem, _trailingButton
  ]];
}

- (void)setupViews {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.barStyle = UIBarStyleBlack;
  self.translucent = YES;
  self.delegate = self;
  [self setShadowImage:[[UIImage alloc] init]
      forToolbarPosition:UIBarPositionAny];

  _leadingButton = [[UIBarButtonItem alloc] init];
  _leadingButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

  _selectTabsButton = [[UIBarButtonItem alloc] init];
  _selectTabsButton.image = [UIImage imageNamed:@"select_tabs_toolbar_button"];
  _selectTabsButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

  // The segmented control has an intrinsic size.
  _pageControl = [[TabGridPageControl alloc] init];
  _pageControl.translatesAutoresizingMaskIntoConstraints = NO;
  _centralItem = [[UIBarButtonItem alloc] initWithCustomView:_pageControl];

  _trailingButton = [[UIBarButtonItem alloc] init];
  _trailingButton.style = UIBarButtonItemStyleDone;
  _trailingButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

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
