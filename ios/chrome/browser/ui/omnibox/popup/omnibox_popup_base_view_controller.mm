// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_base_view_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_base_view_controller+internal.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_layout_util.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"
#include "ios/chrome/browser/ui/omnibox/popup/self_sizing_table_view.h"
#include "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTopAndBottomPadding = 8.0;
}  // namespace

@interface OmniboxPopupBaseViewController () <UITableViewDataSource>

#pragma mark Redeclaration of Internal properties

@property(nonatomic, assign) NSTextAlignment alignment;
@property(nonatomic, assign)
    UISemanticContentAttribute semanticContentAttribute;
@property(nonatomic, strong) UITableView* tableView;

#pragma mark Private properties

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

@property(nonatomic, strong) NSLayoutConstraint* shortcutsViewEdgeConstraint;

@end

@implementation OmniboxPopupBaseViewController

- (instancetype)init {
  if (self = [super initWithNibName:nil bundle:nil]) {
    _forwardsScrollEvents = YES;
    if (IsIPadIdiom()) {
      // The iPad keyboard can cover some of the rows of the scroll view. The
      // scroll view's content inset may need to be updated when the keyboard is
      // displayed.
      NSNotificationCenter* defaultCenter =
          [NSNotificationCenter defaultCenter];
      [defaultCenter addObserver:self
                        selector:@selector(keyboardDidShow:)
                            name:UIKeyboardDidShowNotification
                          object:nil];
    }
  }
  return self;
}

- (void)loadView {
  self.tableView =
      [[SelfSizingTableView alloc] initWithFrame:CGRectZero
                                           style:UITableViewStylePlain];
  self.tableView.delegate = self;
  self.tableView.dataSource = self;
  self.view = self.tableView;
}

- (void)viewDidLoad {
  [super viewDidLoad];

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
  [self.tableView setContentInset:UIEdgeInsetsMake(kTopAndBottomPadding, 0,
                                                   kTopAndBottomPadding, 0)];
  self.tableView.estimatedRowHeight = 0;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateBackgroundColor];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  // Update the leading edge constraints for the shortcuts cell when the view
  // rotates.
  if (self.shortcutsEnabled && self.currentResult.count == 0) {
    __weak __typeof(self) weakSelf = self;
    [coordinator
        animateAlongsideTransition:^(
            id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
          __typeof(self) strongSelf = weakSelf;
          if (!strongSelf) {
            return;
          }
          CGFloat widthInsets = CenteredTilesMarginForWidth(
              strongSelf.traitCollection,
              size.width - strongSelf.view.safeAreaInsets.left -
                  strongSelf.view.safeAreaInsets.right);
          strongSelf.shortcutsViewEdgeConstraint.constant = widthInsets;
          [strongSelf.shortcutsViewController.collectionView
                  .collectionViewLayout invalidateLayout];
          [strongSelf.shortcutsCell layoutIfNeeded];
        }
                        completion:nil];
  }
}

#pragma mark - View lifecycle

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.viewAppearanceTime = base::TimeTicks::Now();
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  UMA_HISTOGRAM_MEDIUM_TIMES("MobileOmnibox.PopupOpenDuration",
                             base::TimeTicks::Now() - self.viewAppearanceTime);
}

#pragma mark - Properties accessors

- (void)setShortcutsEnabled:(BOOL)shortcutsEnabled {
  if (shortcutsEnabled == _shortcutsEnabled) {
    return;
  }

  DCHECK(!shortcutsEnabled || self.shortcutsViewController);

  _shortcutsEnabled = shortcutsEnabled;
  [self.tableView reloadData];
}

- (UITableViewCell*)shortcutsCell {
  if (_shortcutsCell) {
    return _shortcutsCell;
  }

  DCHECK(self.shortcutsEnabled);
  DCHECK(self.shortcutsViewController);

  UITableViewCell* cell = [[UITableViewCell alloc] init];
  _shortcutsCell = cell;
  cell.backgroundColor = [UIColor clearColor];
  [self.shortcutsViewController willMoveToParentViewController:self];
  [self addChildViewController:self.shortcutsViewController];
  [cell.contentView addSubview:self.shortcutsViewController.view];
  self.shortcutsViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  AddSameConstraintsToSides(self.shortcutsViewController.view, cell.contentView,
                            (LayoutSides::kTop | LayoutSides::kBottom));
  AddSameCenterXConstraint(self.shortcutsViewController.view, cell.contentView);
  self.shortcutsViewEdgeConstraint =
      [self.shortcutsViewController.view.leadingAnchor
          constraintEqualToAnchor:cell.contentView.safeAreaLayoutGuide
                                      .leadingAnchor];
  // When the device is rotating, the constraints are slightly off for one
  // runloop. Lower the priority here to prevent unable to satisfy constraints
  // warning.
  self.shortcutsViewEdgeConstraint.priority = UILayoutPriorityRequired - 1;
  self.shortcutsViewEdgeConstraint.active = YES;
  [self.shortcutsViewController didMoveToParentViewController:self];
  cell.accessibilityIdentifier = kShortcutsAccessibilityIdentifier;
  return cell;
}

#pragma mark - AutocompleteResultConsumer

- (void)updateMatches:(NSArray<id<AutocompleteSuggestion>>*)result
        withAnimation:(BOOL)animation {
  self.forwardsScrollEvents = NO;
  // Reset highlight state.
  if (self.highlightedIndexPath) {
    [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
    self.highlightedIndexPath = nil;
  }

  self.currentResult = result;

  [self updateTableViewWithAnimation:animation];
  self.forwardsScrollEvents = YES;
}

- (void)keyboardDidShow:(NSNotification*)notification {
  NSDictionary* keyboardInfo = [notification userInfo];
  NSValue* keyboardFrameValue =
      [keyboardInfo valueForKey:UIKeyboardFrameEndUserInfoKey];
  self.keyboardHeight = CurrentKeyboardHeight(keyboardFrameValue);
  if (self.tableView.contentSize.height > 0)
    [self updateContentInsetForKeyboard];
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

#pragma mark - OmniboxSuggestionCommands

- (void)highlightNextSuggestion {
  NSIndexPath* path = self.highlightedIndexPath;
  if (path == nil) {
    // When nothing is highlighted, pressing Up Arrow doesn't do anything.
    return;
  }

  if (path.row == 0) {
    // Can't move up from first row. Call the delegate again so that the inline
    // autocomplete text is set again (in case the user exited the inline
    // autocomplete).
    [self.delegate autocompleteResultConsumer:self
                              didHighlightRow:self.highlightedIndexPath.row];
    return;
  }

  [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
  self.highlightedIndexPath =
      [NSIndexPath indexPathForRow:self.highlightedIndexPath.row - 1
                         inSection:0];
  [self highlightRowAtIndexPath:self.highlightedIndexPath];

  [self.delegate autocompleteResultConsumer:self
                            didHighlightRow:self.highlightedIndexPath.row];
}

- (void)highlightPreviousSuggestion {
  if (!self.highlightedIndexPath) {
    // Initialize the highlighted row to -1, so that pressing down when nothing
    // is highlighted highlights the first row (at index 0).
    self.highlightedIndexPath = [NSIndexPath indexPathForRow:-1 inSection:0];
  }

  NSIndexPath* path = self.highlightedIndexPath;

  if (path.row == [self.tableView numberOfRowsInSection:0] - 1) {
    // Can't go below last row. Call the delegate again so that the inline
    // autocomplete text is set again (in case the user exited the inline
    // autocomplete).
    [self.delegate autocompleteResultConsumer:self
                              didHighlightRow:self.highlightedIndexPath.row];
    return;
  }

  // There is a row below, move highlight there.
  [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
  self.highlightedIndexPath =
      [NSIndexPath indexPathForRow:self.highlightedIndexPath.row + 1
                         inSection:0];
  [self highlightRowAtIndexPath:self.highlightedIndexPath];

  [self.delegate autocompleteResultConsumer:self
                            didHighlightRow:self.highlightedIndexPath.row];
}

- (void)keyCommandReturn {
  [self.tableView selectRowAtIndexPath:self.highlightedIndexPath
                              animated:YES
                        scrollPosition:UITableViewScrollPositionNone];
}

#pragma mark - Table view delegate

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.shortcutsEnabled && indexPath.row == 0 &&
      self.currentResult.count == 0) {
    return NO;
  }

  return YES;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, self.currentResult.count);
  NSUInteger row = indexPath.row;

  // Crash reports tell us that |row| is sometimes indexed past the end of
  // the results array. In those cases, just ignore the request and return
  // early. See b/5813291.
  if (row >= self.currentResult.count)
    return;
  [self.delegate autocompleteResultConsumer:self didSelectRow:row];
}

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  // Update the leading edge constraints for the shortcuts cell before it is
  // displayed.
  if (self.shortcutsEnabled && indexPath.row == 0 &&
      self.currentResult.count == 0) {
    CGFloat widthInsets = CenteredTilesMarginForWidth(
        self.traitCollection, self.view.bounds.size.width -
                                  self.view.safeAreaInsets.left -
                                  self.view.safeAreaInsets.right);
    if (widthInsets != self.shortcutsViewEdgeConstraint.constant) {
      self.shortcutsViewEdgeConstraint.constant = widthInsets;
      // If the insets have changed, the collection view (and thus the table
      // view) may have changed heights. This could happen due to dynamic type
      // changing the height of the collection view. It is also necessary for
      // the first load.
      [self.shortcutsViewController.collectionView
              .collectionViewLayout invalidateLayout];
      [self.shortcutsCell.contentView layoutIfNeeded];
      [self.tableView reloadData];
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
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  DCHECK_EQ(0, section);
  if (self.shortcutsEnabled && self.currentResult.count == 0) {
    return 1;
  }
  return self.currentResult.count;
}

// Customize the appearance of table view cells.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  return nil;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);

  if (self.shortcutsEnabled && indexPath.row == 0 &&
      self.currentResult.count == 0) {
    return NO;
  }

  // iOS doesn't check -numberOfRowsInSection before checking
  // -canEditRowAtIndexPath in a reload call. If |indexPath.row| is too large,
  // simple return |NO|.
  if ((NSUInteger)indexPath.row >= self.currentResult.count)
    return NO;

  return [self.currentResult[indexPath.row] supportsDeletion];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, self.currentResult.count);
  if (editingStyle == UITableViewCellEditingStyleDelete) {
    [self.delegate autocompleteResultConsumer:self
                      didSelectRowForDeletion:indexPath.row];
  }
}

#pragma mark - Internal API methods

- (void)updateContentInsetForKeyboard {
  CGRect absoluteRect =
      [self.tableView convertRect:self.tableView.bounds
                toCoordinateSpace:UIScreen.mainScreen.coordinateSpace];
  CGFloat screenHeight = CurrentScreenHeight();
  CGFloat bottomInset = screenHeight - self.tableView.contentSize.height -
                        _keyboardHeight - absoluteRect.origin.y -
                        kTopAndBottomPadding * 2;
  bottomInset = MAX(kTopAndBottomPadding, -bottomInset);
  self.tableView.contentInset =
      UIEdgeInsetsMake(kTopAndBottomPadding, 0, bottomInset, 0);
  self.tableView.scrollIndicatorInsets = self.tableView.contentInset;
}

- (void)updateTableViewWithAnimation:(BOOL)animation {
  // for subclassing
}

// Updates the color of the background based on the incognito-ness and the size
// class.
- (void)updateBackgroundColor {
  ToolbarConfiguration* configuration = [[ToolbarConfiguration alloc]
      initWithStyle:self.incognito ? INCOGNITO : NORMAL];

  if (IsRegularXRegularSizeClass(self)) {
    self.view.backgroundColor = configuration.backgroundColor;
  } else {
    self.view.backgroundColor = [UIColor clearColor];
  }
}

#pragma mark Action for append UIButton

- (void)trailingButtonTapped:(id)sender {
  NSUInteger row = [sender tag];
  [self.delegate autocompleteResultConsumer:self
                 didTapTrailingButtonForRow:row];
}

@end
