// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/expanded_manual_fill_header_view.h"

#import <algorithm>

#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
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

// Size of the data type icons representing the segments of the segmented
// control.
constexpr CGFloat kDataTypeIconSize = 18;

// Opacity of the segmented control background color.
constexpr CGFloat kSegmentedControlBackgroundColorOpacity = 0.05;
// Height of the segmented control.
constexpr CGFloat kSegmentedControlHeight = 32;

// Height of the header's top view in narrow layout.
constexpr CGFloat kHeaderTopViewHeightNarrowLayout = 44;
// Vertical spacing between the bottom of the header top view and segmented
// control in narrow layout.
constexpr CGFloat kHeaderTopViewBottomSpacingNarrowLayout = 10;

// Height of the header view in wide layout.
constexpr CGFloat kHeaderViewHeightWideLayout = 44;
// Horizontal spacing between the Chrome logo and segmented control in wide
// layout.
constexpr CGFloat kSegmentedControlLeadingSpacingWideLayout = 18;
// Horizontal spacing between the segmented control and close button in wide
// layout.
constexpr CGFloat kSegmentedControlTrailingSpacingWideLayout = 15;

// Returns the segment index corresponding to `data_type`.
int GetSegmentIndexForDataType(ManualFillDataType data_type) {
  switch (data_type) {
    case ManualFillDataType::kCredential:
      return 0;
    case ManualFillDataType::kPaymentMethod:
      return 1;
    case ManualFillDataType::kAddress:
      return 2;
    case ManualFillDataType::kOther:
      NOTREACHED();
  }
  NOTREACHED();
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

// Returns the trailing offset of the close button to visually align its glyph
// with the trailing margin.
CGFloat GetCloseButtonTrailingOffset(CGSize size) {
  return std::max(size.width - kCloseButtonSize, 0.0) * 0.5;
}

// Returns the foreground color to use for the close button color palette.
UIColor* GetCloseButtonForegroundColor() {
  if (@available(iOS 26, *)) {
    return [UIColor colorNamed:kTextPrimaryColor];
  }
  return [[UIColor colorNamed:kTextSecondaryColor] colorWithAlphaComponent:0.6];
}

}  // namespace

@implementation ExpandedManualFillHeaderView {
  // Layout guide inset horizontally by `horizontalInset`.
  UILayoutGuide* _insetGuide;
  NSLayoutConstraint* _insetGuideLeadingConstraint;
  NSLayoutConstraint* _insetGuideTrailingConstraint;

  // Constraints active only for the current vertical size class.
  NSMutableArray<NSLayoutConstraint*>* _layoutConstraints;

  // View positioned at the top of the header view when in narrow layout,
  // containing the Chrome logo and close button.
  UIView* _headerTopView;
  UIImageView* _chromeLogo;
  ExtendedTouchTargetButton* _closeButton;
  CGFloat _closeButtonTrailingOffset;
}

#pragma mark - Initialization

- (instancetype)initWithDataType:(ManualFillDataType)initialDataType {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.accessibilityIdentifier = manual_fill::kExpandedManualFillHeaderViewID;

    _layoutConstraints = [[NSMutableArray alloc] init];

    _insetGuide = [[UILayoutGuide alloc] init];
    [self addLayoutGuide:_insetGuide];
    _insetGuideLeadingConstraint =
        [_insetGuide.leadingAnchor constraintEqualToAnchor:self.leadingAnchor];
    _insetGuideTrailingConstraint = [_insetGuide.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor];
    [NSLayoutConstraint activateConstraints:@[
      _insetGuideLeadingConstraint,
      _insetGuideTrailingConstraint,
      [_insetGuide.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_insetGuide.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];

    _headerTopView = [self createHeaderTopView];
    _chromeLogo = [self createChromeLogo];
    _closeButton = [self createCloseButton];
    _segmentedControl =
        [self createSegmentedControlAndSelectDataType:initialDataType];

    [self setUpSubviews];
  }
  return self;
}

#pragma mark - Properties

- (void)setHorizontalInset:(CGFloat)horizontalInset {
  if (_horizontalInset == horizontalInset) {
    return;
  }
  _horizontalInset = horizontalInset;
  _insetGuideLeadingConstraint.constant = horizontalInset;
  _insetGuideTrailingConstraint.constant = -horizontalInset;
}

- (void)setWideLayout:(BOOL)wideLayout {
  if (_wideLayout == wideLayout) {
    return;
  }
  _wideLayout = wideLayout;
  [self setUpSubviews];
}

- (UIView*)preferredAccessibilityFocusView {
  return _chromeLogo;
}

#pragma mark - Private

// Sets up subview placement and constraints for narrow (portrait / popover) vs
// wide (compact-height landscape) layouts.
// TODO(crbug.com/590806026): Keep subviews attached and only swap layout
// constraints on trait changes instead of reparenting subviews.
- (void)setUpSubviews {
  [NSLayoutConstraint deactivateConstraints:_layoutConstraints];
  [_layoutConstraints removeAllObjects];

  [_chromeLogo removeFromSuperview];
  [_closeButton removeFromSuperview];
  [_segmentedControl removeFromSuperview];
  [_headerTopView removeFromSuperview];

  if (_wideLayout) {
    [self addSubview:_chromeLogo];
    [self addSubview:_closeButton];
    [self addSubview:_segmentedControl];

    [_layoutConstraints addObjectsFromArray:@[
      [self.heightAnchor constraintEqualToConstant:kHeaderViewHeightWideLayout],
      [_chromeLogo.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [_chromeLogo.leadingAnchor
          constraintEqualToAnchor:_insetGuide.leadingAnchor],
      [_closeButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:_insetGuide.trailingAnchor],
      [_segmentedControl.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [_segmentedControl.leadingAnchor
          constraintEqualToAnchor:_chromeLogo.trailingAnchor
                         constant:kSegmentedControlLeadingSpacingWideLayout],
      [_segmentedControl.trailingAnchor
          constraintEqualToAnchor:_closeButton.leadingAnchor
                         constant:-kSegmentedControlTrailingSpacingWideLayout],
    ]];
  } else {
    [self addSubview:_headerTopView];
    [self addSubview:_segmentedControl];
    [_headerTopView addSubview:_chromeLogo];
    [_headerTopView addSubview:_closeButton];

    [_layoutConstraints addObjectsFromArray:@[
      [_headerTopView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_headerTopView.leadingAnchor
          constraintEqualToAnchor:_insetGuide.leadingAnchor],
      [_headerTopView.trailingAnchor
          constraintEqualToAnchor:_insetGuide.trailingAnchor],
      [_headerTopView.heightAnchor
          constraintEqualToConstant:kHeaderTopViewHeightNarrowLayout],
      [_chromeLogo.centerYAnchor
          constraintEqualToAnchor:_headerTopView.centerYAnchor],
      [_chromeLogo.centerXAnchor
          constraintEqualToAnchor:_headerTopView.centerXAnchor],
      [_closeButton.centerYAnchor
          constraintEqualToAnchor:_headerTopView.centerYAnchor],
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:_headerTopView.trailingAnchor
                         constant:_closeButtonTrailingOffset],
      [_segmentedControl.topAnchor
          constraintEqualToAnchor:_headerTopView.bottomAnchor
                         constant:kHeaderTopViewBottomSpacingNarrowLayout],
      [_segmentedControl.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [_segmentedControl.leadingAnchor
          constraintEqualToAnchor:_insetGuide.leadingAnchor],
      [_segmentedControl.trailingAnchor
          constraintEqualToAnchor:_insetGuide.trailingAnchor],
    ]];
  }

  [NSLayoutConstraint activateConstraints:_layoutConstraints];
}

// Creates and configures the header's top view used in narrow layout.
- (UIView*)createHeaderTopView {
  UIView* headerTopView = [[UIView alloc] init];
  headerTopView.translatesAutoresizingMaskIntoConstraints = NO;
  headerTopView.accessibilityIdentifier =
      manual_fill::kExpandedManualFillHeaderTopViewID;
  return headerTopView;
}

// Creates and configures the Chrome logo.
- (UIImageView*)createChromeLogo {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  UIImage* image = MakeSymbolMulticolor(
      SymbolWithPointSize(SymbolMulticolorChromeball, GetChromeLogoSize()));
#else
  UIImage* image =
      SymbolWithPointSize(SymbolChromeProduct, GetChromeLogoSize());
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)
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
      SymbolWithConfiguration(SymbolXMarkCircleFill, symbolConfiguration),
      @[ GetCloseButtonForegroundColor(), [UIColor tertiarySystemFillColor] ]);
  _closeButtonTrailingOffset = GetCloseButtonTrailingOffset(buttonImage.size);
  [closeButton setImage:buttonImage forState:UIControlStateNormal];

  [closeButton setContentHuggingPriority:UILayoutPriorityRequired
                                 forAxis:UILayoutConstraintAxisHorizontal];
  [closeButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  [closeButton addTarget:self
                  action:@selector(didTapCloseButton:)
        forControlEvents:UIControlEventTouchUpInside];
  return closeButton;
}

// Creates and configures the segmented control with `dataType` selected.
- (UISegmentedControl*)createSegmentedControlAndSelectDataType:
    (ManualFillDataType)dataType {
  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kDataTypeIconSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];

  UIImage* passwordIcon =
      SymbolWithConfiguration(SymbolPassword, symbolConfiguration);
  passwordIcon.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_PASSWORD_TAB_ACCESSIBILITY_LABEL);

  UIImage* cardIcon =
      SymbolWithConfiguration(SymbolCreditCard, symbolConfiguration);
  cardIcon.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_PAYMENT_TAB_ACCESSIBILITY_LABEL);

  UIImage* addressIcon =
      SymbolWithConfiguration(SymbolLocation, symbolConfiguration);
  addressIcon.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_ADDRESS_TAB_ACCESSIBILITY_LABEL);

  UISegmentedControl* segmentedControl = [[UISegmentedControl alloc]
      initWithItems:@[ passwordIcon, cardIcon, addressIcon ]];
  segmentedControl.translatesAutoresizingMaskIntoConstraints = NO;
  segmentedControl.selectedSegmentIndex = GetSegmentIndexForDataType(dataType);
  [segmentedControl.heightAnchor
      constraintEqualToConstant:kSegmentedControlHeight]
      .active = YES;
  [segmentedControl addTarget:self
                       action:@selector(didSelectSegment:)
             forControlEvents:UIControlEventValueChanged];

  if (@available(iOS 26, *)) {
    segmentedControl.backgroundColor = [[UIColor colorNamed:kGrey700Color]
        colorWithAlphaComponent:kSegmentedControlBackgroundColorOpacity];
  }

  return segmentedControl;
}

// Handles taps on the close button.
- (void)didTapCloseButton:(UIButton*)sender {
  base::RecordAction(base::UserMetricsAction("ManualFallback_Close"));
  [self.delegate headerView:self didPressCloseButton:sender];
}

// Handles selection changes on the segmented control.
- (void)didSelectSegment:(UISegmentedControl*)segmentedControl {
  ManualFillDataType selectedType =
      static_cast<ManualFillDataType>(segmentedControl.selectedSegmentIndex);
  [self.delegate headerView:self didSelectDataType:selectedType];
}

@end
