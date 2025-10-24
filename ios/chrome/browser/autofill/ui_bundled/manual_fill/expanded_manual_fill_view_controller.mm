// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/expanded_manual_fill_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

using manual_fill::ManualFillDataType;

namespace {

// Size of the Chrome logo.
constexpr CGFloat kChromeLogoSize = 28;

// Size of the Chrome logo when liquid glass is disabled.
constexpr CGFloat kChromeLogoSizePreLiquidGlass = 24;

// Size of the close button.
constexpr CGFloat kCloseButtonSize = 44;

// Size of the close button when liquid glass is disabled.
constexpr CGFloat kCloseButtonSizePreLiquidGlass = 30;
// Size of the data type icons representing the different segments
// of the segmented control.
constexpr CGFloat kDataTypeIconSize = 18;
// Bottom padding for the header view.
constexpr CGFloat kHeaderViewBottomPadding = 12;
// Leading and trailing padding for the header view.
constexpr CGFloat kHeaderViewHorizontalPadding = 16;
// Extra horizontal inset of the cells of a UITableView
// (UITableViewStyleInsetGrouped) on iOS 26.
constexpr CGFloat kIOS26TableViewCellExtraHorizontalInset = 4;
// Top padding for the header view.
constexpr CGFloat kHeaderViewTopPadding = 8;
// Top padding for the header view when in a bottom popover.
constexpr CGFloat kHeaderViewPopoverTopPadding = 22;

// Opacity of the segmented control background color.
constexpr CGFloat kSegmentedControlBackgroundColorOpacity = 0.05;

// Height of the segmented control.
constexpr CGFloat kSegmentedControlHeight = 32;
// Multiplier used to constraint the view's height.
constexpr CGFloat kViewHeightMultiplier = 0.6;

// Height of the header's top view. Used for the narrow layout only.
constexpr CGFloat kHeaderTopViewHeightNarrowLayout = 44;
// Vertical spacing between the bottom of the header top view and segmented
// control. Used for the narrow layout only.
constexpr CGFloat kHeaderTopViewBottomSpacingNarrowLayout = 4;

// Height of the header view. Used for the wide layout only.
constexpr CGFloat kHeaderViewHeightWideLayout = 44;
// Horizontal spacing between the Chrome logo and segmented control. Used for
// the wide layout only.
constexpr CGFloat kSegmentedControlLeadingSpacingWideLayout = 18;
// Horizontal spacing between the segmented control and close button. Used for
// the wide layout only.
constexpr CGFloat kSegmentedControlTrailingSpacingWideLayout = 15;

// Helper method to get the right segment index depending on the `data_type`.
int GetSegmentIndexForDataType(ManualFillDataType data_type) {
  switch (data_type) {
    case ManualFillDataType::kPassword:
      return 0;
    case ManualFillDataType::kPaymentMethod:
      return 1;
    case ManualFillDataType::kAddress:
      return 2;
    case ManualFillDataType::kOther:
      NOTREACHED();
  }
}

// Returns the color to use for the view's background.
UIColor* GetBackgroundColor() {
  if (@available(iOS 26, *)) {
    return UIColor.clearColor;
  }

  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

// Returns the size to use for the Chrome logo.
CGFloat GetChromeLogoSize() {
  if (@available(iOS 26, *)) {
    return kChromeLogoSize;
  }

  return kChromeLogoSizePreLiquidGlass;
}

// Returns the symbol configuration to use for the close button.
UIImageSymbolConfiguration* GetCloseButtonSymbolConfiguration() {
  if (@available(iOS 26, *)) {
    return [UIImageSymbolConfiguration
        configurationWithPointSize:kCloseButtonSize
                            weight:UIImageSymbolWeightThin
                             scale:UIImageSymbolScaleDefault];
  }

  return [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonSizePreLiquidGlass
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
}

// Returns the trailing offset of the close button.
// The image of the close button comes with a padding, which is noticable for a
// larger button. An offset is needed to visually align the close button to the
// trailing edge of the header view.
// - `size` is the size of the image, also the size of the button.
// - `kCloseButtonSize` is the point size for the symbol configuration of the
// larger button, representing the size of the symbol on screen.
CGFloat GetCloseButtonTrailingOffset(CGSize size) {
  return std::max(size.width - kCloseButtonSize, 0.0) * 0.5;
}

// Returns the foreground color to use for the close button color palette.
UIColor* GetCloseButtonForegroundColor() {
  if (@available(iOS 26, *)) {
    return [UIColor colorNamed:kTextPrimaryColor];
  }

  return [[UIColor secondaryLabelColor] colorWithAlphaComponent:0.6];
}

// Returns the constant to apply to the header view top constraint.
CGFloat GetHeaderViewTopConstraintConstant(bool is_compact_height) {
  if (@available(iOS 26, *)) {
    if (!(is_compact_height ||
          ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)) {
      return -kHeaderViewTopPadding;
    }
  }

  return kHeaderViewTopPadding;
}

// Returns the on-screen horizontal inset of `UITableViewCell`s based on the
// inset from their layout margins.
// As of iOS 26, the actual horizontal cell inset on iPhone is 4pt larger than
// the obtained inset from the layout margins of a cell.
// On iOS 18 and iPad, the two insets are identical.
// This function takes into account the additional inset and returns a value
// that can be used to align other UI elements with the cells.
CGFloat GetUpdatedHorizontalInset(CGFloat inset) {
  if (@available(iOS 26, *)) {
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
      return inset + kIOS26TableViewCellExtraHorizontalInset;
    }
  }
  return inset;
}

// Returns the horizontal inset of the cells of a UITableView.
CGFloat GetTableViewCellHorizontalInset(UITableView* tableView) {
  return GetUpdatedHorizontalInset(
      tableView.visibleCells.firstObject.layoutMargins.left);
}

}  // namespace

@interface ExpandedManualFillViewController ()

// Delegate to handle user interactions.
@property(nonatomic, weak) id<ExpandedManualFillViewControllerDelegate>
    delegate;

// Control allowing switching between the different data types. Not an ivar so
// that it can be used in tests.
@property(nonatomic, strong) UISegmentedControl* segmentedControl;

@end

@implementation ExpandedManualFillViewController {
  // Header view presented at the top of this view controller's view. Contains
  // the Chrome logo, close button and segmented control. The
  // positiong of these elements depends on the device's orientation:
  //   - Narrow layout: When in iPhone portrait mode. Chrome logo and close
  //     button are aligned horizontally above the segmented control.
  //   - Wide layout: When in iPhone landscape mode. Chrome logo, segmented
  //     control and close button are all aligned horizontally.
  UIView* _headerView;

  // View positioned at the top the of the header view when in narrow layout.
  // Contains the Chrome logo and close button.
  UIView* _headerTopView;

  // Header view's height constraint. Used for the wide layout only.
  NSLayoutConstraint* _headerViewHeightConstraint;

  // Header view's leading constraint.
  NSLayoutConstraint* _headerViewLeadingConstraint;

  // Header view's trailing constraint.
  NSLayoutConstraint* _headerViewTrailingConstraint;

  // Header view's top constraint.
  NSLayoutConstraint* _headerViewTopConstraint;

  // Header view's top constraint, when presented in a popover.
  NSLayoutConstraint* _headerViewPopoverTopConstraint;

  // Image view containing the Chrome logo.
  UIImageView* _chromeLogo;

  // Button to close the view.
  ExtendedTouchTargetButton* _closeButton;

  // Trailing offset of the `_closeButton` so the trailing edge of the image can
  // be aligned with its parent.
  CGFloat _closeButtonTrailingOffset;

  // Initial data type to present in the view. Reflects the type of the form the
  // user wants to fill.
  ManualFillDataType _initialDataType;

  // The leading and trailing inset of the child view controller's table view
  // cells. Used to constraint the leading and trailing sides of the header view
  // so that they horizontally align with the cells.
  CGFloat _tableViewCellHorizontalInset;
}

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

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier = manual_fill::kExpandedManualFillViewID;
  self.view.backgroundColor = GetBackgroundColor();

  // Set the view's frame to get the right height initially. Once the view's
  // window is loaded in `viewDidAppear`, the view's height will be
  // dynamically constraint to its window's height instead.
  self.view.autoresizingMask = UIViewAutoresizingNone;
  self.view.frame = CGRectMake(
      0, 0, 0, UIScreen.mainScreen.bounds.size.height * kViewHeightMultiplier);

  _headerView = [self createHeaderView];
  _headerTopView = [self createHeaderTopView];
  _chromeLogo = [self createChromeLogo];
  _closeButton = [self createCloseButton];
  _segmentedControl =
      [self createSegmentedControlAndSelectDataType:_initialDataType];
  _headerViewHeightConstraint = [_headerView.heightAnchor
      constraintEqualToConstant:kHeaderViewHeightWideLayout];
  _headerViewTopConstraint =
      [_headerView.topAnchor constraintEqualToAnchor:self.view.topAnchor];

  [self setUpHeaderView:_headerView
      headerViewHeightConstraint:_headerViewHeightConstraint
         headerViewTopConstraint:_headerViewTopConstraint
                      chromeLogo:_chromeLogo
                     closeButton:_closeButton
                segmentedControl:_segmentedControl
                   headerTopView:_headerTopView];
  [self.view addSubview:_headerView];

  // `_headerView` constraints.
  _headerViewLeadingConstraint = [_headerView.leadingAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                     constant:GetUpdatedHorizontalInset(
                                  kHeaderViewHorizontalPadding)];
  _headerViewTrailingConstraint = [_headerView.trailingAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                     constant:-GetUpdatedHorizontalInset(
                                  kHeaderViewHorizontalPadding)];
  _headerViewPopoverTopConstraint = [_headerView.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:kHeaderViewPopoverTopPadding];
  [NSLayoutConstraint activateConstraints:@[
    _headerViewTopConstraint,
    _headerViewLeadingConstraint,
    _headerViewTrailingConstraint,
  ]];

  NSArray<UITrait>* traits =
      TraitCollectionSetForTraits(@[ UITraitVerticalSizeClass.class ]);
  __weak __typeof(self) weakSelf = self;
  UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                   UITraitCollection* previousCollection) {
    [weakSelf resetHeaderViewOnTraitChange];
  };
  [self registerForTraitChanges:traits withHandler:handler];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [self adjustTopHeaderViewConstraint];
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

  // Bring focus to the expanded view by focusing on the Chrome logo.
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  _chromeLogo);
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  UITableView* tableView = self.childViewController.tableView;
  UITableViewStyle style = tableView.style;
  CGFloat tableViewCellHorizontalInset =
      GetTableViewCellHorizontalInset(tableView);

  // If needed, update the horizontal contraints of the header view so that it
  // is horizontally aligned with the table view cells.
  if (style == UITableViewStyleInsetGrouped && tableViewCellHorizontalInset &&
      _tableViewCellHorizontalInset != tableViewCellHorizontalInset) {
    _tableViewCellHorizontalInset = tableViewCellHorizontalInset;
    [self updateHeaderViewHorizontalConstraints:_headerViewLeadingConstraint
                             trailingConstraint:_headerViewTrailingConstraint
                                       constant:_tableViewCellHorizontalInset];
  }

  [self adjustTopHeaderViewConstraint];
}

#pragma mark - Setters

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

  // `_childViewController.view` constraints.
  [_childViewController.view.topAnchor
      constraintEqualToAnchor:_headerView.bottomAnchor
                     constant:kHeaderViewBottomPadding]
      .active = YES;
  AddSameConstraintsToSides(
      _childViewController.view, self.view,
      LayoutSides::kBottom | LayoutSides::kTrailing | LayoutSides::kLeading);
}

#pragma mark - Private

// Adjusts the top padding so that it takes into account the arrow of the
// popover view if the arrow is at the top of the view.
- (void)adjustTopHeaderViewConstraint {
  BOOL isPopoverUpArrow = self.popoverPresentationController.arrowDirection ==
                          UIPopoverArrowDirectionUp;
  _headerViewTopConstraint.active = !isPopoverUpArrow;
  _headerViewPopoverTopConstraint.active = isPopoverUpArrow;
}

// Creates and configures the header view.
- (UIView*)createHeaderView {
  UIView* headerView = [[UIView alloc] init];
  headerView.translatesAutoresizingMaskIntoConstraints = NO;
  headerView.accessibilityIdentifier =
      manual_fill::kExpandedManualFillHeaderViewID;

  return headerView;
}

// Creates and configures the header's top view.
- (UIView*)createHeaderTopView {
  UIView* headerTopView = [[UIView alloc] init];
  headerTopView.translatesAutoresizingMaskIntoConstraints = NO;
  headerTopView.accessibilityIdentifier =
      manual_fill::kExpandedManualFillHeaderTopViewID;

  return headerTopView;
}

// Creates and configures the Chrome logo.
- (UIImageView*)createChromeLogo {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* image = MakeSymbolMulticolor(CustomSymbolWithPointSize(
      kMulticolorChromeballSymbol, GetChromeLogoSize()));
#else
  UIImage* image =
      CustomSymbolWithPointSize(kChromeProductSymbol, GetChromeLogoSize());
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImageView* chromeLogo = [[UIImageView alloc] initWithImage:image];
  chromeLogo.translatesAutoresizingMaskIntoConstraints = NO;
  chromeLogo.contentMode = UIViewContentModeCenter;
  chromeLogo.isAccessibilityElement = YES;
  chromeLogo.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_VIEW_ACCESSIBILITY_ANNOUNCEMENT);
  chromeLogo.accessibilityTraits = UIAccessibilityTraitNone;
  chromeLogo.accessibilityIdentifier =
      manual_fill::kExpandedManualFillChromeLogoID;

  [chromeLogo setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisHorizontal];
  [chromeLogo
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  return chromeLogo;
}

// Creates and configures the close button.
- (ExtendedTouchTargetButton*)createCloseButton {
  ExtendedTouchTargetButton* closeButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  closeButton.contentMode = UIViewContentModeCenter;
  closeButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_CLOSE_BUTTON_ACCESSIBILITY_LABEL);

  UIImageSymbolConfiguration* symbolConfiguration =
      GetCloseButtonSymbolConfiguration();
  UIImage* buttonImage = SymbolWithPalette(
      DefaultSymbolWithConfiguration(kXMarkCircleFillSymbol,
                                     symbolConfiguration),
      @[ GetCloseButtonForegroundColor(), [UIColor tertiarySystemFillColor] ]);
  _closeButtonTrailingOffset = GetCloseButtonTrailingOffset(buttonImage.size);
  [closeButton setImage:buttonImage forState:UIControlStateNormal];

  [closeButton setContentHuggingPriority:UILayoutPriorityRequired
                                 forAxis:UILayoutConstraintAxisHorizontal];
  [closeButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  [closeButton addTarget:self
                  action:@selector(onCloseButtonPressed:)
        forControlEvents:UIControlEventTouchUpInside];

  return closeButton;
}

// Creates and configures the segmented control. `dataType` indicates which
// segment to select.
- (UISegmentedControl*)createSegmentedControlAndSelectDataType:
    (ManualFillDataType)dataType {
  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kDataTypeIconSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];

  UIImage* passwordIcon =
      CustomSymbolWithConfiguration(kPasswordSymbol, symbolConfiguration);
  passwordIcon.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_PASSWORD_TAB_ACCESSIBILITY_LABEL);

  UIImage* cardIcon =
      DefaultSymbolWithConfiguration(kCreditCardSymbol, symbolConfiguration);
  cardIcon.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_PAYMENT_TAB_ACCESSIBILITY_LABEL);

  UIImage* addressIcon =
      CustomSymbolWithConfiguration(kLocationSymbol, symbolConfiguration);
  addressIcon.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_ADDRESS_TAB_ACCESSIBILITY_LABEL);

  UISegmentedControl* segmentedControl = [[UISegmentedControl alloc]
      initWithItems:@[ passwordIcon, cardIcon, addressIcon ]];
  segmentedControl.translatesAutoresizingMaskIntoConstraints = NO;
  segmentedControl.selectedSegmentIndex = GetSegmentIndexForDataType(dataType);
  [segmentedControl addTarget:self
                       action:@selector(onSegmentSelected:)
             forControlEvents:UIControlEventValueChanged];

  if (@available(iOS 26, *)) {
    segmentedControl.backgroundColor = [[UIColor colorNamed:kGrey700Color]
        colorWithAlphaComponent:kSegmentedControlBackgroundColorOpacity];
  }

  return segmentedControl;
}

// Sets up the header view depending on the device's orientation.
- (void)setUpHeaderView:(UIView*)headerView
    headerViewHeightConstraint:(NSLayoutConstraint*)headerViewHeightConstraint
       headerViewTopConstraint:(NSLayoutConstraint*)headerViewTopConstraint
                    chromeLogo:(UIImageView*)chromeLogo
                   closeButton:(UIButton*)closeButton
              segmentedControl:(UISegmentedControl*)segmentedControl
                 headerTopView:(UIView*)headerTopView {
  // If the vertical size class is compact, apply the wide layout. Otherwise,
  // apply the narrow layout.
  bool isCompactHeight = IsCompactHeight(self);
  _headerViewTopConstraint.constant =
      GetHeaderViewTopConstraintConstant(isCompactHeight);
  if (isCompactHeight) {
    [headerView addSubview:chromeLogo];
    [headerView addSubview:closeButton];
    [headerView addSubview:segmentedControl];

    headerViewHeightConstraint.active = YES;

    [NSLayoutConstraint activateConstraints:@[
      // `chromeLogo` constraints.
      [chromeLogo.centerYAnchor
          constraintEqualToAnchor:headerView.centerYAnchor],
      [chromeLogo.leadingAnchor
          constraintEqualToAnchor:headerView.leadingAnchor],

      // `closeButton` constraints.
      [closeButton.centerYAnchor
          constraintEqualToAnchor:headerView.centerYAnchor],
      [closeButton.trailingAnchor
          constraintEqualToAnchor:headerView.trailingAnchor],

      // `segmentedControl` constraints.
      [segmentedControl.centerYAnchor
          constraintEqualToAnchor:headerView.centerYAnchor],
      [segmentedControl.leadingAnchor
          constraintEqualToAnchor:chromeLogo.trailingAnchor
                         constant:kSegmentedControlLeadingSpacingWideLayout],
      [segmentedControl.trailingAnchor
          constraintEqualToAnchor:closeButton.leadingAnchor
                         constant:-kSegmentedControlTrailingSpacingWideLayout],
    ]];
  } else {
    [headerView addSubview:headerTopView];
    [headerView addSubview:segmentedControl];

    [headerTopView addSubview:chromeLogo];
    [headerTopView addSubview:closeButton];

    headerViewHeightConstraint.active = NO;

    [NSLayoutConstraint activateConstraints:@[
      // `chromeLogo` constraints.
      [chromeLogo.centerYAnchor
          constraintEqualToAnchor:headerTopView.centerYAnchor],
      [chromeLogo.centerXAnchor
          constraintEqualToAnchor:headerTopView.centerXAnchor],

      // `closeButton` constraints.
      [closeButton.centerYAnchor
          constraintEqualToAnchor:headerTopView.centerYAnchor],
      [closeButton.trailingAnchor
          constraintEqualToAnchor:headerTopView.trailingAnchor
                         constant:_closeButtonTrailingOffset],

      // `headerTopView` constraints.
      [headerTopView.topAnchor constraintEqualToAnchor:headerView.topAnchor],
      [headerTopView.trailingAnchor
          constraintEqualToAnchor:headerView.trailingAnchor],
      [headerTopView.leadingAnchor
          constraintEqualToAnchor:headerView.leadingAnchor],
      [headerTopView.heightAnchor
          constraintEqualToConstant:kHeaderTopViewHeightNarrowLayout],

      // `segmentedControl` constraints.
      [segmentedControl.topAnchor
          constraintEqualToAnchor:headerTopView.bottomAnchor
                         constant:kHeaderTopViewBottomSpacingNarrowLayout],
      [segmentedControl.bottomAnchor
          constraintEqualToAnchor:headerView.bottomAnchor],
      [segmentedControl.trailingAnchor
          constraintEqualToAnchor:headerView.trailingAnchor],
      [segmentedControl.leadingAnchor
          constraintEqualToAnchor:headerView.leadingAnchor],
    ]];
  }

  // Constraints that are common to both layouts.
  [NSLayoutConstraint activateConstraints:@[
    // `segmentedControl` constraints.
    [segmentedControl.heightAnchor
        constraintEqualToConstant:kSegmentedControlHeight],
  ]];
}

// Resets the header view. Called when a layout change is needed.
- (void)resetHeaderView:(UIView*)headerView
    headerViewHeightConstraint:(NSLayoutConstraint*)headerViewHeightConstraint
       headerViewTopConstraint:(NSLayoutConstraint*)headerViewTopConstraint
                    chromeLogo:(UIImageView*)chromeLogo
                   closeButton:(UIButton*)closeButton
              segmentedControl:(UISegmentedControl*)segmentedControl
                 headerTopView:(UIView*)headerTopView {
  // Remove subviews to reset their constraints.
  [chromeLogo removeFromSuperview];
  [closeButton removeFromSuperview];
  [segmentedControl removeFromSuperview];
  [headerTopView removeFromSuperview];

  [self setUpHeaderView:headerView
      headerViewHeightConstraint:headerViewHeightConstraint
         headerViewTopConstraint:headerViewTopConstraint
                      chromeLogo:chromeLogo
                     closeButton:closeButton
                segmentedControl:segmentedControl
                   headerTopView:headerTopView];
}

// Updates the horizontal constraints of the header view with the given
// `constant`.
- (void)updateHeaderViewHorizontalConstraints:
            (NSLayoutConstraint*)leadingConstraint
                           trailingConstraint:
                               (NSLayoutConstraint*)trailingConstraint
                                     constant:(CGFloat)constant {
  leadingConstraint.constant = constant;
  trailingConstraint.constant = -constant;
}

// Handles taps on the close button.
- (void)onCloseButtonPressed:(id)sender {
  base::RecordAction(base::UserMetricsAction("ManualFallback_Close"));
  [self.delegate expandedManualFillViewController:self
                              didPressCloseButton:sender];
}

// Handles the selection of a different data type from the segmented control.
- (void)onSegmentSelected:(UISegmentedControl*)segmentedControl {
  ManualFillDataType selectedType =
      static_cast<ManualFillDataType>(segmentedControl.selectedSegmentIndex);
  [self.delegate expandedManualFillViewController:self
                           didSelectSegmentOfType:selectedType];
}

// Update the header view's layout when the view's vertical size class
// changes.
- (void)resetHeaderViewOnTraitChange {
  [self resetHeaderView:_headerView
      headerViewHeightConstraint:_headerViewHeightConstraint
         headerViewTopConstraint:_headerViewTopConstraint
                      chromeLogo:_chromeLogo
                     closeButton:_closeButton
                segmentedControl:_segmentedControl
                   headerTopView:_headerTopView];
}

@end
