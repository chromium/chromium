// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/format_macros.h"
#import "base/logging.h"
#import "base/metrics/histogram_macros.h"
#import "base/time/time.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_with_payload.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"
#import "ios/chrome/browser/shared/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/carousel_item.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/omnibox_popup_carousel_cell.h"
#import "ios/chrome/browser/ui/omnibox/popup/content_providing.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_match_preview_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/omnibox_popup_actions_row_content_configuration.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/omnibox_popup_actions_row_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/suggest_action.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_content_configuration.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_delegate.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

namespace {
const CGFloat kTopPadding = 8.0;
const CGFloat kBottomPadding = 8.0;
const CGFloat kFooterHeight = 4.0;
/// Percentage of the suggestion height that needs to be visible in order to
/// consider the suggestion as visible.
const CGFloat kVisibleSuggestionThreshold = 0.6;
/// Minimum size of the fetched favicon for tiles.
const CGFloat kMinTileFaviconSize = 32.0f;
/// Maximum size of the fetched favicon for tiles.
const CGFloat kMaxTileFaviconSize = 48.0f;

/// Bottom padding for table view headers.
const CGFloat kHeaderPaddingBottom = 10.0f;
/// Leading and trailing padding for table view headers.
const CGFloat kHeaderPadding = 2.0f;
/// Top padding for table view headers.
const CGFloat kHeaderTopPadding = 16.0f;

}  // namespace

@interface OmniboxPopupViewController () <OmniboxPopupActionsRowDelegate,
                                          OmniboxPopupCarouselCellDelegate,
                                          OmniboxPopupRowDelegate,
                                          UITableViewDataSource,
                                          UITableViewDelegate>

/// Index path of currently highlighted row. The rows can be highlighted by
/// tapping and holding on them or by using arrow keys on a hardware keyboard.
@property(nonatomic, strong) NSIndexPath* highlightedIndexPath;

/// Flag that enables forwarding scroll events to the delegate. Disabled while
/// updating the cells to avoid defocusing the omnibox when the omnibox popup
/// changes size and table view issues a scroll event.
@property(nonatomic, assign) BOOL forwardsScrollEvents;

/// The height of the keyboard. Used to determine the content inset for the
/// scroll view.
@property(nonatomic, assign) CGFloat keyboardHeight;

/// Time the view appeared on screen. Used to record a metric of how long this
/// view controller was on screen.
@property(nonatomic, assign) base::TimeTicks viewAppearanceTime;
/// Table view that displays the results.
@property(nonatomic, strong) UITableView* tableView;

/// Alignment of omnibox text. Popup text should match this alignment.
@property(nonatomic, assign) NSTextAlignment alignment;

/// Semantic content attribute of omnibox text. Popup should match this
/// attribute. This is used by the new omnibox popup.
@property(nonatomic, assign)
    UISemanticContentAttribute semanticContentAttribute;

/// Estimated maximum number of visible suggestions.
/// Only updated in `newResultsAvailable` method, were the value is used.
@property(nonatomic, assign) NSUInteger visibleSuggestionCount;

/// Boolean to update visible suggestion count only once on event such as device
/// orientation change or multitasking window change, where multiple keyboard
/// and view updates are received.
@property(nonatomic, assign) BOOL shouldUpdateVisibleSuggestionCount;

/// Index of the suggestion group that contains the first suggestion to preview
/// and highlight.
@property(nonatomic, assign) NSUInteger preselectedMatchGroupIndex;

/// Provider used to fetch carousel favicons.
@property(nonatomic, strong)
    FaviconAttributesProvider* carouselAttributeProvider;

/// UITableViewCell displaying the most visited carousel in (Web and SRP) ZPS
/// state.
@property(nonatomic, strong) OmniboxPopupCarouselCell* carouselCell;

/// Flag that tracks if the carousel should be hidden. It is only true when we
/// show the carousel, then the user deletes every item in it before the UI has
/// updated.
@property(nonatomic, assign) BOOL shouldHideCarousel;

/// Cached `tableView.visibleContentSize.height` used in `viewDidLayoutSubviews`
/// to avoid infinite loop and redudant computation when updating table view's
/// content inset.
@property(nonatomic, assign) CGFloat cachedContentHeight;

/// Layout guide that tracks the position of the omnibox in the top toolbar.
/// This is useful to add constraints to, or to derive manual layout values off
/// of.
@property(nonatomic, readonly) UILayoutGuide* omniboxGuide;

@end

@implementation OmniboxPopupViewController

@synthesize omniboxGuide = _omniboxGuide;

- (instancetype)init {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    _forwardsScrollEvents = YES;
    _preselectedMatchGroupIndex = 0;
    _visibleSuggestionCount = 0;
    _cachedContentHeight = 0;
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    // Listen to keyboard frame change event to detect keyboard frame changes
    // (ex: when changing input method) to update the estimated number of
    // visible suggestions.
    [defaultCenter addObserver:self
                      selector:@selector(keyboardDidChangeFrame:)
                          name:UIKeyboardDidChangeFrameNotification
                        object:nil];

    // Listen to content size change to update the estimated number of visible
    // suggestions.
    [defaultCenter addObserver:self
                      selector:@selector(contentSizeDidChange:)
                          name:UIContentSizeCategoryDidChangeNotification
                        object:nil];
  }
  return self;
}

- (void)loadView {
  // TODO(crbug.com/40866206): Check why largeIconService not available in
  // incognito.
  if (self.largeIconService) {
    _carouselAttributeProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kMaxTileFaviconSize
             minFaviconSize:kMinTileFaviconSize
           largeIconService:self.largeIconService];
    _carouselAttributeProvider.cache = self.largeIconCache;
  }
  self.tableView =
      [[SelfSizingTableView alloc] initWithFrame:CGRectZero
                                           style:UITableViewStyleGrouped];
  self.tableView.delegate = self;
  self.tableView.dataSource = self;
  self.view = self.tableView;
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateUIOnTraitChange];
}
#endif

- (void)toggleOmniboxDebuggerView {
  if (self.debugInfoViewController.viewIfLoaded.window) {
    [self dismissViewControllerAnimated:YES completion:nil];
  } else {
    [self showDebugUI];
  }
}

#pragma mark - Getter/Setter

- (void)setHighlightedIndexPath:(NSIndexPath*)highlightedIndexPath {
  // Special case for highlight moving inside a carousel-style section.
  if (_highlightedIndexPath &&
      highlightedIndexPath.section == _highlightedIndexPath.section &&
      self.currentResult[highlightedIndexPath.section].displayStyle ==
          SuggestionGroupDisplayStyleCarousel) {
    // The highlight moved inside the section horizontally. No need to
    // unhighlight the previous row. Just notify the delegate.
    _highlightedIndexPath = highlightedIndexPath;
    [self didHighlightSelectedSuggestion];
    return;
  }

  // General case: highlighting moved between different rows.
  if (_highlightedIndexPath) {
    [self unhighlightRowAtIndexPath:_highlightedIndexPath];
  }
  _highlightedIndexPath = highlightedIndexPath;
  if (highlightedIndexPath) {
    [self highlightRowAtIndexPath:_highlightedIndexPath];
    [self didHighlightSelectedSuggestion];
  }
}

- (OmniboxPopupCarouselCell*)carouselCell {
  if (!_carouselCell) {
    _carouselCell = [[OmniboxPopupCarouselCell alloc] init];
    _carouselCell.delegate = self;
    _carouselCell.menuProvider = self.carouselMenuProvider;
  }
  return _carouselCell;
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      kOmniboxPopupTableViewAccessibilityIdentifier;
  self.tableView.insetsContentViewsToSafeArea = YES;

  // Initialize the same size as the parent view, autoresize will correct this.
  [self.view setFrame:CGRectZero];
  [self.view setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight)];

  [self updateBackgroundColor];

  // Table configuration.
  self.tableView.allowsMultipleSelectionDuringEditing = NO;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.separatorInset = UIEdgeInsetsZero;
  if ([self.tableView respondsToSelector:@selector(setLayoutMargins:)]) {
    [self.tableView setLayoutMargins:UIEdgeInsetsZero];
  }
  self.tableView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentAutomatic;

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    self.tableView.tableFooterView =
        [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, FLT_MIN)];
    [self.tableView
        setDirectionalLayoutMargins:NSDirectionalEdgeInsetsMake(
                                        kTopPadding, 0, kBottomPadding, 0)];
    self.tableView.contentInset =
        UIEdgeInsetsMake(kTopPadding, 0, kBottomPadding, 0);
  } else {
    [self.tableView setDirectionalLayoutMargins:NSDirectionalEdgeInsetsMake(
                                                    0, 0, kBottomPadding, 0)];
    self.tableView.contentInset = UIEdgeInsetsMake(kTopPadding, 0, 0, 0);
  }

  self.tableView.sectionHeaderHeight = 0.1;
  self.tableView.estimatedRowHeight = 0;

  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = kOmniboxPopupCellMinimumHeight;

  [self.tableView registerClass:[UITableViewCell class]
         forCellReuseIdentifier:OmniboxPopupRowCellReuseIdentifier];
  [self.tableView registerClass:[UITableViewCell class]
         forCellReuseIdentifier:OmniboxPopupActionsRowCellReuseIdentifier];
  [self.tableView registerClass:[UITableViewHeaderFooterView class]
      forHeaderFooterViewReuseIdentifier:NSStringFromClass(
                                             [UITableViewHeaderFooterView
                                                 class])];
  self.shouldUpdateVisibleSuggestionCount = YES;
  self.tableView.sectionHeaderTopPadding = 0;

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(nil);
    [self registerForTraitChanges:traits
                       withAction:@selector(updateUIOnTraitChange)];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self adjustMarginsToMatchOmniboxWidth];

  self.viewAppearanceTime = base::TimeTicks::Now();
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  UMA_HISTOGRAM_MEDIUM_TIMES("MobileOmnibox.PopupOpenDuration",
                             base::TimeTicks::Now() - self.viewAppearanceTime);
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self.tableView setEditing:NO animated:NO];
  self.shouldUpdateVisibleSuggestionCount = YES;

  __weak __typeof__(self) weakSelf = self;

  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf adjustMarginsToMatchOmniboxWidth];
      }
      completion:^(id<UIViewControllerTransitionCoordinatorContext>) {
        // Make sure the margins are correct after the animation.
        [weakSelf adjustMarginsToMatchOmniboxWidth];
      }];
}

- (void)adjustMarginsToMatchOmniboxWidth {
  if (!self.omniboxGuide) {
    return;
  }

  // Adjust the carousel to be aligned with the omnibox textfield.
  UIEdgeInsets margins = self.carouselCell.layoutMargins;
  self.carouselCell.layoutMargins =
      UIEdgeInsetsMake(margins.top, 0, margins.bottom, 0);

  // Update the headers padding.
  for (NSInteger i = 0; i < self.tableView.numberOfSections; ++i) {
    UITableViewHeaderFooterView* headerView =
        [self.tableView headerViewForSection:i];
    [headerView setNeedsUpdateConfiguration];
  }

  // Update cells' configuration to realign the text to the omnibox.
  for (UITableViewCell* cell in self.tableView.visibleCells) {
    if ([cell.contentConfiguration
            isKindOfClass:OmniboxPopupRowContentConfiguration.class]) {
      [cell setNeedsUpdateConfiguration];
    }
  }
}

#pragma mark - AutocompleteResultConsumer

- (void)updateMatches:(NSArray<id<AutocompleteSuggestionGroup>>*)result
    preselectedMatchGroupIndex:(NSInteger)groupIndex {
  DCHECK(groupIndex == 0 || groupIndex < (NSInteger)result.count);
  self.shouldHideCarousel = NO;
  self.forwardsScrollEvents = NO;
  // Reset highlight state.
  self.highlightedIndexPath = nil;

  self.preselectedMatchGroupIndex = groupIndex;
  self.currentResult = result;

  [self.tableView reloadData];
  self.forwardsScrollEvents = YES;
  id<AutocompleteSuggestion> firstSuggestionOfPreselectedGroup =
      [self suggestionAtIndexPath:[NSIndexPath indexPathForRow:0
                                                     inSection:groupIndex]];
  [self.matchPreviewDelegate
      setPreviewSuggestion:firstSuggestionOfPreselectedGroup
             isFirstUpdate:YES];
}

/// Set text alignment for popup cells.
- (void)setTextAlignment:(NSTextAlignment)alignment {
  self.alignment = alignment;
}

- (void)setDebugInfoViewController:(UIViewController*)viewController {
  DCHECK(experimental_flags::IsOmniboxDebuggingEnabled());
  _debugInfoViewController = viewController;

  UITapGestureRecognizer* debugGestureRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(showDebugUI)];
#if TARGET_OS_SIMULATOR
  // One tap for easy trigger on simulator.
  debugGestureRecognizer.numberOfTapsRequired = 1;
#else
  debugGestureRecognizer.numberOfTapsRequired = 2;
#endif
  debugGestureRecognizer.numberOfTouchesRequired = 2;
  [self.view addGestureRecognizer:debugGestureRecognizer];
}

- (void)newResultsAvailable {
  if (self.shouldUpdateVisibleSuggestionCount) {
    [self updateVisibleSuggestionCount];
  }
  [self.dataSource
      requestResultsWithVisibleSuggestionCount:self.visibleSuggestionCount];
}

#pragma mark - OmniboxKeyboardDelegate

- (BOOL)canPerformKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  UITableViewCell* cell =
      [self.tableView cellForRowAtIndexPath:self.highlightedIndexPath];

  BOOL isActionsRowCell = [cell.contentConfiguration
      isKindOfClass:OmniboxPopupActionsRowContentConfiguration.class];

  if (isActionsRowCell) {
    OmniboxPopupActionsRowContentConfiguration* configuration =
        base::apple::ObjCCastStrict<OmniboxPopupActionsRowContentConfiguration>(
            cell.contentConfiguration);
    if ([configuration canPerformKeyboardAction:keyboardAction]) {
      return YES;
    }
  }

  switch (keyboardAction) {
    case OmniboxKeyboardActionUpArrow:
    case OmniboxKeyboardActionDownArrow:
      return YES;
    case OmniboxKeyboardActionLeftArrow:
    case OmniboxKeyboardActionRightArrow:
      if (self.carouselCell.isHighlighted) {
        return [self.carouselCell canPerformKeyboardAction:keyboardAction];
      }
      return NO;
  }
}

- (void)performKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  DCHECK([self canPerformKeyboardAction:keyboardAction]);

  UITableViewCell* cell =
      [self.tableView cellForRowAtIndexPath:self.highlightedIndexPath];

  BOOL isActionsRowCell = [cell.contentConfiguration
      isKindOfClass:OmniboxPopupActionsRowContentConfiguration.class];

  if (isActionsRowCell) {
    OmniboxPopupActionsRowContentConfiguration* configuration =
        base::apple::ObjCCastStrict<OmniboxPopupActionsRowContentConfiguration>(
            cell.contentConfiguration);
    if ([configuration canPerformKeyboardAction:keyboardAction]) {
      [configuration performKeyboardAction:keyboardAction];
      cell.contentConfiguration = configuration;
      return;
    }
  }

  switch (keyboardAction) {
    case OmniboxKeyboardActionUpArrow:
      [self highlightPreviousSuggestion];
      break;
    case OmniboxKeyboardActionDownArrow:
      [self highlightNextSuggestion];
      break;
    case OmniboxKeyboardActionLeftArrow:
    case OmniboxKeyboardActionRightArrow:
      if (self.carouselCell.isHighlighted) {
        DCHECK(self.highlightedIndexPath.section ==
               [self.tableView indexPathForCell:self.carouselCell].section);

        [self.carouselCell performKeyboardAction:keyboardAction];
        NSInteger highlightedTileIndex = self.carouselCell.highlightedTileIndex;
        if (highlightedTileIndex == NSNotFound) {
          self.highlightedIndexPath = nil;
        } else {
          self.highlightedIndexPath =
              [NSIndexPath indexPathForRow:highlightedTileIndex
                                 inSection:self.highlightedIndexPath.section];
        }
      }
      break;
  }
}

#pragma mark OmniboxKeyboardDelegate Private

- (void)highlightPreviousSuggestion {
  NSIndexPath* path = self.highlightedIndexPath;
  if (path == nil) {
    // If there is a section above `preselectedMatchGroupIndex` select the last
    // suggestion of this section.
    if (self.preselectedMatchGroupIndex > 0 &&
        self.currentResult.count > self.preselectedMatchGroupIndex - 1) {
      NSInteger sectionAbovePreselectedGroup =
          self.preselectedMatchGroupIndex - 1;
      NSIndexPath* suggestionIndex = [NSIndexPath
          indexPathForRow:(NSInteger)self
                              .currentResult[sectionAbovePreselectedGroup]
                              .suggestions.count -
                          1
                inSection:sectionAbovePreselectedGroup];
      if ([self suggestionAtIndexPath:suggestionIndex]) {
        self.highlightedIndexPath = suggestionIndex;
      }
    }
    return;
  }

  id<AutocompleteSuggestionGroup> suggestionGroup =
      self.currentResult[self.highlightedIndexPath.section];
  BOOL isCurrentHighlightedRowFirstInSection =
      suggestionGroup.displayStyle == SuggestionGroupDisplayStyleCarousel ||
      (path.row == 0);

  if (isCurrentHighlightedRowFirstInSection) {
    NSInteger previousSection = path.section - 1;
    NSInteger previousSectionCount =
        (previousSection >= 0)
            ? [self.tableView numberOfRowsInSection:previousSection]
            : 0;
    BOOL prevSectionHasItems = previousSectionCount > 0;
    if (prevSectionHasItems) {
      path = [NSIndexPath indexPathForRow:previousSectionCount - 1
                                inSection:previousSection];
    } else {
      // Can't move up from first row. Call the delegate again so that the
      // inline autocomplete text is set again (in case the user exited the
      // inline autocomplete).
      [self didHighlightSelectedSuggestion];
      return;
    }
  } else {
    path = [NSIndexPath indexPathForRow:path.row - 1 inSection:path.section];
  }

  [self.tableView scrollToRowAtIndexPath:path
                        atScrollPosition:UITableViewScrollPositionTop
                                animated:NO];

  self.highlightedIndexPath = path;
}

- (void)highlightNextSuggestion {
  if (!self.highlightedIndexPath) {
    NSIndexPath* preselectedSuggestionIndex =
        [NSIndexPath indexPathForRow:0
                           inSection:self.preselectedMatchGroupIndex];
    if ([self suggestionAtIndexPath:preselectedSuggestionIndex]) {
      self.highlightedIndexPath = preselectedSuggestionIndex;
    }
    return;
  }

  NSIndexPath* path = self.highlightedIndexPath;
  id<AutocompleteSuggestionGroup> suggestionGroup =
      self.currentResult[self.highlightedIndexPath.section];
  BOOL isCurrentHighlightedRowLastInSection =
      suggestionGroup.displayStyle == SuggestionGroupDisplayStyleCarousel ||
      path.row == [self.tableView numberOfRowsInSection:path.section] - 1;
  if (isCurrentHighlightedRowLastInSection) {
    NSInteger nextSection = path.section + 1;
    BOOL nextSectionHasItems =
        [self.tableView numberOfSections] > nextSection &&
        [self.tableView numberOfRowsInSection:nextSection] > 0;

    if (nextSectionHasItems) {
      path = [NSIndexPath indexPathForRow:0 inSection:nextSection];
    } else {
      // Can't go below last row. Call the delegate again so that the inline
      // autocomplete text is set again (in case the user exited the inline
      // autocomplete).
      [self didHighlightSelectedSuggestion];
      return;
    }
  } else {
    path = [NSIndexPath indexPathForRow:path.row + 1 inSection:path.section];
  }

  [self.tableView scrollToRowAtIndexPath:path
                        atScrollPosition:UITableViewScrollPositionBottom
                                animated:NO];

  // There is a row below, move highlight there.
  self.highlightedIndexPath = path;
}

- (void)highlightRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.currentResult[indexPath.section].displayStyle ==
      SuggestionGroupDisplayStyleCarousel) {
    indexPath = [NSIndexPath indexPathForRow:0 inSection:indexPath.section];
  }
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setHighlighted:YES animated:NO];
}

- (void)unhighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.currentResult[indexPath.section].displayStyle ==
      SuggestionGroupDisplayStyleCarousel) {
    indexPath = [NSIndexPath indexPathForRow:0 inSection:indexPath.section];
  }
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setHighlighted:NO animated:NO];
}

- (void)didHighlightSelectedSuggestion {
  id<AutocompleteSuggestion> suggestion =
      [self suggestionAtIndexPath:self.highlightedIndexPath];
  DCHECK(suggestion);
  [self.matchPreviewDelegate setPreviewSuggestion:suggestion isFirstUpdate:NO];
}

#pragma mark - OmniboxPopupRowDelegate

- (void)omniboxPopupRowWithConfiguration:
            (OmniboxPopupRowContentConfiguration*)configuration
         didTapTrailingButtonAtIndexPath:(NSIndexPath*)indexPath {
  id<AutocompleteSuggestion> suggestion =
      [self suggestionAtIndexPath:indexPath];
  if (suggestion != configuration.suggestion) {
    return;
  }
  [self.delegate autocompleteResultConsumer:self
           didTapTrailingButtonOnSuggestion:suggestion
                                      inRow:indexPath.row];
}

- (void)omniboxPopupRowWithConfiguration:
            (OmniboxPopupRowContentConfiguration*)configuration
    didUpdateAccessibilityActionsAtIndexPath:(NSIndexPath*)indexPath {
  id<AutocompleteSuggestion> suggestion =
      [self suggestionAtIndexPath:indexPath];
  if (suggestion != configuration.suggestion) {
    return;
  }
  // Actions reference the configuration that created them. When applying a
  // new configuration to the content view, also update the actions to avoid
  // retaining the old configuration.
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  cell.accessibilityCustomActions = configuration.accessibilityCustomActions;
}

#pragma mark - OmniboxPopupActionsRowDelegate

- (void)omniboxPopupRowActionSelectedWithConfiguration:
            (OmniboxPopupActionsRowContentConfiguration*)configuration
                                                action:(SuggestAction*)action {
  id<AutocompleteSuggestion> suggestion =
      [self suggestionAtIndexPath:configuration.indexPath];

  CHECK(suggestion == configuration.suggestion);

  [self.delegate autocompleteResultConsumer:self
                  didSelectSuggestionAction:action
                                 suggestion:suggestion
                                      inRow:configuration.indexPath.row];
}

#pragma mark - OmniboxReturnDelegate

- (void)omniboxReturnPressed:(id)sender {
  if (self.highlightedIndexPath) {
    id<AutocompleteSuggestion> suggestion =
        [self suggestionAtIndexPath:self.highlightedIndexPath];

    UITableViewCell* cell =
        [self.tableView cellForRowAtIndexPath:self.highlightedIndexPath];
    BOOL isActionsRowCell = [cell.contentConfiguration
        isKindOfClass:OmniboxPopupActionsRowContentConfiguration.class];

    if (isActionsRowCell) {
      OmniboxPopupActionsRowContentConfiguration* config =
          base::apple::ObjCCastStrict<
              OmniboxPopupActionsRowContentConfiguration>(
              cell.contentConfiguration);
      if (config.highlightedActionIndex != NSNotFound) {
        [config omniboxReturnPressed:sender];
        return;
      }
    }

    if (suggestion) {
      NSInteger absoluteRow =
          [self absoluteRowIndexForIndexPath:self.highlightedIndexPath];
      [self.delegate autocompleteResultConsumer:self
                            didSelectSuggestion:suggestion
                                          inRow:absoluteRow];
      return;
    }
  }
  [self.acceptReturnDelegate omniboxReturnPressed:sender];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell.contentConfiguration
          isKindOfClass:OmniboxPopupRowContentConfiguration.class]) {
    cell.accessibilityIdentifier = [OmniboxPopupAccessibilityIdentifierHelper
        accessibilityIdentifierForRowAtIndexPath:indexPath];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSUInteger row = indexPath.row;
  NSUInteger section = indexPath.section;

  // In rare cases when the device is slow, user might be able to tap a
  // suggestion row twice before the event is being delivered. In this case, on
  // the second touch, the popup will already be cleared, but the table view
  // will still dispatch a didSelectRowAtIndexPath event for a non-existent
  // index path. Ignore these double touches.
  if (section >= self.currentResult.count ||
      row >= self.currentResult[indexPath.section].suggestions.count)
    return;
  NSInteger absoluteRow = [self absoluteRowIndexForIndexPath:indexPath];
  [self.delegate
      autocompleteResultConsumer:self
             didSelectSuggestion:[self suggestionAtIndexPath:indexPath]
                           inRow:absoluteRow];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  BOOL hasTitle = self.currentResult[section].title.length > 0;
  return hasTitle ? UITableViewAutomaticDimension : FLT_MIN;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  // Don't show the footer on the last section, to not increase the size of the
  // popup on iPad.
  if (section == (tableView.numberOfSections - 1)) {
    return FLT_MIN;
  }

  // When most visited tiles are enabled, only allow section separator under the
  // verbatim suggestion.
  if (section > 0) {
    return FLT_MIN;
  }

  return kFooterHeight;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  // Do not show footer for the last section
  if (section == (tableView.numberOfSections - 1)) {
    return nil;
  }
  // Do not show footer when there is a header for the next section.
  if (self.currentResult[section + 1].title.length > 0) {
    return nil;
  }

  UIView* footer = [[UIView alloc] init];
  footer.backgroundColor = tableView.backgroundColor;
  UIView* hairline = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, tableView.bounds.size.width,
                               2 / tableView.window.screen.scale)];

  hairline.backgroundColor = [UIColor
      colorNamed:ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
                     ? kOmniboxPopoutSuggestionRowSeparatorColor
                     : kOmniboxSuggestionRowSeparatorColor];
  [footer addSubview:hairline];
  hairline.autoresizingMask = UIViewAutoresizingFlexibleWidth;

  return footer;
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return self.currentResult.count;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  switch (self.currentResult[section].displayStyle) {
    case SuggestionGroupDisplayStyleDefault:
      return self.currentResult[section].suggestions.count;
    case SuggestionGroupDisplayStyleCarousel:
      if (self.shouldHideCarousel) {
        return 0;
      }

      // The carousel displays suggestions on one row.
      return 1;
  }
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // iOS doesn't check -numberOfRowsInSection before checking
  // -canEditRowAtIndexPath in a reload call. If `indexPath.row` is too large,
  // simple return `NO`.
  if ((NSUInteger)indexPath.row >=
      self.currentResult[indexPath.section].suggestions.count)
    return NO;

  if (self.currentResult[indexPath.section].displayStyle ==
      SuggestionGroupDisplayStyleCarousel) {
    return NO;
  }

  return [self.currentResult[indexPath.section].suggestions[indexPath.row]
      supportsDeletion];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_LT((NSUInteger)indexPath.row,
            self.currentResult[indexPath.section].suggestions.count);
  id<AutocompleteSuggestion> suggestion =
      [self suggestionAtIndexPath:indexPath];
  DCHECK(suggestion);
  if (editingStyle == UITableViewCellEditingStyleDelete) {
    [self.delegate autocompleteResultConsumer:self
               didSelectSuggestionForDeletion:suggestion
                                        inRow:indexPath.row];
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewAutomaticDimension;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  NSString* title = self.currentResult[section].title;
  if (!title) {
    return nil;
  }

  UITableViewHeaderFooterView* header =
      [tableView dequeueReusableHeaderFooterViewWithIdentifier:
                     NSStringFromClass([UITableViewHeaderFooterView class])];

  UIListContentConfiguration* contentConfiguration =
      header.defaultContentConfiguration;

  contentConfiguration.text = title;
  contentConfiguration.textProperties.font =
      [UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
  contentConfiguration.textProperties.transform =
      UIListContentTextTransformUppercase;
  contentConfiguration.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      kHeaderTopPadding, kHeaderPadding, kHeaderPaddingBottom, kHeaderPadding);

  __weak __typeof__(self) weakSelf = self;
  UITableViewHeaderFooterViewConfigurationUpdateHandler configurationUpdater =
      ^void(__kindof UITableViewHeaderFooterView* headerView,
            UIViewConfigurationState* state) {
        __typeof__(self) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        // Inset the header to match the omnibox width, similar to
        // `adjustMarginsToMatchOmniboxWidth` method.
        CGFloat leadingPadding = kHeaderPadding;
        if (IsRegularXRegularSizeClass(strongSelf) && strongSelf.omniboxGuide) {
          leadingPadding += CGRectGetMinX(weakSelf.omniboxGuide.layoutFrame);
        }

        UIListContentConfiguration* configurationCopy =
            (UIListContentConfiguration*)headerView.contentConfiguration;
        configurationCopy.directionalLayoutMargins =
            NSDirectionalEdgeInsetsMake(kHeaderTopPadding, leadingPadding,
                                        kHeaderPaddingBottom, kHeaderPadding);
        headerView.contentConfiguration = configurationCopy;
      };
  header.contentConfiguration = contentConfiguration;
  header.configurationUpdateHandler = configurationUpdater;
  return header;
}

- (void)tableView:(UITableView*)tableView
    didEndDisplayingCell:(UITableViewCell*)cell
       forRowAtIndexPath:(NSIndexPath*)indexPath {
  // Action in suggest buttons respond to touch-up events which could be
  // triggered after the cell was removed (see b/350911243).
  // Remove the delegate from a cell when it is no longer visible, ensuring that
  // actions are not dispatched for stale cells.
  BOOL isActionsRowCell = [cell.contentConfiguration
      isKindOfClass:OmniboxPopupActionsRowContentConfiguration.class];

  if (!isActionsRowCell) {
    return;
  }

  OmniboxPopupActionsRowContentConfiguration* configuration =
      base::apple::ObjCCastStrict<OmniboxPopupActionsRowContentConfiguration>(
          cell.contentConfiguration);

  configuration.delegate = nil;
}

/// Customize the appearance of table view cells.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_LT((NSUInteger)indexPath.row,
            self.currentResult[indexPath.section].suggestions.count);

  switch (self.currentResult[indexPath.section].displayStyle) {
    case SuggestionGroupDisplayStyleDefault: {
      id<AutocompleteSuggestion> suggestion =
          self.currentResult[indexPath.section].suggestions[indexPath.row];

        UITableViewCell* cell;
        OmniboxPopupRowContentConfiguration* configuration;

        if (base::FeatureList::IsEnabled(kOmniboxActionsInSuggest) &&
            suggestion.actionsInSuggest.count > 0) {
          cell = [self.tableView dequeueReusableCellWithIdentifier:
                                     OmniboxPopupActionsRowCellReuseIdentifier
                                                      forIndexPath:indexPath];
          configuration =
              [OmniboxPopupActionsRowContentConfiguration cellConfiguration];
        } else {
          cell = [self.tableView dequeueReusableCellWithIdentifier:
                                     OmniboxPopupRowCellReuseIdentifier
                                                      forIndexPath:indexPath];
          configuration =
              [OmniboxPopupRowContentConfiguration cellConfiguration];
        }

        DCHECK(cell);
        DCHECK(configuration);
        configuration.suggestion = suggestion;
        configuration.delegate = self;
        configuration.indexPath = indexPath;
        configuration.showSeparator =
            (NSUInteger)indexPath.row <
            self.currentResult[indexPath.section].suggestions.count - 1;
        configuration.semanticContentAttribute = self.semanticContentAttribute;
        configuration.faviconRetriever = self.faviconRetriever;
        configuration.imageRetriever = self.imageRetriever;

        [cell setContentConfiguration:configuration];
        cell.backgroundConfiguration =
            [UIBackgroundConfiguration clearConfiguration];
        return cell;
    }
    case SuggestionGroupDisplayStyleCarousel: {
      NSArray<CarouselItem*>* carouselItems = [self
          carouselItemsFromSuggestionGroup:self.currentResult[indexPath.section]
                            groupIndexPath:indexPath];
      [self.carouselCell setupWithCarouselItems:carouselItems];
      for (CarouselItem* item in carouselItems) {
        [self fetchFaviconForCarouselItem:item];
      }
      return self.carouselCell;
    }
  }
}

#pragma mark - OmniboxPopupCarouselCellDelegate

- (void)carouselCellDidChangeItemCount:(OmniboxPopupCarouselCell*)carouselCell {
  if (carouselCell.tileCount == 0) {
    // Hide the carousel row.
    self.shouldHideCarousel = YES;
    NSInteger carouselSection =
        [self.tableView indexPathForCell:self.carouselCell].section;
    [self.tableView
        deleteRowsAtIndexPaths:@[ [NSIndexPath
                                   indexPathForRow:0
                                         inSection:carouselSection] ]
              withRowAnimation:UITableViewRowAnimationAutomatic];
    [self resetHighlighting];
    return;
  }

  if (self.highlightedIndexPath.section !=
      [self.tableView indexPathForCell:self.carouselCell].section) {
    return;
  }

  // Defensively update highlightedIndexPath, because the highlighted tile might
  // have been removed.
  NSInteger highlightedTileIndex = self.carouselCell.highlightedTileIndex;
  if (highlightedTileIndex == NSNotFound) {
    [self resetHighlighting];
  } else {
    self.highlightedIndexPath =
        [NSIndexPath indexPathForRow:highlightedTileIndex
                           inSection:self.highlightedIndexPath.section];
  }
}

- (void)carouselCell:(OmniboxPopupCarouselCell*)carouselCell
    didTapCarouselItem:(CarouselItem*)carouselItem {
  id<AutocompleteSuggestion> suggestion =
      [self suggestionAtIndexPath:carouselItem.indexPath];
  DCHECK(suggestion);

  NSInteger absoluteRow =
      [self absoluteRowIndexForIndexPath:carouselItem.indexPath];
  [self.delegate autocompleteResultConsumer:self
                        didSelectSuggestion:suggestion
                                      inRow:absoluteRow];
}

#pragma mark - Internal API methods

/// Reset the highlighting to the first suggestion when it's available. Reset
/// to nil otherwise.
- (void)resetHighlighting {
  if (self.currentResult.firstObject.suggestions.count > 0) {
    self.highlightedIndexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  } else {
    self.highlightedIndexPath = nil;
  }
}

/// Updates the color of the background based on the incognito-ness and the size
/// class.
- (void)updateBackgroundColor {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    return;
  }

  self.view.backgroundColor = [UIColor clearColor];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // TODO(crbug.com/41325585): Default to the dragging check once it's been
  // tested on trunk.
  if (!scrollView.dragging)
    return;

  // TODO(crbug.com/40604984): The following call chain ultimately just
  // dismisses the keyboard, but involves many layers of plumbing, and should be
  // refactored.
  if (self.forwardsScrollEvents)
    [self.delegate autocompleteResultConsumerDidScroll:self];

  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
}

#pragma mark - Keyboard events

- (void)keyboardDidChangeFrame:(NSNotification*)notification {
  CGFloat keyboardHeight =
      [KeyboardObserverHelper keyboardHeightInWindow:self.tableView.window];
  if (self.keyboardHeight != keyboardHeight) {
    self.keyboardHeight = keyboardHeight;
    self.shouldUpdateVisibleSuggestionCount = YES;
  }
}

#pragma mark - Content size events

- (void)contentSizeDidChange:(NSNotification*)notification {
  self.shouldUpdateVisibleSuggestionCount = YES;
}

#pragma mark - ContentProviding

- (BOOL)hasContent {
  return self.tableView.numberOfSections > 0 &&
         [self.tableView numberOfRowsInSection:0] > 0;
}

#pragma mark - CarouselItemConsumer

- (void)deleteCarouselItem:(CarouselItem*)carouselItem {
  [self.carouselCell deleteCarouselItem:carouselItem];
}

#pragma mark - Private Methods

- (id<AutocompleteSuggestion>)suggestionAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section < 0 || indexPath.row < 0) {
    return nil;
  }
  if (!self.currentResult || self.currentResult.count == 0 ||
      self.currentResult.count <= (NSUInteger)indexPath.section ||
      self.currentResult[indexPath.section].suggestions.count <=
          (NSUInteger)indexPath.row) {
    return nil;
  }
  return self.currentResult[indexPath.section].suggestions[indexPath.row];
}

/// Returns the absolute row number for `indexPath`, counting every row in every
/// section above. Used for logging.
- (NSInteger)absoluteRowIndexForIndexPath:(NSIndexPath*)indexPath {
  if (![self suggestionAtIndexPath:indexPath]) {
    return NSNotFound;
  }
  NSInteger rowCount = 0;
  // For each section above `indexPath` add the number of row used by the
  // section.
  for (NSInteger i = 0; i < indexPath.section; ++i) {
    rowCount += [self.tableView numberOfRowsInSection:i];
  }
  switch (self.currentResult[indexPath.section].displayStyle) {
    case SuggestionGroupDisplayStyleDefault:
      return rowCount + indexPath.row;
    case SuggestionGroupDisplayStyleCarousel:
      return rowCount;
  }
}

- (void)updateVisibleSuggestionCount {
  CGRect tableViewFrameInCurrentWindowCoordinateSpace =
      [self.tableView convertRect:self.tableView.bounds
                toCoordinateSpace:self.tableView.window.coordinateSpace];
  // Computes the visible area between the omnibox and the keyboard.
  CGFloat visibleTableViewHeight =
      CGRectGetHeight(self.tableView.window.bounds) -
      tableViewFrameInCurrentWindowCoordinateSpace.origin.y -
      self.keyboardHeight - self.tableView.contentInset.top;
  // Use font size to estimate the size of a omnibox search suggestion.
  CGFloat fontSizeHeight = [@"T" sizeWithAttributes:@{
                             NSFontAttributeName : [UIFont
                                 preferredFontForTextStyle:UIFontTextStyleBody]
                           }]
                               .height;
  // Add padding to the estimated row height and set its minimum to be at
  // `kOmniboxPopupCellMinimumHeight`.
  CGFloat estimatedRowHeight =
      MAX(fontSizeHeight + 2 * kBottomPadding, kOmniboxPopupCellMinimumHeight);
  CGFloat visibleRows = visibleTableViewHeight / estimatedRowHeight;
  // A row is considered visible if `kVisibleSuggestionTreshold` percent of its
  // height is visible.
  self.visibleSuggestionCount =
      floor(visibleRows + (1.0 - kVisibleSuggestionThreshold));
  self.shouldUpdateVisibleSuggestionCount = NO;
}

- (NSArray<CarouselItem*>*)
    carouselItemsFromSuggestionGroup:(id<AutocompleteSuggestionGroup>)group
                      groupIndexPath:(NSIndexPath*)groupIndexPath {
  NSMutableArray* carouselItems =
      [[NSMutableArray alloc] initWithCapacity:group.suggestions.count];
  for (NSUInteger i = 0; i < group.suggestions.count; ++i) {
    id<AutocompleteSuggestion> suggestion = group.suggestions[i];
    NSIndexPath* itemIndexPath =
        [NSIndexPath indexPathForRow:i inSection:groupIndexPath.section];
    CarouselItem* item = [[CarouselItem alloc] init];
    item.title = suggestion.text.string;
    item.indexPath = itemIndexPath;
    item.URL = suggestion.destinationUrl;
    [carouselItems addObject:item];
  }
  return carouselItems;
}

// TODO(crbug.com/40866206): Move to a mediator.
- (void)fetchFaviconForCarouselItem:(CarouselItem*)carouselItem {
  __weak OmniboxPopupCarouselCell* weakCell = self.carouselCell;
  __weak CarouselItem* weakItem = carouselItem;

  void (^completion)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
    OmniboxPopupCarouselCell* strongCell = weakCell;
    CarouselItem* strongItem = weakItem;
    if (!strongCell || !strongItem)
      return;

    strongItem.faviconAttributes = attributes;
    [strongCell updateCarouselItem:strongItem];
  };

  [self.carouselAttributeProvider
      fetchFaviconAttributesForURL:carouselItem.URL.gurl
                        completion:completion];
}

- (void)showDebugUI {
  [self presentViewController:self.debugInfoViewController
                     animated:YES
                   completion:nil];
}

- (UILayoutGuide*)omniboxGuide {
  if (!_omniboxGuide) {
    _omniboxGuide =
        [self.layoutGuideCenter makeLayoutGuideNamed:kTopOmniboxGuide];
    [self.view addLayoutGuide:_omniboxGuide];
  }
  return _omniboxGuide;
}

// Update the view controller's background color and notifies `delegate` when a
// UITrait has been changed.
- (void)updateUIOnTraitChange {
  [self updateBackgroundColor];
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [self.delegate autocompleteResultConsumerDidChangeTraitCollection:self];
  }
}

@end
