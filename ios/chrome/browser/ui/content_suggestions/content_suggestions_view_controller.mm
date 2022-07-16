// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_discover_header_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/discover_feed_menu_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/theme_change_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_layout_util.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
const CGFloat kMostVisitedBottomMargin = 13;
const CGFloat kCardBorderRadius = 11;
const CGFloat kDiscoverFeedContentWith = 430;
// Height for the Discover Feed section header.
const CGFloat kDiscoverFeedFeaderHeight = 30;
}

@interface ContentSuggestionsViewController () <UIGestureRecognizerDelegate>

@property(nonatomic, strong)
    ContentSuggestionsCollectionUpdater* collectionUpdater;

// The layout of the content suggestions collection view.
@property(nonatomic, strong) ContentSuggestionsLayout* layout;

@end

@implementation ContentSuggestionsViewController

@dynamic collectionViewModel;

#pragma mark - Lifecycle

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style {
  _layout = [[ContentSuggestionsLayout alloc] init];
  self = [super initWithLayout:_layout style:style];
  if (self) {
    _collectionUpdater = [[ContentSuggestionsCollectionUpdater alloc] init];
    _discoverFeedHeaderDelegate = _collectionUpdater;
  }
  return self;
}

#pragma mark - Public

- (void)setDataSource:(id<ContentSuggestionsDataSource>)dataSource {
  self.collectionUpdater.dataSource = dataSource;
}

- (void)setDispatcher:(id<SnackbarCommands>)dispatcher {
  self.collectionUpdater.dispatcher = dispatcher;
}

- (void)dismissEntryAtIndexPath:(NSIndexPath*)indexPath {
  if (!indexPath || ![self.collectionViewModel hasItemAtIndexPath:indexPath]) {
    return;
  }

  if ([self.collectionUpdater isReturnToRecentTabSection:indexPath.section]) {
    [self.suggestionCommandHandler hideMostRecentTab];
    return;
  }

  [self.collectionView performBatchUpdates:^{
    [self collectionView:self.collectionView
        willDeleteItemsAtIndexPaths:@[ indexPath ]];
    [self.collectionView deleteItemsAtIndexPaths:@[ indexPath ]];

    // Check if the section is now empty.
    [self addEmptySectionPlaceholderIfNeeded:indexPath.section];
  }
      completion:^(BOOL) {
        // The context menu could be displayed for the deleted entry.
        [self.suggestionCommandHandler dismissModals];
      }];
}

- (void)dismissSection:(NSInteger)section {
  if (section >= [self numberOfSectionsInCollectionView:self.collectionView]) {
    return;
  }

  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];

  [self.collectionView performBatchUpdates:^{
    [self.collectionViewModel removeSectionWithIdentifier:sectionIdentifier];
    [self.collectionView deleteSections:[NSIndexSet indexSetWithIndex:section]];
  }
      completion:^(BOOL) {
        // The context menu could be displayed for the deleted entries.
        [self.suggestionCommandHandler dismissModals];
      }];
}

- (void)addSuggestions:(NSArray<CSCollectionViewItem*>*)suggestions
         toSectionInfo:(ContentSuggestionsSectionInformation*)sectionInfo {
  void (^batchUpdates)(void) = ^{
    NSIndexSet* addedSections = [self.collectionUpdater
        addSectionsForSectionInfoToModel:@[ sectionInfo ]];
    [self.collectionView insertSections:addedSections];

    NSIndexPath* removedItem = [self.collectionUpdater
        removeEmptySuggestionsForSectionInfo:sectionInfo];
    if (removedItem) {
      [self.collectionView deleteItemsAtIndexPaths:@[ removedItem ]];
    }

    NSArray<NSIndexPath*>* addedItems =
        [self.collectionUpdater addSuggestionsToModel:suggestions
                                      withSectionInfo:sectionInfo];
    [self.collectionView insertItemsAtIndexPaths:addedItems];
  };

  [self.collectionView performBatchUpdates:batchUpdates completion:nil];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.collectionView.prefetchingEnabled = NO;
  // Overscroll action does not work well with content offset, so set this
  // to never and internally offset the UI to account for safe area insets.
  self.collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  _collectionUpdater.collectionViewController = self;

  self.collectionView.delegate = self;
  self.collectionView.backgroundColor = ntp_home::kNTPBackgroundColor();
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.cardBorderRadius = kCardBorderRadius;
  self.styler.separatorColor = [UIColor colorNamed:kSeparatorColor];
  self.collectionView.translatesAutoresizingMaskIntoConstraints = NO;

  ApplyVisualConstraints(@[ @"V:|[collection]|", @"H:|[collection]|" ],
                         @{@"collection" : self.collectionView});

    UILongPressGestureRecognizer* longPressRecognizer =
        [[UILongPressGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(handleLongPress:)];
    longPressRecognizer.delegate = self;
    [self.collectionView addGestureRecognizer:longPressRecognizer];

}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // TODO(crbug.com/1200303): Reload data is needed here so the content matches
  // the current UI Layout after changing the Feed state (e.g. Turned On/Off).
  // This shouldn't be necessary once we stop starting and stopping the
  // Coordinator to achieve this.
  [self.collectionView reloadData];
  [self.bubblePresenter presentDiscoverFeedHeaderTipBubble];
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
  switch ([self.collectionUpdater contentSuggestionTypeForItem:item]) {
    case ContentSuggestionTypeMostVisited:
      [self.suggestionCommandHandler openMostVisitedItem:item
                                                 atIndex:indexPath.item];
      break;
    case ContentSuggestionTypeReturnToRecentTab:
      [self.suggestionCommandHandler openMostRecentTab:item];
      break;
    case ContentSuggestionTypePromo:
      [self dismissSection:indexPath.section];
      [self.suggestionCommandHandler handlePromoTapped];
      [self.collectionViewLayout invalidateLayout];
      break;
    case ContentSuggestionTypeDiscover:
    case ContentSuggestionTypeEmpty:
      break;
  }
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell = [super collectionView:collectionView
                              cellForItemAtIndexPath:indexPath];
  if ([self.collectionUpdater isMostVisitedSection:indexPath.section]) {
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

#pragma mark - UICollectionViewDataSource

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  UICollectionReusableView* cell = [super collectionView:collectionView
                       viewForSupplementaryElementOfKind:kind
                                             atIndexPath:indexPath];
  if ([kind isEqualToString:UICollectionElementKindSectionHeader] &&
      [self.collectionUpdater isDiscoverSection:indexPath.section]) {
    ContentSuggestionsDiscoverHeaderCell* discoverFeedHeader =
        base::mac::ObjCCastStrict<ContentSuggestionsDiscoverHeaderCell>(cell);
    [discoverFeedHeader.menuButton addTarget:self
                                      action:@selector(openDiscoverFeedMenu)
                            forControlEvents:UIControlEventTouchUpInside];
    [self.audience discoverHeaderMenuButtonShown:discoverFeedHeader.menuButton];
  }
  return cell;
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([self.collectionUpdater isMostVisitedSection:indexPath.section]) {
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
  if ([self.collectionUpdater isHeaderSection:section]) {
    parentInset.top = 0;
    parentInset.left = 0;
    parentInset.right = 0;
  } else if ([self.collectionUpdater isReturnToRecentTabSection:section]) {
    CGFloat collectionWidth = collectionView.bounds.size.width;
    CGFloat maxCardWidth = content_suggestions::searchFieldWidth(
        collectionWidth, self.traitCollection);
    CGFloat margin =
        MAX(0, (collectionView.frame.size.width - maxCardWidth) / 2);
    parentInset.left = margin;
    parentInset.right = margin;
    parentInset.bottom =
        content_suggestions::kReturnToRecentTabSectionBottomMargin;
  } else if ([self.collectionUpdater isMostVisitedSection:section] ||
             [self.collectionUpdater isPromoSection:section]) {
    CGFloat margin = CenteredTilesMarginForWidth(
        self.traitCollection, collectionView.frame.size.width);
    parentInset.left = margin;
    parentInset.right = margin;
    if ([self.collectionUpdater isMostVisitedSection:section]) {
      parentInset.bottom = kMostVisitedBottomMargin;
    }
  } else if ([self.collectionUpdater isDiscoverSection:section]) {
    // TODO(crbug.com/1085419): Get card width from Mulder.
    CGFloat feedCardWidth = kDiscoverFeedContentWith;
    CGFloat margin =
        MAX(0, (collectionView.frame.size.width - feedCardWidth) / 2);
    parentInset.left = margin;
    parentInset.right = margin;
  } else if (self.styler.cellStyle == MDCCollectionViewCellStyleCard) {
    CGFloat collectionWidth = collectionView.bounds.size.width;
    CGFloat maxCardWidth = content_suggestions::searchFieldWidth(
        collectionWidth, self.traitCollection);
    CGFloat margin =
        MAX(0, (collectionView.frame.size.width - maxCardWidth) / 2);
    parentInset.left = margin;
    parentInset.right = margin;
  }
  return parentInset;
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
                                 layout:(UICollectionViewLayout*)
                                            collectionViewLayout
    minimumLineSpacingForSectionAtIndex:(NSInteger)section {
  if ([self.collectionUpdater isMostVisitedSection:section]) {
    return kNtpTilesVerticalSpacing;
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
  if ([self.collectionUpdater
          shouldUseCustomStyleForSection:indexPath.section]) {
    return UIColor.clearColor;
  }
  return ntp_home::kNTPBackgroundColor();
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  if ([self.collectionUpdater isHeaderSection:section]) {
    return CGSizeMake(0, [self.headerProvider headerHeight]);
  }
  if ([self.collectionUpdater isDiscoverSection:section]) {
    return CGSizeMake(0, kDiscoverFeedFeaderHeight);
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
  return
      [self.collectionUpdater shouldUseCustomStyleForSection:indexPath.section];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideHeaderBackgroundForSection:(NSInteger)section {
  return [self.collectionUpdater shouldUseCustomStyleForSection:section];
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
  return [self.collectionUpdater shouldUseCustomStyleForSection:section];
}


#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  return touch.view.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID() &&
         touch.view.superview.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID();
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

#pragma mark - Private

- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer {
  if (self.editor.editing ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }

  CGPoint touchLocation =
      [gestureRecognizer locationOfTouch:0 inView:self.collectionView];
  NSIndexPath* touchedItemIndexPath =
      [self.collectionView indexPathForItemAtPoint:touchLocation];
  if (!touchedItemIndexPath ||
      ![self.collectionViewModel hasItemAtIndexPath:touchedItemIndexPath]) {
    // Make sure there is an item at this position.
    return;
  }
  CollectionViewItem* touchedItem =
      [self.collectionViewModel itemAtIndexPath:touchedItemIndexPath];

  ContentSuggestionType type =
      [self.collectionUpdater contentSuggestionTypeForItem:touchedItem];
  switch (type) {
    case ContentSuggestionTypeMostVisited:
      break;
    default:
      break;
  }
}

// Checks if the |section| is empty and add an empty element if it is the case.
// Must be called from inside a performBatchUpdates: block.
- (void)addEmptySectionPlaceholderIfNeeded:(NSInteger)section {
  if ([self.collectionViewModel numberOfItemsInSection:section] > 0)
    return;

  NSIndexPath* emptyItem =
      [self.collectionUpdater addEmptyItemForSection:section];
  if (emptyItem)
    [self.collectionView insertItemsAtIndexPaths:@[ emptyItem ]];
}

// Opens top-level feed menu when pressing |menuButton|.
- (void)openDiscoverFeedMenu {
  [self.discoverFeedMenuHandler openDiscoverFeedMenu];
}

@end
