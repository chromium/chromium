// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_data_source.h"
#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_item.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_group_confirmation_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator_items_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+Testing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+subclassing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_header.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/modals/modals_api.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util.h"

using base::apple::ObjCCast;
using base::apple::ObjCCastStrict;

class ScopedScrollingTimeLogger {
 public:
  ScopedScrollingTimeLogger() : start_(base::TimeTicks::Now()) {}
  ~ScopedScrollingTimeLogger() {
    base::TimeDelta duration = base::TimeTicks::Now() - start_;
    base::UmaHistogramTimes("IOS.TabSwitcher.TimeSpentScrolling", duration);
  }

 private:
  base::TimeTicks start_;
};

namespace {
NSString* const kCellIdentifier = @"GridCellIdentifier";
NSString* const kGroupCellIdentifier = @"GroupGridCellIdentifier";

// Returns the accessibility identifier to set on a GridCell when positioned at
// the given index.
NSString* GridCellAccessibilityIdentifier(NSUInteger index) {
  return [NSString stringWithFormat:@"%@%ld", kGridCellIdentifierPrefix, index];
}

// Returns the accessibility identifier to set on a GroupGridCell when
// positioned at the given index.
NSString* GroupGridCellAccessibilityIdentifier(NSUInteger index) {
  return [NSString
      stringWithFormat:@"%@%ld", kGroupGridCellIdentifierPrefix, index];
}

}  // namespace

@interface BaseGridViewController () <GridCellDelegate,
                                      GroupGridCellDelegate,
                                      SuggestedActionsViewControllerDelegate,
                                      UICollectionViewDropDelegate,
                                      UIPointerInteractionDelegate>
// A collection view of items in a grid format.
@property(nonatomic, weak) UICollectionView* collectionView;
// The collection view's data source.
@property(nonatomic, strong) GridDiffableDataSource* diffableDataSource;
// Identifier of the selected item. This should only be used for lookup or
// equality checks, in that it is usually not possible to fetch its images
// (favicon, snapshot). Use the GridItemIdentifier from the data source that
// matches instead.
@property(nonatomic, strong) GridItemIdentifier* selectedItemIdentifier;
// Index of the selected item.
@property(nonatomic, readonly) NSUInteger selectedIndex;
// ID of the last item to be inserted. This is used to track if the active tab
// was newly created when building the animation layout for transitions.
@property(nonatomic, assign) web::WebStateID lastInsertedItemID;
// Animator to show or hide the empty state.
@property(nonatomic, strong) UIViewPropertyAnimator* emptyStateAnimator;
// The layout for the tab grid.
@property(nonatomic, strong) GridLayout* gridLayout;
// The view controller that holds the view of the suggested search actions.
@property(nonatomic, strong)
    SuggestedActionsViewController* suggestedActionsViewController;
// Grid cells for which pointer interactions have been added. Pointer
// interactions should only be added to displayed cells (not transition cells).
// This is only expected to get as large as the number of reusable grid cells in
// memory.
@property(nonatomic, strong) NSHashTable<GridCell*>* pointerInteractionCells;
// YES while batch updates and the batch update completion are being performed.
@property(nonatomic) BOOL updating;
// YES while the grid has the suggested actions section.
@property(nonatomic) BOOL showingSuggestedActions;
// YES if the dragged tab moved to a new index.
@property(nonatomic, assign) BOOL dragEndAtNewIndex;
// Tracks if a drop action initiated in this grid is in progress.
@property(nonatomic) BOOL localDragActionInProgress;
// Tracks if the items are in a batch action, which are the "Close All" or
// "Undo" the close all.
@property(nonatomic) BOOL isClosingAllOrUndoRunning;
@end

@implementation BaseGridViewController {
  // Tracks when the grid view is scrolling. Create a new instance to start
  // timing and reset to stop and log the associated time histogram.
  std::optional<ScopedScrollingTimeLogger> _scopedScrollingTimeLogger;

  // The cell registration for grid cells.
  UICollectionViewCellRegistration* _gridCellRegistration;
  // The cell registration for grid group cells.
  UICollectionViewCellRegistration* _groupGridCellRegistration;
  // The cell registration for the Suggested Actions cell.
  UICollectionViewCellRegistration* _suggestedActionsCellRegistration;

  // The supplementary view registration for the grid header.
  UICollectionViewSupplementaryRegistration* _gridHeaderRegistration;

  // Latest dragged item identifier. This property is set when the item is
  // long pressed which does not always result in a drag action.
  GridItemIdentifier* _draggedItemIdentifier;

  // Current mode of the Tab Grid. Should be set through consumer protocol.
  TabGridMode _mode;
}

- (instancetype)init {
  if ((self = [super init])) {
    _dropAnimationInProgress = NO;
    _localDragActionInProgress = NO;
    _notSelectedTabCellOpacity = 1.0;
    _mode = TabGridMode::kNormal;

    // Register for VoiceOver notifications.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(voiceOverStatusDidChange)
               name:UIAccessibilityVoiceOverStatusDidChangeNotification
             object:nil];

    // Register for Dynamic Type notifications.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(preferredContentSizeCategoryDidChange)
               name:UIContentSizeCategoryDidChangeNotification
             object:nil];
  }

  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;

  GridLayout* gridLayout = [[GridLayout alloc] init];
  self.gridLayout = gridLayout;

  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:gridLayout];
  // If this stays as the default `YES`, then cells aren't highlighted
  // immediately on touch, but after a short delay.
  collectionView.delaysContentTouches = NO;
  collectionView.alwaysBounceVertical = YES;

  [self createRegistrations];

  __weak __typeof(self) weakSelf = self;
  GridDiffableDataSource* diffableDataSource =
      [[UICollectionViewDiffableDataSource alloc]
          initWithCollectionView:collectionView
                    cellProvider:^UICollectionViewCell*(
                        UICollectionView* innerCollectionView,
                        NSIndexPath* indexPath,
                        GridItemIdentifier* itemIdentifier) {
                      return [weakSelf cellForItemAtIndexPath:indexPath
                                               itemIdentifier:itemIdentifier];
                    }];
  self.diffableDataSource = diffableDataSource;

  gridLayout.diffableDataSource = diffableDataSource;

  diffableDataSource.supplementaryViewProvider = ^UICollectionReusableView*(
      UICollectionView* innerCollectionView, NSString* elementKind,
      NSIndexPath* indexPath) {
    return [weakSelf headerForSectionAtIndexPath:indexPath];
  };
  collectionView.dataSource = diffableDataSource;

  GridSnapshot* snapshot = [[GridSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kGridOpenTabsSectionIdentifier ]];
  [diffableDataSource applySnapshotUsingReloadData:snapshot];

  // UICollectionViewDropPlaceholder uses a GridCell and needs the class to be
  // registered.
  [collectionView registerClass:[GridCell class]
      forCellWithReuseIdentifier:kCellIdentifier];
  [collectionView registerClass:[GroupGridCell class]
      forCellWithReuseIdentifier:kGroupCellIdentifier];

  collectionView.delegate = self;
  collectionView.backgroundView = [[UIView alloc] init];
  collectionView.backgroundColor = [UIColor clearColor];
  collectionView.backgroundView.accessibilityIdentifier =
      kGridBackgroundIdentifier;

  // CollectionView, in contrast to TableView, doesn’t inset the
  // cell content to the safe area guide by default. We will just manage the
  // collectionView contentInset manually to fit in the safe area instead.
  collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  collectionView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
  self.collectionView = collectionView;
  self.view = collectionView;

  // A single selection collection view's default behavior is to momentarily
  // deselect the selected cell on touch down then select the new cell on touch
  // up. In this tab grid, the selection ring should stay visible on the
  // selected cell on touch down. Multiple selection disables the deselection
  // behavior. Multiple selection will not actually be possible since
  // `-collectionView:shouldSelectItemAtIndexPath:` returns NO.
  collectionView.allowsMultipleSelection = YES;
  collectionView.dragDelegate = self;
  collectionView.dropDelegate = self;
  self.collectionView.dragInteractionEnabled =
      [self shouldEnableDrapAndDropInteraction];

  self.pointerInteractionCells = [NSHashTable<GridCell*> weakObjectsHashTable];

  [self updateTabsSectionHeaderType];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self contentWillAppearAnimated:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  // Dismisses the confirmation dialog for tab group if it's displayed.
  [self.tabGroupConfirmationHandler dismissTabGroupConfirmation];

  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.collectionView.collectionViewLayout invalidateLayout];
      }
      completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        if (ios::provider::IsRaccoonEnabled()) {
          for (UICollectionViewCell* cell in self.collectionView.visibleCells) {
            [self setHoverEffectToCell:cell];
          }
        }
        [self.collectionView setNeedsLayout];
        [self.collectionView layoutIfNeeded];
      }];
}

#pragma mark - Public

- (BOOL)isScrolledToTop {
  return IsScrollViewScrolledToTop(self.collectionView);
}

- (BOOL)isScrolledToBottom {
  return IsScrollViewScrolledToBottom(self.collectionView);
}

- (BOOL)isGridScrollsToTopEnabled {
  return self.collectionView.scrollsToTop;
}

- (void)setGridScrollsToTopEnabled:(BOOL)gridScrollsToTopEnabled {
  self.collectionView.scrollsToTop = gridScrollsToTopEnabled;
}

- (void)setEmptyStateView:(UIView<GridEmptyView>*)emptyStateView {
  if (_emptyStateView) {
    [_emptyStateView removeFromSuperview];
  }
  _emptyStateView = emptyStateView;
  emptyStateView.scrollViewContentInsets =
      self.collectionView.adjustedContentInset;
  emptyStateView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.collectionView.backgroundView addSubview:emptyStateView];
  id<LayoutGuideProvider> safeAreaGuide =
      self.collectionView.backgroundView.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [self.collectionView.backgroundView.centerYAnchor
        constraintEqualToAnchor:emptyStateView.centerYAnchor],
    [safeAreaGuide.leadingAnchor
        constraintEqualToAnchor:emptyStateView.leadingAnchor],
    [safeAreaGuide.trailingAnchor
        constraintEqualToAnchor:emptyStateView.trailingAnchor],
    [emptyStateView.topAnchor
        constraintGreaterThanOrEqualToAnchor:safeAreaGuide.topAnchor],
    [emptyStateView.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeAreaGuide.bottomAnchor],
  ]];
}

- (BOOL)isGridEmpty {
  return [self numberOfTabs] == 0;
}

- (BOOL)isContainedGridEmpty {
  return YES;
}

- (void)setTabGridMode:(TabGridMode)mode {
  if (_mode == mode) {
    return;
  }

  TabGridMode previousMode = _mode;
  _mode = mode;

  self.collectionView.dragInteractionEnabled =
      [self shouldEnableDrapAndDropInteraction];
  self.emptyStateView.tabGridMode = _mode;

  if (mode == TabGridMode::kSearch && self.suggestedActionsDelegate) {
    if (!self.suggestedActionsViewController) {
      self.suggestedActionsViewController =
          [[SuggestedActionsViewController alloc] initWithDelegate:self];
    }
  }
  [self updateTabsSectionHeaderType];
  [self updateSuggestedActionsSection];

  // Reconfigure all tabs.
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  [self updateSnapshotForModeUpdate:snapshot];
  [snapshot reconfigureItemsWithIdentifiers:snapshot.itemIdentifiers];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
  [self.gridLayout invalidateLayout];

  NSUInteger selectedIndex = self.selectedIndex;
  if (previousMode != TabGridMode::kSelection && mode == TabGridMode::kNormal &&
      selectedIndex != NSNotFound &&
      static_cast<NSInteger>(selectedIndex) < [self numberOfTabs]) {
    // Scroll to the selected item here, so the action of reloading and
    // scrolling happens at once.
    [self.collectionView
        scrollToItemAtIndexPath:[self indexPathForTabIndex:selectedIndex]
               atScrollPosition:UICollectionViewScrollPositionTop
                       animated:NO];
  }

  if (mode == TabGridMode::kNormal) {
    // After transition from other modes to the normal mode, the selection
    // border doesn't show around the selected item, because reloading
    // operations lose the selected items. The collection view needs to be
    // updated with the selected item again for it to appear correctly.
    [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];

    self.searchText = nil;
  } else if (mode == TabGridMode::kSelection) {
    // The selected state is not visible in TabGridMode::kSelection mode, but
    // VoiceOver surfaces it. Deselects all the collection view items.
    // The selection will be reinstated when moving off of
    // TabGridMode::kSelection.
    NSArray<NSIndexPath*>* indexPathsForSelectedItems =
        [self.collectionView indexPathsForSelectedItems];
    for (NSIndexPath* itemIndexPath in indexPathsForSelectedItems) {
      [self.collectionView deselectItemAtIndexPath:itemIndexPath animated:NO];
    }
  }
}

- (void)setSearchText:(NSString*)searchText {
  _searchText = searchText;
  _suggestedActionsViewController.searchText = searchText;
  [self updateTabsSectionHeaderType];
  [self updateSuggestedActionsSection];
}

- (BOOL)isSelectedCellVisible {
  // The collection view's selected item may not have updated yet, so use the
  // selected index.
  NSUInteger selectedIndex = self.selectedIndex;
  if (selectedIndex == NSNotFound) {
    return NO;
  }
  NSIndexPath* selectedIndexPath = [self indexPathForTabIndex:selectedIndex];
  return [self.collectionView.indexPathsForVisibleItems
      containsObject:selectedIndexPath];
}

- (void)setContentInsets:(UIEdgeInsets)contentInsets {
  // Set the vertical insets on the collection view…
  self.collectionView.contentInset =
      UIEdgeInsetsMake(contentInsets.top, 0, contentInsets.bottom, 0);
  // … and the horizontal insets on the layout sections.
  // This is a workaround, as setting the horizontal insets on the collection
  // view isn't honored by the layout when computing the item sizes (items are
  // too big in landscape iPhones with a notch or Dynamic Island).
  self.gridLayout.sectionInsets = NSDirectionalEdgeInsetsMake(
      0, contentInsets.left, 0, contentInsets.right);
  _contentInsets = contentInsets;
}

- (LegacyGridTransitionLayout*)transitionLayout {
  [self.collectionView layoutIfNeeded];
  NSMutableArray<LegacyGridTransitionItem*>* items =
      [[NSMutableArray alloc] init];
  LegacyGridTransitionActiveItem* activeItem;
  LegacyGridTransitionItem* selectionItem;
  NSInteger tabSectionIndex = [self.diffableDataSource
      indexForSectionIdentifier:kGridOpenTabsSectionIdentifier];
  for (NSIndexPath* path in self.collectionView.indexPathsForVisibleItems) {
    if (path.section != tabSectionIndex) {
      continue;
    }
    UICollectionViewCell* collectionViewCell =
        [self.collectionView cellForItemAtIndexPath:path];
    if (![collectionViewCell isKindOfClass:[GridCell class]]) {
      // TODO(crbug.com/334885429): Update once the transition animation for the
      // group cells is available.
      continue;
    }
    GridCell* cell = ObjCCastStrict<GridCell>(collectionViewCell);
    UICollectionViewLayoutAttributes* attributes =
        [self.collectionView layoutAttributesForItemAtIndexPath:path];
    // Normalize frame to window coordinates. The attributes class applies this
    // change to the other properties such as center, bounds, etc.
    attributes.frame = [self.collectionView convertRect:attributes.frame
                                                 toView:nil];
    if ([cell.itemIdentifier isEqual:self.selectedItemIdentifier]) {
      GridTransitionCell* activeCell =
          [GridTransitionCell transitionCellFromCell:cell];
      activeItem =
          [LegacyGridTransitionActiveItem itemWithCell:activeCell
                                                center:attributes.center
                                                  size:attributes.size];
      // If the active item is the last inserted item, it needs to be animated
      // differently.
      if (cell.itemIdentifier.tabSwitcherItem.identifier ==
          self.lastInsertedItemID) {
        activeItem.isAppearing = YES;
      }
      selectionItem = [LegacyGridTransitionItem
          itemWithCell:[GridCell transitionSelectionCellFromCell:cell]
                center:attributes.center];
    } else {
      UIView* cellSnapshot = [cell snapshotViewAfterScreenUpdates:YES];
      LegacyGridTransitionItem* item =
          [LegacyGridTransitionItem itemWithCell:cellSnapshot
                                          center:attributes.center];
      [items addObject:item];
    }
  }
  return [LegacyGridTransitionLayout layoutWithInactiveItems:items
                                                  activeItem:activeItem
                                               selectionItem:selectionItem];
}

- (TabGridTransitionItem*)transitionItemForActiveCell {
  [self.collectionView layoutIfNeeded];

  NSIndexPath* selectedItemIndexPath =
      [self indexPathForTabIndex:self.selectedIndex];
  if (![self.collectionView.indexPathsForVisibleItems
          containsObject:selectedItemIndexPath]) {
    return nil;
  }
  UICollectionViewCell* collectionViewCell =
      [self.collectionView cellForItemAtIndexPath:selectedItemIndexPath];
  if ([collectionViewCell isKindOfClass:[GroupGridCell class]]) {
    // TODO(crbug.com/40942154): Handle once the annimations are available for
    // group cells.
    return nil;
  }
  GridCell* cell = ObjCCastStrict<GridCell>(collectionViewCell);

  UICollectionViewLayoutAttributes* attributes = [self.collectionView
      layoutAttributesForItemAtIndexPath:selectedItemIndexPath];

  // Removes the cell header height from the orignal frame.
  CGRect attributesFrame = attributes.frame;
  attributesFrame.origin.y += kGridCellHeaderHeight;
  attributesFrame.size.height -= kGridCellHeaderHeight;

  // Normalize frame to window coordinates. The attributes class applies this
  // change to the other properties such as center, bounds, etc.
  CGRect frameInWindow = [self.collectionView convertRect:attributesFrame
                                                   toView:nil];

  return [TabGridTransitionItem itemWithView:cell originalFrame:frameInWindow];
}

- (void)contentWillAppearAnimated:(BOOL)animated {
  self.gridLayout.animatesItemUpdates = YES;
  // Selection is invalid if there are no items.
  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
    return;
  }

  [self updateSelectedCollectionViewItemRingAndBringIntoView:YES];

  [self removeEmptyStateAnimated:NO];
  self.lastInsertedItemID = web::WebStateID();
}

- (void)prepareForDismissal {
  // Stop animating the collection view to prevent the insertion animation from
  // interfering with the tab presentation animation.
  self.gridLayout.animatesItemUpdates = NO;
}

- (void)centerVisibleCellsToPoint:(CGPoint)center
            translationCompletion:(CGFloat)translationCompletion
                        withScale:(CGFloat)scale {
  // Make sure to layout the collection view to ensure that the correct cells
  // are displayed.
  [self.collectionView layoutIfNeeded];

  for (UIView* cell in self.collectionView.visibleCells) {
    CGPoint transformedOrigin = [self.collectionView convertPoint:center
                                                         fromView:self.view];
    CGFloat dX =
        (transformedOrigin.x - cell.center.x) * (1 - translationCompletion);
    CGFloat dY =
        (transformedOrigin.y - cell.center.y) * (1 - translationCompletion);
    CGAffineTransform transform = CGAffineTransformMakeTranslation(dX, dY);
    transform = CGAffineTransformScale(transform, scale, scale);
    cell.transform = transform;
  }
}

- (void)resetVisibleCellsCenterAndScale {
  for (UIView* cell in self.collectionView.visibleCells) {
    cell.transform = CGAffineTransformIdentity;
  }
}

#pragma mark - UICollectionView Diffable Data Source Helpers

// Configures the grid header for the given section.
- (void)configureGridHeader:(GridHeader*)gridHeader
       forSectionIdentifier:(NSString*)sectionIdentifier {
  if ([sectionIdentifier isEqualToString:kGridOpenTabsSectionIdentifier]) {
    gridHeader.title = l10n_util::GetNSString(
        IDS_IOS_TABS_SEARCH_OPEN_TABS_SECTION_HEADER_TITLE);
    NSString* resultsCount = [@([self numberOfTabs]) stringValue];
    gridHeader.value =
        l10n_util::GetNSStringF(IDS_IOS_TABS_SEARCH_OPEN_TABS_COUNT,
                                base::SysNSStringToUTF16(resultsCount));
  } else if ([sectionIdentifier
                 isEqualToString:kSuggestedActionsSectionIdentifier]) {
    gridHeader.title =
        l10n_util::GetNSString(IDS_IOS_TABS_SEARCH_SUGGESTED_ACTIONS);
  }
}

#pragma mark - UICollectionViewDelegate

// Selection events will be signaled through the model layer and handled in
// the TabCollectionConsumer -selectItemWithID: method.
- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

// Selection events will be signaled through the model layer and handled in
// the TabCollectionConsumer -selectItemWithID: method.
- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldDeselectItemAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

- (void)collectionView:(UICollectionView*)collectionView
    performPrimaryActionForItemAtIndexPath:(NSIndexPath*)indexPath {
  [self tappedItemAtIndexPath:indexPath];
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemsAtIndexPaths:
        (NSArray<NSIndexPath*>*)indexPaths
                                           point:(CGPoint)point {
  if (indexPaths.count != 1) {
    return nil;
  }

  NSIndexPath* indexPath = indexPaths[0];

  // Context menu shouldn't appear in the selection mode.
  if (_mode == TabGridMode::kSelection) {
    return nil;
  }

  NSString* sectionIdentifier =
      [self.diffableDataSource sectionIdentifierForIndex:indexPath.section];
  // No context menu on suggested actions or inactive tabs section.
  if ([sectionIdentifier isEqualToString:kSuggestedActionsSectionIdentifier] ||
      [sectionIdentifier isEqualToString:kInactiveTabButtonSectionIdentifier]) {
    return nil;
  }

  [self.delegate gridViewControllerDidRequestContextMenu:self];

  UICollectionViewCell* collectionViewCell =
      [self.collectionView cellForItemAtIndexPath:indexPath];

  // GroupGridCell case.
  if ([collectionViewCell isKindOfClass:[GroupGridCell class]]) {
    return [self.menuProvider
        contextMenuConfigurationForTabGroupCell:ObjCCastStrict<GroupGridCell>(
                                                    collectionViewCell)
                                   menuScenario:
                                       kMenuScenarioHistogramTabGroupGridEntry];
  }

  // GridCell case.
  GridCell* cell = ObjCCastStrict<GridCell>(collectionViewCell);

  MenuScenarioHistogram scenario = [self scenarioForContextMenu];

  return [self.menuProvider contextMenuConfigurationForTabCell:cell
                                                  menuScenario:scenario];
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndDisplayingCell:(UICollectionViewCell*)cell
      forItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell isKindOfClass:[GridCell class]]) {
    // Stop animation of GridCells when removing them from the collection view.
    // This is important to prevent cells from animating indefinitely. This is
    // safe because the animation state of GridCells is set in
    // `configureCell:withItem:atIndex:` whenever a cell is used.
    [ObjCCastStrict<GridCell>(cell) hideActivityIndicator];
  }
}

#pragma mark - UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  UIPointerLiftEffect* effect = [UIPointerLiftEffect
      effectWithPreview:[[UITargetedPreview alloc]
                            initWithView:interaction.view]];
  return [UIPointerStyle styleWithEffect:effect shape:nil];
}

#pragma mark - UICollectionViewDragDelegate

- (void)collectionView:(UICollectionView*)collectionView
    dragSessionWillBegin:(id<UIDragSession>)session {
  self.dragEndAtNewIndex = NO;
  self.localDragActionInProgress = YES;

  if (!_draggedItemIdentifier) {
    CHECK_EQ(_mode, TabGridMode::kSelection);
    base::UmaHistogramEnumeration(kUmaGridViewDragDropMultiSelectEvent,
                                  DragDropItem::kDragBegin);
    [self.delegate gridViewControllerDragSessionWillBeginForTab:self];
    return;
  }
  switch (_draggedItemIdentifier.type) {
    case GridItemType::kInactiveTabsButton:
      NOTREACHED();
    case GridItemType::kTab: {
      base::UmaHistogramEnumeration(kUmaGridViewDragDropTabsEvent,
                                    DragDropItem::kDragBegin);
      [self.delegate gridViewControllerDragSessionWillBeginForTab:self];
      break;
    }
    case GridItemType::kGroup: {
      base::UmaHistogramEnumeration(kUmaGridViewDragDropGroupsEvent,
                                    DragDropItem::kDragBegin);
      [self.delegate gridViewControllerDragSessionWillBeginForTabGroup:self];
      break;
    }
    case GridItemType::kSuggestedActions:
      NOTREACHED();
  }
}

- (void)collectionView:(UICollectionView*)collectionView
     dragSessionDidEnd:(id<UIDragSession>)session {
  self.localDragActionInProgress = NO;

  DragDropItem dragEvent = self.dragEndAtNewIndex
                               ? DragDropItem::kDragEndAtNewIndex
                               : DragDropItem::kDragEndAtSameIndex;
  // If a drop animation is in progress and the drag didn't end at a new index,
  // that means the item has been dropped outside of its collection view.
  if (_dropAnimationInProgress && !_dragEndAtNewIndex) {
    dragEvent = DragDropItem::kDragEndInOtherCollection;
  }

  if (_draggedItemIdentifier) {
    switch (_draggedItemIdentifier.type) {
      case GridItemType::kInactiveTabsButton:
        NOTREACHED();
      case GridItemType::kTab:
        base::UmaHistogramEnumeration(kUmaGridViewDragDropTabsEvent, dragEvent);
        break;
      case GridItemType::kGroup:
        base::UmaHistogramEnumeration(kUmaGridViewDragDropGroupsEvent,
                                      dragEvent);
        break;
      case GridItemType::kSuggestedActions:
        NOTREACHED();
    }
  } else {
    base::UmaHistogramEnumeration(kUmaGridViewDragDropMultiSelectEvent,
                                  dragEvent);
  }

  // Used to let the Taptic Engine return to its idle state.
  // To preserve power, the Taptic Engine remains in a prepared state for only a
  // short period of time (on the order of seconds). If for some reason the
  // interactive move / reordering session is not completely finished, the
  // unfinished `UIFeedbackGenerator` may result in a crash.
  [self.collectionView endInteractiveMovement];

  [self.delegate gridViewControllerDragSessionDidEnd:self];
  [self.dragDropHandler dragSessionDidEnd];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  if (self.dragDropHandler == nil) {
    // Don't support dragging items if the drag&drop handler is not set.
    return @[];
  }
  if (_mode == TabGridMode::kSearch) {
    // TODO(crbug.com/40824160): Enable dragging items from search results.
    return @[];
  }
  NSString* sectionIdentifier =
      [self.diffableDataSource sectionIdentifierForIndex:indexPath.section];
  if ([sectionIdentifier isEqualToString:kSuggestedActionsSectionIdentifier] ||
      [sectionIdentifier isEqualToString:kInactiveTabButtonSectionIdentifier]) {
    // Return an empty array because the suggested actions cell or the inactive
    // tabs button should not be dragged.
    return @[];
  }
  GridItemIdentifier* draggedItem =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];
  if (_mode != TabGridMode::kSelection) {
    UIDragItem* dragItem;
    _draggedItemIdentifier = draggedItem;
    switch (_draggedItemIdentifier.type) {
      case GridItemType::kInactiveTabsButton:
        NOTREACHED();
      case GridItemType::kTab:
        dragItem = [self.dragDropHandler
            dragItemForItem:_draggedItemIdentifier.tabSwitcherItem];
        break;

      case GridItemType::kGroup:
        dragItem = [self.dragDropHandler
            dragItemForTabGroupItem:_draggedItemIdentifier.tabGroupItem];
        break;
      case GridItemType::kSuggestedActions:
        NOTREACHED();
    }
    if (!dragItem) {
      return @[];
    }
    return @[ dragItem ];
  }

  // Make sure that the long pressed cell is selected before initiating a drag
  // from it.
  [self.mutator addToSelectionItemID:draggedItem];
  [self reconfigureItem:draggedItem];
  return [self.dragDropHandler allSelectedDragItems];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
            itemsForAddingToDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath
                                  point:(CGPoint)point {
  // TODO(crbug.com/40695113): Allow multi-select.
  // Prevent more items from getting added to the drag session.
  return @[];
}

- (UIDragPreviewParameters*)collectionView:(UICollectionView*)collectionView
    dragPreviewParametersForItemAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section ==
      [self.diffableDataSource
          indexForSectionIdentifier:kSuggestedActionsSectionIdentifier]) {
    // Return nil so that the suggested actions cell doesn't superpose the
    // dragged cell.
    return nil;
  }

  UICollectionViewCell* collectionViewCell =
      [self.collectionView cellForItemAtIndexPath:indexPath];
  if ([collectionViewCell isKindOfClass:[GroupGridCell class]]) {
    return nil;
  }
  GridCell* gridCell = ObjCCastStrict<GridCell>(collectionViewCell);
  return gridCell.dragPreviewParameters;
}

#pragma mark - UICollectionViewDropDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    canHandleDropSession:(id<UIDropSession>)session {
  if (self.dragDropHandler == nil) {
    // Don't support dropping items if the drag&drop handler is not set.
    return NO;
  }
  // Prevent dropping tabs into grid while displaying search results.
  return (_mode != TabGridMode::kSearch);
}

- (UICollectionViewDropProposal*)
              collectionView:(UICollectionView*)collectionView
        dropSessionDidUpdate:(id<UIDropSession>)session
    withDestinationIndexPath:(NSIndexPath*)destinationIndexPath {
  if ([[self.diffableDataSource
          sectionIdentifierForIndex:destinationIndexPath.section]
          isEqualToString:kInactiveTabButtonSectionIdentifier]) {
    // Disallow dropping in the inactive tab section.
    return [[UICollectionViewDropProposal alloc]
        initWithDropOperation:UIDropOperationForbidden
                       intent:UICollectionViewDropIntentUnspecified];
  }
  // This is how the explicit forbidden icon or (+) copy icon is shown. Move has
  // no explicit icon.
  UIDropOperation dropOperation = [self.dragDropHandler
      dropOperationForDropSession:session
                          toIndex:destinationIndexPath.item];
  return [[UICollectionViewDropProposal alloc]
      initWithDropOperation:dropOperation
                     intent:
                         UICollectionViewDropIntentInsertAtDestinationIndexPath];
}

- (void)collectionView:(UICollectionView*)collectionView
    performDropWithCoordinator:
        (id<UICollectionViewDropCoordinator>)coordinator {
  NSArray<id<UICollectionViewDropItem>>* items = coordinator.items;
  for (id<UICollectionViewDropItem> item in items) {
    // Append to the end of the collection, unless drop index is specified.
    // The sourceIndexPath is nil if the drop item is not from the same
    // collection view. Set the destinationIndex to reflect the addition of an
    // item.
    NSInteger numberOfTabs = [self numberOfTabs];
    NSUInteger destinationIndex =
        item.sourceIndexPath ? numberOfTabs - 1 : numberOfTabs;
    if (coordinator.destinationIndexPath) {
      destinationIndex =
          base::checked_cast<NSUInteger>(coordinator.destinationIndexPath.item);
    }
    self.dragEndAtNewIndex = YES;

    NSIndexPath* dropIndexPath = [self indexPathForTabIndex:destinationIndex];
    // Drop synchronously if local object is available.
    if (item.dragItem.localObject) {
      _dropAnimationInProgress = YES;
      [self.delegate gridViewControllerDropAnimationWillBegin:self];
      __weak __typeof(self) weakSelf = self;
      [[coordinator dropItem:item.dragItem toItemAtIndexPath:dropIndexPath]
          addCompletion:^(UIViewAnimatingPosition finalPosition) {
            [weakSelf.delegate gridViewControllerDropAnimationDidEnd:weakSelf];
            weakSelf.dropAnimationInProgress = NO;
          }];
      // The sourceIndexPath is non-nil if the drop item is from this same
      // collection view.
      [self.dragDropHandler dropItem:item.dragItem
                             toIndex:destinationIndex
                  fromSameCollection:(item.sourceIndexPath != nil)];
    } else {
      // Drop asynchronously if local object is not available.
      UICollectionViewDropPlaceholder* placeholder =
          [[UICollectionViewDropPlaceholder alloc]
              initWithInsertionIndexPath:dropIndexPath
                         reuseIdentifier:kCellIdentifier];
      placeholder.cellUpdateHandler = ^(UICollectionViewCell* placeholderCell) {
        GridCell* gridCell = ObjCCastStrict<GridCell>(placeholderCell);
        gridCell.theme = self.theme;
      };
      placeholder.previewParametersProvider =
          ^UIDragPreviewParameters*(UICollectionViewCell* placeholderCell) {
            GridCell* gridCell = ObjCCastStrict<GridCell>(placeholderCell);
            return gridCell.dragPreviewParameters;
          };

      id<UICollectionViewDropPlaceholderContext> context =
          [coordinator dropItem:item.dragItem toPlaceholder:placeholder];
      [self.dragDropHandler dropItemFromProvider:item.dragItem.itemProvider
                                         toIndex:destinationIndex
                              placeholderContext:context];
    }
  }
  [self.delegate gridViewControllerDragSessionDidEnd:self];
}

- (void)collectionView:(UICollectionView*)collectionView
    dropSessionDidEnter:(id<UIDropSession>)session {
  if (IsPinnedTabsEnabled()) {
    if (_draggedItemIdentifier &&
        _draggedItemIdentifier.type == GridItemType::kGroup) {
      // Don't notify the delegate if the dragged item is a local tab group.
      return;
    }
    // Notify the delegate that a drag cames from another app.
    [self.delegate gridViewControllerDragSessionWillBeginForTab:self];
  }
  if (!_localDragActionInProgress) {
    // Disable buttons toolbar if no items are dragged in the current collection
    // view.
    [self.delegate gridViewControllerDropSessionDidEnter:self];
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    dropSessionDidExit:(id<UIDropSession>)session {
  if (!_localDragActionInProgress) {
    // Enable back toolbar buttons if no items are dragged in the current
    // collection view.
    [self.delegate gridViewControllerDropSessionDidExit:self];
  }
}

- (void)collectionView:(UICollectionView*)collectionView
     dropSessionDidEnd:(id<UIDropSession>)session {
  if (IsPinnedTabsEnabled()) {
    // Notify the delegate that a drag ends from another app.
    [self.delegate gridViewControllerDropAnimationDidEnd:self];
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self.delegate gridViewControllerScrollViewDidScroll:self];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  base::RecordAction(base::UserMetricsAction("MobileTabGridUserScrolled"));
  _scopedScrollingTimeLogger = ScopedScrollingTimeLogger();
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  if (!decelerate) {
    _scopedScrollingTimeLogger.reset();
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  _scopedScrollingTimeLogger.reset();
}

- (void)scrollViewDidScrollToTop:(UIScrollView*)scrollView {
  base::RecordAction(base::UserMetricsAction("MobileTabGridUserScrolledToTop"));
}

- (void)scrollViewDidChangeAdjustedContentInset:(UIScrollView*)scrollView {
  self.emptyStateView.scrollViewContentInsets = scrollView.contentInset;
}

#pragma mark - GridCellDelegate

- (void)closeButtonTappedForCell:(GridCell*)cell {
  // Record when a tab is closed via the X.
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseControlTapped"));
  if (_mode == TabGridMode::kSearch) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridCloseControlTappedDuringSearch"));
  }
  [self.mutator closeItemWithIdentifier:cell.itemIdentifier];
}

#pragma mark - GroupGridCellDelegate

- (void)closeButtonTappedForGroupCell:(GroupGridCell*)cell {
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseTabGroupControlTapped"));
  [self.mutator closeItemWithIdentifier:cell.itemIdentifier];
}

#pragma mark - SuggestedActionsViewControllerDelegate

- (void)suggestedActionsViewController:
            (SuggestedActionsViewController*)viewController
    fetchHistoryResultsCountWithCompletion:(void (^)(size_t))completion {
  [self.suggestedActionsDelegate
      fetchSearchHistoryResultsCountForText:self.searchText
                                 completion:completion];
}

- (void)didSelectSearchHistoryInSuggestedActionsViewController:
    (SuggestedActionsViewController*)viewController {
  base::RecordAction(
      base::UserMetricsAction("TabsSearch.SuggestedActions.SearchHistory"));
  [self.tabGridHandler showHistoryForText:self.searchText];
}

- (void)didSelectSearchRecentTabsInSuggestedActionsViewController:
    (SuggestedActionsViewController*)viewController {
  base::RecordAction(
      base::UserMetricsAction("TabsSearch.SuggestedActions.RecentTabs"));
  [self.tabGridHandler showRecentTabsForText:self.searchText];
}

- (void)didSelectSearchWebInSuggestedActionsViewController:
    (SuggestedActionsViewController*)viewController {
  base::RecordAction(
      base::UserMetricsAction("TabsSearch.SuggestedActions.SearchOnWeb"));
  [self.tabGridHandler showWebSearchForText:self.searchText];
}

#pragma mark - TabCollectionConsumer

- (void)populateItems:(NSArray<GridItemIdentifier*>*)items
    selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  CHECK(!HasDuplicateGroupsAndTabsIdentifiers(items));
  // Call self.view to ensure that the collection view is created.
  [self view];
  CHECK(self.diffableDataSource);

  self.selectedItemIdentifier = selectedItemIdentifier;

  GridSnapshot* snapshot = [[GridSnapshot alloc] init];

  // Open Tabs section.
  [snapshot appendSectionsWithIdentifiers:@[ kGridOpenTabsSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:items];

  // Give subclasses the opportunity to contribute to the snapshot.
  [self addAdditionalItemsToSnapshot:snapshot];

  // Optional Suggested Actions section.
  if (self.showingSuggestedActions) {
    [snapshot
        appendSectionsWithIdentifiers:@[ kSuggestedActionsSectionIdentifier ]];
    GridItemIdentifier* itemIdentifier =
        [GridItemIdentifier suggestedActionsIdentifier];
    [snapshot appendItemsWithIdentifiers:@[ itemIdentifier ]];
  }

  [snapshot reconfigureItemsWithIdentifiers:items];
  [self.diffableDataSource applySnapshot:snapshot
                    animatingDifferences:YES
                              completion:nil];

  [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
  [self updateVisibleCellIdentifiers];

  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
  } else {
    [self removeEmptyStateAnimated:YES];
  }

  if (_mode == TabGridMode::kSearch) {
    if (_searchText.length) {
      [self updateSearchResultsHeader];
    }
    [self.collectionView
        setContentOffset:CGPointMake(
                             -self.collectionView.adjustedContentInset.left,
                             -self.collectionView.adjustedContentInset.top)
                animated:NO];
  }
}

- (void)insertItem:(GridItemIdentifier*)item
              beforeItemID:(GridItemIdentifier*)nextItemIdentifier
    selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  if (_mode == TabGridMode::kSearch) {
    // Prevent inserting items while viewing search results.
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self
      performModelAndViewUpdates:^(GridSnapshot* snapshot) {
        [weakSelf
            applyModelAndViewUpdatesForInsertionOfItem:item
                                          beforeItemID:nextItemIdentifier
                                selectedItemIdentifier:selectedItemIdentifier
                                              snapshot:snapshot];
      }
      completion:^{
        [weakSelf
            modelAndViewUpdatesForInsertionDidCompleteForItemIdentifier:item];
      }];
}

- (void)removeItemWithIdentifier:(GridItemIdentifier*)removedItem
          selectedItemIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  NSIndexPath* removedItemIndexPath =
      [self.diffableDataSource indexPathForItemIdentifier:removedItem];

  // Do not remove if not showing the item (i.e. showing search results).
  if (!removedItemIndexPath) {
    [self selectItemWithIdentifier:selectedItemIdentifier];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self
      performModelAndViewUpdates:^(GridSnapshot* snapshot) {
        [weakSelf applyModelAndViewUpdatesForRemovalOfItemWithID:removedItem
                                          selectedItemIdentifier:
                                              selectedItemIdentifier
                                                        snapshot:snapshot];
      }
      completion:^{
        [weakSelf modelAndViewUpdatesForRemovalDidCompleteForItemWithID:
                      removedItem.tabSwitcherItem.identifier];
      }];

  if (_mode == TabGridMode::kSearch && _searchText.length) {
    [self updateSearchResultsHeader];
  }
}

- (void)selectItemWithIdentifier:(GridItemIdentifier*)selectedItemIdentifier {
  if ([self.selectedItemIdentifier isEqual:selectedItemIdentifier]) {
    return;
  }

  self.selectedItemIdentifier = selectedItemIdentifier;
  [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
}

- (void)replaceItem:(GridItemIdentifier*)item
    withReplacementItem:(GridItemIdentifier*)replacementItem {
  CHECK((item.type != GridItemType::kSuggestedActions) &&
        (replacementItem.type != GridItemType::kSuggestedActions));

  NSIndexPath* existingItemIndexPath =
      [self.diffableDataSource indexPathForItemIdentifier:item];

  if (!existingItemIndexPath) {
    return;
  }

  BOOL replacementItemIsEqualToItem = [replacementItem isEqual:item];

  // Consistency check: `replacementItem` is either equal to item or not in the
  // collection view.
  CHECK(replacementItemIsEqualToItem ||
        ![self.diffableDataSource indexPathForItemIdentifier:replacementItem]);

  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  if (replacementItemIsEqualToItem) {
    [snapshot reconfigureItemsWithIdentifiers:@[ item ]];
  } else {
    // Add the new item before the existing item.
    [snapshot insertItemsWithIdentifiers:@[ replacementItem ]
                beforeItemWithIdentifier:item];
    [snapshot deleteItemsWithIdentifiers:@[ item ]];
  }
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

- (void)moveItem:(GridItemIdentifier*)item
      beforeItem:(GridItemIdentifier*)nextItemIdentifier {
  if (_mode == TabGridMode::kSearch) {
    // Prevent moving items while viewing search results.
    return;
  }

  if (!item) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self
      performModelAndViewUpdates:^(GridSnapshot* snapshot) {
        [weakSelf applyModelAndViewUpdatesForMoveOfItem:item
                                             beforeItem:nextItemIdentifier
                                               snapshot:snapshot];
      }
      completion:^{
        [weakSelf modelAndViewUpdatesForMoveDidComplete];
      }];
}

- (void)bringItemIntoView:(GridItemIdentifier*)item animated:(BOOL)animated {
  NSIndexPath* indexPath =
      [self.diffableDataSource indexPathForItemIdentifier:item];
  [self.collectionView
      scrollToItemAtIndexPath:indexPath
             atScrollPosition:UICollectionViewScrollPositionCenteredVertically
                     animated:animated];
}

- (void)dismissModals {
  ios::provider::DismissModalsForCollectionView(self.collectionView);
}

- (void)reload {
  [self.collectionView reloadData];
}

- (void)willCloseAll {
  self.isClosingAllOrUndoRunning = YES;
}

- (void)didCloseAll {
  self.isClosingAllOrUndoRunning = NO;
  [self updateTabsSectionHeaderType];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)willUndoCloseAll {
  self.isClosingAllOrUndoRunning = YES;
}

- (void)didUndoCloseAll {
  self.isClosingAllOrUndoRunning = NO;
  [self updateTabsSectionHeaderType];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark - Suggested Actions Section

- (void)updateSuggestedActionsSection {
  if (!self.suggestedActionsDelegate) {
    return;
  }

  // In search mode if there is already a search query, and the suggested
  // actions section is not yet added, add it. Otherwise remove the section if
  // it exists and the search mode is not active.
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  if (self.mode == TabGridMode::kSearch && self.searchText.length) {
    if (!self.showingSuggestedActions) {
      [snapshot appendSectionsWithIdentifiers:@[
        kSuggestedActionsSectionIdentifier
      ]];
      GridItemIdentifier* itemIdentifier =
          [GridItemIdentifier suggestedActionsIdentifier];
      [snapshot appendItemsWithIdentifiers:@[ itemIdentifier ]];

      self.showingSuggestedActions = YES;
    }
  } else {
    if (self.showingSuggestedActions) {
      [snapshot deleteSectionsWithIdentifiers:@[
        kSuggestedActionsSectionIdentifier
      ]];
      self.showingSuggestedActions = NO;
    }
  }
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - Private helpers for joint model and view updates

// Performs model and view updates together.
- (void)performModelAndViewUpdates:
            (void (^)(GridSnapshot* snapshot))modelAndViewUpdates
                        completion:(ProceduralBlock)completion {
  self.updating = YES;
  // Synchronize model and diffable snapshot updates.
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  modelAndViewUpdates(snapshot);
  __weak __typeof(self) weakSelf = self;
  [self.diffableDataSource applySnapshot:snapshot
                    animatingDifferences:YES
                              completion:^{
                                if (weakSelf) {
                                  completion();
                                  weakSelf.updating = NO;
                                }
                              }];

  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
  } else {
    [self removeEmptyStateAnimated:YES];
  }

  [self updateVisibleCellIdentifiers];
}

// Makes the required changes to the data source when a new item is inserted
// before the given `nextItemIdentifier`. If `nextItemIdentifier` is nil,
// `item` is append at the end of the section.
- (void)applyModelAndViewUpdatesForInsertionOfItem:(GridItemIdentifier*)item
                                      beforeItemID:(GridItemIdentifier*)
                                                       nextItemIdentifier
                            selectedItemIdentifier:
                                (GridItemIdentifier*)selectedItemIdentifier
                                          snapshot:(GridSnapshot*)snapshot {
  CHECK(item.type == GridItemType::kTab || item.type == GridItemType::kGroup);
  // TODO(crbug.com/40069795): There are crash reports that show there could be
  // cases where the open tabs section is not present in the snapshot. If so,
  // don't perform the update.
  NSInteger section =
      [snapshot indexOfSectionIdentifier:kGridOpenTabsSectionIdentifier];
  DUMP_WILL_BE_CHECK(section != NSNotFound)
      << base::SysNSStringToUTF8([snapshot description]);
  if (section == NSNotFound) {
    return;
  }

  // Consistency check: `item`'s ID is not in the collection view.
  CHECK(![self.diffableDataSource indexPathForItemIdentifier:item]);

  self.selectedItemIdentifier = selectedItemIdentifier;
  if (item.type == GridItemType::kTab) {
    self.lastInsertedItemID = item.tabSwitcherItem.identifier;
  } else if (item.type == GridItemType::kGroup) {
    self.lastInsertedItemID = web::WebStateID();
  }

  if (nextItemIdentifier) {
    [snapshot insertItemsWithIdentifiers:@[ item ]
                beforeItemWithIdentifier:nextItemIdentifier];
  } else {
    [snapshot appendItemsWithIdentifiers:@[ item ]
               intoSectionWithIdentifier:kGridOpenTabsSectionIdentifier];
  }
}

// Makes the required changes when a new item has been inserted.
- (void)modelAndViewUpdatesForInsertionDidCompleteForItemIdentifier:
    (GridItemIdentifier*)item {
  [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
}

// Makes the required changes to the data source when an existing item is
// removed.
- (void)applyModelAndViewUpdatesForRemovalOfItemWithID:
            (GridItemIdentifier*)removedItemIdentifier
                                selectedItemIdentifier:
                                    (GridItemIdentifier*)selectedItemIdentifier
                                              snapshot:(GridSnapshot*)snapshot {
  self.selectedItemIdentifier = selectedItemIdentifier;
  [self.mutator removeFromSelectionItemID:removedItemIdentifier];

  [snapshot deleteItemsWithIdentifiers:@[ removedItemIdentifier ]];
}

// Makes the required changes when a new item has been removed.
- (void)modelAndViewUpdatesForRemovalDidCompleteForItemWithID:
    (web::WebStateID)removedItemID {
  NSInteger numberOfTabs = [self numberOfTabs];
  if (numberOfTabs > 0) {
    [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
  }
  [self.delegate gridViewController:self didRemoveItemWithID:removedItemID];
}

// Makes the required changes to the data source when an existing item is moved.
- (void)applyModelAndViewUpdatesForMoveOfItem:(GridItemIdentifier*)item
                                   beforeItem:
                                       (GridItemIdentifier*)nextItemIdentifier
                                     snapshot:(GridSnapshot*)snapshot {
  if (nextItemIdentifier) {
    [snapshot moveItemWithIdentifier:item
            beforeItemWithIdentifier:nextItemIdentifier];
  } else {
    NSInteger section = [self.diffableDataSource
        indexForSectionIdentifier:kGridOpenTabsSectionIdentifier];
    NSIndexPath* lastIndexPath =
        [NSIndexPath indexPathForItem:[self numberOfTabs] - 1
                            inSection:section];
    GridItemIdentifier* lastItem =
        [self.diffableDataSource itemIdentifierForIndexPath:lastIndexPath];
    if (lastItem == item) {
      return;
    }

    // If the moved item was pinned, it does not belong to the collection view
    // yet.
    if ([snapshot indexOfItemIdentifier:item] == NSNotFound) {
      [snapshot insertItemsWithIdentifiers:@[ item ]
                   afterItemWithIdentifier:lastItem];
    } else {
      [snapshot moveItemWithIdentifier:item afterItemWithIdentifier:lastItem];
    }
  }
}

// Makes the required changes when an item has been moved.
- (void)modelAndViewUpdatesForMoveDidComplete {
  [self.delegate gridViewControllerDidMoveItem:self];
}

#pragma mark - Private properties

- (NSUInteger)selectedIndex {
  if (!self.selectedItemIdentifier) {
    return NSNotFound;
  }
  NSIndexPath* selectedIndexPath = [self.diffableDataSource
      indexPathForItemIdentifier:self.selectedItemIdentifier];
  if (selectedIndexPath) {
    return selectedIndexPath.item;
  }
  return NSNotFound;
}

#pragma mark - Protected

- (TabGridMode)mode {
  return _mode;
}

- (void)createRegistrations {
  __weak __typeof(self) weakSelf = self;

  // Register GridCell.
  auto configureGridCell =
      ^(GridCell* cell, NSIndexPath* indexPath, TabSwitcherItem* item) {
        [weakSelf configureCell:cell withItem:item atIndex:indexPath.item];
      };
  _gridCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:GridCell.class
           configurationHandler:configureGridCell];

  // Register GroupGridCell.
  auto configureGroupGridCell =
      ^(GroupGridCell* cell, NSIndexPath* indexPath, TabGroupItem* item) {
        [weakSelf configureGroupCell:cell withItem:item atIndex:indexPath.item];
      };
  _groupGridCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:GroupGridCell.class
           configurationHandler:configureGroupGridCell];

  // Register SuggestedActionsGridCell.
  auto configureSuggestedActionsCell =
      ^(SuggestedActionsGridCell* suggestedActionsCell, NSIndexPath* indexPath,
        id item) {
        CHECK(weakSelf.suggestedActionsViewController);
        suggestedActionsCell.suggestedActionsView =
            weakSelf.suggestedActionsViewController.view;
      };
  _suggestedActionsCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:SuggestedActionsGridCell.class
           configurationHandler:configureSuggestedActionsCell];

  // Register GridHeader.
  auto configureGridHeader =
      ^(GridHeader* gridHeader, NSString* elementKind, NSIndexPath* indexPath) {
        NSString* sectionIdentifier = [weakSelf.diffableDataSource
            sectionIdentifierForIndex:indexPath.section];
        [weakSelf configureGridHeader:gridHeader
                 forSectionIdentifier:sectionIdentifier];
      };
  _gridHeaderRegistration = [UICollectionViewSupplementaryRegistration
      registrationWithSupplementaryClass:[GridHeader class]
                             elementKind:UICollectionElementKindSectionHeader
                    configurationHandler:configureGridHeader];
}

- (UICollectionReusableView*)headerForSectionAtIndexPath:
    (NSIndexPath*)indexPath {
  UICollectionViewSupplementaryRegistration* registration;
  switch (_mode) {
    case TabGridMode::kNormal:
      if (IsInactiveTabButtonRefactoringEnabled()) {
        return nil;
      } else {
        NOTREACHED() << "Should be implemented in a subclass.";
      }
    case TabGridMode::kSelection:
      NOTREACHED() << "Should not happen.";
    case TabGridMode::kSearch:
      registration = _gridHeaderRegistration;
      break;
  }
  return [self.collectionView
      dequeueConfiguredReusableSupplementaryViewWithRegistration:registration
                                                    forIndexPath:indexPath];
}

- (UICollectionViewCell*)cellForItemAtIndexPath:(NSIndexPath*)indexPath
                                 itemIdentifier:
                                     (GridItemIdentifier*)itemIdentifier {
  switch (itemIdentifier.type) {
    case GridItemType::kInactiveTabsButton:
      // Must be handled in the subclasses.
      NOTREACHED();
    case GridItemType::kTab: {
      UICollectionViewCellRegistration* registration = _gridCellRegistration;
      return [self.collectionView
          dequeueConfiguredReusableCellWithRegistration:registration
                                           forIndexPath:indexPath
                                                   item:itemIdentifier
                                                            .tabSwitcherItem];
    }
    case GridItemType::kGroup: {
      UICollectionViewCellRegistration* registration =
          _groupGridCellRegistration;
      return [self.collectionView
          dequeueConfiguredReusableCellWithRegistration:registration
                                           forIndexPath:indexPath
                                                   item:itemIdentifier
                                                            .tabGroupItem];
    }
    case GridItemType::kSuggestedActions:
      UICollectionViewCellRegistration* registration =
          _suggestedActionsCellRegistration;
      return [self.collectionView
          dequeueConfiguredReusableCellWithRegistration:registration
                                           forIndexPath:indexPath
                                                   item:itemIdentifier];
  }
}

- (void)updateSelectedCollectionViewItemRingAndBringIntoView:
    (BOOL)shouldBringItemIntoView {
  // Deselects all the collection view items.
  NSArray<NSIndexPath*>* indexPathsForSelectedItems =
      [self.collectionView indexPathsForSelectedItems];
  for (NSIndexPath* itemIndexPath in indexPathsForSelectedItems) {
    [self.collectionView deselectItemAtIndexPath:itemIndexPath animated:NO];
  }

  // Select the collection view item for the selected index.
  NSInteger selectedIndex = self.selectedIndex;
  CHECK(selectedIndex >= 0);
  // Check `selectedIndex` boundaries in order to filter out possible race
  // conditions while mutating the collection.
  if (selectedIndex == NSNotFound ||
      selectedIndex >= [self numberOfTabs]) {
    return;
  }
  NSIndexPath* selectedIndexPath = [self indexPathForTabIndex:selectedIndex];
  UICollectionViewScrollPosition scrollPosition =
      shouldBringItemIntoView ? UICollectionViewScrollPositionTop
                              : UICollectionViewScrollPositionNone;
  [self.collectionView selectItemAtIndexPath:selectedIndexPath
                                    animated:NO
                              scrollPosition:scrollPosition];
}

- (void)updateTabsSectionHeaderType {
  self.gridLayout.tabsSectionHeaderType =
      [self tabsSectionHeaderTypeForMode:_mode];
  [self.gridLayout invalidateLayout];
}

- (TabsSectionHeaderType)tabsSectionHeaderTypeForMode:(TabGridMode)mode {
  switch (mode) {
    case TabGridMode::kNormal:
    case TabGridMode::kSelection:
      return TabsSectionHeaderType::kNone;
    case TabGridMode::kSearch:
      if (_searchText.length == 0) {
        return TabsSectionHeaderType::kNone;
      }
      return TabsSectionHeaderType::kSearch;
  }
}

- (void)addAdditionalItemsToSnapshot:(GridSnapshot*)snapshot {
  // Base class implementation is doing nothing.
}

- (void)updateSnapshotForModeUpdate:(GridSnapshot*)snapshot {
  // Base class implementation is doing nothing.
}

- (MenuScenarioHistogram)scenarioForContextMenu {
  switch (_mode) {
    case TabGridMode::kSearch:
      return kMenuScenarioHistogramTabGridSearchResult;
    case TabGridMode::kNormal:
    case TabGridMode::kSelection:
      return kMenuScenarioHistogramTabGridEntry;
  }
}

#pragma mark - Private

- (void)voiceOverStatusDidChange {
  self.collectionView.dragInteractionEnabled =
      [self shouldEnableDrapAndDropInteraction];
}

- (void)preferredContentSizeCategoryDidChange {
  [self.collectionView.collectionViewLayout invalidateLayout];
}

// Returns YES if drag and drop is enabled.
// TODO(crbug.com/40824160): Enable dragging items from search results.
- (BOOL)shouldEnableDrapAndDropInteraction {
  // Don't enable drag and drop when voice over is enabled.
  return !UIAccessibilityIsVoiceOverRunning()
         // Dragging multiple tabs to reorder them is not supported. So there is
         // no need to enable dragging when multiple items are selected in
         // devices that don't support multiple windows.
         && ((self.mode == TabGridMode::kSelection &&
              base::ios::IsMultipleScenesSupported()) ||
             self.mode == TabGridMode::kNormal);
}

// Configures `groupCell`'s identifier and title synchronously, and pass the
// list of `GroupTabInfo`asynchronously with information from `item`. Updates
// the `cell`'s theme to this view controller's theme. This view controller
// becomes the delegate for the cell.
- (void)configureGroupCell:(GroupGridCell*)cell
                  withItem:(TabGroupItem*)item
                   atIndex:(NSUInteger)index {
  CHECK(cell);
  CHECK(item);
  GridItemIdentifier* groupItemIdentifier =
      [[GridItemIdentifier alloc] initWithGroupItem:item];
  cell.delegate = self;
  cell.theme = self.theme;
  cell.itemIdentifier = groupItemIdentifier;
  cell.groupColor = item.groupColor;
  cell.tabsCount = item.numberOfTabsInGroup;
  cell.title = item.title;
  cell.accessibilityIdentifier = GroupGridCellAccessibilityIdentifier(index);
  if (self.mode == TabGridMode::kSelection) {
    if ([self.gridProvider isItemSelected:groupItemIdentifier]) {
      cell.state = GridCellStateEditingSelected;
    } else {
      cell.state = GridCellStateEditingUnselected;
    }
  } else {
    cell.state = GridCellStateNotEditing;
  }

  [item fetchGroupTabInfos:^(TabGroupItem* innerItem,
                             NSArray<GroupTabInfo*>* groupTabInfos) {
    if ([cell.itemIdentifier.tabGroupItem isEqual:innerItem]) {
      [cell configureWithGroupTabInfos:groupTabInfos
                        totalTabsCount:innerItem.numberOfTabsInGroup];
    }
  }];
}

// Configures `cell`'s identifier and title synchronously, and favicon and
// snapshot asynchronously with information from `item`. Updates the `cell`'s
// theme to this view controller's theme. This view controller becomes the
// delegate for the cell.
- (void)configureCell:(GridCell*)cell
             withItem:(TabSwitcherItem*)item
              atIndex:(NSUInteger)index {
  CHECK(cell);
  CHECK(item);
  GridItemIdentifier* itemIdentifier =
      [[GridItemIdentifier alloc] initWithTabItem:item];
  cell.delegate = self;
  cell.theme = self.theme;
  cell.itemIdentifier = itemIdentifier;
  cell.title = item.title;
  cell.titleHidden = item.hidesTitle;
  cell.accessibilityIdentifier = GridCellAccessibilityIdentifier(index);
  if (self.mode == TabGridMode::kSelection) {
    if ([self.gridProvider isItemSelected:itemIdentifier]) {
      cell.state = GridCellStateEditingSelected;
    } else {
      cell.state = GridCellStateEditingUnselected;
    }
  } else {
    cell.state = GridCellStateNotEditing;
  }
  [item fetchFavicon:^(TabSwitcherItem* innerItem, UIImage* icon) {
    // Only update the icon if the cell is not already reused for another item.
    if ([cell.itemIdentifier.tabSwitcherItem isEqual:innerItem]) {
      cell.icon = icon;
    }
  }];

  [item fetchSnapshot:^(TabSwitcherItem* innerItem, UIImage* snapshot) {
    // Only update the icon if the cell is not already reused for another item.
    if ([cell.itemIdentifier.tabSwitcherItem isEqual:innerItem]) {
      cell.snapshot = snapshot;
    }
  }];

  web::WebStateID itemID = item.identifier;
  [self.priceCardDataSource
      priceCardForIdentifier:itemID
                  completion:^(PriceCardItem* priceCardItem) {
                    if (priceCardItem &&
                        cell.itemIdentifier.tabSwitcherItem.identifier ==
                            itemID) {
                      [cell setPriceDrop:priceCardItem.price
                           previousPrice:priceCardItem.previousPrice];
                    }
                  }];
  cell.opacity = 1.0f;
  if (item.showsActivity) {
    [cell showActivityIndicator];
  } else {
    [cell hideActivityIndicator];
  }
  if (![self.pointerInteractionCells containsObject:cell]) {
    [cell addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
    // `self.pointerInteractionCells` is only expected to get as large as the
    // number of reusable grid cells in memory.
    [self.pointerInteractionCells addObject:cell];
  }
  if (ios::provider::IsRaccoonEnabled()) {
    [self setHoverEffectToCell:cell];
  }
}

// Tells the delegate that the user tapped the item with identifier
// corresponding to `indexPath`.
- (void)tappedItemAtIndexPath:(NSIndexPath*)indexPath {
  // Speculative fix for crbug.com/1134663, where this method is called while
  // updates from a tab insertion are processing.
  // *** Do not add any code before this check. ***
  if (self.updating) {
    return;
  }

  NSString* sectionIdentifier =
      [self.diffableDataSource sectionIdentifierForIndex:indexPath.section];
  CHECK(
      [sectionIdentifier isEqualToString:kInactiveTabButtonSectionIdentifier] ||
      [sectionIdentifier isEqualToString:kGridOpenTabsSectionIdentifier]);

  GridItemIdentifier* itemIdentifier =
      [self.diffableDataSource itemIdentifierForIndexPath:indexPath];

  CHECK(itemIdentifier.type == GridItemType::kInactiveTabsButton ||
        itemIdentifier.type == GridItemType::kGroup ||
        itemIdentifier.type == GridItemType::kTab);

  [self.mutator userTappedOnItemID:itemIdentifier];
  if (_mode == TabGridMode::kSelection) {
    // Reconfigure the item.
    [self reconfigureItem:itemIdentifier];
  }

  switch (itemIdentifier.type) {
    case GridItemType::kInactiveTabsButton: {
      CHECK(IsInactiveTabButtonRefactoringEnabled());
      [self.delegate didTapInactiveTabsButtonInGridViewController:self];
      break;
    }
    case GridItemType::kTab: {
      web::WebStateID itemID = itemIdentifier.tabSwitcherItem.identifier;
      [self.delegate gridViewController:self didSelectItemWithID:itemID];
      break;
    }
    case GridItemType::kGroup: {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridTabGroupCellTapped"));
      const TabGroup* group = itemIdentifier.tabGroupItem.tabGroup;
      if (group) {
        [self.delegate gridViewController:self didSelectGroup:group];
      }
      break;
    }
    case GridItemType::kSuggestedActions:
      NOTREACHED();
  }
}

// Animates the empty state into view.
- (void)animateEmptyStateIn {
  // TODO(crbug.com/40566436) : Polish the animation, and put constants where
  // they belong.
  [self.emptyStateAnimator stopAnimation:YES];
  self.emptyStateAnimator = [[UIViewPropertyAnimator alloc]
      initWithDuration:1.0 - self.emptyStateView.alpha
          dampingRatio:1.0
            animations:^{
              self.emptyStateView.alpha = 1.0;
              self.emptyStateView.transform = CGAffineTransformIdentity;
            }];
  [self.emptyStateAnimator startAnimation];
}

// Removes the empty state out of view, with animation if `animated` is YES.
- (void)removeEmptyStateAnimated:(BOOL)animated {
  // TODO(crbug.com/40566436) : Polish the animation, and put constants where
  // they belong.
  [self.emptyStateAnimator stopAnimation:YES];
  auto removeEmptyState = ^{
    self.emptyStateView.alpha = 0.0;
    self.emptyStateView.transform = CGAffineTransformScale(
        CGAffineTransformIdentity, /*sx=*/0.9, /*sy=*/0.9);
  };
  if (animated) {
    self.emptyStateAnimator = [[UIViewPropertyAnimator alloc]
        initWithDuration:self.emptyStateView.alpha
            dampingRatio:1.0
              animations:removeEmptyState];
    [self.emptyStateAnimator startAnimation];
  } else {
    removeEmptyState();
  }
}

// Update visible cells identifier, following a reorg of cells.
- (void)updateVisibleCellIdentifiers {
  for (NSIndexPath* indexPath in self.collectionView
           .indexPathsForVisibleItems) {
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    if (![cell isKindOfClass:[GridCell class]]) {
      continue;
    }
    NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
    cell.accessibilityIdentifier = GridCellAccessibilityIdentifier(itemIndex);
  }
}

- (BOOL)shouldShowEmptyState {
  if (self.showingSuggestedActions) {
    return NO;
  }
  return self.gridEmpty;
}

// Updates the number of results found on the search open tabs section header.
- (void)updateSearchResultsHeader {
  CHECK_EQ(_mode, TabGridMode::kSearch, base::NotFatalUntil::M129);
  CHECK_GT(_searchText.length, 0ul, base::NotFatalUntil::M129);
  NSInteger tabSectionIndex = [self.diffableDataSource
      indexForSectionIdentifier:kGridOpenTabsSectionIdentifier];
  GridHeader* headerView = base::apple::ObjCCast<
      GridHeader>([self.collectionView
      supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                          atIndexPath:[NSIndexPath
                                          indexPathForRow:0
                                                inSection:tabSectionIndex]]);
  if (!headerView) {
    return;
  }
  NSString* resultsCount = [@([self numberOfTabs]) stringValue];
  headerView.value =
      l10n_util::GetNSStringF(IDS_IOS_TABS_SEARCH_OPEN_TABS_COUNT,
                              base::SysNSStringToUTF16(resultsCount));
}

// Returns the number of tabs in the collection view.
- (NSInteger)numberOfTabs {
  NSInteger sectionIndex = [self.diffableDataSource
      indexForSectionIdentifier:kGridOpenTabsSectionIdentifier];
  return [self.collectionView numberOfItemsInSection:sectionIndex];
}

// Returns the IndexPath of the item having the `ID`.
- (NSIndexPath*)indexPathForID:(web::WebStateID)ID {
  TabSwitcherItem* tabItem = [[TabSwitcherItem alloc] initWithIdentifier:ID];
  GridItemIdentifier* lookupItemIdentifier =
      [[GridItemIdentifier alloc] initWithTabItem:tabItem];
  return
      [self.diffableDataSource indexPathForItemIdentifier:lookupItemIdentifier];
}

// Returns the indexPath for the tab at `index`.
- (NSIndexPath*)indexPathForTabIndex:(NSInteger)index {
  NSInteger sectionIndex = [_diffableDataSource
      indexForSectionIdentifier:kGridOpenTabsSectionIdentifier];
  return [NSIndexPath indexPathForItem:index inSection:sectionIndex];
}

// Sets the hover effect to a cell. The shape of the hover effect is exactly the
// same as the border of a selected tab.
- (void)setHoverEffectToCell:(UICollectionViewCell*)cell {
  DCHECK(ios::provider::IsRaccoonEnabled());
  if (@available(iOS 17.0, *)) {
    CGFloat margin =
        kGridCellSelectionRingTintWidth + kGridCellSelectionRingGapWidth;
    cell.hoverStyle = [UIHoverStyle
        styleWithShape:[UIShape
                           fixedRectShapeWithRect:CGRectMake(
                                                      -margin, -margin,
                                                      cell.bounds.size.width +
                                                          margin * 2,
                                                      cell.bounds.size.height +
                                                          margin * 2)
                                     cornerRadius:kGridCellCornerRadius +
                                                  margin]];
  }
}

// Reconfigures `itemIdentifier`.
- (void)reconfigureItem:(GridItemIdentifier*)itemIdentifier {
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  [snapshot reconfigureItemsWithIdentifiers:@[ itemIdentifier ]];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

@end
