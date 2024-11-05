// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/inactive_tabs_button_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/tabs_closure_animation.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_preamble_header.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

using base::apple::ObjCCast;
using base::apple::ObjCCastStrict;

namespace {

// Returns the views to animate a tab group closure. If the entire group is to
// be closed, i.e. all tabs of `group_grid_cell` are in
// `indexes_in_group_to_close`, then returns the entire tab group grid cell. If
// only some tabs of the group are to be closed, then it returns only the views
// inside the group view that correspond to the tabs to be closed.
NSArray<UIView*>* GetTabGroupViewsToAnimateClosure(
    GroupGridCell* group_grid_cell,
    std::set<int> indexes_in_group_to_close) {
  CHECK(!indexes_in_group_to_close.empty());

  // If the entire group is going to be closed, then animate the entire grid
  // cell.
  if ((long)indexes_in_group_to_close.size() == group_grid_cell.tabsCount) {
    return @[ group_grid_cell ];
  }

  // If only some of the tabs inside the tab group are going to be closed, then
  // animate the corresponding views inside the tab group cell.
  NSArray<UIView*>* all_views = [group_grid_cell allGroupTabViews];
  NSMutableArray<UIView*>* all_views_to_close = [[NSMutableArray alloc] init];

  NSInteger numberOfViews = (NSInteger)all_views.count;

  // The last view inside a group can represent a tab or a counter.
  NSInteger lastViewIsCounter = group_grid_cell.tabsCount > numberOfViews;
  NSInteger numberOfTabViews =
      lastViewIsCounter ? numberOfViews - 1 : numberOfViews;

  // Loop through all the views that represent tabs. A tab view is animated out
  // if that tab is going to be closed.
  for (NSInteger index = 0; index < numberOfTabViews; index++) {
    if (indexes_in_group_to_close.contains(index)) {
      [all_views_to_close addObject:[all_views objectAtIndex:index]];
    }
  }

  // Deal with the view that represents a counter, if it exists. A counter view
  // is animated out if it will disappear after the animation.
  if (lastViewIsCounter) {
    NSInteger numberOfTabsAfterClosure =
        group_grid_cell.tabsCount - indexes_in_group_to_close.size();
    BOOL animateCounter = numberOfTabsAfterClosure <= numberOfViews;
    if (animateCounter) {
      [all_views_to_close
          addObject:[all_views objectAtIndex:numberOfViews - 1]];
    }
  }

  return all_views_to_close;
}

}  // namespace

@implementation RegularGridViewController {
  // The number of currently inactive tabs. If there are (inactiveTabsCount > 0)
  // and the grid is in TabGridMode::kNormal, a button is displayed at the top,
  // advertizing them.
  NSInteger _inactiveTabsCount;
  // The number of days after which tabs are considered inactive. This is
  // displayed to the user in the Inactive Tabs button when inactiveTabsCount >
  // 0.
  NSInteger _inactiveTabsDaysThreshold;

  // The cell registration for inactive tabs button cell.
  UICollectionViewCellRegistration* _inactiveTabsButtonCellRegistration;

  // The object responsible for animating the tabs closure.
  TabsClosureAnimation* _tabsClosureAnimation;
}

#pragma mark - Public

- (void)animateTabsClosureForTabs:(std::set<web::WebStateID>)tabsToClose
                           groups:
                               (std::map<tab_groups::TabGroupId, std::set<int>>)
                                   groupsWithTabsToClose
                  allInactiveTabs:(BOOL)animateAllInactiveTabs
                completionHandler:(ProceduralBlock)completionHandler {
  base::Time startTime = base::Time::Now();
  NSMutableArray<UIView*>* gridCells = [[NSMutableArray alloc] init];

  for (NSIndexPath* path in self.collectionView.indexPathsForVisibleItems) {
    GridItemIdentifier* item =
        [self.diffableDataSource itemIdentifierForIndexPath:path];
    UICollectionViewCell* collectionViewCell =
        [self.collectionView cellForItemAtIndexPath:path];

    switch (item.type) {
      case GridItemType::kTab:
        if (tabsToClose.contains(item.tabSwitcherItem.identifier)) {
          [gridCells addObject:collectionViewCell];
        }
        break;
      case GridItemType::kGroup:
        if (groupsWithTabsToClose.contains(
                item.tabGroupItem.tabGroup->tab_group_id())) {
          [gridCells addObjectsFromArray:
                         GetTabGroupViewsToAnimateClosure(
                             ObjCCastStrict<GroupGridCell>(collectionViewCell),
                             groupsWithTabsToClose[item.tabGroupItem.tabGroup
                                                       ->tab_group_id()])];
        }
        break;
      case GridItemType::kInactiveTabsButton:
        if (animateAllInactiveTabs) {
          [gridCells addObject:collectionViewCell];
        }
        break;
      case GridItemType::kSuggestedActions:
        // No-op.
        break;
      case GridItemType::kActivitySummary:
        // No-op.
        break;
    }
  }

  __weak RegularGridViewController* weakSelf = self;
  _tabsClosureAnimation =
      [[TabsClosureAnimation alloc] initWithWindow:self.view.window
                                         gridCells:gridCells];

  [_tabsClosureAnimation animateWithCompletion:^{
    [weakSelf onTabsClosureAnimationEndWithCompletion:completionHandler];
    base::TimeDelta delta = base::Time::Now() - startTime;
    base::UmaHistogramMicrosecondsTimes(
        "Privacy.DeleteBrowsingData.Duration.TabsClosureAnimationDuration",
        delta);
  }];

  base::TimeDelta delta = base::Time::Now() - startTime;
  base::UmaHistogramMicrosecondsTimes(
      "Privacy.DeleteBrowsingData.Duration.TabsClosureAnimationStartDelay",
      delta);
}

#pragma mark - Parent's functions

- (BOOL)isContainedGridEmpty {
  return _inactiveTabsCount == 0;
}

- (UICollectionViewCell*)cellForItemAtIndexPath:(NSIndexPath*)indexPath
                                 itemIdentifier:
                                     (GridItemIdentifier*)itemIdentifier {
  if (itemIdentifier.type == GridItemType::kInactiveTabsButton) {
    UICollectionViewCellRegistration* registration =
        _inactiveTabsButtonCellRegistration;
    return [self.collectionView
        dequeueConfiguredReusableCellWithRegistration:registration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  }

  return [super cellForItemAtIndexPath:indexPath itemIdentifier:itemIdentifier];
}

- (void)createRegistrations {
  __weak __typeof(self) weakSelf = self;
  // Register InactiveTabsButtonCell.
  auto configureInactiveTabsButtonCell =
      ^(InactiveTabsButtonCell* cell, NSIndexPath* indexPath, id item) {
        [weakSelf configureInativeTabsButtonCell:cell];
      };
  _inactiveTabsButtonCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:InactiveTabsButtonCell.class
           configurationHandler:configureInactiveTabsButtonCell];

  [super createRegistrations];
}

- (void)addAdditionalItemsToSnapshot:(GridSnapshot*)snapshot {
  [self updateInactiveTabsButtonInSnapshot:snapshot];
}

- (void)updateSnapshotForModeUpdate:(GridSnapshot*)snapshot {
  [self updateInactiveTabsButtonInSnapshot:snapshot];
}

#pragma mark - InactiveTabsInfoConsumer

- (void)updateInactiveTabsCount:(NSInteger)count {
  if (_inactiveTabsCount == count) {
    return;
  }
  _inactiveTabsCount = count;

  GridSnapshot* snapshot = [self.diffableDataSource snapshot];
  [self updateInactiveTabsButtonInSnapshot:snapshot];
}

- (void)updateInactiveTabsDaysThreshold:(NSInteger)daysThreshold {
  if (_inactiveTabsDaysThreshold == daysThreshold) {
    return;
  }
  _inactiveTabsDaysThreshold = daysThreshold;

  GridSnapshot* snapshot = [self.diffableDataSource snapshot];
  [self updateInactiveTabsButtonInSnapshot:snapshot];
}

#pragma mark - Private

// Callback of `_tabsClosureAnimation` when the animation has been completed.
// Closes the actual tabs in `tabsToClose`.
- (void)onTabsClosureAnimationEndWithCompletion:
    (ProceduralBlock)closeSelectedTabsOnCompletion {
  CHECK(closeSelectedTabsOnCompletion);
  // Close selected tabs which which rearranges the grid to not include the tabs
  // hidden by the animation.
  closeSelectedTabsOnCompletion();

  _tabsClosureAnimation = nil;
}

// Updates the inactive tabs button (reconfigure, show or remove) based on its
// visible state.
- (void)updateInactiveTabsButtonInSnapshot:(GridSnapshot*)snapshot {
  if (!IsInactiveTabsAvailable()) {
    return;
  }

  BOOL isEnabled = _inactiveTabsDaysThreshold != kInactiveTabsDisabledByUser;
  BOOL hasInactiveTabs = _inactiveTabsCount != 0;
  BOOL isInNormalMode = self.mode == TabGridMode::kNormal;

  BOOL visible = isEnabled && hasInactiveTabs && isInNormalMode;

  if (visible) {
    GridItemIdentifier* item =
        [GridItemIdentifier inactiveTabsButtonIdentifier];

    if ([snapshot indexOfItemIdentifier:item] != NSNotFound) {
      [snapshot reconfigureItemsWithIdentifiers:@[ item ]];
    } else {
      [self addInactiveTabsButtonToSnapshot:snapshot];
    }
  } else {
    BOOL isSectionInSnapshot =
        [snapshot
            indexOfSectionIdentifier:kInactiveTabButtonSectionIdentifier] !=
        NSNotFound;

    if (isSectionInSnapshot) {
      [snapshot deleteSectionsWithIdentifiers:@[
        kInactiveTabButtonSectionIdentifier
      ]];
    }
  }
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Adds the inactive tabs button to `snapshot` if it is not there yet.
- (void)addInactiveTabsButtonToSnapshot:(GridSnapshot*)snapshot {
  NSInteger sectionIndex =
      [snapshot indexOfSectionIdentifier:kInactiveTabButtonSectionIdentifier];

  if (sectionIndex == NSNotFound) {
    [snapshot
        insertSectionsWithIdentifiers:@[ kInactiveTabButtonSectionIdentifier ]
          beforeSectionWithIdentifier:kGridOpenTabsSectionIdentifier];
  }

  GridItemIdentifier* item = [GridItemIdentifier inactiveTabsButtonIdentifier];

  if ([snapshot indexOfItemIdentifier:item] == NSNotFound) {
    [snapshot appendItemsWithIdentifiers:@[ item ]
               intoSectionWithIdentifier:kInactiveTabButtonSectionIdentifier];
  }
}

// Configures `cell` according to the current state.
- (void)configureInativeTabsButtonCell:(InactiveTabsButtonCell*)cell {
  cell.count = _inactiveTabsCount;
  cell.daysThreshold = _inactiveTabsDaysThreshold;
}

@end
