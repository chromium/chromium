// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_view_controller.h"

#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PinnedTabsViewController ()

// UICollectionView of pinned tabs.
@property(nonatomic, weak) UICollectionView* collectionView;

// Constraints used to update the view during drag and drop actions.
@property(nonatomic, strong) NSLayoutConstraint* dragEnabledConstraint;
@property(nonatomic, strong) NSLayoutConstraint* defaultConstraint;

// Tracks if the view is available. It does not track if the view is visible.
@property(nonatomic, assign, getter=isAvailable) BOOL available;

@end

@implementation PinnedTabsViewController

#pragma mark - UIViewController

- (void)loadView {
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  self.available = YES;

  UICollectionView* collectionView = [[UICollectionView alloc]
             initWithFrame:CGRectZero
      collectionViewLayout:[[UICollectionViewLayout alloc] init]];
  collectionView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  collectionView.layer.cornerRadius = kPinnedViewCornerRadius;
  collectionView.translatesAutoresizingMaskIntoConstraints = NO;

  self.collectionView = collectionView;
  self.view = collectionView;

  self.dragEnabledConstraint = [collectionView.heightAnchor
      constraintEqualToConstant:kPinnedViewDragEnabledHeight];
  self.defaultConstraint = [collectionView.heightAnchor
      constraintEqualToConstant:kPinnedViewDefaultHeight];
  self.defaultConstraint.active = YES;
}

#pragma mark - Public

- (void)pinnedTabsAvailable:(BOOL)available {
  if (available == self.isAvailable)
    return;

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kPinnedViewFadeInTime
      animations:^{
        self.view.alpha = available ? 1.0 : 0.0;
      }
      completion:^(BOOL finished) {
        weakSelf.view.hidden = !available;
        weakSelf.available = available;
      }];
}

@end
