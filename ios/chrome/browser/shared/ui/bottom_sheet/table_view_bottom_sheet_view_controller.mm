// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

#import <Foundation/Foundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Estimated base height value for the bottom sheet without the table view.
CGFloat const kEstimatedBaseHeightForBottomSheet = 195;

// Sets a custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

// Estimated row height for each cell in the table view.
CGFloat const kTableViewEstimatedRowHeight = 75;

// Radius size of the table view.
CGFloat const kTableViewCornerRadius = 10;

// TableView's width constraint multiplier in Portrait mode for iPhone only.
CGFloat const kPortraitIPhoneTableViewWidthMultiplier = 0.95;

// TableView's width constraint multiplier in all mode (except iPhone Portrait).
CGFloat const kTableViewWidthMultiplier = 0.65;

// Scroll view's bottom anchor constant.
CGFloat const kScrollViewBottomAnchorConstant = 10;

// Initial height's extra bottom height padding so it does not crop the cell.
CGFloat const kInitialHeightPadding = 5;

}  // namespace

@interface TableViewBottomSheetViewController () {
  // Table view for the list of suggestions.
  UITableView* _tableView;

  // TableView's width constraint in portrait mode.
  NSLayoutConstraint* _portraitTableWidthConstraint;

  // TableView's width constraint in landscape mode.
  NSLayoutConstraint* _landscapeTableWidthConstraint;
}

@end

@implementation TableViewBottomSheetViewController

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

  [self selectFirstRow];

  return _tableView;
}

- (void)expand:(NSInteger)numberOfRows {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  if (@available(iOS 16, *)) {
    // Update the bottom anchor constant value only for iPhone.
    if ([UIDevice currentDevice].userInterfaceIdiom ==
        UIUserInterfaceIdiomPhone) {
      [self
          changeScrollViewBottomAnchorConstant:kScrollViewBottomAnchorConstant];
    }

    // Expand to custom size (only available for iOS 16+).
    CGFloat fullHeight = [self fullHeight:numberOfRows];

    __weak __typeof(self) weakSelf = self;
    auto fullHeightBlock = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      BOOL tooLarge = (fullHeight > context.maximumDetentValue);
      [weakSelf setTableViewScrollEnabled:tooLarge];
      if (tooLarge) {
        // Reset bottom anchor constant value so there is enough space for the
        // gradient view.
        [self resetScrollViewBottomAnchorConstant];
      }
      return tooLarge ? context.maximumDetentValue : fullHeight;
    };
    UISheetPresentationControllerDetent* customDetentExpand =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:@"customDetentExpand"
                              resolver:fullHeightBlock];
    NSMutableArray* currentDetents =
        [presentationController.detents mutableCopy];
    [currentDetents addObject:customDetentExpand];
    presentationController.detents = [currentDetents copy];
    [presentationController animateChanges:^{
      presentationController.selectedDetentIdentifier = @"customDetentExpand";
    }];
  } else {
    // Expand to large detent.
    [self setTableViewScrollEnabled:YES];
    [presentationController animateChanges:^{
      presentationController.selectedDetentIdentifier =
          UISheetPresentationControllerDetentIdentifierLarge;
    }];
  }

  [self selectFirstRow];
}

- (CGFloat)bottomSheetEstimatedHeight {
  return kEstimatedBaseHeightForBottomSheet;
}

- (CGFloat)tableViewEstimatedRowHeight {
  return kTableViewEstimatedRowHeight;
}

- (NSInteger)selectedRow {
  return _tableView.indexPathForSelectedRow.row;
}

- (CGFloat)tableViewHeight {
  return _tableView.contentSize.height;
}

- (UITableView*)tableView {
  return _tableView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.underTitleView = [self createTableView];

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.imageHasFixedSize = YES;
  self.showsVerticalScrollIndicator = NO;
  self.showDismissBarButton = NO;
  self.customSpacingAfterImage = 0;
  self.topAlignedLayout = YES;
  self.scrollEnabled = NO;
  self.customScrollViewBottomInsets = 0;

  [self updateCustomGradientViewHeight:0];

  [super viewDidLoad];

  // Assign table view's width anchor now that it is in the same hierarchy as
  // the top view.
  [self createTableViewWidthConstraint:self.view.layoutMarginsGuide];

  [self setUpBottomSheet];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self adjustTableViewWidthConstraint];
}

- (void)viewWillAppear:(BOOL)animated {
  // Update the custom detent with the correct initial height for the bottom
  // sheet. (Initial height is not calculated properly in -viewDidLoad, but we
  // need to setup the bottom sheet in that method so there is not a delay when
  // showing the table view and the action buttons).
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  if (@available(iOS 16, *)) {
    CGFloat bottomSheetHeight = [self initialHeight];
    auto detentBlock = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      return bottomSheetHeight;
    };
    UISheetPresentationControllerDetent* customDetent =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:@"customDetent"
                              resolver:detentBlock];
    presentationController.detents = @[ customDetent ];
    presentationController.selectedDetentIdentifier = @"customDetent";
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

#pragma mark - Public

- (void)selectFirstRow {
  [_tableView selectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]
                          animated:NO
                    scrollPosition:UITableViewScrollPositionNone];
}

- (CGFloat)initialHeight {
  CGFloat bottomSheetHeight = [self bottomSheetHeight];
  if (bottomSheetHeight > 0) {
    return bottomSheetHeight + kInitialHeightPadding;
  }
  // Return an estimated height if we can't calculate the actual height.
  return kEstimatedBaseHeightForBottomSheet +
         kTableViewEstimatedRowHeight * [self initialNumberOfVisibleCells];
}

- (CGFloat)fullHeight:(NSInteger)numberOfRows {
  CGFloat bottomSheetHeight = [self bottomSheetHeight];
  if (bottomSheetHeight > 0) {
    return bottomSheetHeight;
  }

  // Return an estimated height for the bottom sheet while showing all rows
  // (using estimated heights).
  return kEstimatedBaseHeightForBottomSheet +
         (kTableViewEstimatedRowHeight * numberOfRows);
}

- (void)setTableViewScrollEnabled:(BOOL)enabled {
  _tableView.scrollEnabled = enabled;
  self.scrollEnabled = enabled;

  // Add gradient view to show that the user can scroll.
  if (enabled) {
    [self updateCustomGradientViewHeight:16];
  }
}

- (CGFloat)initialNumberOfVisibleCells {
  return 1;
}

#pragma mark - Private

// Configures the bottom sheet's appearance and detents.
- (void)setUpBottomSheet {
  self.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  if (@available(iOS 16, *)) {
    CGFloat bottomSheetHeight = [self initialHeight];
    auto detentBlock = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      return bottomSheetHeight;
    };
    UISheetPresentationControllerDetent* customDetent =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:@"customDetent"
                              resolver:detentBlock];
    presentationController.detents = @[ customDetent ];
    presentationController.selectedDetentIdentifier = @"customDetent";
  } else {
    presentationController.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];
  }
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
}

// Creates the tableview's width constraints and set their initial active state.
- (void)createTableViewWidthConstraint:(UILayoutGuide*)margins {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  _portraitTableWidthConstraint = [_tableView.widthAnchor
      constraintGreaterThanOrEqualToAnchor:margins.widthAnchor
                                multiplier:
                                    (idiom == UIUserInterfaceIdiomPad)
                                        ? kTableViewWidthMultiplier
                                        : kPortraitIPhoneTableViewWidthMultiplier];
  _landscapeTableWidthConstraint = [_tableView.widthAnchor
      constraintGreaterThanOrEqualToAnchor:margins.widthAnchor
                                multiplier:kTableViewWidthMultiplier];
  [self adjustTableViewWidthConstraint];
}

// Change the tableview's width constraint based on the screen's orientation.
- (void)adjustTableViewWidthConstraint {
  BOOL isLandscape =
      UIDeviceOrientationIsLandscape([UIDevice currentDevice].orientation);
  _landscapeTableWidthConstraint.active = isLandscape;
  _portraitTableWidthConstraint.active = !isLandscape;
}

// Returns the height of the bottom sheet view.
- (CGFloat)bottomSheetHeight {
  return
      [self.view
          systemLayoutSizeFittingSize:CGSizeMake(self.view.frame.size.width, 1)]
          .height;
}

@end
