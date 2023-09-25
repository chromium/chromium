// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

#import <Foundation/Foundation.h>

namespace {

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

// Custom height for the gradient view of the bottom sheet.
CGFloat const kCustomGradientViewHeight = 30;

// Custom detent identifier for when the bottom sheet is minimized.
NSString* const kCustomMinimizedDetentIdentifier = @"customMinimizedDetent";

// Custom detent identifier for when the bottom sheet is expanded.
NSString* const kCustomExpandedDetentIdentifier = @"customExpandedDetent";

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
    // Expand to custom size (only available for iOS 16+).
    CGFloat fullHeight = [self preferredHeightForContent];
    auto resolver = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      BOOL tooLarge = (fullHeight > context.maximumDetentValue);
      [self displayGradientView:tooLarge];
      return tooLarge ? context.maximumDetentValue : fullHeight;
    };
    UISheetPresentationControllerDetent* customDetentExpand =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:kCustomExpandedDetentIdentifier
                              resolver:resolver];
    NSMutableArray* currentDetents =
        [presentationController.detents mutableCopy];
    [currentDetents addObject:customDetentExpand];
    presentationController.detents = currentDetents;
    [presentationController animateChanges:^{
      presentationController.selectedDetentIdentifier =
          kCustomExpandedDetentIdentifier;
    }];
  } else {
    // Expand to large detent.
    [presentationController animateChanges:^{
      presentationController.selectedDetentIdentifier =
          UISheetPresentationControllerDetentIdentifierLarge;
    }];
  }

  [self selectFirstRow];
}

- (void)reloadTableViewData {
  [_tableView reloadData];
}

- (CGFloat)tableViewEstimatedRowHeight {
  return kTableViewEstimatedRowHeight;
}

- (NSInteger)selectedRow {
  return _tableView.indexPathForSelectedRow.row;
}

- (CGFloat)tableViewContentSizeHeight {
  return _tableView.contentSize.height;
}

- (CGFloat)tableViewWidth {
  return _tableView.frame.size.width;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.underTitleView = [self createTableView];

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.imageHasFixedSize = YES;
  self.showsVerticalScrollIndicator = NO;
  self.showDismissBarButton = NO;
  self.topAlignedLayout = YES;
  self.customScrollViewBottomInsets = 0;
  self.customGradientViewHeight = kCustomGradientViewHeight;

  [super viewDidLoad];

  [self displayGradientView:NO];

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

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Update the custom detent with the correct initial height when trait
  // collection changed (for example when the user uses large font).
  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    UISheetPresentationController* presentationController =
        self.sheetPresentationController;
    if (@available(iOS 16, *)) {
      CGFloat bottomSheetHeight = [self preferredHeightForContent];
      auto resolver = ^CGFloat(
          id<UISheetPresentationControllerDetentResolutionContext> context) {
        return bottomSheetHeight;
      };

      UISheetPresentationControllerDetent* customDetent =
          [UISheetPresentationControllerDetent
              customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                                resolver:resolver];
      presentationController.detents = @[ customDetent ];
      presentationController.selectedDetentIdentifier =
          kCustomMinimizedDetentIdentifier;
    }
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

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self displayGradientView:![self isScrolledToBottom]];
}

#pragma mark - Public

- (void)selectFirstRow {
  [_tableView selectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]
                          animated:NO
                    scrollPosition:UITableViewScrollPositionNone];
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
    CGFloat bottomSheetHeight = [self preferredHeightForContent];
    auto resolver = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      return bottomSheetHeight;
    };
    UISheetPresentationControllerDetent* customDetent =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                              resolver:resolver];
    presentationController.detents = @[ customDetent ];
    presentationController.selectedDetentIdentifier =
        kCustomMinimizedDetentIdentifier;
  } else {
    presentationController.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];
    presentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierMedium;
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

@end
