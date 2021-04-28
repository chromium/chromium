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
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_discover_header_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_discover_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_action_handler.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recording.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/discover_feed_menu_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/discover_feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/theme_change_delegate.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_layout_util.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/menu_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
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
// Value representing offset from bottom of the page to trigger pagination.
const CGFloat kPaginationOffset = 800;
// Height for the Discover Feed section header.
const CGFloat kDiscoverFeedFeaderHeight = 30;
// Minimum height of the Discover feed content to indicate that the articles
// have loaded.
const CGFloat kDiscoverFeedLoadedHeight = 1000;
}

@interface ContentSuggestionsViewController ()<UIGestureRecognizerDelegate> {
  CGFloat _initialContentOffset;
}

@property(nonatomic, strong)
    ContentSuggestionsCollectionUpdater* collectionUpdater;

// The overscroll actions controller managing accelerators over the toolbar.
@property(nonatomic, strong)
    OverscrollActionsController* overscrollActionsController;

// The DiscoverFeedVC that might be displayed by this VC.
@property(nonatomic, weak) UIViewController* discoverFeedVC;
// The FeedView CollectionView contained by discoverFeedVC.
@property(nonatomic, strong) UICollectionView* feedView;

// Navigation offset applied to the layout height to maintain the scroll
// position, since the feed height is dynamic.
@property(nonatomic) CGFloat offset;

// Represents the last recorded height of the Discover feed for tracking when to
// trigger the infinite feed.
@property(nonatomic, assign) CGFloat discoverFeedHeight;

// Whether this VC is observing the discoverFeedHeight using KVO or not.
@property(nonatomic, assign) BOOL observingDiscoverFeedHeight;

// The CollectionViewController scroll position when an scrolling event starts.
@property(nonatomic, assign) int scrollStartPosition;

// The layout of the content suggestions collection view.
@property(nonatomic, strong) ContentSuggestionsLayout* layout;

@end

@implementation ContentSuggestionsViewController

@synthesize audience = _audience;
@synthesize suggestionCommandHandler = _suggestionCommandHandler;
@synthesize headerSynchronizer = _headerSynchronizer;
@synthesize collectionUpdater = _collectionUpdater;
@synthesize overscrollActionsController = _overscrollActionsController;
@synthesize overscrollDelegate = _overscrollDelegate;
@synthesize scrolledToTop = _scrolledToTop;
@synthesize metricsRecorder = _metricsRecorder;
@dynamic collectionViewModel;

#pragma mark - Lifecycle

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
                       offset:(CGFloat)offset
        refactoredFeedVisible:(BOOL)visible {
  _offset = offset;
  _layout = [[ContentSuggestionsLayout alloc] initWithOffset:offset
                                                 feedVisible:visible];
  self = [super initWithLayout:_layout style:style];
  if (self) {
    _collectionUpdater = [[ContentSuggestionsCollectionUpdater alloc] init];
    _initialContentOffset = NAN;
    _discoverFeedHeaderDelegate = _collectionUpdater;
  }
  return self;
}

- (void)dealloc {
  [self removeContentSizeKVO];
  if (self.discoverFeedVC.parentViewController) {
    [self.discoverFeedVC willMoveToParentViewController:nil];
    [self.discoverFeedVC.view removeFromSuperview];
    [self.discoverFeedVC removeFromParentViewController];
  }
  [self.overscrollActionsController invalidate];
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

  [self.metricsRecorder
      onSuggestionDismissed:[self.collectionViewModel itemAtIndexPath:indexPath]
                atIndexPath:indexPath
      suggestionsShownAbove:[self numberOfSuggestionsAbove:indexPath.section]];

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

- (NSInteger)numberOfSuggestionsAbove:(NSInteger)section {
  NSInteger suggestionsAbove = 0;
  for (NSInteger sectionAbove = 0; sectionAbove < section; sectionAbove++) {
    if ([self.collectionUpdater isContentSuggestionsSection:sectionAbove]) {
      suggestionsAbove +=
          [self.collectionViewModel numberOfItemsInSection:sectionAbove];
    }
  }
  return suggestionsAbove;
}

- (NSInteger)numberOfSectionsAbove:(NSInteger)section {
  NSInteger sectionsAbove = 0;
  for (NSInteger sectionAbove = 0; sectionAbove < section; sectionAbove++) {
    if ([self.collectionUpdater isContentSuggestionsSection:sectionAbove]) {
      sectionsAbove++;
    }
  }
  return sectionsAbove;
}

- (void)updateConstraints {
  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.headerSynchronizer updateConstraints];
  [self.collectionView reloadData];
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
}

- (void)clearOverscroll {
  [self.overscrollActionsController clear];
}

- (void)setContentOffset:(CGFloat)offset {
  _initialContentOffset = offset;
  if (self.isViewLoaded && self.collectionView.window &&
      self.collectionView.contentSize.height != 0) {
    [self applyContentOffset];
  }
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

  self.overscrollActionsController = [[OverscrollActionsController alloc]
      initWithScrollView:self.collectionView];
  [self.overscrollActionsController
      setStyle:OverscrollStyle::NTP_NON_INCOGNITO];
  self.overscrollActionsController.delegate = self.overscrollDelegate;
  [self updateOverscrollActionsState];
}

- (void)updateOverscrollActionsState {
  if (IsSplitToolbarMode(self)) {
    [self.overscrollActionsController enableOverscrollActions];
  } else {
    [self.overscrollActionsController disableOverscrollActions];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.headerSynchronizer.showing = YES;
  // Reload data to ensure the Most Visited tiles and fakeOmnibox are correctly
  // positionned, in particular during a rotation while a ViewController is
  // presented in front of the NTP.
  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.collectionView.collectionViewLayout invalidateLayout];
  // Ensure initial fake omnibox layout.
  [self.headerSynchronizer updateFakeOmniboxForScrollPosition];
  // TODO(crbug.com/1114792): Plumb the collection view.
  self.layout.parentCollectionView =
      static_cast<UICollectionView*>(self.view.superview);
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // Resize the collection as it might have been rotated while not being
  // presented (e.g. rotation on stack view).
  [self updateConstraints];
  // Remove forced height if it was already applied, since the scroll position
  // was already maintained.
  if (self.offset > 0) {
    self.layout.offset = 0;
  }

  [self.bubblePresenter presentDiscoverFeedHeaderTipBubble];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self applyContentOffset];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  self.headerSynchronizer.showing = NO;
  if (ShouldShowReturnToMostRecentTabForStartSurface()) {
    [self.audience viewDidDisappear];
  }
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent)
    return;
  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.parentViewController.view.bounds.size
                                      .width];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  void (^alongsideBlock)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.headerSynchronizer updateFakeOmniboxOnNewWidth:size.width];
        [self.collectionView.collectionViewLayout invalidateLayout];
      };
  [coordinator animateAlongsideTransition:alongsideBlock completion:nil];
}

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super willTransitionToTraitCollection:newCollection
               withTransitionCoordinator:coordinator];
  // Invalidating the layout after changing the cellStyle results in the layout
  // not being updated. Do it before to have it taken into account.
  [self.collectionView.collectionViewLayout invalidateLayout];
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self.collectionViewLayout invalidateLayout];
    [self.headerSynchronizer updateFakeOmniboxForScrollPosition];
  }
  [self.headerSynchronizer updateConstraints];
  [self updateOverscrollActionsState];
  if (previousTraitCollection.userInterfaceStyle !=
      self.traitCollection.userInterfaceStyle) {
    [self.themeChangeDelegate handleThemeChange];
  }
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];

  // Only get the bottom safe area inset.
  UIEdgeInsets insets = UIEdgeInsetsZero;
  insets.bottom = self.view.safeAreaInsets.bottom;
  self.collectionView.contentInset = insets;

  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.headerSynchronizer updateConstraints];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  [self.headerSynchronizer unfocusOmnibox];

  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch ([self.collectionUpdater contentSuggestionTypeForItem:item]) {
    case ContentSuggestionTypeReadingList:
      base::RecordAction(base::UserMetricsAction("MobileReadingListOpen"));
      [self.suggestionCommandHandler openPageForItemAtIndexPath:indexPath];
      break;
    case ContentSuggestionTypeArticle:
      [self.suggestionCommandHandler openPageForItemAtIndexPath:indexPath];
      break;
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
    case ContentSuggestionTypeLearnMore:
      [self.suggestionCommandHandler handleLearnMoreTapped];
      break;
    case ContentSuggestionTypeDiscover:
    case ContentSuggestionTypeEmpty:
      break;
  }
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  CSCollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  if ([self.collectionUpdater
          isDiscoverItem:[self.collectionViewModel
                             itemTypeForIndexPath:indexPath]]) {
    // TODO(crbug.com/1114792): Remove DiscoverItem logic once we stop
    // containing the DiscoverFeed inside a cell.
    ContentSuggestionsDiscoverItem* discoverFeedItem =
        static_cast<ContentSuggestionsDiscoverItem*>(item);
    UIViewController* newFeedViewController = discoverFeedItem.discoverFeed;

    if (newFeedViewController != self.discoverFeedVC) {
      // If previous VC is not nil, remove it from the view hierarchy.
      if (self.discoverFeedVC) {
        self.feedView = nil;
        [self.discoverFeedVC willMoveToParentViewController:nil];
        [self.discoverFeedVC.view removeFromSuperview];
        [self.discoverFeedVC removeFromParentViewController];
      }

      // If new VC is not nil, add it to the view hierarchy.
      if (newFeedViewController) {
        [self addChildViewController:newFeedViewController];
        UICollectionViewCell* cell = [super collectionView:collectionView
                                    cellForItemAtIndexPath:indexPath];
        [newFeedViewController didMoveToParentViewController:self];

        // Observe its CollectionView for contentSize changes.
        for (UIView* view in newFeedViewController.view.subviews) {
          if ([view isKindOfClass:[UICollectionView class]]) {
            self.feedView = static_cast<UICollectionView*>(view);
          }
        }
        self.discoverFeedVC = newFeedViewController;
        return cell;
      }
    }
  }

  if ([self.collectionUpdater isContentSuggestionsSection:indexPath.section] &&
      [self.collectionUpdater contentSuggestionTypeForItem:item] !=
          ContentSuggestionTypeEmpty &&
      !item.metricsRecorded) {
    [self.metricsRecorder
            onSuggestionShown:item
                  atIndexPath:indexPath
        suggestionsShownAbove:[self
                                  numberOfSuggestionsAbove:indexPath.section]];
    item.metricsRecorded = YES;
  }

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
                                         point:(CGPoint)point
    API_AVAILABLE(ios(13.0)) {
  if (!IsNativeContextMenuEnabled()) {
    // Returning nil will allow the gesture to be captured and show the old
    // context menus.
    return nil;
  }

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
          self.traitCollection.preferredContentSizeCategory) &&
      [self.collectionUpdater isContentSuggestionsSection:section]) {
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
    shouldHideItemSeparatorAtIndexPath:(NSIndexPath*)indexPath {
  // Show separators for all cells in content suggestion sections.
  return !
      [self.collectionUpdater isContentSuggestionsSection:indexPath.section];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideHeaderSeparatorForSection:(NSInteger)section {
  return [self.collectionUpdater shouldUseCustomStyleForSection:section];
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsSwipeToDismissItem:
    (UICollectionView*)collectionView {
  return YES;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canSwipeToDismissItemAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  return ![self.collectionUpdater isMostVisitedSection:indexPath.section] &&
         ![self.collectionUpdater isPromoSection:indexPath.section] &&
         ![self.collectionUpdater isDiscoverSection:indexPath.section] &&
         [self.collectionUpdater contentSuggestionTypeForItem:item] !=
             ContentSuggestionTypeLearnMore &&
         [self.collectionUpdater contentSuggestionTypeForItem:item] !=
             ContentSuggestionTypeEmpty;
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndSwipeToDismissItemAtIndexPath:(NSIndexPath*)indexPath {
  [self.collectionUpdater
      dismissItem:[self.collectionViewModel itemAtIndexPath:indexPath]];
  [self dismissEntryAtIndexPath:indexPath];
}

#pragma mark - ThumbStripSupporting

- (BOOL)isThumbStripEnabled {
  return self.panGestureHandler != nil;
}

- (void)thumbStripEnabledWithPanHandler:
    (ViewRevealingVerticalPanHandler*)panHandler {
  DCHECK(!self.thumbStripEnabled);
  self.panGestureHandler = panHandler;
}

- (void)thumbStripDisabled {
  DCHECK(self.thumbStripEnabled);
  self.panGestureHandler = nil;
}

#pragma mark - UIScrollViewDelegate Methods.

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];
  [self.panGestureHandler scrollViewDidScroll:scrollView];
  [self.overscrollActionsController scrollViewDidScroll:scrollView];
  [self.headerSynchronizer updateFakeOmniboxForScrollPosition];
  self.scrolledToTop =
      scrollView.contentOffset.y >= [self.headerSynchronizer pinnedOffsetY];

  if (IsDiscoverFeedEnabled() && self.contentSuggestionsEnabled) {
    if ([self shouldTriggerInfiniteFeed:scrollView]) {
      CGFloat currentHeight = self.feedView.contentSize.height;
      if (currentHeight != self.discoverFeedHeight) {
        self.discoverFeedHeight = currentHeight;
        [self.handler loadMoreFeedArticles];
      }
    }
  }
}

- (BOOL)scrollViewShouldScrollToTop:(UIScrollView*)scrollView {
  // User has tapped the status bar to scroll to the top.
  // Prevent scrolling back to pre-focus state, making sure we don't have
  // two scrolling animations running at the same time.
  [self.headerSynchronizer resetPreFocusOffset];
  // Unfocus omnibox without scrolling back.
  [self.headerSynchronizer unfocusOmnibox];
  return YES;
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [self.overscrollActionsController scrollViewWillBeginDragging:scrollView];
  [self.panGestureHandler scrollViewWillBeginDragging:scrollView];
  self.scrollStartPosition = scrollView.contentOffset.y;
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  [super scrollViewDidEndDragging:scrollView willDecelerate:decelerate];
  [self.overscrollActionsController scrollViewDidEndDragging:scrollView
                                              willDecelerate:decelerate];
  [self.panGestureHandler scrollViewDidEndDragging:scrollView
                                    willDecelerate:decelerate];
  if (IsDiscoverFeedEnabled()) {
    [self.discoverFeedMetricsRecorder
        recordFeedScrolled:scrollView.contentOffset.y -
                           self.scrollStartPosition];
  } else {
    [self.metricsRecorder recordFeedScrolled:scrollView.contentOffset.y -
                                             self.scrollStartPosition];
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  [super scrollViewWillEndDragging:scrollView
                      withVelocity:velocity
               targetContentOffset:targetContentOffset];
  [self.overscrollActionsController
      scrollViewWillEndDragging:scrollView
                   withVelocity:velocity
            targetContentOffset:targetContentOffset];
  [self.panGestureHandler scrollViewWillEndDragging:scrollView
                                       withVelocity:velocity
                                targetContentOffset:targetContentOffset];
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

#pragma mark - ContentSuggestionsConsumer

- (void)setContentSuggestionsEnabled:(BOOL)enabled {
  _contentSuggestionsEnabled = enabled;
}

- (void)setContentSuggestionsVisible:(BOOL)visible {
  [self.collectionUpdater changeDiscoverFeedHeaderVisibility:visible];
}

#pragma mark - NSKeyValueObserving

// TODO(crbug.com/1114792): Remove once we stop containing the DiscoverFeed
// inside a cell.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if (object == self.feedView && [keyPath isEqualToString:@"contentSize"]) {
    // Reload the CollectionView data to adjust to the new Feed height.
    [self.collectionView reloadData];
    // Indicates that the feed articles have been loaded by checking its height.
    // TODO(crbug.com/1126940): Use a callback from Mulder to determine this
    // more reliably.
    if (self.feedView.contentSize.height > kDiscoverFeedLoadedHeight) {
      [self.discoverFeedMenuHandler notifyFeedLoadedForHeaderMenu];
      [self.audience discoverFeedShown];
    }
  }
}

#pragma mark - Private

// |self.feedView| setter.
- (void)setFeedView:(UICollectionView*)feedView {
  if (feedView != _feedView) {
    [self removeContentSizeKVO];
    _feedView = feedView;
    [self addContentSizeKVO];
  }
}

// Adds KVO observing for the feedView contentSize if there is not one already.
- (void)addContentSizeKVO {
  if (!self.observingDiscoverFeedHeight) {
    [self.feedView addObserver:self
                    forKeyPath:@"contentSize"
                       options:0
                       context:nil];
    self.observingDiscoverFeedHeight = YES;
  }
}

// Removes KVO observing for the feedView contentSize if one exists.
- (void)removeContentSizeKVO {
  if (self.observingDiscoverFeedHeight) {
    [self.feedView removeObserver:self forKeyPath:@"contentSize"];
    self.observingDiscoverFeedHeight = NO;
  }
}

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
    case ContentSuggestionTypeArticle:
      [self.suggestionCommandHandler
          displayContextMenuForSuggestion:touchedItem
                                  atPoint:touchLocation
                              atIndexPath:touchedItemIndexPath
                          readLaterAction:YES];
      break;
    case ContentSuggestionTypeReadingList:
      [self.suggestionCommandHandler
          displayContextMenuForSuggestion:touchedItem
                                  atPoint:touchLocation
                              atIndexPath:touchedItemIndexPath
                          readLaterAction:NO];
      break;
    case ContentSuggestionTypeMostVisited:
      if (!IsNativeContextMenuEnabled()) {
        [self.suggestionCommandHandler
            displayContextMenuForMostVisitedItem:touchedItem
                                         atPoint:touchLocation
                                     atIndexPath:touchedItemIndexPath];
      }
      break;
    default:
      break;
  }

  if (IsRegularXRegularSizeClass(self))
    [self.headerSynchronizer unfocusOmnibox];
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

// Sets the collectionView's contentOffset if |_initialContentOffset| is set.
- (void)applyContentOffset {
  if (!isnan(_initialContentOffset)) {
    UICollectionView* collection = self.collectionView;
    // Don't set the offset such as the content of the collection is smaller
    // than the part of the collection which should be displayed with that
    // offset, taking into account the size of the toolbar.
    CGFloat offset = MAX(
        0, MIN(_initialContentOffset,
               collection.contentSize.height - collection.bounds.size.height -
                   ToolbarExpandedHeight(
                       self.traitCollection.preferredContentSizeCategory) +
                   collection.contentInset.bottom));
    if (collection.contentOffset.y != offset) {
        collection.contentOffset = CGPointMake(0, offset);
        // Update the constraints in case the omnibox needs to be moved.
        [self updateConstraints];
    }
  }
  _initialContentOffset = NAN;
}

// Opens top-level feed menu when pressing |menuButton|.
- (void)openDiscoverFeedMenu {
  [self.discoverFeedMenuHandler openDiscoverFeedMenu];
}

// Evaluates whether or not another set of Discover feed articles should be
// fetched when scrolling.
- (BOOL)shouldTriggerInfiniteFeed:(UIScrollView*)scrollView {
  float scrollPosition =
      scrollView.contentOffset.y + scrollView.frame.size.height;
  // Check if view is bouncing to ignore overscoll positions for infinite feed
  // triggering.
  BOOL isBouncing =
      (scrollView.contentOffset.y >=
       (scrollView.contentSize.height - scrollView.bounds.size.height));
  ContentSuggestionsLayout* layout = static_cast<ContentSuggestionsLayout*>(
      self.collectionView.collectionViewLayout);
  return (scrollPosition > scrollView.contentSize.height - kPaginationOffset &&
          scrollPosition > layout.ntpHeight && !isBouncing);
}

@end
