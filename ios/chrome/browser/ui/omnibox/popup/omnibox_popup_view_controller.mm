// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"

#include "base/format_macros.h"
#import "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#include "ios/chrome/browser/ui/elements/self_sizing_table_view.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/content_providing.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell.h"
#include "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTopAndBottomPadding = 8.0;
const CGFloat kTopPaddingVariation1 = 8.0;
const CGFloat kTopPaddingVariation2 = 10.0;
const CGFloat kTopBottomPaddingVariation2Ipad = 16.0;
const CGFloat kFooterHeightVariation1 = 12.0;
const CGFloat kFooterHeightVariation2 = 16.0;
// Percentage of the suggestion height that needs to be visible in order to
// consider the suggestion as visible.
const CGFloat kVisibleSuggestionThreshold = 0.6;
}  // namespace

@interface OmniboxPopupViewController () <UITableViewDataSource,
                                          UITableViewDelegate,
                                          OmniboxPopupRowCellDelegate,
                                          KeyboardObserverHelperConsumer>

// Index path of currently highlighted row. The rows can be highlighted by
// tapping and holding on them or by using arrow keys on a hardware keyboard.
@property(nonatomic, strong) NSIndexPath* highlightedIndexPath;

// Flag that enables forwarding scroll events to the delegate. Disabled while
// updating the cells to avoid defocusing the omnibox when the omnibox popup
// changes size and table view issues a scroll event.
@property(nonatomic, assign) BOOL forwardsScrollEvents;

// The height of the keyboard. Used to determine the content inset for the
// scroll view.
@property(nonatomic, assign) CGFloat keyboardHeight;

// Time the view appeared on screen. Used to record a metric of how long this
// view controller was on screen.
@property(nonatomic, assign) base::TimeTicks viewAppearanceTime;
// Table view that displays the results.
@property(nonatomic, strong) UITableView* tableView;

// Alignment of omnibox text. Popup text should match this alignment.
@property(nonatomic, assign) NSTextAlignment alignment;

// Semantic content attribute of omnibox text. Popup should match this
// attribute. This is used by the new omnibox popup.
@property(nonatomic, assign)
    UISemanticContentAttribute semanticContentAttribute;

// Estimated maximum number of visible suggestions.
// Only updated in `newResultsAvailable` method, were the value is used.
@property(nonatomic, assign) NSInteger visibleSuggestionCount;

// Boolean to update visible suggestion count only once on event such as device
// orientation change or multitasking window change, where multiple keyboard and
// view updates are received.
@property(nonatomic, assign) BOOL shouldUpdateVisibleSuggestionCount;

@end

@implementation OmniboxPopupViewController

- (instancetype)init {
  if (self = [super initWithNibName:nil bundle:nil]) {
    _forwardsScrollEvents = YES;
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      // The iPad keyboard can cover some of the rows of the scroll view. The
      // scroll view's content inset may need to be updated when the keyboard is
      // displayed.
      [defaultCenter addObserver:self
                        selector:@selector(keyboardDidShow:)
                            name:UIKeyboardDidShowNotification
                          object:nil];
    }
    // Listen to keyboard observer to detect `KeyboardState` changes in order to
    // update the estimated number of visible suggestions.
    [KeyboardObserverHelper.sharedKeyboardObserver addConsumer:self];

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
  UITableViewStyle style = IsOmniboxActionsVisualTreatment2()
                               ? UITableViewStyleInsetGrouped
                               : UITableViewStylePlain;
  self.tableView = [[SelfSizingTableView alloc] initWithFrame:CGRectZero
                                                        style:style];
  self.tableView.delegate = self;
  self.tableView.dataSource = self;
  self.view = self.tableView;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateBackgroundColor];
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
      IsOmniboxActionsVisualTreatment2()
          ? UIScrollViewContentInsetAdjustmentNever
          : UIScrollViewContentInsetAdjustmentAutomatic;
  [self.tableView setContentInset:UIEdgeInsetsMake(self.topPadding, 0,
                                                   self.bottomPadding, 0)];

  self.tableView.sectionHeaderHeight = 0.1;
  self.tableView.estimatedRowHeight = 0;

  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = kOmniboxPopupCellMinimumHeight;

  [self.tableView registerClass:[OmniboxPopupRowCell class]
         forCellReuseIdentifier:OmniboxPopupRowCellReuseIdentifier];
  self.shouldUpdateVisibleSuggestionCount = YES;

  if (@available(iOS 15.0, *)) {
    self.tableView.sectionHeaderTopPadding = 0;
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  if (IsOmniboxActionsVisualTreatment2()) {
    [self adjustMarginsToMatchOmniboxWidth];
  }

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

  if (IsOmniboxActionsVisualTreatment2()) {
    [coordinator
        animateAlongsideTransition:^(
            id<UIViewControllerTransitionCoordinatorContext> context) {
          [self adjustMarginsToMatchOmniboxWidth];
        }
                        completion:nil];
  }
}

- (void)adjustMarginsToMatchOmniboxWidth {
  NamedGuide* layoutGuide = [NamedGuide guideWithName:kOmniboxGuide
                                                 view:self.view];
  if (!layoutGuide) {
    return;
  }

  CGRect omniboxFrame = [layoutGuide.constrainedView
      convertRect:layoutGuide.constrainedView.bounds
           toView:self.view];
  CGFloat leftMargin =
      IsRegularXRegularSizeClass(self) ? omniboxFrame.origin.x : 0;
  CGFloat rightMargin = IsRegularXRegularSizeClass(self)
                            ? self.view.bounds.size.width -
                                  omniboxFrame.origin.x -
                                  omniboxFrame.size.width
                            : 0;
  self.tableView.layoutMargins =
      UIEdgeInsetsMake(self.tableView.layoutMargins.top, leftMargin,
                       self.tableView.layoutMargins.bottom, rightMargin);
}

#pragma mark - AutocompleteResultConsumer

- (void)updateMatches:(NSArray<id<AutocompleteSuggestionGroup>>*)result
    preselectedMatchGroupIndex:(NSInteger)groupIndex {
  self.forwardsScrollEvents = NO;
  // Reset highlight state.
  if (self.highlightedIndexPath) {
    [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
    self.highlightedIndexPath = nil;
  }

  self.currentResult = result;

  [self.tableView reloadData];
  self.forwardsScrollEvents = YES;
}

- (void)highlightRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setHighlighted:YES animated:NO];
}

- (void)unhighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setHighlighted:NO animated:NO];
}

// Set text alignment for popup cells.
- (void)setTextAlignment:(NSTextAlignment)alignment {
  self.alignment = alignment;
}

- (void)newResultsAvailable {
  if (self.shouldUpdateVisibleSuggestionCount) {
    [self updateVisibleSuggestionCount];
  }
  [self.dataSource
      requestResultsWithVisibleSuggestionCount:self.visibleSuggestionCount];
}

#pragma mark - OmniboxSuggestionCommands

- (void)highlightPreviousSuggestion {
  NSIndexPath* path = self.highlightedIndexPath;
  if (path == nil) {
    // When nothing is highlighted, pressing Up Arrow doesn't do anything.
    return;
  }

  BOOL isCurrentHighlightedRowFirstInSection = (path.row == 0);
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
      [self.delegate
          autocompleteResultConsumer:self
                     didHighlightRow:self.highlightedIndexPath.row
                           inSection:self.highlightedIndexPath.section];
      return;
    }
  } else {
    path = [NSIndexPath indexPathForRow:path.row - 1 inSection:path.section];
  }

  [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
  self.highlightedIndexPath = path;
  [self highlightRowAtIndexPath:self.highlightedIndexPath];

  [self.delegate autocompleteResultConsumer:self
                            didHighlightRow:self.highlightedIndexPath.row
                                  inSection:self.highlightedIndexPath.section];
}

- (void)highlightNextSuggestion {
  if ([self.tableView numberOfRowsInSection:0] == 0) {
    return;
  }
  if (!self.highlightedIndexPath) {
    // Initialize the highlighted row to -1, so that pressing down when nothing
    // is highlighted highlights the first row (at index 0).
    self.highlightedIndexPath = [NSIndexPath indexPathForRow:-1 inSection:0];
  }

  NSIndexPath* path = self.highlightedIndexPath;
  BOOL isCurrentHighlightedRowLastInSection =
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
      [self.delegate
          autocompleteResultConsumer:self
                     didHighlightRow:self.highlightedIndexPath.row
                           inSection:self.highlightedIndexPath.section];
      return;
    }
  } else {
    path = [NSIndexPath indexPathForRow:path.row + 1 inSection:path.section];
  }

  // There is a row below, move highlight there.
  [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
  self.highlightedIndexPath = path;
  [self highlightRowAtIndexPath:self.highlightedIndexPath];

  [self.delegate autocompleteResultConsumer:self
                            didHighlightRow:self.highlightedIndexPath.row
                                  inSection:self.highlightedIndexPath.section];
}

- (void)keyCommandReturn {
  [self.tableView selectRowAtIndexPath:self.highlightedIndexPath
                              animated:YES
                        scrollPosition:UITableViewScrollPositionNone];
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell isKindOfClass:[OmniboxPopupRowCell class]]) {
    OmniboxPopupRowCell* rowCell =
        base::mac::ObjCCastStrict<OmniboxPopupRowCell>(cell);
    // This has to be set here because the cell's content view has its
    // semantic content attribute reset before the cell is displayed (and before
    // this method is called).
    rowCell.omniboxSemanticContentAttribute = self.semanticContentAttribute;

    rowCell.accessibilityIdentifier = [OmniboxPopupAccessibilityIdentifierHelper
        accessibilityIdentifierForRowAtIndexPath:indexPath];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_LT((NSUInteger)indexPath.row,
            self.currentResult[indexPath.section].suggestions.count);
  NSUInteger row = indexPath.row;

  // Crash reports tell us that `row` is sometimes indexed past the end of
  // the results array. In those cases, just ignore the request and return
  // early. See b/5813291.
  if (row >= self.currentResult[indexPath.section].suggestions.count)
    return;
  [self.delegate autocompleteResultConsumer:self
                               didSelectRow:row
                                  inSection:indexPath.section];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return FLT_MIN;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if (!IsOmniboxActionsEnabled()) {
    return FLT_MIN;
  }
  if (section == (tableView.numberOfSections - 1)) {
    return FLT_MIN;
  }

  return IsOmniboxActionsVisualTreatment1() ? kFooterHeightVariation1
                                            : kFooterHeightVariation2;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  if (!IsOmniboxActionsEnabled()) {
    return nil;
  }

  // Do not show footer for the last section
  if (section == (tableView.numberOfSections - 1)) {
    return nil;
  }
  if (IsOmniboxActionsVisualTreatment2()) {
    return [[UIView alloc] init];
  }

  UIView* footer = [[UIView alloc] init];
  footer.backgroundColor = tableView.backgroundColor;
  UIView* hairline = [[UIView alloc]
      initWithFrame:CGRectMake(0, 8, tableView.bounds.size.width,
                               2 / tableView.window.screen.scale)];
  hairline.backgroundColor =
      self.incognito ? [UIColor.whiteColor colorWithAlphaComponent:0.12]
                     : [UIColor.blackColor colorWithAlphaComponent:0.12];
  [footer addSubview:hairline];
  hairline.autoresizingMask = UIViewAutoresizingFlexibleWidth;

  return footer;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  return nil;
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return self.currentResult.count;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return self.currentResult[section].suggestions.count;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // iOS doesn't check -numberOfRowsInSection before checking
  // -canEditRowAtIndexPath in a reload call. If `indexPath.row` is too large,
  // simple return `NO`.
  if ((NSUInteger)indexPath.row >=
      self.currentResult[indexPath.section].suggestions.count)
    return NO;

  return [self.currentResult[indexPath.section].suggestions[indexPath.row]
      supportsDeletion];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_LT((NSUInteger)indexPath.row,
            self.currentResult[indexPath.section].suggestions.count);
  if (editingStyle == UITableViewCellEditingStyleDelete) {
    [self.delegate autocompleteResultConsumer:self
                      didSelectRowForDeletion:indexPath.row
                                    inSection:indexPath.section];
  }
}

#pragma mark - Internal API methods

// Adjust the inset on the table view to prevent keyboard from overlapping the
// text.
- (void)updateContentInsetForKeyboard {
  UIScreen* currentScreen = self.tableView.window.screen;
  CGRect absoluteRect =
      [self.tableView convertRect:self.tableView.bounds
                toCoordinateSpace:currentScreen.coordinateSpace];
  CGFloat screenHeight = currentScreen.bounds.size.height;
  CGFloat bottomInset = screenHeight - self.tableView.contentSize.height -
                        _keyboardHeight - absoluteRect.origin.y -
                        self.bottomPadding - self.topPadding;
  bottomInset = MAX(self.bottomPadding, -bottomInset);
  self.tableView.contentInset =
      UIEdgeInsetsMake(self.topPadding, 0, bottomInset, 0);
  self.tableView.scrollIndicatorInsets = self.tableView.contentInset;
}

// Updates the color of the background based on the incognito-ness and the size
// class.
- (void)updateBackgroundColor {
  ToolbarConfiguration* configuration = [[ToolbarConfiguration alloc]
      initWithStyle:self.incognito ? INCOGNITO : NORMAL];

  if (IsOmniboxActionsVisualTreatment2()) {
    self.view.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
    return;
  }

  if (IsRegularXRegularSizeClass(self)) {
    self.view.backgroundColor = configuration.backgroundColor;
  } else {
    self.view.backgroundColor = [UIColor clearColor];
  }
}

#pragma mark Action for append UIButton

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  _semanticContentAttribute = semanticContentAttribute;
  // If there are any visible cells, update them right away.
  for (UITableViewCell* cell in self.tableView.visibleCells) {
    if ([cell isKindOfClass:[OmniboxPopupRowCell class]]) {
      OmniboxPopupRowCell* rowCell =
          base::mac::ObjCCastStrict<OmniboxPopupRowCell>(cell);
      // This has to be set here because the cell's content view has its
      // semantic content attribute reset before the cell is displayed (and
      // before this method is called).
      rowCell.omniboxSemanticContentAttribute = self.semanticContentAttribute;
    }
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // TODO(crbug.com/733650): Default to the dragging check once it's been tested
  // on trunk.
  if (!scrollView.dragging)
    return;

  // TODO(crbug.com/911534): The following call chain ultimately just dismisses
  // the keyboard, but involves many layers of plumbing, and should be
  // refactored.
  if (self.forwardsScrollEvents)
    [self.delegate autocompleteResultConsumerDidScroll:self];

  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
}


#pragma mark - Table view data source

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewAutomaticDimension;
}

// Customize the appearance of table view cells.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_LT((NSUInteger)indexPath.row,
            self.currentResult[indexPath.section].suggestions.count);
  OmniboxPopupRowCell* cell = [self.tableView
      dequeueReusableCellWithIdentifier:OmniboxPopupRowCellReuseIdentifier
                           forIndexPath:indexPath];
  cell.faviconRetriever = self.faviconRetriever;
  cell.imageRetriever = self.imageRetriever;
  [cell setupWithAutocompleteSuggestion:self.currentResult[indexPath.section]
                                            .suggestions[indexPath.row]
                              incognito:self.incognito];
  cell.showsSeparator =
      (NSUInteger)indexPath.row <
      self.currentResult[indexPath.section].suggestions.count - 1;
  cell.delegate = self;

  return cell;
}

#pragma mark - OmniboxPopupRowCellDelegate

- (void)trailingButtonTappedForCell:(OmniboxPopupRowCell*)cell {
  NSIndexPath* indexPath = [self.tableView indexPathForCell:cell];
  [self.delegate autocompleteResultConsumer:self
                 didTapTrailingButtonForRow:indexPath.row
                                  inSection:indexPath.section];
}

#pragma mark - Keyboard events

- (void)keyboardDidShow:(NSNotification*)notification {
  NSDictionary* keyboardInfo = [notification userInfo];
  NSValue* keyboardFrameValue =
      [keyboardInfo valueForKey:UIKeyboardFrameEndUserInfoKey];
  self.keyboardHeight =
      KeyboardObserverHelper.keyboardScreen == self.view.window.screen
          ? CurrentKeyboardHeight(keyboardFrameValue)
          : 0;
  if (self.tableView.contentSize.height > 0)
    [self updateContentInsetForKeyboard];
}

- (void)keyboardDidChangeFrame:(NSNotification*)notification {
  if (KeyboardObserverHelper.sharedKeyboardObserver.visibleKeyboardHeight > 0) {
    self.shouldUpdateVisibleSuggestionCount = YES;
  }
}

#pragma mark - KeyboardObserverHelperConsumer

- (void)keyboardWillChangeToState:(KeyboardState)keyboardState {
  self.shouldUpdateVisibleSuggestionCount = YES;
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

#pragma mark - Private Methods

- (void)updateVisibleSuggestionCount {
  CGFloat keyboardHeight =
      [[KeyboardObserverHelper sharedKeyboardObserver] visibleKeyboardHeight];
  UIScreen* currentScreen = self.tableView.window.screen;
  CGRect tableViewFrameInCurrentScreenCoordinateSpace =
      [self.tableView convertRect:self.tableView.bounds
                toCoordinateSpace:currentScreen.coordinateSpace];
  // Computes the visible area between the omnibox and the keyboard.
  CGFloat visibleTableViewHeight =
      CurrentScreenHeight() -
      tableViewFrameInCurrentScreenCoordinateSpace.origin.y - keyboardHeight -
      self.tableView.contentInset.top;

  // Use font size to estimate the size of a omnibox search suggestion.
  CGFloat fontSizeHeight = [@"T" sizeWithAttributes:@{
                             NSFontAttributeName : [UIFont
                                 preferredFontForTextStyle:UIFontTextStyleBody]
                           }]
                               .height;
  // Add padding to the estimated row height and set its minimum to be at
  // `kOmniboxPopupCellMinimumHeight`.
  CGFloat estimatedRowHeight = MAX(fontSizeHeight + 2 * kTopAndBottomPadding,
                                   kOmniboxPopupCellMinimumHeight);
  CGFloat visibleRows = visibleTableViewHeight / estimatedRowHeight;
  // A row is considered visible if `kVisibleSuggestionTreshold` percent of its
  // height is visible.
  self.visibleSuggestionCount =
      floor(visibleRows + (1.0 - kVisibleSuggestionThreshold));
  self.shouldUpdateVisibleSuggestionCount = NO;
}

- (CGFloat)topPadding {
  CGFloat topPadding = kTopAndBottomPadding;
  if (IsOmniboxActionsVisualTreatment1()) {
    topPadding = kTopPaddingVariation1;
  }
  if (IsOmniboxActionsVisualTreatment2()) {
    // On iPad, even in compact width, the popup is displayed differently than
    // on the iPhone (it's "under" the always visible toolbar). So the check
    // here is intentionally for device type, not size class.
    BOOL isIpad = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
    topPadding =
        isIpad ? kTopBottomPaddingVariation2Ipad : kTopPaddingVariation2;
  }
  return topPadding;
}

- (CGFloat)bottomPadding {
  if (IsOmniboxActionsVisualTreatment2() &&
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)) {
    return kTopBottomPaddingVariation2Ipad;
  }
  return kTopAndBottomPadding;
}

@end
