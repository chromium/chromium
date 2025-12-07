// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_view_controller.h"

#import <QuartzCore/QuartzCore.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/menu/ui_bundled/action_factory.h"
#import "ios/chrome/browser/menu/ui_bundled/menu_histograms.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/ui/tab_group_favicons_grid.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_empty_state_view.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_cell.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_item_data.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_mutator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_notification_cell.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_out_of_date_message_cell.h"

using tab_groups::SharingState;

namespace {

// Layout.
const CGFloat kInterItemSpacing = 24;
const CGFloat kInterGroupSpacing = 16;
const CGFloat kEstimatedNotificationHeight = 60;
const CGFloat kEstimatedTabGroupHeight = 96;
// The minimum width to display two columns. Under that value, display only one
// column.
const CGFloat kColumnCountWidthThreshold = 1000;
const CGFloat kVerticalInset = 20;
const CGFloat kLargeVerticalInset = 40;
const CGFloat kHorizontalInset = 16;
// Percentage of the width dedicated to an horizontal inset when there is only
// one centered element.
const CGFloat kHorizontalInsetPercentageWhenLargeAndOneItem = 0.3125;
// Percentage of the width dedicated to an horizontal inset when there are two
// elements.
const CGFloat kHorizontalInsetPercentageWhenLargeAndTwoItems = 0.125;
NSString* const kOutOfDateMessageSection = @"OutOfDateMessage";
NSString* const kNotificationsSection = @"Notifications";
NSString* const kTabGroupsSection = @"TabGroups";

typedef NSDiffableDataSourceSnapshot<NSString*, TabGroupsPanelItem*>
    TabGroupsPanelSnapshot;

// Returns the accessibility identifier to set on a TabGroupsPanelCell when
// positioned at the given index.
NSString* NotificationCellAccessibilityIdentifier(NSUInteger index) {
  return [NSString
      stringWithFormat:@"%@%ld",
                       kTabGroupsPanelNotificationCellIdentifierPrefix, index];
}

// Returns the accessibility identifier to set on a TabGroupsPanelCell when
// positioned at the given index.
NSString* TabGroupCellAccessibilityIdentifier(NSUInteger index) {
  return [NSString
      stringWithFormat:@"%@%ld", kTabGroupsPanelCellIdentifierPrefix, index];
}

}  // namespace

@interface TabGroupsPanelViewController () <
    TabGroupsPanelNotificationCellDelegate,
    TabGroupsPanelOutOfDateMessageCellDelegate,
    UICollectionViewDelegate>
@end

@implementation TabGroupsPanelViewController {
  UICollectionView* _collectionView;
  UICollectionViewDiffableDataSource<NSString*, TabGroupsPanelItem*>*
      _dataSource;
  // The cell registration for the out-of-date message cell.
  UICollectionViewCellRegistration* _outOfDateMessageRegistration;
  // The cell registration for notifications cells.
  UICollectionViewCellRegistration* _notificationRegistration;
  // The cell registration for tab groups cells.
  UICollectionViewCellRegistration* _tabGroupRegistration;
  TabGridEmptyStateView* _emptyStateView;
  UIViewPropertyAnimator* _emptyStateAnimator;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  self.view.accessibilityIdentifier = kTabGroupsPanelIdentifier;

  _collectionView =
      [[UICollectionView alloc] initWithFrame:self.view.bounds
                         collectionViewLayout:[self createLayout]];
  _collectionView.allowsSelection = NO;
  _collectionView.backgroundColor = UIColor.clearColor;
  // CollectionView, in contrast to TableView, doesn’t inset the
  // cell content to the safe area guide by default. We will just manage the
  // collectionView contentInset manually to fit in the safe area instead.
  _collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  _collectionView.backgroundView = [[UIView alloc] init];
  _collectionView.backgroundColor = UIColor.clearColor;
  _collectionView.delegate = self;
  _collectionView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:_collectionView];

  [self createRegistrations];

  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^(UICollectionView* collectionView,
                               NSIndexPath* indexPath,
                               TabGroupsPanelItem* item) {
                  return [weakSelf cellForItem:item atIndexPath:indexPath];
                }];

  _emptyStateView =
      [[TabGridEmptyStateView alloc] initWithPage:TabGridPageTabGroups];
  _emptyStateView.scrollViewContentInsets =
      _collectionView.adjustedContentInset;
  _emptyStateView.translatesAutoresizingMaskIntoConstraints = NO;
  [_collectionView.backgroundView addSubview:_emptyStateView];
  UILayoutGuide* safeAreaGuide =
      _collectionView.backgroundView.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [_collectionView.backgroundView.centerYAnchor
        constraintEqualToAnchor:_emptyStateView.centerYAnchor],
    [safeAreaGuide.leadingAnchor
        constraintEqualToAnchor:_emptyStateView.leadingAnchor],
    [safeAreaGuide.trailingAnchor
        constraintEqualToAnchor:_emptyStateView.trailingAnchor],
    [_emptyStateView.topAnchor
        constraintGreaterThanOrEqualToAnchor:safeAreaGuide.topAnchor],
    [_emptyStateView.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeAreaGuide.bottomAnchor],
  ]];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [_collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark Public

- (BOOL)isScrolledToTop {
  return IsScrollViewScrolledToTop(_collectionView);
}

- (BOOL)isScrolledToBottom {
  return IsScrollViewScrolledToBottom(_collectionView);
}

- (void)setContentInsets:(UIEdgeInsets)contentInsets {
  // Set the vertical insets on the collection view…
  _collectionView.contentInset =
      UIEdgeInsetsMake(contentInsets.top, 0, contentInsets.bottom, 0);
  // … and the horizontal insets on the layout sections.
  // This is a workaround, as setting the horizontal insets on the collection
  // view isn't honored by the layout when computing the item sizes (items are
  // too big in landscape iPhones with a notch or Dynamic Island).
  _contentInsets = contentInsets;
  [_collectionView.collectionViewLayout invalidateLayout];
}

- (void)prepareForAppearance {
  [_collectionView reloadData];
}

#pragma mark TabGroupsPanelOutOfDateCellDelegate

- (void)updateButtonTappedForOutOfDateMessageCell:
    (TabGroupsPanelOutOfDateMessageCell*)outOfDateMessageCell {
  TabGroupsPanelItem* item = outOfDateMessageCell.outOfDateMessageItem;
  CHECK_EQ(item.type, TabGroupsPanelItemType::kOutOfDateMessage);
  [self.mutator updateAppWithOutOfDateMessageItem:item];
}

- (void)closeButtonTappedForOutOfDateMessageCell:
    (TabGroupsPanelOutOfDateMessageCell*)outOfDateMessageCell {
  TabGroupsPanelItem* item = outOfDateMessageCell.outOfDateMessageItem;
  CHECK_EQ(item.type, TabGroupsPanelItemType::kOutOfDateMessage);
  [self.mutator deleteOutOfDateMessageItem:item];
}

#pragma mark TabGroupsPanelNotificationCellDelegate

- (void)closeButtonTappedForNotificationItem:(TabGroupsPanelItem*)item {
  [self.mutator deleteNotificationItem:item];
}

#pragma mark TabGroupsPanelConsumer

- (void)populateOutOfDateMessageItem:(TabGroupsPanelItem*)outOfDateMessageItem
                    notificationItem:(TabGroupsPanelItem*)notificationItem
                       tabGroupItems:
                           (NSArray<TabGroupsPanelItem*>*)tabGroupItems {
  // Sanity check.
  CHECK_NE(tabGroupItems, nil);
  if (outOfDateMessageItem) {
    CHECK_EQ(outOfDateMessageItem.type,
             TabGroupsPanelItemType::kOutOfDateMessage);
  }
  if (notificationItem) {
    CHECK_EQ(notificationItem.type, TabGroupsPanelItemType::kNotification);
  }
  for (TabGroupsPanelItem* tabGroupItem in tabGroupItems) {
    CHECK_EQ(tabGroupItem.type, TabGroupsPanelItemType::kSavedTabGroup);
  }

  const BOOL hadOneTabGroup = [self hasOnlyOneTabGroup];

  // Update the data source.
  CHECK(_dataSource);
  TabGroupsPanelSnapshot* snapshot = [[TabGroupsPanelSnapshot alloc] init];
  if (outOfDateMessageItem) {
    [snapshot appendSectionsWithIdentifiers:@[ kOutOfDateMessageSection ]];
    [snapshot appendItemsWithIdentifiers:@[ outOfDateMessageItem ]];
    [snapshot reconfigureItemsWithIdentifiers:@[ outOfDateMessageItem ]];
  }
  if (notificationItem) {
    [snapshot appendSectionsWithIdentifiers:@[ kNotificationsSection ]];
    [snapshot appendItemsWithIdentifiers:@[ notificationItem ]];
    [snapshot reconfigureItemsWithIdentifiers:@[ notificationItem ]];
  }
  if (tabGroupItems.count > 0) {
    [snapshot appendSectionsWithIdentifiers:@[ kTabGroupsSection ]];
    [snapshot appendItemsWithIdentifiers:tabGroupItems];
    [snapshot reconfigureItemsWithIdentifiers:tabGroupItems];
  }
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];

  // Invalidate the layout when getting to or coming from 1 item.
  if (hadOneTabGroup != [self hasOnlyOneTabGroup]) {
    [_collectionView.collectionViewLayout invalidateLayout];
  }

  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
  } else {
    [self removeEmptyStateAnimated:YES];
  }
}

- (void)reconfigureItem:(TabGroupsPanelItem*)item {
  TabGroupsPanelSnapshot* snapshot = [_dataSource snapshot];
  if ([snapshot indexOfItemIdentifier:item] == NSNotFound) {
    return;
  }
  [snapshot reconfigureItemsWithIdentifiers:@[ item ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)dismissModals {
  [_collectionView.contextMenuInteraction dismissMenu];
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    performPrimaryActionForItemAtIndexPath:(NSIndexPath*)indexPath {
  TabGroupsPanelItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  switch (item.type) {
    case TabGroupsPanelItemType::kOutOfDateMessage:
      // No-op.
      break;
    case TabGroupsPanelItemType::kNotification:
      if (UIAccessibilityIsVoiceOverRunning() ||
          UIAccessibilityIsSwitchControlRunning()) {
        [self.mutator deleteNotificationItem:item];
      }
      break;
    case TabGroupsPanelItemType::kSavedTabGroup:
      base::RecordAction(base::UserMetricsAction("MobileGroupPanelOpenGroup"));
      [self.mutator selectTabGroupsPanelItem:item];
      break;
  }
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  TabGroupsPanelItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  switch (item.type) {
    case TabGroupsPanelItemType::kOutOfDateMessage:
    case TabGroupsPanelItemType::kNotification:
      return nil;
    case TabGroupsPanelItemType::kSavedTabGroup: {
      TabGroupsPanelCell* cell =
          base::apple::ObjCCastStrict<TabGroupsPanelCell>(
              [_collectionView cellForItemAtIndexPath:indexPath]);
      return [self
          contextMenuConfigurationForCell:cell
                             menuScenario:
                                 kMenuScenarioHistogramTabGroupsPanelEntry];
    }
  }
}

#pragma mark UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self.UIDelegate tabGroupsPanelViewControllerDidScroll:self];
}

- (void)scrollViewDidChangeAdjustedContentInset:(UIScrollView*)scrollView {
  _emptyStateView.scrollViewContentInsets = scrollView.contentInset;
}

#pragma mark Private

// Returns a configured cell for the given `item` and `indexPath`.
- (UICollectionViewCell*)cellForItem:(TabGroupsPanelItem*)item
                         atIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCellRegistration* registration;
  switch (item.type) {
    case TabGroupsPanelItemType::kOutOfDateMessage:
      registration = _outOfDateMessageRegistration;
      break;
    case TabGroupsPanelItemType::kNotification:
      registration = _notificationRegistration;
      break;
    case TabGroupsPanelItemType::kSavedTabGroup:
      registration = _tabGroupRegistration;
      break;
  }
  return [_collectionView
      dequeueConfiguredReusableCellWithRegistration:registration
                                       forIndexPath:indexPath
                                               item:item];
}

// Creates the cell registrations and assigns them to the appropriate
// properties.
- (void)createRegistrations {
  __weak __typeof(self) weakSelf = self;

  // Register TabGroupsPanelOutOfDateMessageCell.
  _outOfDateMessageRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:TabGroupsPanelOutOfDateMessageCell.class
           configurationHandler:^(TabGroupsPanelOutOfDateMessageCell* cell,
                                  NSIndexPath* indexPath,
                                  TabGroupsPanelItem* item) {
             [weakSelf configureOutOfDateMessageCell:cell
                                            withItem:item
                                             atIndex:indexPath.item];
           }];

  // Register TabGroupsPanelNotificationCell.
  _notificationRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:TabGroupsPanelNotificationCell.class
           configurationHandler:^(TabGroupsPanelNotificationCell* cell,
                                  NSIndexPath* indexPath,
                                  TabGroupsPanelItem* item) {
             [weakSelf configureNotificationCell:cell
                                        withItem:item
                                         atIndex:indexPath.item];
           }];

  // Register TabGroupsPanelCell.
  _tabGroupRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:TabGroupsPanelCell.class
           configurationHandler:^(TabGroupsPanelCell* cell,
                                  NSIndexPath* indexPath,
                                  TabGroupsPanelItem* item) {
             [weakSelf configureTabGroupCell:cell
                                    withItem:item
                                     atIndex:indexPath.item];
           }];
}

- (BOOL)hasOutOfDateMessage {
  return [_dataSource.snapshot
             indexOfSectionIdentifier:kOutOfDateMessageSection] != NSNotFound;
}

- (BOOL)hasNotifications {
  return [_dataSource.snapshot
             indexOfSectionIdentifier:kNotificationsSection] != NSNotFound;
}

- (BOOL)hasTabGroups {
  return [_dataSource.snapshot indexOfSectionIdentifier:kTabGroupsSection] !=
         NSNotFound;
}

// Returns whether the data source has only one item. It's used to configure the
// layout.
- (BOOL)hasOnlyOneTabGroup {
  if ([_dataSource.snapshot indexOfSectionIdentifier:kTabGroupsSection] ==
      NSNotFound) {
    return NO;
  }
  return [_dataSource.snapshot numberOfItemsInSection:kTabGroupsSection] == 1;
}

// Returns the compositional layout for the Tab Groups panel.
- (UICollectionViewLayout*)createLayout {
  __weak __typeof(self) weakSelf = self;
  UICollectionViewLayout* layout = [[UICollectionViewCompositionalLayout alloc]
      initWithSectionProvider:^(
          NSInteger sectionIndex,
          id<NSCollectionLayoutEnvironment> layoutEnvironment) {
        return [weakSelf makeSectionAtIndex:sectionIndex
                          layoutEnvironment:layoutEnvironment];
      }];
  return layout;
}

// Returns the appropriate layout section for the given index.
- (NSCollectionLayoutSection*)
    makeSectionAtIndex:(NSInteger)sectionIndex
     layoutEnvironment:(id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  NSString* sectionIdentifier =
      [_dataSource sectionIdentifierForIndex:sectionIndex];
  if ([sectionIdentifier isEqualToString:kOutOfDateMessageSection]) {
    return [self
        makeOutOfDateMessageSectionWithLayoutEnvironment:layoutEnvironment];
  }
  if ([sectionIdentifier isEqualToString:kNotificationsSection]) {
    return
        [self makeNotificationsSectionWithLayoutEnvironment:layoutEnvironment];
  }
  if ([sectionIdentifier isEqualToString:kTabGroupsSection]) {
    return [self makeTabGroupsSectionWithLayoutEnvironment:layoutEnvironment];
  }
  NOTREACHED();
}

// Returns the layout section for the out-of-date message.
- (NSCollectionLayoutSection*)makeOutOfDateMessageSectionWithLayoutEnvironment:
    (id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  const BOOL onlyOneTabGroup = [self hasOnlyOneTabGroup];
  const CGFloat width = layoutEnvironment.container.effectiveContentSize.width;
  const BOOL hasLargeWidth = width > kColumnCountWidthThreshold;

  NSCollectionLayoutDimension* itemWidth =
      [NSCollectionLayoutDimension fractionalWidthDimension:1];
  NSCollectionLayoutDimension* itemHeight = [NSCollectionLayoutDimension
      estimatedDimension:kEstimatedNotificationHeight];
  NSCollectionLayoutSize* itemSize =
      [NSCollectionLayoutSize sizeWithWidthDimension:itemWidth
                                     heightDimension:itemHeight];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutDimension* groupWidth =
      [NSCollectionLayoutDimension fractionalWidthDimension:1];
  NSCollectionLayoutSize* groupSize =
      [NSCollectionLayoutSize sizeWithWidthDimension:groupWidth
                                     heightDimension:itemHeight];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                    subitems:@[ item ]];
  group.interItemSpacing =
      [NSCollectionLayoutSpacing fixedSpacing:kInterItemSpacing];

  CGFloat groupHorizontalInset = kHorizontalInset;
  const BOOL hasLargeInset =
      layoutEnvironment.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassRegular;
  if (hasLargeInset) {
    groupHorizontalInset =
        width * kHorizontalInsetPercentageWhenLargeAndTwoItems;
    if (hasLargeWidth && onlyOneTabGroup) {
      groupHorizontalInset =
          width * kHorizontalInsetPercentageWhenLargeAndOneItem;
    }
  }
  group.contentInsets = NSDirectionalEdgeInsetsMake(0, groupHorizontalInset, 0,
                                                    groupHorizontalInset);

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.interGroupSpacing = kInterGroupSpacing;
  const CGFloat topInset = hasLargeInset ? kLargeVerticalInset : kVerticalInset;
  const CGFloat bottomInset = [self hasNotifications] || [self hasTabGroups]
                                  ? kVerticalInset
                                  : kLargeVerticalInset;
  // Use the `_contentInsets` horizontal insets. See `setContentInsets:` for
  // more details.
  section.contentInsets = NSDirectionalEdgeInsetsMake(
      topInset, self.contentInsets.left, bottomInset, self.contentInsets.right);
  return section;
}

// Returns the layout section for the notifications.
- (NSCollectionLayoutSection*)makeNotificationsSectionWithLayoutEnvironment:
    (id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  const BOOL onlyOneTabGroup = [self hasOnlyOneTabGroup];
  const CGFloat width = layoutEnvironment.container.effectiveContentSize.width;
  const BOOL hasLargeWidth = width > kColumnCountWidthThreshold;

  NSCollectionLayoutDimension* itemWidth =
      [NSCollectionLayoutDimension fractionalWidthDimension:1];
  NSCollectionLayoutDimension* itemHeight = [NSCollectionLayoutDimension
      estimatedDimension:kEstimatedNotificationHeight];
  NSCollectionLayoutSize* itemSize =
      [NSCollectionLayoutSize sizeWithWidthDimension:itemWidth
                                     heightDimension:itemHeight];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutDimension* groupWidth =
      [NSCollectionLayoutDimension fractionalWidthDimension:1];
  NSCollectionLayoutSize* groupSize =
      [NSCollectionLayoutSize sizeWithWidthDimension:groupWidth
                                     heightDimension:itemHeight];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                    subitems:@[ item ]];
  group.interItemSpacing =
      [NSCollectionLayoutSpacing fixedSpacing:kInterItemSpacing];

  CGFloat groupHorizontalInset = kHorizontalInset;
  const BOOL hasLargeInset =
      layoutEnvironment.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassRegular;
  if (hasLargeInset) {
    groupHorizontalInset =
        width * kHorizontalInsetPercentageWhenLargeAndTwoItems;
    if (hasLargeWidth && onlyOneTabGroup) {
      groupHorizontalInset =
          width * kHorizontalInsetPercentageWhenLargeAndOneItem;
    }
  }
  group.contentInsets = NSDirectionalEdgeInsetsMake(0, groupHorizontalInset, 0,
                                                    groupHorizontalInset);

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.interGroupSpacing = kInterGroupSpacing;
  const CGFloat topInset = hasLargeInset ? kLargeVerticalInset : kVerticalInset;
  const CGFloat bottomInset =
      [self hasTabGroups] ? kVerticalInset : kLargeVerticalInset;
  // Use the `_contentInsets` horizontal insets. See `setContentInsets:` for
  // more details.
  section.contentInsets = NSDirectionalEdgeInsetsMake(
      topInset, self.contentInsets.left, bottomInset, self.contentInsets.right);
  return section;
}

// Returns the layout section for the Tab Groups.
- (NSCollectionLayoutSection*)makeTabGroupsSectionWithLayoutEnvironment:
    (id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  const BOOL onlyOneTabGroup = [self hasOnlyOneTabGroup];
  const CGFloat width = layoutEnvironment.container.effectiveContentSize.width;
  const BOOL hasLargeWidth = width > kColumnCountWidthThreshold;
  const NSInteger columnCount = hasLargeWidth && !onlyOneTabGroup ? 2 : 1;

  NSCollectionLayoutDimension* itemWidth =
      [NSCollectionLayoutDimension fractionalWidthDimension:1. / columnCount];
  NSCollectionLayoutDimension* itemHeight =
      [NSCollectionLayoutDimension estimatedDimension:kEstimatedTabGroupHeight];
  NSCollectionLayoutSize* itemSize =
      [NSCollectionLayoutSize sizeWithWidthDimension:itemWidth
                                     heightDimension:itemHeight];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutDimension* groupWidth =
      [NSCollectionLayoutDimension fractionalWidthDimension:1];
  NSCollectionLayoutSize* groupSize =
      [NSCollectionLayoutSize sizeWithWidthDimension:groupWidth
                                     heightDimension:itemHeight];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                            repeatingSubitem:item
                                                       count:columnCount];
  group.interItemSpacing =
      [NSCollectionLayoutSpacing fixedSpacing:kInterItemSpacing];

  CGFloat groupHorizontalInset = kHorizontalInset;
  const BOOL hasLargeInset =
      layoutEnvironment.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassRegular;
  if (hasLargeInset) {
    groupHorizontalInset =
        width * kHorizontalInsetPercentageWhenLargeAndTwoItems;
    if (hasLargeWidth && onlyOneTabGroup) {
      groupHorizontalInset =
          width * kHorizontalInsetPercentageWhenLargeAndOneItem;
    }
  }
  group.contentInsets = NSDirectionalEdgeInsetsMake(0, groupHorizontalInset, 0,
                                                    groupHorizontalInset);

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.interGroupSpacing = kInterGroupSpacing;
  const CGFloat topInset = [self hasNotifications] || [self hasOutOfDateMessage]
                               ? kVerticalInset
                               : kLargeVerticalInset;
  const CGFloat bottomInset =
      hasLargeInset ? kLargeVerticalInset : kVerticalInset;
  // Use the `_contentInsets` horizontal insets. See `setContentInsets:` for
  // more details.
  section.contentInsets = NSDirectionalEdgeInsetsMake(
      topInset, self.contentInsets.left, bottomInset, self.contentInsets.right);
  return section;
}

// Whether to show the empty state view.
- (BOOL)shouldShowEmptyState {
  return _dataSource.snapshot.numberOfItems == 0;
}

// Animates the empty state into view.
- (void)animateEmptyStateIn {
  [_emptyStateAnimator stopAnimation:YES];
  UIView* emptyStateView = _emptyStateView;
  _emptyStateAnimator = [[UIViewPropertyAnimator alloc]
      initWithDuration:1.0 - _emptyStateView.alpha
          dampingRatio:1.0
            animations:^{
              emptyStateView.alpha = 1.0;
              emptyStateView.transform = CGAffineTransformIdentity;
            }];
  [_emptyStateAnimator startAnimation];
}

// Removes the empty state out of view, with animation if `animated` is YES.
- (void)removeEmptyStateAnimated:(BOOL)animated {
  [_emptyStateAnimator stopAnimation:YES];
  UIView* emptyStateView = _emptyStateView;
  auto removeEmptyState = ^{
    emptyStateView.alpha = 0.0;
    emptyStateView.transform = CGAffineTransformScale(CGAffineTransformIdentity,
                                                      /*sx=*/0.9, /*sy=*/0.9);
  };
  if (animated) {
    _emptyStateAnimator =
        [[UIViewPropertyAnimator alloc] initWithDuration:_emptyStateView.alpha
                                            dampingRatio:1.0
                                              animations:removeEmptyState];
    [_emptyStateAnimator startAnimation];
  } else {
    removeEmptyState();
  }
}

// Configures the out-of-date message cell.
- (void)configureOutOfDateMessageCell:(TabGroupsPanelOutOfDateMessageCell*)cell
                             withItem:(TabGroupsPanelItem*)item
                              atIndex:(NSUInteger)index {
  CHECK(cell);
  CHECK(item);
  CHECK_EQ(item.type, TabGroupsPanelItemType::kOutOfDateMessage);
  cell.outOfDateMessageItem = item;
  cell.delegate = self;
}

// Configures the notification cell.
- (void)configureNotificationCell:(TabGroupsPanelNotificationCell*)cell
                         withItem:(TabGroupsPanelItem*)item
                          atIndex:(NSUInteger)index {
  CHECK(cell);
  CHECK(item);
  CHECK_EQ(item.type, TabGroupsPanelItemType::kNotification);
  cell.notificationItem = item;
  cell.accessibilityIdentifier = NotificationCellAccessibilityIdentifier(index);
  cell.delegate = self;
}

// Configures the saved tab group cell.
- (void)configureTabGroupCell:(TabGroupsPanelCell*)cell
                     withItem:(TabGroupsPanelItem*)item
                      atIndex:(NSUInteger)index {
  CHECK(cell);
  CHECK(item);
  CHECK_EQ(item.type, TabGroupsPanelItemType::kSavedTabGroup);
  cell.item = item;
  cell.accessibilityIdentifier = TabGroupCellAccessibilityIdentifier(index);
  TabGroupsPanelItemData* itemData = [_itemDataSource dataForItem:item];
  cell.titleLabel.text = itemData.title;
  cell.dot.backgroundColor = itemData.color;
  cell.subtitleLabel.text = itemData.creationText;
  NSUInteger numberOfTabs = itemData.numberOfTabs;
  cell.faviconsGrid.numberOfTabs = numberOfTabs;

  cell.facePileProvider = [_itemDataSource facePileProviderForItem:item];

  [_itemDataSource fetchFaviconsForCell:cell];
}

// Returns a context menu configuration instance for the given cell in the tab
// groups panel.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForCell:(TabGroupsPanelCell*)cell
                       menuScenario:(MenuScenarioHistogram)scenario {
  // Record that this context menu was shown to the user.
  RecordMenuShown(scenario);

  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];

  __weak TabGroupsPanelViewController* weakSelf = self;
  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  switch (cell.item.sharingState) {
    case SharingState::kNotShared: {
      [menuElements addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                      [weakSelf.mutator deleteTabGroupsPanelItem:cell.item
                                                      sourceView:cell];
                    }]];
      break;
    }
    case SharingState::kShared: {
      [menuElements
          addObject:[actionFactory actionToLeaveSharedTabGroupWithBlock:^{
            [weakSelf.mutator leaveSharedTabGroupsPanelItem:cell.item
                                                 sourceView:cell];
          }]];
      break;
    }
    case SharingState::kSharedAndOwned: {
      [menuElements
          addObject:[actionFactory actionToDeleteSharedTabGroupWithBlock:^{
            [weakSelf.mutator deleteSharedTabGroupsPanelItem:cell.item
                                                  sourceView:cell];
          }]];
      break;
    }
  }

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

@end
