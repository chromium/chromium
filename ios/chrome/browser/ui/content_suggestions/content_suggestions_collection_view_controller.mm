// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_header_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_parent_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_selection_actions.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_text_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kCardBorderRadius = 11;
}  // namespace

@interface ContentSuggestionsCollectionViewController () <
    UIGestureRecognizerDelegate,
    ContentSuggestionsSelectionActions>

// The layout of the content suggestions collection view.
@property(nonatomic, strong) ContentSuggestionsLayout* layout;

// Dictionary keyed by SectionIdentifier containing section configuration
// information.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, ContentSuggestionsSectionInformation*>*
        sectionInfoBySectionIdentifier;

// Ordered list of sections being shown.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsSectionInformation*>* orderedSectionsInfo;
// Whether an item of type ItemTypePromo has already been added to the model.
@property(nonatomic, assign) BOOL promoAdded;

@end

@implementation ContentSuggestionsCollectionViewController

@dynamic collectionViewModel;

#pragma mark - Lifecycle

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style {
  _layout = [[ContentSuggestionsLayout alloc] init];
  self = [super initWithLayout:_layout style:style];
  return self;
}

#pragma mark - Public

// Removes the `section`.
- (void)dismissSection:(NSInteger)section {
  if (section >= [self numberOfSectionsInCollectionView:self.collectionView]) {
    return;
  }

  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSectionIndex:section];

  [self.collectionView
      performBatchUpdates:^{
        [self.collectionViewModel
            removeSectionWithIdentifier:sectionIdentifier];
        [self.collectionView
            deleteSections:[NSIndexSet indexSetWithIndex:section]];
      }
               completion:nil];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.collectionView.prefetchingEnabled = NO;
  // Overscroll action does not work well with content offset, so set this
  // to never and internally offset the UI to account for safe area insets.
  self.collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;

  self.collectionView.delegate = self;
  self.collectionView.backgroundColor = ntp_home::kNTPBackgroundColor();
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.cardBorderRadius = kCardBorderRadius;
  self.styler.separatorColor = [UIColor clearColor];
  self.collectionView.translatesAutoresizingMaskIntoConstraints = NO;

  ApplyVisualConstraints(@[ @"V:|[collection]|", @"H:|[collection]|" ],
                         @{@"collection" : self.collectionView});
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if (ShouldShowReturnToMostRecentTabForStartSurface()) {
    [self.audience viewDidDisappear];
  }
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch ([self contentSuggestionTypeForItem:item]) {
    case ContentSuggestionTypeMostVisited:
      [self.suggestionCommandHandler openMostVisitedItem:item
                                                 atIndex:indexPath.item];
      break;
    case ContentSuggestionTypeReturnToRecentTab:
      [self.suggestionCommandHandler openMostRecentTab];
      break;
    case ContentSuggestionTypePromo:
      [self dismissSection:indexPath.section];
      [self.suggestionCommandHandler handlePromoTapped];
      [self.collectionViewLayout invalidateLayout];
      break;
    case ContentSuggestionTypeEmpty:
      break;
  }
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell = [super collectionView:collectionView
                              cellForItemAtIndexPath:indexPath];
  if ([self isMostVisitedSection:indexPath.section]) {
    cell.accessibilityIdentifier = [NSString
        stringWithFormat:
            @"%@%li",
            kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
            indexPath.row];
    // Apple doesn't handle the transparency of the background during animations
    // linked to context menus. To prevent the cell from turning black during
    // animations, its background is set to be the same as the NTP background.
    // See: crbug.com/1120321.
    cell.backgroundColor = ntp_home::kNTPBackgroundColor();
    [self.collectionViewModel itemAtIndexPath:indexPath]
        .accessibilityIdentifier = cell.accessibilityIdentifier;
  }

  return cell;
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  if (![item isKindOfClass:[ContentSuggestionsMostVisitedItem class]])
    return nil;

  ContentSuggestionsMostVisitedItem* contentSuggestionsItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);

  return [self.menuProvider
      contextMenuConfigurationForItem:contentSuggestionsItem
                             fromView:[self.collectionView
                                          cellForItemAtIndexPath:indexPath]];
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isMostVisitedSection:indexPath.section]) {
    return [ContentSuggestionsMostVisitedCell defaultSize];
  }
  CGSize size = [super collectionView:collectionView
                               layout:collectionViewLayout
               sizeForItemAtIndexPath:indexPath];
  return size;
}

- (UIEdgeInsets)collectionView:(UICollectionView*)collectionView
                        layout:(UICollectionViewLayout*)collectionViewLayout
        insetForSectionAtIndex:(NSInteger)section {
  UIEdgeInsets parentInset = [super collectionView:collectionView
                                            layout:collectionViewLayout
                            insetForSectionAtIndex:section];
  if ([self isHeaderSection:section] || [self isSingleCellSection:section]) {
    parentInset.top = 0;
    parentInset.left = 0;
    parentInset.right = 0;
  } else if ([self isReturnToRecentTabSection:section]) {
    CGFloat collectionWidth = collectionView.bounds.size.width;
    CGFloat maxCardWidth = content_suggestions::searchFieldWidth(
        collectionWidth, self.traitCollection);
    CGFloat margin =
        MAX(0, (collectionView.frame.size.width - maxCardWidth) / 2);
    parentInset.left = margin;
    parentInset.right = margin;
    parentInset.bottom =
        content_suggestions::kReturnToRecentTabSectionBottomMargin;
  } else if ([self isMostVisitedSection:section] ||
             [self isPromoSection:section]) {
    CGFloat margin = CenteredTilesMarginForWidth(
        self.traitCollection, collectionView.frame.size.width);
    parentInset.left = margin;
    parentInset.right = margin;
    if ([self isMostVisitedSection:section]) {
      parentInset.bottom = kMostVisitedBottomMargin;
    }
  }
  return parentInset;
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
                                 layout:(UICollectionViewLayout*)
                                            collectionViewLayout
    minimumLineSpacingForSectionAtIndex:(NSInteger)section {
  if ([self isMostVisitedSection:section]) {
    return kContentSuggestionsTilesVerticalSpacing;
  }
  return [super collectionView:collectionView
                                   layout:collectionViewLayout
      minimumLineSpacingForSectionAtIndex:section];
}

#pragma mark - MDCCollectionViewStylingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

- (UIColor*)collectionView:(nonnull UICollectionView*)collectionView
    cellBackgroundColorAtIndexPath:(nonnull NSIndexPath*)indexPath {
  if ([self shouldUseCustomStyleForSection:indexPath.section]) {
    return UIColor.clearColor;
  }
  return ntp_home::kNTPBackgroundColor();
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  if ([self isHeaderSection:section]) {
    DCHECK(!IsContentSuggestionsHeaderMigrationEnabled());
    return CGSizeMake(0, [self.headerProvider headerHeight]);
  }
  CGSize defaultSize = [super collectionView:collectionView
                                      layout:collectionViewLayout
             referenceSizeForHeaderInSection:section];
  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    // Double the size of the header as it is now on two lines.
    defaultSize.height *= 2;
  }
  return defaultSize;
}

- (BOOL)collectionView:(nonnull UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(nonnull NSIndexPath*)indexPath {
  return [self shouldUseCustomStyleForSection:indexPath.section];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideHeaderBackgroundForSection:(NSInteger)section {
  return [self shouldUseCustomStyleForSection:section];
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CSCollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];
  CGFloat width =
      CGRectGetWidth(collectionView.bounds) - inset.left - inset.right;

  return [item cellHeightForWidth:width];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideHeaderSeparatorForSection:(NSInteger)section {
  return [self shouldUseCustomStyleForSection:section];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  return touch.view.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID() &&
         touch.view.superview.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID();
}

#pragma mark - ContentSuggestionsCollectionConsumer

- (void)reloadDataWithSections:
            (NSArray<ContentSuggestionsSectionInformation*>*)sections
                      andItems:
                          (NSMutableDictionary<NSNumber*, NSArray*>*)items {
  [self resetModels];
  self.orderedSectionsInfo = [sections mutableCopy];

  // The data is reset, add the new data directly in the model then reload the
  // collection.
  [self addSectionsForSectionInfoToModel:sections withItems:items];
  for (ContentSuggestionsSectionInformation* sectionInfo in sections) {
    if (sectionInfo.sectionID == ContentSuggestionsSectionSingleCell) {
      DCHECK(IsSingleCellContentSuggestionsEnabled());
      DCHECK_EQ(1.0, [items[@(sectionInfo.sectionID)] count]);
      ContentSuggestionsParentItem* item =
          static_cast<ContentSuggestionsParentItem*>(
              items[@(sectionInfo.sectionID)][0]);
      item.tapTarget = self;
      item.menuProvider = self.menuProvider;
    }
    [self addSuggestionsToModel:items[@(sectionInfo.sectionID)]
                withSectionInfo:sectionInfo];
  }
  [self.collectionView reloadData];
}

- (void)addSection:(ContentSuggestionsSectionInformation*)sectionInfo
         withItems:(NSArray<CSCollectionViewItem*>*)items
        completion:(void (^)(void))completion {
  SectionIdentifier sectionIdentifier =
      [self sectionIdentifierForInfo:sectionInfo];
  CSCollectionViewModel* model = self.collectionViewModel;

  if ([model hasSectionForSectionIdentifier:sectionIdentifier])
    return;

  auto addSectionBlock = ^{
    NSIndexSet* addedSection = [self
        addSectionsForSectionInfoToModel:@[ sectionInfo ]
                               withItems:@{@(sectionInfo.sectionID) : items}];
    [self.collectionView insertSections:addedSection];
    NSArray<NSIndexPath*>* addedItems =
        [self addSuggestionsToModel:items withSectionInfo:sectionInfo];
    [self.collectionView insertItemsAtIndexPaths:addedItems];
  };

  [UIView performWithoutAnimation:^{
    [self.collectionView performBatchUpdates:addSectionBlock
                                  completion:^(BOOL finished) {
                                    completion();
                                  }];
  }];
}

- (void)clearSection:(ContentSuggestionsSectionInformation*)sectionInfo {
  SectionIdentifier sectionIdentifier =
      [self sectionIdentifierForInfo:sectionInfo];
  CSCollectionViewModel* model = self.collectionViewModel;

  if (![model hasSectionForSectionIdentifier:sectionIdentifier])
    return;

  NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];

  [self dismissSection:section];
}

- (void)itemHasChanged:(CollectionViewItem<SuggestedContent>*)item {
  if (![self.collectionViewModel hasItem:item]) {
    return;
  }
  if (IsSingleCellContentSuggestionsEnabled()) {
    ContentSuggestionsParentItem* parentItem =
        static_cast<ContentSuggestionsParentItem*>(item);
    parentItem.tapTarget = self;
  }
  [self reconfigureCellsForItems:@[ item ]];
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction {
  CGFloat toolbarHeight =
      ToolbarExpandedHeight(self.traitCollection.preferredContentSizeCategory);
  // The collection displays the fake omnibox on the top of the other elements.
  // The default scrolling action scrolls for the full height of the collection,
  // hiding elements behing the fake omnibox. This reduces the scrolling by the
  // height of the fake omnibox.
  if (direction == UIAccessibilityScrollDirectionDown) {
    CGFloat newYOffset = self.collectionView.contentOffset.y +
                         self.collectionView.bounds.size.height - toolbarHeight;
    newYOffset = MIN(self.collectionView.contentSize.height -
                         self.collectionView.bounds.size.height,
                     newYOffset);
    self.collectionView.contentOffset =
        CGPointMake(self.collectionView.contentOffset.x, newYOffset);
  } else if (direction == UIAccessibilityScrollDirectionUp) {
    CGFloat newYOffset = self.collectionView.contentOffset.y -
                         self.collectionView.bounds.size.height + toolbarHeight;
    newYOffset = MAX(0, newYOffset);
    self.collectionView.contentOffset =
        CGPointMake(self.collectionView.contentOffset.x, newYOffset);
  } else {
    return NO;
  }
  return YES;
}

#pragma mark - ContentSuggestionsSelectionActions

- (void)contentSuggestionsElementTapped:(UIGestureRecognizer*)sender {
  if ([sender.view
          isKindOfClass:[ContentSuggestionsMostVisitedTileView class]]) {
    ContentSuggestionsMostVisitedTileView* mostVisitedView =
        static_cast<ContentSuggestionsMostVisitedTileView*>(sender.view);
    [self.suggestionCommandHandler
        openMostVisitedItem:mostVisitedView.config
                    atIndex:mostVisitedView.config.index];
  } else if ([sender.view
                 isKindOfClass:[ContentSuggestionsShortcutTileView class]]) {
    ContentSuggestionsShortcutTileView* shortcutView =
        static_cast<ContentSuggestionsShortcutTileView*>(sender.view);
    int index = static_cast<int>(shortcutView.config.index);
    [self.suggestionCommandHandler openMostVisitedItem:shortcutView.config
                                               atIndex:index];
  } else if ([sender.view isKindOfClass:[ContentSuggestionsReturnToRecentTabView
                                            class]]) {
    ContentSuggestionsReturnToRecentTabView* returnToRecentTabView =
        static_cast<ContentSuggestionsReturnToRecentTabView*>(sender.view);
    __weak ContentSuggestionsReturnToRecentTabView* weakRecentTabView =
        returnToRecentTabView;
    UIGestureRecognizerState state = sender.state;
    if (state == UIGestureRecognizerStateChanged ||
        state == UIGestureRecognizerStateCancelled) {
      // Do nothing if isn't a gesture start or end.
      // If the gesture was cancelled by the system, then reset the background
      // color since UIGestureRecognizerStateEnded will not be received.
      if (state == UIGestureRecognizerStateCancelled) {
        returnToRecentTabView.backgroundColor = [UIColor clearColor];
      }
      return;
    }
    BOOL touchBegan = state == UIGestureRecognizerStateBegan;
    [UIView transitionWithView:returnToRecentTabView
                      duration:ios::material::kDuration8
                       options:UIViewAnimationOptionCurveEaseInOut
                    animations:^{
                      weakRecentTabView.backgroundColor =
                          touchBegan ? [UIColor colorNamed:kGrey100Color]
                                     : [UIColor clearColor];
                    }
                    completion:nil];
    if (state == UIGestureRecognizerStateEnded) {
      CGPoint point = [sender locationInView:returnToRecentTabView];
      if (point.x < 0 || point.y < 0 ||
          point.x > kReturnToRecentTabSize.width ||
          point.y > kReturnToRecentTabSize.height) {
        // Reset the highlighted state and do nothing if the gesture ended
        // outside of the tile.
        returnToRecentTabView.backgroundColor = [UIColor clearColor];
        return;
      }
      [self.suggestionCommandHandler openMostRecentTab];
    }
  } else if ([sender.view
                 isKindOfClass:[ContentSuggestionsWhatsNewView class]]) {
    [self.suggestionCommandHandler handlePromoTapped];
  }
}

#pragma mark - Private

// Checks if the `section` is empty and add an empty element if it is the case.
// Must be called from inside a performBatchUpdates: block.
- (void)addEmptySectionPlaceholderIfNeeded:(NSInteger)section {
  if ([self.collectionViewModel numberOfItemsInSection:section] > 0)
    return;

  NSIndexPath* emptyItem = [self addEmptyItemForSection:section];
  if (emptyItem)
    [self.collectionView insertItemsAtIndexPaths:@[ emptyItem ]];
}

// Returns the ContentSuggestionType associated with an ItemType `type`.
- (ContentSuggestionType)contentSuggestionTypeForItemType:(NSInteger)type {
  switch (type) {
    case ItemTypeEmpty:
      return ContentSuggestionTypeEmpty;
    case ItemTypeReturnToRecentTab:
      return ContentSuggestionTypeReturnToRecentTab;
    case ItemTypeMostVisited:
      return ContentSuggestionTypeMostVisited;
    case ItemTypePromo:
      return ContentSuggestionTypePromo;
    default:
      return ContentSuggestionTypeEmpty;
  }
}

// Returns the item type corresponding to the section `info`.
- (ItemType)itemTypeForInfo:(ContentSuggestionsSectionInformation*)info {
  switch (info.sectionID) {
    case ContentSuggestionsSectionReturnToRecentTab:
      return ItemTypeReturnToRecentTab;
    case ContentSuggestionsSectionMostVisited:
      return ItemTypeMostVisited;
    case ContentSuggestionsSectionPromo:
      return ItemTypePromo;
    case ContentSuggestionsSectionSingleCell:
      return ItemTypeSingleCell;
    case ContentSuggestionsSectionLogo:
    case ContentSuggestionsSectionUnknown:
      return ItemTypeUnknown;
  }
}

// Returns the section identifier corresponding to the section `info`.
- (SectionIdentifier)sectionIdentifierForInfo:
    (ContentSuggestionsSectionInformation*)info {
  switch (info.sectionID) {
    case ContentSuggestionsSectionMostVisited:
      return SectionIdentifierMostVisited;
    case ContentSuggestionsSectionLogo:
      return SectionIdentifierLogo;
    case ContentSuggestionsSectionReturnToRecentTab:
      return SectionIdentifierReturnToRecentTab;
    case ContentSuggestionsSectionPromo:
      return SectionIdentifierPromo;
    case ContentSuggestionsSectionSingleCell:
      return SectionIdentifierSingleCell;
    case ContentSuggestionsSectionUnknown:
      return SectionIdentifierDefault;
  }
}

- (BOOL)shouldUseCustomStyleForSection:(NSInteger)section {
  NSNumber* identifier =
      @([self.collectionViewModel sectionIdentifierForSectionIndex:section]);
  ContentSuggestionsSectionInformation* sectionInformation =
      self.sectionInfoBySectionIdentifier[identifier];
  return sectionInformation.layout == ContentSuggestionsSectionLayoutCustom;
}

- (ContentSuggestionType)contentSuggestionTypeForItem:
    (CollectionViewItem*)item {
  return [self contentSuggestionTypeForItemType:item.type];
}

- (NSArray<NSIndexPath*>*)
    addSuggestionsToModel:(NSArray<CSCollectionViewItem*>*)suggestions
          withSectionInfo:(ContentSuggestionsSectionInformation*)sectionInfo {
  NSMutableArray<NSIndexPath*>* indexPaths = [NSMutableArray array];

  CSCollectionViewModel* model = self.collectionViewModel;
  NSInteger sectionIdentifier = [self sectionIdentifierForInfo:sectionInfo];

  if (suggestions.count == 0) {
    // No suggestions for this section. Add the item signaling this section is
    // empty if there is currently no item in it.
    if ([model hasSectionForSectionIdentifier:sectionIdentifier] &&
        [model numberOfItemsInSection:[model sectionForSectionIdentifier:
                                                 sectionIdentifier]] == 0) {
      NSIndexPath* emptyItemIndexPath =
          [self addEmptyItemForSection:
                    [model sectionForSectionIdentifier:sectionIdentifier]];
      if (emptyItemIndexPath) {
        [indexPaths addObject:emptyItemIndexPath];
      }
    }
    return indexPaths;
  }

  // Add the items from this section.
  [suggestions enumerateObjectsUsingBlock:^(CSCollectionViewItem* item,
                                            NSUInteger index, BOOL* stop) {
    ItemType type = [self itemTypeForInfo:sectionInfo];
    if (type == ItemTypePromo && !self.promoAdded) {
      self.promoAdded = YES;
      [self.audience promoShown];
    }
    item.type = type;
    NSIndexPath* addedIndexPath = [self addItem:item
                        toSectionWithIdentifier:sectionIdentifier];

    [indexPaths addObject:addedIndexPath];
  }];

  return indexPaths;
}

- (NSIndexSet*)
    addSectionsForSectionInfoToModel:
        (NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo
                           withItems:(NSDictionary<NSNumber*, NSArray*>*)items {
  NSMutableIndexSet* addedSectionIdentifiers = [NSMutableIndexSet indexSet];

  CSCollectionViewModel* model = self.collectionViewModel;
  for (ContentSuggestionsSectionInformation* sectionInfo in sectionsInfo) {
    NSInteger sectionIdentifier = [self sectionIdentifierForInfo:sectionInfo];
    NSArray* itemsArray = items[@(sectionInfo.sectionID)];
    if ([model hasSectionForSectionIdentifier:sectionIdentifier] ||
        (!sectionInfo.showIfEmpty && [itemsArray count] == 0)) {
      continue;
    }

    NSUInteger sectionIndex = 0;
    for (ContentSuggestionsSectionInformation* orderedSectionInfo in self
             .orderedSectionsInfo) {
      NSInteger orderedSectionIdentifier =
          [self sectionIdentifierForInfo:orderedSectionInfo];
      if (orderedSectionIdentifier == sectionIdentifier) {
        break;
      }
      if ([model hasSectionForSectionIdentifier:orderedSectionIdentifier]) {
        sectionIndex++;
      }
    }
    [model insertSectionWithIdentifier:sectionIdentifier atIndex:sectionIndex];

    self.sectionInfoBySectionIdentifier[@(sectionIdentifier)] = sectionInfo;
    [addedSectionIdentifiers addIndex:sectionIdentifier];

    if (sectionIdentifier == SectionIdentifierLogo) {
      [self addLogoHeaderIfNeeded];
    }
  }

  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
  [addedSectionIdentifiers enumerateIndexesUsingBlock:^(
                               NSUInteger sectionIdentifier, BOOL* stop) {
    [indexSet addIndex:[model sectionForSectionIdentifier:sectionIdentifier]];
  }];
  return indexSet;
}

- (NSIndexPath*)addEmptyItemForSection:(NSInteger)section {
  CSCollectionViewModel* model = self.collectionViewModel;
  NSInteger sectionIdentifier =
      [model sectionIdentifierForSectionIndex:section];
  ContentSuggestionsSectionInformation* sectionInfo =
      self.sectionInfoBySectionIdentifier[@(sectionIdentifier)];

  CSCollectionViewItem* item = [self emptyItemForSectionInfo:sectionInfo];
  if (!item) {
    return nil;
  }
  return [self addItem:item toSectionWithIdentifier:sectionIdentifier];
}

- (BOOL)isReturnToRecentTabSection:(NSInteger)section {
  return [self.collectionViewModel sectionIdentifierForSectionIndex:section] ==
         SectionIdentifierReturnToRecentTab;
}

- (BOOL)isMostVisitedSection:(NSInteger)section {
  return [self.collectionViewModel sectionIdentifierForSectionIndex:section] ==
         SectionIdentifierMostVisited;
}

- (BOOL)isHeaderSection:(NSInteger)section {
  return [self.collectionViewModel sectionIdentifierForSectionIndex:section] ==
         SectionIdentifierLogo;
}

- (BOOL)isPromoSection:(NSInteger)section {
  return [self.collectionViewModel sectionIdentifierForSectionIndex:section] ==
         SectionIdentifierPromo;
}

- (BOOL)isSingleCellSection:(NSInteger)section {
  return [self.collectionViewModel sectionIdentifierForSectionIndex:section] ==
         SectionIdentifierSingleCell;
}

// Adds the header for the first section, containing the logo and the omnibox,
// if there is no header for the section.
- (void)addLogoHeaderIfNeeded {
  DCHECK(!IsContentSuggestionsHeaderMigrationEnabled());
  if (![self.collectionViewModel
          headerForSectionWithIdentifier:SectionIdentifierLogo]) {
    ContentSuggestionsHeaderItem* header =
        [[ContentSuggestionsHeaderItem alloc] initWithType:ItemTypeHeader];
    header.view =
        [self headerViewForWidth:self.collectionView.bounds.size.width];
    [self.collectionViewModel setHeader:header
               forSectionWithIdentifier:SectionIdentifierLogo];
  }
}

- (UIView*)headerViewForWidth:(CGFloat)width {
  return [self.headerProvider
      headerForWidth:width
      safeAreaInsets:[self.audience safeAreaInsetsForDiscoverFeed]];
}

// Resets the models, removing the current CollectionViewItem and the
// SectionInfo.
- (void)resetModels {
  [self loadModel];
  self.sectionInfoBySectionIdentifier = [[NSMutableDictionary alloc] init];
}

// Returns a item to be displayed when the section identified by `sectionInfo`
// is empty.
// Returns nil if there is no empty item for this section info.
- (CSCollectionViewItem*)emptyItemForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  if (!sectionInfo.emptyText || !sectionInfo.expanded)
    return nil;
  ContentSuggestionsTextItem* item =
      [[ContentSuggestionsTextItem alloc] initWithType:ItemTypeEmpty];
  item.text = l10n_util::GetNSString(IDS_NTP_TITLE_NO_SUGGESTIONS);
  item.detailText = sectionInfo.emptyText;

  return item;
}

// Adds `item` to `sectionIdentifier` section of the model of the
// CollectionView. Returns the IndexPath of the newly added item.
- (NSIndexPath*)addItem:(CSCollectionViewItem*)item
    toSectionWithIdentifier:(NSInteger)sectionIdentifier {
  CSCollectionViewModel* model = self.collectionViewModel;
  NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
  NSInteger itemNumber = [model numberOfItemsInSection:section];
  [model addItem:item toSectionWithIdentifier:sectionIdentifier];

  return [NSIndexPath indexPathForItem:itemNumber inSection:section];
}

@end
