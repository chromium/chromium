// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_top_toolbar.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridTopToolbar () <UIToolbarDelegate>
@end

@implementation TabGridTopToolbar {
  UIBarButtonItem* _centralItem;
  UIBarButtonItem* _spaceItem;
  UIBarButtonItem* _newTabButton;
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
    [self setItems:@[ _spaceItem, _centralItem, _spaceItem ]];
  } else {
    // The new tab button is only used if the thumb strip is enabled. In other
    // cases, there is a floating new tab button on the bottom.
    if (ShowThumbStripInTraitCollection(traitCollection)) {
      [self setItems:@[
        _leadingButton, _spaceItem, _centralItem, _spaceItem, _newTabButton,
        _trailingButton
      ]];
    } else {
      [self setItems:@[
        _leadingButton, _spaceItem, _centralItem, _spaceItem, _trailingButton
      ]];
    }
  }
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

  _spaceItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  [self setItemsForTraitCollection:self.traitCollection];
}

@end
