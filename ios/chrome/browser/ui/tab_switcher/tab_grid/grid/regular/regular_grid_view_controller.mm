// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button_ui_swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_preamble_header.h"

using base::apple::ObjCCast;

namespace {

constexpr base::TimeDelta kInactiveTabsHeaderAnimationDuration =
    base::Seconds(0.3);

}  // namespace

@implementation RegularGridViewController {
  // Tracks if the Inactive Tabs button is being animated out.
  BOOL _inactiveTabsHeaderHideAnimationInProgress;
  // The number of currently inactive tabs. If there are (inactiveTabsCount > 0)
  // and the grid is in TabGridModeNormal, a button is displayed at the top,
  // advertizing them.
  NSInteger _inactiveTabsCount;
  // The number of days after which tabs are considered inactive. This is
  // displayed to the user in the Inactive Tabs button when inactiveTabsCount >
  // 0.
  NSInteger _inactiveTabsDaysThreshold;
  // The supplementary view registration for the Inactive Tabs button header.
  UICollectionViewSupplementaryRegistration*
      _inactiveTabsButtonHeaderRegistration;
  // The supplementary view registration for the Inactive Tabs preamble header.
  UICollectionViewSupplementaryRegistration*
      _inactiveTabsPreambleHeaderRegistration;
}

#pragma mark - Parent's functions

- (BOOL)isContainedGridEmpty {
  return _inactiveTabsCount == 0;
}

// TODO(crbug.com/1504112): Remove this method when the compositional layout is
// fully landed.
- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  if (self.mode == TabGridModeNormal) {
    if (!IsInactiveTabsAvailable()) {
      return CGSizeZero;
    }
    if (self.isClosingAllOrUndoRunning) {
      return CGSizeZero;
    }
    if (_inactiveTabsHeaderHideAnimationInProgress) {
      // The header is animated out to a height of 0.1.
      return CGSizeMake(collectionView.bounds.size.width, 0.1);
    }
    if (_inactiveTabsCount == 0) {
      return CGSizeZero;
    }
    // The Regular Tabs grid has a button to inform about the hidden inactive
    // tabs.
    return [self inactiveTabsButtonHeaderSize];
  } else if (self.mode == TabGridModeInactive) {
    if (!IsInactiveTabsEnabled()) {
      return CGSizeZero;
    }
    // The Inactive Tabs grid has a header to inform about the feature and a
    // link to its settings.
    return [self inactiveTabsPreambleHeaderSize];
  }

  return [super collectionView:collectionView
                               layout:collectionViewLayout
      referenceSizeForHeaderInSection:section];
}

// Returns a configured header for the given index path.
- (UICollectionReusableView*)headerForSectionAtIndexPath:
    (NSIndexPath*)indexPath {
  if (self.mode == TabGridModeNormal) {
    CHECK(IsInactiveTabsAvailable());
    // The Regular Tabs grid has a button to inform about the hidden inactive
    // tabs.
    return [self.collectionView
        dequeueConfiguredReusableSupplementaryViewWithRegistration:
            _inactiveTabsButtonHeaderRegistration
                                                      forIndexPath:indexPath];
  }
  if (self.mode == TabGridModeInactive) {
    CHECK(IsInactiveTabsAvailable());
    // The Inactive Tabs grid has a header to inform about the feature and a
    // link to its settings.
    return [self.collectionView
        dequeueConfiguredReusableSupplementaryViewWithRegistration:
            _inactiveTabsPreambleHeaderRegistration
                                                      forIndexPath:indexPath];
  }
  return [super headerForSectionAtIndexPath:indexPath];
}

- (void)createRegistrations {
  __weak __typeof(self) weakSelf = self;
  // Register InactiveTabsButtonHeader.
  auto configureInactiveTabsButtonHeader =
      ^(InactiveTabsButtonHeader* header, NSString* elementKind,
        NSIndexPath* indexPath) {
        [weakSelf configureInactiveTabsButtonHeader:header];
      };
  _inactiveTabsButtonHeaderRegistration =
      [UICollectionViewSupplementaryRegistration
          registrationWithSupplementaryClass:[InactiveTabsButtonHeader class]
                                 elementKind:
                                     UICollectionElementKindSectionHeader
                        configurationHandler:configureInactiveTabsButtonHeader];

  // Register InactiveTabsPreambleHeader.
  auto configureInactiveTabsPreambleHeader =
      ^(InactiveTabsPreambleHeader* header, NSString* elementKind,
        NSIndexPath* indexPath) {
        [weakSelf configureInactiveTabsPreambleHeader:header];
      };
  _inactiveTabsPreambleHeaderRegistration =
      [UICollectionViewSupplementaryRegistration
          registrationWithSupplementaryClass:[InactiveTabsPreambleHeader class]
                                 elementKind:
                                     UICollectionElementKindSectionHeader
                        configurationHandler:
                            configureInactiveTabsPreambleHeader];

  [super createRegistrations];
}

- (TabsSectionHeaderType)tabsSectionHeaderTypeForMode:(TabGridMode)mode {
  if (mode == TabGridModeNormal) {
    if (!IsInactiveTabsAvailable()) {
      return TabsSectionHeaderType::kNone;
    }
    if (self.isClosingAllOrUndoRunning) {
      return TabsSectionHeaderType::kNone;
    }
    if (_inactiveTabsHeaderHideAnimationInProgress) {
      return TabsSectionHeaderType::kAnimatingOut;
    }
    if (_inactiveTabsCount == 0) {
      return TabsSectionHeaderType::kNone;
    }
    return TabsSectionHeaderType::kInactiveTabs;
  }

  return [super tabsSectionHeaderTypeForMode:mode];
}

#pragma mark - InactiveTabsInfoConsumer

- (void)updateInactiveTabsCount:(NSInteger)count {
  if (_inactiveTabsCount == count) {
    return;
  }
  NSInteger oldCount = _inactiveTabsCount;
  _inactiveTabsCount = count;

  // Update the layout.
  [self updateTabsSectionHeaderType];

  // Update the header.
  if (oldCount == 0) {
    [self showInactiveTabsButtonHeader];
  } else if (count == 0) {
    [self hideInactiveTabsButtonHeader];
  } else {
    // The header just needs to be updated with the new count.
    [self updateInactiveTabsButtonHeader];
  }
}

- (void)updateInactiveTabsDaysThreshold:(NSInteger)daysThreshold {
  if (_inactiveTabsDaysThreshold == daysThreshold) {
    return;
  }
  NSInteger oldDaysThreshold = _inactiveTabsDaysThreshold;
  _inactiveTabsDaysThreshold = daysThreshold;

  // Update the header.
  if (oldDaysThreshold == kInactiveTabsDisabledByUser ||
      daysThreshold == kInactiveTabsDisabledByUser) {
    // The header should appear or disappear. Reload the section.
    [self reloadInactiveTabsButtonHeader];
  } else {
    // The header just needs to be updated with the new days threshold.
    [self updateInactiveTabsButtonHeader];
  }

  // Update the preamble.
  [self updateInactiveTabsPreambleHeader];
}

#pragma mark - Actions

// Called when the Inactive Tabs button is tapped.
- (void)didTapInactiveTabsButton {
  [self.delegate didTapInactiveTabsButtonInGridViewController:self];
}

// Called when the Inactive Tabs settings link is tapped.
- (void)didTapInactiveTabsSettingsLink {
  [self.delegate didTapInactiveTabsSettingsLinkInGridViewController:self];
}

#pragma mark - Private

// Returns the size that should be dedicated to the Inactive Tabs button
// header.
- (CGSize)inactiveTabsButtonHeaderSize {
  // Keep a sizing header.
  static InactiveTabsButtonHeader* gHeader =
      [[InactiveTabsButtonHeader alloc] init];
  gHeader.tabGridCompositionalLayoutEnabled =
      IsTabGridCompositionalLayoutEnabled();

  // Configure it.
  [gHeader configureWithDaysThreshold:_inactiveTabsDaysThreshold];
  if (IsShowInactiveTabsCountEnabled()) {
    [gHeader configureWithCount:_inactiveTabsCount];
  }

  // Get its fitting size.
  CGFloat width = CGRectGetWidth(self.collectionView.bounds);
  CGSize targetSize = CGSize(width, UILayoutFittingExpandedSize.height);
  // Host the view in the hierarchy for it to get the appropriate trait
  // collection. This might be due a UIKit/SwiftUI interaction bug, as this is
  // not necessary for `InactiveTabsPreambleHeader` below for example.
  gHeader.parent = self;
  [self.view addSubview:gHeader];

  CGSize size =
      [gHeader systemLayoutSizeFittingSize:targetSize
             withHorizontalFittingPriority:UILayoutPriorityRequired
                   verticalFittingPriority:UILayoutPriorityFittingSizeLevel];

  // De-parent the header.
  [gHeader removeFromSuperview];
  gHeader.parent = nil;

  return CGSizeMake(width, size.height);
}

// Returns the size that should be dedicated to the Inactive Tabs preamble
// header.
- (CGSize)inactiveTabsPreambleHeaderSize {
  // Keep a sizing header.
  static InactiveTabsPreambleHeader* gHeader =
      [[InactiveTabsPreambleHeader alloc] init];

  // Configure it.
  gHeader.daysThreshold = _inactiveTabsDaysThreshold;

  // Get its fitting size.
  CGFloat width = CGRectGetWidth(self.collectionView.bounds);
  CGSize targetSize = CGSize(width, UILayoutFittingExpandedSize.height);
  CGSize size =
      [gHeader systemLayoutSizeFittingSize:targetSize
             withHorizontalFittingPriority:UILayoutPriorityRequired
                   verticalFittingPriority:UILayoutPriorityFittingSizeLevel];

  return CGSizeMake(width, size.height);
}

- (void)showInactiveTabsButtonHeader {
  // Contrary to `hideInactiveTabsButtonHeader`, this doesn't need to be
  // animated.
  [self reloadInactiveTabsButtonHeader];
}

- (void)hideInactiveTabsButtonHeader {
  NSIndexPath* indexPath =
      [NSIndexPath indexPathForItem:0 inSection:kGridOpenTabsSectionIndex];
  InactiveTabsButtonHeader* header =
      ObjCCast<InactiveTabsButtonHeader>([self.collectionView
          supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                              atIndexPath:indexPath]);
  if (!header) {
    return;
  }

  _inactiveTabsHeaderHideAnimationInProgress = YES;
  [UIView animateWithDuration:kInactiveTabsHeaderAnimationDuration.InSecondsF()
      animations:^{
        header.alpha = 0;
        [self.collectionView.collectionViewLayout invalidateLayout];
      }
      completion:^(BOOL finished) {
        header.hidden = YES;
        self->_inactiveTabsHeaderHideAnimationInProgress = NO;
        // Update the header to make it entirely disappear once the animation is
        // done. This is done after a delay because the completion can be called
        // before the animation ended, causing a visual glitch.
        __weak __typeof(self) weakSelf = self;
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, base::BindOnce(^{
              [weakSelf reloadInactiveTabsButtonHeader];
            }),
            kInactiveTabsHeaderAnimationDuration);
      }];
}

// Reloads the section containing the Inactive Tabs button header.
- (void)reloadInactiveTabsButtonHeader {
  // Prevent the animation, as it leads to a jarring effect when closing all
  // inactive tabs: the inactive tabs view controller gets popped, and the
  // underlying regular Tab Grid moves tabs up.
  // Note: this could be revisited when supporting iPad, as the user could have
  // closed all inactive tabs in a different window.
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  [snapshot reloadSectionsWithIdentifiers:@[ kGridOpenTabsSectionIdentifier ]];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];

  // Make sure to restore the selection. Reloading the section cleared it.
  // https://developer.apple.com/forums/thread/656529
  [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
}

// Reconfigures the Inactive Tabs button header.
- (void)updateInactiveTabsButtonHeader {
  NSIndexPath* indexPath =
      [NSIndexPath indexPathForItem:0 inSection:kGridOpenTabsSectionIndex];
  InactiveTabsButtonHeader* header =
      ObjCCast<InactiveTabsButtonHeader>([self.collectionView
          supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                              atIndexPath:indexPath]);
  // Note: At this point, `header` could be nil if not visible, or if the
  // supplementary view is not an InactiveTabsButtonHeader.
  [self configureInactiveTabsButtonHeader:header];
}

// Reconfigures the Inactive Tabs preamble header.
- (void)updateInactiveTabsPreambleHeader {
  NSIndexPath* indexPath =
      [NSIndexPath indexPathForItem:0 inSection:kGridOpenTabsSectionIndex];
  InactiveTabsPreambleHeader* header =
      ObjCCast<InactiveTabsPreambleHeader>([self.collectionView
          supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                              atIndexPath:indexPath]);
  // Note: At this point, `header` could be nil if not visible, or if the
  // supplementary view is not an InactiveTabsPreambleHeader.
  header.daysThreshold = _inactiveTabsDaysThreshold;
}

// Configures the Inactive Tabs Button header according to the current state.
- (void)configureInactiveTabsButtonHeader:(InactiveTabsButtonHeader*)header {
  header.tabGridCompositionalLayoutEnabled =
      IsTabGridCompositionalLayoutEnabled();
  header.parent = self;
  __weak __typeof(self) weakSelf = self;
  header.buttonAction = ^{
    [weakSelf didTapInactiveTabsButton];
  };
  [header configureWithDaysThreshold:_inactiveTabsDaysThreshold];
  if (IsShowInactiveTabsCountEnabled()) {
    [header configureWithCount:_inactiveTabsCount];
  }
  header.hidden = _inactiveTabsCount == 0;
}

// Configures the Inactive Tabs Preamble header according to the current state.
- (void)configureInactiveTabsPreambleHeader:
    (InactiveTabsPreambleHeader*)header {
  __weak __typeof(self) weakSelf = self;
  header.settingsLinkAction = ^{
    [weakSelf didTapInactiveTabsSettingsLink];
  };
  header.daysThreshold = _inactiveTabsDaysThreshold;
  header.hidden = !IsInactiveTabsEnabled();
}

@end
