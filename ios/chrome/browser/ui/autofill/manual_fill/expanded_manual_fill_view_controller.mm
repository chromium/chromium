// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_view_controller.h"

#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using manual_fill::ManualFillDataType;

namespace {

// Size of the Chrome logo.
constexpr CGFloat kChromeLogoSize = 24;
// Size of the close button.
constexpr CGFloat kCloseButtonSize = 30;
// Size of the data type icons representing the different segments
// of the segmented control.
constexpr CGFloat kDataTypeIconSize = 18;
// Leading and trailing padding for the header view.
constexpr CGFloat kHeaderViewHorizontalPadding = 20;
// Top padding for the header view.
constexpr CGFloat kHeaderViewTopPadding = 8;
// Height of the segmented control.
constexpr CGFloat kSegmentedControlHeight = 32;

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
  }
}

}  // namespace

@interface ExpandedManualFillViewController ()

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

  // Image view containing the Chrome logo.
  UIImageView* _chromeLogo;

  // Button to close the view.
  ExtendedTouchTargetButton* _closeButton;

  // Initial data type to present in the view. Reflects the type of the form the
  // user wants to fill.
  ManualFillDataType _initialDataType;
}

- (instancetype)initForDataType:(ManualFillDataType)dataType {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _initialDataType = dataType;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier = manual_fill::kExpandedManualFillViewID;
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  _headerView = [self createHeaderView];
  _headerTopView = [self createHeaderTopView];
  _chromeLogo = [self createChromeLogo];
  _closeButton = [self createCloseButton];
  _segmentedControl =
      [self createSegmentedControlAndSelectDataType:_initialDataType];
  _headerViewHeightConstraint = [_headerView.heightAnchor
      constraintEqualToConstant:kHeaderViewHeightWideLayout];

  [self setUpHeaderView:_headerView
      headerViewHeightConstraint:_headerViewHeightConstraint
                      chromeLogo:_chromeLogo
                     closeButton:_closeButton
                segmentedControl:_segmentedControl
                   headerTopView:_headerTopView];
  [self.view addSubview:_headerView];

  [NSLayoutConstraint activateConstraints:@[
    // `_headerView` constraints.
    [_headerView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                          constant:kHeaderViewTopPadding],
    [_headerView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHeaderViewHorizontalPadding],
    [_headerView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kHeaderViewHorizontalPadding],
  ]];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  UIAccessibilityPostNotification(
      UIAccessibilityAnnouncementNotification,
      l10n_util::GetNSString(
          IDS_IOS_EXPANDED_MANUAL_FILL_VIEW_ACCESSIBILITY_ANNOUNCEMENT));
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass) {
    // Update the header view's layout when the view's vertical size class
    // changes.
    [self resetHeaderView:_headerView
        headerViewHeightConstraint:_headerViewHeightConstraint
                        chromeLogo:_chromeLogo
                       closeButton:_closeButton
                  segmentedControl:_segmentedControl
                     headerTopView:_headerTopView];
  }
}

#pragma mark - Private

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
  UIImage* image = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kChromeLogoSize));
#else
  UIImage* image =
      CustomSymbolWithPointSize(kChromeProductSymbol, kChromeLogoSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImageView* chromeLogo = [[UIImageView alloc] initWithImage:image];
  chromeLogo.translatesAutoresizingMaskIntoConstraints = NO;
  chromeLogo.accessibilityIdentifier =
      manual_fill::kExpandedManualFillChromeLogoID;

  return chromeLogo;
}

// Creates and configures the close button.
- (ExtendedTouchTargetButton*)createCloseButton {
  ExtendedTouchTargetButton* closeButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  closeButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_CLOSE_BUTTON_ACCESSIBILITY_LABEL);

  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];

  UIImage* buttonImage =
      SymbolWithPalette(DefaultSymbolWithConfiguration(kXMarkCircleFillSymbol,
                                                       symbolConfiguration),
                        @[
                          [UIColor colorNamed:kGrey600Color],
                          [UIColor colorNamed:kUpdatedTertiaryBackgroundColor]
                        ]);
  [closeButton setImage:buttonImage forState:UIControlStateNormal];

  [closeButton addTarget:self
                  action:@selector(onCloseButtonPressed)
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

  return segmentedControl;
}

// Sets up the header view depending on the device's orientation.
- (void)setUpHeaderView:(UIView*)headerView
    headerViewHeightConstraint:(NSLayoutConstraint*)headerViewHeightConstraint
                    chromeLogo:(UIImageView*)chromeLogo
                   closeButton:(UIButton*)closeButton
              segmentedControl:(UISegmentedControl*)segmentedControl
                 headerTopView:(UIView*)headerTopView {
  // If the vertical size class is compact, apply the wide layout. Otherwise,
  // apply the narrow layout.
  if (IsCompactHeight(self)) {
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
          constraintEqualToAnchor:headerTopView.trailingAnchor],

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
    // `chromeLogo` constraints.
    [chromeLogo.heightAnchor constraintEqualToConstant:kChromeLogoSize],
    [chromeLogo.widthAnchor constraintEqualToConstant:kChromeLogoSize],

    // `closeButton` constraints.
    [closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    [closeButton.widthAnchor constraintEqualToConstant:kCloseButtonSize],

    // `segmentedControl` constraints.
    [segmentedControl.heightAnchor
        constraintEqualToConstant:kSegmentedControlHeight],
  ]];
}

// Resets the header view. Called when a layout change is needed.
- (void)resetHeaderView:(UIView*)headerView
    headerViewHeightConstraint:(NSLayoutConstraint*)headerViewHeightConstraint
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
                      chromeLogo:chromeLogo
                     closeButton:closeButton
                segmentedControl:segmentedControl
                   headerTopView:headerTopView];
}

// Handles taps on the close button.
- (void)onCloseButtonPressed {
  //  TODO(b/40942168): Implement logic.
}

// Handles the selection of a data type from the segmented control.
- (void)onSegmentSelected:(UISegmentedControl*)segmentedControl {
  //  TODO(b/40942168): Implement logic.
}

@end
