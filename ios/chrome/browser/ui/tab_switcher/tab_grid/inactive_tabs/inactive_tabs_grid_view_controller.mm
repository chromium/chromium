// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_grid_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+subclassing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_preamble_header.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@implementation InactiveGridViewController {
  // The supplementary view registration for the Inactive Tabs preamble header.
  UICollectionViewSupplementaryRegistration*
      _inactiveTabsPreambleHeaderRegistration;
  // The number of days after which tabs are considered inactive.
  NSInteger _inactiveTabsDaysThreshold;
}

#pragma mark - Parent's functions

- (void)closeButtonTappedForCell:(GridCell*)cell {
  [self.delegate
      gridViewController:self
      didCloseItemWithID:cell.itemIdentifier.tabSwitcherItem.identifier];
}

- (UICollectionReusableView*)headerForSectionAtIndexPath:
    (NSIndexPath*)indexPath {
  CHECK(IsInactiveTabsAvailable());
  // The Inactive Tabs grid has a header to inform about the feature and a
  // link to its settings.
  return [self.collectionView
      dequeueConfiguredReusableSupplementaryViewWithRegistration:
          _inactiveTabsPreambleHeaderRegistration
                                                    forIndexPath:indexPath];
}

- (void)createRegistrations {
  __weak __typeof(self) weakSelf = self;

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
  return TabsSectionHeaderType::kInactiveTabs;
}

- (MenuScenarioHistogram)scenarioForContextMenu {
  return kMenuScenarioHistogramInactiveTabsEntry;
}

#pragma mark - InactiveTabsInfoConsumer

- (void)updateInactiveTabsCount:(NSInteger)count {
  // No op.
}

- (void)updateInactiveTabsDaysThreshold:(NSInteger)daysThreshold {
  if (_inactiveTabsDaysThreshold == daysThreshold) {
    return;
  }

  _inactiveTabsDaysThreshold = daysThreshold;

  NSInteger tabSectionIndex = [self.diffableDataSource
      indexForSectionIdentifier:kGridOpenTabsSectionIdentifier];
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0
                                               inSection:tabSectionIndex];
  InactiveTabsPreambleHeader* header =
      base::apple::ObjCCast<InactiveTabsPreambleHeader>([self.collectionView
          supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                              atIndexPath:indexPath]);
  // Note: At this point, `header` could be nil if not visible, or if the
  // supplementary view is not an InactiveTabsPreambleHeader.
  header.daysThreshold = _inactiveTabsDaysThreshold;
}

#pragma mark - Private

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

// Called when the Inactive Tabs settings link is tapped.
- (void)didTapInactiveTabsSettingsLink {
  [self.delegate didTapInactiveTabsSettingsLinkInGridViewController:self];
}

@end
