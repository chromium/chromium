// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

#import <Foundation/Foundation.h>
#import <optional>
#import <ostream>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {

// Estimated row height for each cell in the table view.
CGFloat const kTableViewEstimatedRowHeight = 75;

// Radius size of the table view.
CGFloat const kTableViewCornerRadius = 10;

// Custom detent identifier for when the bottom sheet is minimized.
NSString* const kCustomMinimizedDetentIdentifier = @"customMinimizedDetent";

// Default custom detent identifier.
NSString* const kCustomDetentIdentifier = @"customDetent";

}  // namespace

@interface TableViewBottomSheetViewController () <
    UISheetPresentationControllerDelegate> {
  // Table view for the list of suggestions.
  UITableView* _tableView;

  // If YES: the table view is currently showing a number of rows equal to
  // `initialNumberOfVisibleCells`. If NO: the table view is currently showing
  // all rows.
  BOOL _tableViewIsMinimized;

  // Height constraint for the bottom sheet when showing a number of rows equal
  // to `initialNumberOfVisibleCells`.
  NSLayoutConstraint* _minimizedHeightConstraint;

  // Height constraint for the bottom sheet when showing all suggestions.
  NSLayoutConstraint* _heightConstraint;

  // YES if the expanded bottom sheet size takes the whole screen.
  BOOL _expandSizeTooLarge;

  // Keep track of the minimized state height.
  std::optional<CGFloat> _minimizedStateHeight;
}

@end

@implementation TableViewBottomSheetViewController

- (void)reloadTableViewData {
  [_tableView reloadData];
  [self updateHeight];
}

- (NSInteger)selectedRow {
  return _tableView.indexPathForSelectedRow.row;
}

- (CGFloat)tableViewWidth {
  return _tableView.frame.size.width;
}

- (UIEdgeInsets)separatorInsetForTableViewWidth:(CGFloat)tableViewWidth
                                    atIndexPath:(NSIndexPath*)indexPath {
  // Make separator invisible on last cell
  CGFloat separatorLeftMargin =
      [self isLastRow:indexPath] ? tableViewWidth : kTableViewHorizontalSpacing;
  return UIEdgeInsetsMake(0.f, separatorLeftMargin, 0.f, 0.f);
}

- (UITableViewCellAccessoryType)accessoryType:(NSIndexPath*)indexPath {
  return ([self selectedRow] == indexPath.row)
             ? UITableViewCellAccessoryCheckmark
             : UITableViewCellAccessoryNone;
}

- (void)adjustTransactionsPrimaryActionButtonHorizontalConstraints {
  CGFloat buttonHorizontalMargin =
      ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad
           ? 64.0
           : 24.0);

  [self.primaryActionButton.leadingAnchor
      constraintEqualToAnchor:(self.view.leadingAnchor)
                     constant:buttonHorizontalMargin]
      .active = YES;
  [self.primaryActionButton.trailingAnchor
      constraintEqualToAnchor:(self.view.trailingAnchor)
                     constant:-buttonHorizontalMargin]
      .active = YES;
}

#pragma mark - Subclassing

- (UITableView*)createTableView {
  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStylePlain];

  _tableView.layer.cornerRadius = kTableViewCornerRadius;
  _tableView.estimatedRowHeight = kTableViewEstimatedRowHeight;
  _tableView.scrollEnabled = NO;
  _tableView.showsVerticalScrollIndicator = NO;
  _tableView.delegate = self;
  _tableView.userInteractionEnabled = YES;

  _tableView.translatesAutoresizingMaskIntoConstraints = NO;

  _minimizedHeightConstraint = [_tableView.heightAnchor
      constraintEqualToConstant:kTableViewEstimatedRowHeight *
                                [self initialNumberOfVisibleCells]];
  _minimizedHeightConstraint.priority = UILayoutPriorityDefaultLow;
  _heightConstraint = [_tableView.heightAnchor
      constraintEqualToConstant:kTableViewEstimatedRowHeight * [self rowCount]];

  _minimizedHeightConstraint.active = _tableViewIsMinimized;
  _heightConstraint.active = !_tableViewIsMinimized;

  [self selectFirstRow];

  return _tableView;
}

- (NSUInteger)rowCount {
  NOTREACHED() << "Subclasses of TableViewBottomSheetViewController "
                  "must implement this method.";
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  NOTREACHED() << "Subclasses of TableViewBottomSheetViewController "
                  "must implement this method.";
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityViewIsModal = YES;

  // If the table has too many rows for the initial state, we open bottom sheet
  // minimized.
  _tableViewIsMinimized = [self rowCount] > [self initialNumberOfVisibleCells];

  self.underTitleView = [self createTableView];

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.imageHasFixedSize = YES;
  self.showsVerticalScrollIndicator = NO;
  self.showDismissBarButton = NO;
  self.topAlignedLayout = YES;
  self.customScrollViewBottomInsets = 0;

  [super viewDidLoad];

  [self displayGradientView:NO];

  // Assign table view's width anchor now that it is in the same hierarchy as
  // the top view.
  [_tableView.widthAnchor
      constraintEqualToAnchor:self.primaryActionButton.widthAnchor]
      .active = YES;

  [self setUpBottomSheetDetents];

  // Set selection to the first one.
  [self selectFirstRow];
}

- (void)viewIsAppearing:(BOOL)animated {
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 170000
  [super viewIsAppearing:animated];
#endif

  [self updateHeight];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    _minimizedStateHeight = std::nullopt;
    [self updateHeight];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView cellForRowAtIndexPath:indexPath].accessoryType =
      UITableViewCellAccessoryCheckmark;
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView cellForRowAtIndexPath:indexPath].accessoryType =
      UITableViewCellAccessoryNone;
}

// It is called when the table view is about to draw a cell for a particular
// row.
- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  // If only one suggestion exists, the item should not be selectable.
  cell.userInteractionEnabled = [self rowCount] > 1;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self displayGradientView:![self isScrolledToBottom]];
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)sheetPresentationControllerDidChangeSelectedDetentIdentifier:
    (UISheetPresentationController*)sheetPresentationController
    API_AVAILABLE(ios(16)) {
  // Show the gradient view to let the user know that the view can be scrolled
  // when the bottom sheet is in minimized state or if the expanded state takes
  // more space than the screen.
  NSString* selectedDetentIdentifier =
      sheetPresentationController.selectedDetentIdentifier;
  [self displayGradientView:selectedDetentIdentifier ==
                                kCustomMinimizedDetentIdentifier ||
                            (selectedDetentIdentifier ==
                                 kCustomDetentIdentifier &&
                             _expandSizeTooLarge)];
}

#pragma mark - Private

// Maximum initial number of visible cells.
- (CGFloat)initialNumberOfVisibleCells {
  return 2.5;
}

// Select the first row in the table view.
- (void)selectFirstRow {
  [_tableView selectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]
                          animated:NO
                    scrollPosition:UITableViewScrollPositionNone];
}

// Mocks cells to compute the table view height for the given number of rows.
- (CGFloat)computeTableViewHeight:(NSUInteger)rowCount {
  CGFloat height = 0;
  for (NSUInteger i = 0; i < rowCount; i++) {
    CGFloat cellHeight = [self computeTableViewCellHeightAtIndex:i];
    height += cellHeight;
  }
  return height;
}

// Updates the bottom sheet's height.
- (void)updateHeight {
  BOOL useMinimizedState = _tableViewIsMinimized;

  NSUInteger rowCount = [self rowCount];
  if (rowCount) {
    [self.view layoutIfNeeded];
    CGFloat fullHeight = [self computeTableViewHeight:rowCount];
    if (fullHeight > 0) {
      // Update height constraints for the table view.
      _heightConstraint.constant = fullHeight;

      if (rowCount > [self initialNumberOfVisibleCells]) {
        _minimizedHeightConstraint.constant =
            [self computeTableViewHeightForMinimizedState:rowCount];
      } else {
        _minimizedHeightConstraint.constant = fullHeight;
      }

      // Do not use minized state if it is larger than the superview height.
      useMinimizedState &=
          [self initialHeight] < self.parentViewControllerHeight;
    }
  }

  // Update the custom detent with the correct initial height for the bottom
  // sheet. (Initial height is not calculated properly in -viewDidLoad, but we
  // need to setup the bottom sheet in that method so there is not a delay when
  // showing the table view and the action buttons).
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.delegate = self;
  // Setup the minimized height (if the table has more than
  // `initialNumberOfVisibleCells` rows).
  NSMutableArray* currentDetents = [[NSMutableArray alloc] init];
  if (useMinimizedState) {
    // Show gradient view when the user is in minimized state to show that the
    // view can be scrolled.
    [self displayGradientView:YES];

    CGFloat bottomSheetHeight = [self initialHeight];
    auto detentBlock = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      return bottomSheetHeight;
    };
    UISheetPresentationControllerDetent* customDetent =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                              resolver:detentBlock];
    [currentDetents addObject:customDetent];
  }

  // Done calculating the height for the bottom sheet for
  // `initialNumberOfVisibleCells` rows, disable minimized height constraint.
  _minimizedHeightConstraint.active = NO;
  _heightConstraint.active = YES;

  // Calculate the full height of the bottom sheet with the minimized height
  // constraint disabled.
  __weak __typeof(self) weakSelf = self;
  auto fullHeightBlock = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf computeHeight:context.maximumDetentValue];
  };
  UISheetPresentationControllerDetent* customDetentExpand =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kCustomDetentIdentifier
                            resolver:fullHeightBlock];
  [currentDetents addObject:customDetentExpand];
  presentationController.detents = currentDetents;
  presentationController.selectedDetentIdentifier =
      useMinimizedState ? kCustomMinimizedDetentIdentifier
                        : kCustomDetentIdentifier;
}

// Returns whether the provided index path points to the last row of the table
// view.
- (BOOL)isLastRow:(NSIndexPath*)indexPath {
  return NSUInteger(indexPath.row) == ([self rowCount] - 1);
}

// Mocks the cells to calculate the real table view height in minized state.
- (CGFloat)computeTableViewHeightForMinimizedState:(NSUInteger)rowCount {
  CHECK(rowCount > [self initialNumberOfVisibleCells]);
  CGFloat height = 0;
  NSInteger count =
      static_cast<NSInteger>(floor([self initialNumberOfVisibleCells]));
  for (NSInteger i = 0; i <= count; i++) {
    CGFloat cellHeight = [self computeTableViewCellHeightAtIndex:i];
    if (i == count) {
      CGFloat diff = abs([self initialNumberOfVisibleCells] - count);
      height += cellHeight * diff;
    } else {
      height += cellHeight;
    }
  }
  return height;
}

// Returns the bottom sheet's height, limited to the maximum possible height.
- (CGFloat)computeHeight:(CGFloat)maximumDetentValue {
  CGFloat preferredHeight = [self preferredHeightForContent];
  _expandSizeTooLarge = (preferredHeight > maximumDetentValue);
  return _expandSizeTooLarge ? maximumDetentValue : preferredHeight;
}

- (CGFloat)initialHeight {
  if (!_minimizedStateHeight.has_value()) {
    _minimizedStateHeight = [self preferredHeightForContent];
  }
  return _minimizedStateHeight.value();
}
@end
