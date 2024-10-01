// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view.h"

#import "base/check.h"
#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/metrics/histogram_macros.h"
#import "base/numerics/safe_conversions.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/edit_button_config.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_edit_button_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_layout_configurator.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_collection_view_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/placeholder_config.h"

namespace {

// Constants const for users scrolling metrics.
const char kMagicStackScrollToIndexHistogram[] =
    "IOS.MagicStack.ScrollActionToIndex";
const float kMaxModuleHistogramIndex = 50;

}  // namespace

typedef NSDiffableDataSourceSnapshot<NSString*, MagicStackModule*>
    MagicStackSnapshot;

@interface MagicStackCollectionViewController () <UICollectionViewDelegate>

// This UICollectionView's UICollectionViewDiffableDataSource.
@property(strong, nonatomic) MagicStackDiffableDataSource* diffableDataSource;

@end

@implementation MagicStackCollectionViewController {
  MagicStackLayoutConfigurator* _magicStackCollectionViewLayoutConfigurator;
  UICollectionView* _collectionView;
  UICollectionViewCellRegistration* _moduleCellRegistration;
  UICollectionViewCellRegistration* _editButtonRegistration;
  // The most recently selected MagicStack module's page index.
  NSUInteger _magicStackPage;
  BOOL _hasSeenEphemeralCard;
}

- (void)loadView {
  [super loadView];

  [self populateWithPlaceholders];

  self.view = _collectionView;
  [NSLayoutConstraint
      activateConstraints:@[ [_collectionView.heightAnchor
                              constraintEqualToConstant:kModuleMaxHeight] ]];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  _collectionView.clipsToBounds = [self shouldHaveWideLayout];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  __weak UICollectionView* weakCollectionView = _collectionView;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        UICollectionView* strongRefCollectionView = weakCollectionView;
        if (!strongRefCollectionView) {
          return;
        }
        [strongRefCollectionView.collectionViewLayout invalidateLayout];
        [strongRefCollectionView setNeedsLayout];
      }
                      completion:nil];
}

#pragma mark - Public

- (void)moduleWidthDidUpdate {
  if (_collectionView) {
    [self snapToNearestMagicStackModule];
  }
}

- (void)reset {
  [self populateWithPlaceholders];
}

#pragma mark - MagicStackConsumer

- (void)populateItems:(NSArray<MagicStackModule*>*)items {
  if ([items count] > 0) {
    MagicStackModule* card = items[0];
    LogTopModuleImpressionForType(card.type);
    if ([self isCardEphemeral:card]) {
      _hasSeenEphemeralCard = YES;
      [self.audience logEphemeralCardVisibility:card.type];
    }
  }

  for (NSUInteger index = 0; index < [items count]; index++) {
    [items[index].delegate magicStackModule:items[index]
                        wasDisplayedAtIndex:index];
  }
  [self populateItems:items arePlaceholders:NO];
}

- (void)insertItem:(MagicStackModule*)item atIndex:(NSUInteger)index {
  if (index == 0) {
    LogTopModuleImpressionForType(item.type);
    if ([self isCardEphemeral:item]) {
      _hasSeenEphemeralCard = YES;
      [self.audience logEphemeralCardVisibility:item.type];
    }
  }
  [item.delegate magicStackModule:item wasDisplayedAtIndex:index];

  MagicStackSnapshot* snapshot = [self.diffableDataSource snapshot];
  NSInteger section =
      [snapshot indexOfSectionIdentifier:kMagicStackSectionIdentifier];

  // Consistency check: `item`'s ID is not in the collection view.
  if ([self.diffableDataSource indexPathForItemIdentifier:item]) {
    // TODO(b/341410600): Remove once validate in stable that it can be a hard
    // expectation.
    base::debug::DumpWithoutCrashing();
    return;
  }

  // Store the identifier of the current item at the given index, if any, prior
  // to model updates.
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:index
                                               inSection:section];
  MagicStackModule* previousItemIdentifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  // The snapshot API doesn't provide a way to insert at a given index (that's
  // its purpose actually), only before/after an existing item, or by
  // appending to an existing section.
  // If the new item is taking the spot of an existing item, insert the new
  // one before it. Otherwise (if the section is empty, or the new index is
  // the new last position), append at the end of the section.
  if (previousItemIdentifier) {
    [snapshot insertItemsWithIdentifiers:@[ item ]
                beforeItemWithIdentifier:previousItemIdentifier];
  } else {
    [snapshot appendItemsWithIdentifiers:@[ item ]
               intoSectionWithIdentifier:kMagicStackSectionIdentifier];
  }
  [self.diffableDataSource applySnapshot:snapshot
                    animatingDifferences:YES
                              completion:nil];
}

- (void)replaceItem:(MagicStackModule*)oldItem
           withItem:(MagicStackModule*)item {
  NSIndexPath* existingItemIndexPath =
      [self.diffableDataSource indexPathForItemIdentifier:oldItem];
  if (!existingItemIndexPath) {
    return;
  }
  if (item.type == oldItem.type) {
    // Calling a reconfigure requires passing the exisitng item, never passing
    // the latest `item` to the cell. Updates to the same cell requires pushing
    // updates directly to the cell.
    return;
  }
  [item.delegate magicStackModule:item
              wasDisplayedAtIndex:existingItemIndexPath.item];

  MagicStackSnapshot* snapshot = [self.diffableDataSource snapshot];
  // Add the new item before the existing item.
  [snapshot insertItemsWithIdentifiers:@[ item ]
              beforeItemWithIdentifier:oldItem];
  [snapshot deleteItemsWithIdentifiers:@[ oldItem ]];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

- (void)removeItem:(MagicStackModule*)item {
  NSIndexPath* existingItemIndexPath =
      [self.diffableDataSource indexPathForItemIdentifier:item];
  if (!existingItemIndexPath) {
    return;
  }
  MagicStackSnapshot* snapshot = [self.diffableDataSource snapshot];
  [snapshot deleteItemsWithIdentifiers:@[ item ]];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

- (void)reconfigureItem:(MagicStackModule*)item {
  NSIndexPath* existingItemIndexPath =
      [self.diffableDataSource indexPathForItemIdentifier:item];
  if (!existingItemIndexPath) {
    return;
  }
  MagicStackSnapshot* snapshot = [self.diffableDataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ item ]];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  targetContentOffset->x =
      [self getNextPageOffsetForOffset:scrollView.contentOffset.x
                              velocity:velocity.x];
}

#pragma mark - Private

// Configures the collectionView.
- (void)configureCollectionView {
  _magicStackCollectionViewLayoutConfigurator =
      [[MagicStackLayoutConfigurator alloc] init];
  _collectionView = [[UICollectionView alloc]
             initWithFrame:CGRectZero
      collectionViewLayout:_magicStackCollectionViewLayoutConfigurator
                               .magicStackCompositionalLayout];
  _collectionView.accessibilityIdentifier =
      kMagicStackScrollViewAccessibilityIdentifier;
  _collectionView.clipsToBounds = [self shouldHaveWideLayout];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.showsHorizontalScrollIndicator = NO;
  _collectionView.showsVerticalScrollIndicator = NO;
  _collectionView.delegate = self;
  _collectionView.backgroundColor = [UIColor clearColor];

  __weak MagicStackCollectionViewController* weakSelf = self;
  auto configureModuleCell = ^(MagicStackModuleCollectionViewCell* cell,
                               NSIndexPath* indexPath, MagicStackModule* item) {
    [weakSelf configureCell:cell withItem:item atIndex:indexPath.item];
  };
  _moduleCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[MagicStackModuleCollectionViewCell class]
           configurationHandler:configureModuleCell];

  auto configureEditButtonCell =
      ^(MagicStackEditButtonCell* cell, NSIndexPath* indexPath,
        MagicStackModule* item) {
        [weakSelf configureEditButtonCell:cell];
      };
  _editButtonRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[MagicStackEditButtonCell class]
           configurationHandler:configureEditButtonCell];

  self.diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* innerCollectionView,
                    NSIndexPath* indexPath, MagicStackModule* itemIdentifier) {
                  return [weakSelf cellForItemAtIndexPath:indexPath
                                           itemIdentifier:itemIdentifier];
                }];

  _collectionView.dataSource = self.diffableDataSource;
  _magicStackCollectionViewLayoutConfigurator.dataSource =
      self.diffableDataSource;
}

- (UICollectionViewDiffableDataSource*)diffableDataSource {
  if (!_diffableDataSource) {
    [self configureCollectionView];
  }
  return _diffableDataSource;
}

// Returns YES if the MagicStack should be using a wide layout to accomodate for
// larger horizontal device space. This is needed in landscape and on iPads.
- (BOOL)shouldHaveWideLayout {
  return self.traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassRegular ||
         IsLandscape(self.view.window);
}

// Cell provider helper.
- (UICollectionViewCell*)cellForItemAtIndexPath:(NSIndexPath*)indexPath
                                 itemIdentifier:
                                     (MagicStackModule*)itemIdentifier {
  if (indexPath.section ==
      [self.diffableDataSource.snapshot
          indexOfSectionIdentifier:kMagicStackEditSectionIdentifier]) {
    return [_collectionView
        dequeueConfiguredReusableCellWithRegistration:_editButtonRegistration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  }
  return [_collectionView
      dequeueConfiguredReusableCellWithRegistration:_moduleCellRegistration
                                       forIndexPath:indexPath
                                               item:itemIdentifier];
}

// Cell configuration handler helper.
- (void)configureCell:(MagicStackModuleCollectionViewCell*)cell
             withItem:(MagicStackModule*)item
              atIndex:(NSUInteger)index {
  cell.delegate = self.audience;
  [cell configureWithConfig:item];
}

- (void)configureEditButtonCell:(MagicStackEditButtonCell*)cell {
  cell.audience = self.audience;
}

// Creates two placeholder module configs and inserts them as the initial items.
- (void)populateWithPlaceholders {
  NSMutableArray<MagicStackModule*>* items =
      [[NSMutableArray<MagicStackModule*> alloc] init];
  [items addObject:[[PlaceholderConfig alloc] init]];
  [self populateItems:items arePlaceholders:YES];
}

// Populate the Magic Stack section with `items` with the edit button cell when
// `isPlaceholder` is NO.
- (void)populateItems:(NSArray<MagicStackModule*>*)items
      arePlaceholders:(BOOL)isPlaceholder {
  MagicStackSnapshot* snapshot = [[MagicStackSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kMagicStackSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:items
             intoSectionWithIdentifier:kMagicStackSectionIdentifier];
  if (!isPlaceholder) {
    [snapshot
        appendSectionsWithIdentifiers:@[ kMagicStackEditSectionIdentifier ]];
    [snapshot appendItemsWithIdentifiers:@[ [[EditButtonConfig alloc] init] ]
               intoSectionWithIdentifier:kMagicStackEditSectionIdentifier];
  }

  [self.diffableDataSource applySnapshot:snapshot
                    animatingDifferences:!isPlaceholder];
}

// Determines the final page offset given the scroll `offset` and the `velocity`
// scroll. If the drag is slow enough, then the closest page is the final state.
// If the drag is in the negative direction, then go to the page previous to the
// closest current page. If the drag is in the positive direction, then go to
// the page after the closest current page.
- (CGFloat)getNextPageOffsetForOffset:(CGFloat)offset
                             velocity:(CGFloat)velocity {
  CGFloat moduleWidth =
      self.view.frame.size.width -
      ModuleNarrowerWidthToAllowPeekingForTraitCollection(self.traitCollection);

  // Find closest page to the current scroll offset.
  CGFloat closestPage = roundf(offset / moduleWidth);

  if (velocity <= -kMagicStackMinimumPaginationScrollVelocity) {
    closestPage--;

    UMA_HISTOGRAM_EXACT_LINEAR(kMagicStackScrollToIndexHistogram, closestPage,
                               kMaxModuleHistogramIndex);
  } else if (velocity >= kMagicStackMinimumPaginationScrollVelocity) {
    closestPage++;
    UMA_HISTOGRAM_EXACT_LINEAR(kMagicStackScrollToIndexHistogram, closestPage,
                               kMaxModuleHistogramIndex);
  }
  NSArray<MagicStackModule*>* items =
      [self.diffableDataSource.snapshot itemIdentifiers];
  closestPage = std::clamp<CGFloat>(closestPage, 0, [items count] - 1);
  _magicStackPage = closestPage;
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::
              kSegmentationPlatformEphemeralCardRanker)) {
    if ([items count] > 0 && !_hasSeenEphemeralCard &&
        [self isCardEphemeral:items[_magicStackPage]]) {
      [self.audience logEphemeralCardVisibility:items[_magicStackPage].type];
    }
  }
  return _magicStackPage * (moduleWidth + kMagicStackSpacing) -
         [self peekOffsetForMagicStackPage:_magicStackPage];
}

// Returns the extra offset needed to have a MagicStack module be left, center,
// or right aligned depending on whether the module is first, in the middle, or
// last.
- (CGFloat)peekOffsetForMagicStackPage:(NSInteger)page {
  if (page == [self.diffableDataSource.snapshot
                  numberOfItemsInSection:kMagicStackSectionIdentifier] -
                  1) {
    // The last module should be trailing aligned so the previous module peeks.
    return [self magicStackPeekInset];
  }
  return 0;
}

// Returns the amount that MagicStack modules are narrower than the ScrollView,
// in order to allow peeking at the sides.
- (CGFloat)magicStackPeekInset {
  // For the narrow width layout, adjust the inset just enough to have the
  // UICollectionView render the adjacent module.
  return [self shouldHaveWideLayout] ? kMagicStackPeekInsetLandscape
                                     : kMagicStackPeekInset + 1;
}

// Snaps the MagicStack ScrollView's contentOffset to the nearest module. Can
// be used after the width of the MagicStack changes to ensure that it doesn't
// end up scrolled to the middle of a module.
- (void)snapToNearestMagicStackModule {
  CGFloat moduleWidth =
      self.view.frame.size.width -
      ModuleNarrowerWidthToAllowPeekingForTraitCollection(self.traitCollection);
  CGPoint offset = _collectionView.contentOffset;
  offset.x = _magicStackPage * (moduleWidth + kMagicStackSpacing) -
             [self peekOffsetForMagicStackPage:_magicStackPage];
  // Do not allow scrolling beyond the end of content, which also ensures that
  // the "edit menu" page doesn't end up left-aligned after a rotation.
  CGFloat maxOffset = MAX(
      0, _collectionView.contentSize.width - _collectionView.bounds.size.width);
  offset.x = MIN(offset.x, maxOffset);
  _collectionView.contentOffset = offset;
}

- (BOOL)isCardEphemeral:(MagicStackModule*)card {
  switch (card.type) {
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
      return YES;
    case ContentSuggestionsModuleType::kMostVisited:
    case ContentSuggestionsModuleType::kShortcuts:
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kTabResumption:
    case ContentSuggestionsModuleType::kParcelTracking:
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
    case ContentSuggestionsModuleType::kPlaceholder:
    case ContentSuggestionsModuleType::kInvalid:
      return NO;
  }
}

@end
