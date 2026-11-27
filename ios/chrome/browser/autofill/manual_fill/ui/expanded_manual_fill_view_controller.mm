// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/expanded_manual_fill_view_controller.h"

#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/expanded_manual_fill_header_view.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/fallback_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

using manual_fill::ManualFillDataType;

namespace {

// Bottom padding for the header view.
constexpr CGFloat kHeaderViewBottomPadding = 12;
// Leading and trailing padding for the header view.
constexpr CGFloat kHeaderViewHorizontalPadding = 16;
// Extra horizontal inset of the cells of a UITableView
// (UITableViewStyleInsetGrouped) on iOS 26.
constexpr CGFloat kIOS26TableViewCellExtraHorizontalInset = 4;
// Top padding for the header view.
constexpr CGFloat kHeaderViewTopPadding = 8;
// Top padding for the header view on iPhones with a normal height and iOS 26.0,
// but not including iOS 26.4. iPhone models with iOS 26.0 to iOS 26.3 have
// extra space on the top of the bottom sheet while iOS 26.4 returns to normal
// spacing. See crbug.com/495768760.
constexpr CGFloat kIOS26HeaderViewTopPadding = -10;
// Top padding for the header view when in a bottom popover.
constexpr CGFloat kHeaderViewPopoverTopPadding = 22;

// Multiplier used to constrain the view's height.
constexpr CGFloat kViewHeightMultiplier = 0.6;
// Height used during the initial layout until the view is added to the view
// hierarchy.
constexpr CGFloat kInitialHeightPlaceholder = 400;

// Returns the color to use for the view's background.
UIColor* GetBackgroundColor() {
  if (@available(iOS 26, *)) {
    return UIColor.clearColor;
  }
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

// Returns the constant to apply to the header view top constraint.
CGFloat GetHeaderViewTopConstraintConstant(
    bool is_compact_height,
    id<UITraitEnvironment> trait_environment) {
  // Negative top padding should be applied only for iOS 26.0 - iOS 26.3.
  if (@available(iOS 26.0, *)) {
    if (!@available(iOS 26.4, *)) {
      if (!is_compact_height &&
          ![ManualFillUtil shouldUsePopover:trait_environment]) {
        return kIOS26HeaderViewTopPadding;
      }
    }
  }
  return kHeaderViewTopPadding;
}

// Returns the on-screen horizontal inset of `UITableViewCell`s based on the
// inset from their layout margins.
// As of iOS 26, the actual horizontal cell inset on iPhone is 4pt larger than
// the obtained inset from the layout margins of a cell.
// On iOS 18 and iPad, the two insets are identical.
CGFloat GetUpdatedHorizontalInset(CGFloat inset,
                                  id<UITraitEnvironment> trait_environment) {
  if (@available(iOS 26, *)) {
    if (![ManualFillUtil shouldUsePopover:trait_environment]) {
      return inset + kIOS26TableViewCellExtraHorizontalInset;
    }
  }
  return inset;
}

// Returns the horizontal inset of the cells of `table_view`, or 0 if no cell is
// currently visible.
CGFloat GetTableViewCellHorizontalInset(
    UITableView* table_view,
    id<UITraitEnvironment> trait_environment) {
  UITableViewCell* first_cell = table_view.visibleCells.firstObject;
  if (!first_cell) {
    return 0;
  }
  return GetUpdatedHorizontalInset(first_cell.layoutMargins.left,
                                   trait_environment);
}

}  // namespace

@interface ExpandedManualFillViewController () <
    ExpandedManualFillHeaderViewDelegate>

// Delegate to handle user interactions.
@property(nonatomic, weak) id<ExpandedManualFillViewControllerDelegate>
    delegate;

// Control allowing switching between the different data types, exposed for
// testing.
@property(nonatomic, strong, readonly) UISegmentedControl* segmentedControl;

@end

@implementation ExpandedManualFillViewController {
  // Header view presented at the top of this view controller's view.
  ExpandedManualFillHeaderView* _headerView;

  // Header view's top constraint.
  NSLayoutConstraint* _headerViewTopConstraint;
  // Header view's top constraint when presented in a popover.
  NSLayoutConstraint* _headerViewPopoverTopConstraint;

  // Initial data type to present in the view.
  ManualFillDataType _initialDataType;

  // Cached horizontal inset of the child table view's cells.
  CGFloat _tableViewCellHorizontalInset;
}

#pragma mark - Initialization

- (instancetype)initWithDelegate:
                    (id<ExpandedManualFillViewControllerDelegate>)delegate
                     forDataType:(ManualFillDataType)dataType {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _delegate = delegate;
    _initialDataType = dataType;
  }
  return self;
}

#pragma mark - Properties

- (UISegmentedControl*)segmentedControl {
  return _headerView.segmentedControl;
}

- (void)setChildViewController:(FallbackViewController*)childViewController {
  if (_childViewController == childViewController) {
    return;
  }

  // Remove the previous child view controller.
  [_childViewController willMoveToParentViewController:nil];
  [_childViewController.view removeFromSuperview];
  [_childViewController removeFromParentViewController];

  _childViewController = childViewController;
  _childViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [_childViewController willMoveToParentViewController:self];
  [self addChildViewController:_childViewController];
  [self.view addSubview:self.childViewController.view];
  [_childViewController didMoveToParentViewController:self];

  [_childViewController.view.topAnchor
      constraintEqualToAnchor:_headerView.bottomAnchor
                     constant:kHeaderViewBottomPadding]
      .active = YES;
  AddSameConstraintsToSides(_childViewController.view, self.view,
                            LayoutSides::kBottom | LayoutSides::kHorizontal);
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier = manual_fill::kExpandedManualFillViewID;
  self.view.backgroundColor = GetBackgroundColor();

  // Set the view's frame to get the right height initially. Once the view's
  // window is loaded in `viewDidAppear`, the view's height will be dynamically
  // constrained to its window's height instead.
  self.view.autoresizingMask = UIViewAutoresizingNone;
  self.view.frame = CGRectMake(0, 0, 0, kInitialHeightPlaceholder);

  _headerView =
      [[ExpandedManualFillHeaderView alloc] initWithDataType:_initialDataType];
  _headerView.delegate = self;
  _headerView.horizontalInset =
      GetUpdatedHorizontalInset(kHeaderViewHorizontalPadding, self);
  [self.view addSubview:_headerView];

  _headerViewTopConstraint = [_headerView.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:GetHeaderViewTopConstraintConstant(
                                  IsCompactHeight(self), self)];
  _headerViewPopoverTopConstraint = [_headerView.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:kHeaderViewPopoverTopPadding];
  [NSLayoutConstraint activateConstraints:@[
    _headerViewTopConstraint,
    [_headerView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
    [_headerView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
  ]];

  [self updateHeaderForCurrentTraits];

  __weak __typeof(self) weakSelf = self;
  [self registerForTraitChanges:@[ UITraitVerticalSizeClass.class ]
                    withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                  UITraitCollection* previousCollection) {
                      [weakSelf updateHeaderForCurrentTraits];
                    }];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [self updateHeaderForCurrentTraits];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Anchor the view's height to its window's height so that the view's height
  // resizes dynamically when switching between portrait and landscape modes.
  self.view.autoresizingMask = UIViewAutoresizingFlexibleHeight;
  [NSLayoutConstraint activateConstraints:@[
    [self.view.heightAnchor
        constraintEqualToAnchor:self.view.window.heightAnchor
                     multiplier:kViewHeightMultiplier],
  ]];

  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  _headerView.preferredAccessibilityFocusView);
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  UITableView* tableView = self.childViewController.tableView;
  UITableViewStyle style = tableView.style;
  CGFloat tableViewCellHorizontalInset =
      GetTableViewCellHorizontalInset(tableView, self);

  // Align the header controls horizontally with the table view cells.
  if (style == UITableViewStyleInsetGrouped && tableViewCellHorizontalInset &&
      _tableViewCellHorizontalInset != tableViewCellHorizontalInset) {
    _tableViewCellHorizontalInset = tableViewCellHorizontalInset;
    _headerView.horizontalInset = _tableViewCellHorizontalInset;
  }

  [self adjustTopHeaderViewConstraint];
}

#pragma mark - ExpandedManualFillHeaderViewDelegate

- (void)headerView:(ExpandedManualFillHeaderView*)headerView
    didPressCloseButton:(UIButton*)closeButton {
  [self.delegate expandedManualFillViewController:self
                              didPressCloseButton:closeButton];
}

- (void)headerView:(ExpandedManualFillHeaderView*)headerView
    didSelectDataType:(ManualFillDataType)dataType {
  [self.delegate expandedManualFillViewController:self
                           didSelectSegmentOfType:dataType];
}

#pragma mark - Private

// Updates the header's top padding and narrow/wide layout for the current
// traits.
- (void)updateHeaderForCurrentTraits {
  [self adjustTopHeaderViewConstraint];
  _headerView.wideLayout = IsCompactHeight(self);
}

// Adjusts the top padding to account for a popover up-arrow or compact height.
- (void)adjustTopHeaderViewConstraint {
  _headerViewTopConstraint.constant =
      GetHeaderViewTopConstraintConstant(IsCompactHeight(self), self);
  if (!_tableViewCellHorizontalInset) {
    _headerView.horizontalInset =
        GetUpdatedHorizontalInset(kHeaderViewHorizontalPadding, self);
  }
  BOOL isPopoverUpArrow = self.popoverPresentationController.arrowDirection ==
                          UIPopoverArrowDirectionUp;
  _headerViewTopConstraint.active = !isPopoverUpArrow;
  _headerViewPopoverTopConstraint.active = isPopoverUpArrow;
}

@end
