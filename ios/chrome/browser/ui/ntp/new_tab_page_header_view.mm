// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view.h"

#import <UIKit/UIKit.h>

#import <algorithm>

#import "base/check.h"
#import "base/feature_list.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/lens/lens_availability.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

// Fakebox highlight animation duration.
const CGFloat kFakeboxHighlightDuration = 0.4;

// Fakebox highlight background alpha.
const CGFloat kFakeboxHighlightAlpha = 0.06;

// Height margin of the fake location bar.
const CGFloat kFakeLocationBarHeightMargin = 2;

// The constants for the constraints affecting the end button; either Lens or
// Voice Search, depending on if Lens is enabled.
const CGFloat kEndButtonFakeboxTrailingSpace = 12.0;
const CGFloat kEndButtonOmniboxTrailingSpace = 7.0;

// The constants for the constraints the leading-edge aligned UI elements.
const CGFloat kHintLabelFakeboxLeadingSpace = 18.0;
const CGFloat kHintLabelOmniboxLeadingSpace = 13.0;
const CGFloat kLargeFakeboxHintLabelFakeboxLeadingSpace = 26.0;
const CGFloat kLargeFakeboxHintLabelOmniboxLeadingSpace = 21.0;

// The constants for the constraints affecting the separation between the Lens
// and Voice Search buttons.
const CGFloat kEndButtonSeparation = 19.0;

// The leading space / padding in the unscrolled fakebox.
CGFloat HintLabelFakeboxLeadingSpace() {
  return IsIOSLargeFakeboxEnabled() ? kLargeFakeboxHintLabelFakeboxLeadingSpace
                                    : kHintLabelFakeboxLeadingSpace;
}

// The leading space / padding in the scrolled fakebox.
CGFloat HintLabelOmniboxLeadingSpace() {
  return IsIOSLargeFakeboxEnabled() ? kLargeFakeboxHintLabelOmniboxLeadingSpace
                                    : kHintLabelOmniboxLeadingSpace;
}

// Returns the top color of the Fakebox's gradient background.
UIColor* FakeboxTopColor() {
  if (IsMagicStackEnabled()) {
    if (IsIOSLargeFakeboxEnabled()) {
      return UIAccessibilityIsReduceTransparencyEnabled()
                 ? [UIColor colorNamed:@"fake_omnibox_solid_background_color"]
                 : [UIColor colorNamed:@"fake_omnibox_top_gradient_color"];
    }
    return [UIColor colorNamed:@"fake_omnibox_background_color"];
  }
  return [UIColor colorNamed:kTextfieldBackgroundColor];
}

// Returns the bottom color of the Fakebox's gradient background.
UIColor* FakeboxBottomColor() {
  if (IsMagicStackEnabled()) {
    if (IsIOSLargeFakeboxEnabled()) {
      return UIAccessibilityIsReduceTransparencyEnabled()
                 ? [UIColor colorNamed:@"fake_omnibox_solid_background_color"]
                 : [UIColor colorNamed:@"fake_omnibox_bottom_gradient_color"];
    }
    return [UIColor colorNamed:@"fake_omnibox_background_color"];
  }
  return [UIColor colorNamed:kTextfieldBackgroundColor];
}

// Returns the background color for the NTP Header view. This is the color
// that shows when the fakebox is scrolled up.
UIColor* HeaderBackgroundColor() {
  if (IsIOSLargeFakeboxEnabled()) {
    return [UIColor colorNamed:kBackgroundColor];
  } else if (IsMagicStackEnabled()) {
    return [UIColor colorNamed:@"ntp_background_color"];
  } else {
    return ntp_home::NTPBackgroundColor();
  }
}

// Returns a color which is a blend of `color_1` and `color_2`, depending on
// the value of `fraction`. `fraction` is a value between 0 and 1. If it is
// closer to 0, the output will be closer to `color_1`, and if it is closer to
// 1 the output will be closer to `color_2`.
UIColor* BlendColors(UIColor* color_1, UIColor* color_2, CGFloat fraction) {
  if (fraction <= 0.0) {
    return color_1;
  } else if (fraction >= 1.0) {
    return color_2;
  } else if ([color_1 isEqual:color_2]) {
    return color_1;
  }

  // Get RGBA components for the two colors, as inputs to the blend.
  CGFloat in_1[4];
  CGFloat in_2[4];
  [color_1 getRed:&in_1[0] green:&in_1[1] blue:&in_1[2] alpha:&in_1[3]];
  [color_2 getRed:&in_2[0] green:&in_2[1] blue:&in_2[2] alpha:&in_2[3]];

  // Blend each RGBA color component, based on the given fraction.
  CGFloat out[4];
  CGFloat inverse = 1.0 - fraction;
  for (int i = 0; i < 4; i++) {
    out[i] = inverse * in_1[i] + fraction * in_2[i];
  }

  return [UIColor colorWithRed:out[0] green:out[1] blue:out[2] alpha:out[3]];
}

// Returns a value in the range of `from` up to `to`, depending on the given
// `percent`.
CGFloat Interpolate(CGFloat from, CGFloat to, CGFloat percent) {
  if (percent <= 0.0) {
    return from;
  } else if (percent >= 1.0) {
    return to;
  }
  return from + (to - from) * percent;
}

}  // namespace

@interface NewTabPageHeaderView ()

// The Lens button. May be null if Lens is not available.
@property(nonatomic, strong, readwrite) ExtendedTouchTargetButton* lensButton;

@property(nonatomic, strong, readwrite)
    ExtendedTouchTargetButton* voiceSearchButton;

@property(nonatomic, strong) UIView* separator;

// Layout constraints for fake omnibox background image and blur.
@property(nonatomic, strong) NSLayoutConstraint* fakeLocationBarTopConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* hintLabelLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* hintLabelTrailingConstraint;
// In the new layout, the hint label should always be at least inside the fake
// omnibox. When the fake omnibox is shrunk, the position from the leading side
// of the search field should yield. This constraint is not defined for the old
// layout.
@property(nonatomic, strong)
    NSLayoutConstraint* hintLabelLeadingMarginConstraint;
// The end button should always be at least inside the fake omnibox.
// When the fake omnibox is shrunk, the position from the trailing side of
// the search field should yield.
@property(nonatomic, strong)
    NSLayoutConstraint* endButtonTrailingMarginConstraint;
// Constraint for positioning the end button away from the fake box rounded
// rectangle.
@property(nonatomic, strong) NSLayoutConstraint* endButtonTrailingConstraint;
// Layout constraint for the invisible button that is where the omnibox should
// be and that focuses the omnibox when tapped.
@property(nonatomic, strong) NSLayoutConstraint* invisibleOmniboxConstraint;
// View used to add on-touch highlight to the fake omnibox.
@property(nonatomic, strong) UIView* fakeLocationBarHighlightView;
// View used to simulate the top toolbar when the header is stuck to the top of
// the NTP.
@property(nonatomic, strong) UIView* fakeToolbar;

@end

@implementation NewTabPageHeaderView {
  CGFloat _lastAnimationPercent;
}

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.clipsToBounds = YES;
  }
  return self;
}

- (void)addToolbarView:(UIView*)toolbarView {
  _toolBarView = toolbarView;
  [self addSubview:toolbarView];
  self.invisibleOmniboxConstraint =
      [toolbarView.topAnchor constraintEqualToAnchor:self.topAnchor
                                            constant:self.safeAreaInsets.top];
  [NSLayoutConstraint activateConstraints:@[
    [toolbarView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [toolbarView.heightAnchor
        constraintEqualToConstant:ToolbarExpandedHeight(
                                      [UIApplication sharedApplication]
                                          .preferredContentSizeCategory)],
    [toolbarView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    self.invisibleOmniboxConstraint,
  ]];
}

- (void)setIdentityDiscView:(UIView*)identityDiscView {
  DCHECK(identityDiscView);
  _identityDiscView = identityDiscView;
  [self.toolBarView addSubview:_identityDiscView];

  // Sets the layout constraints for size of Identity Disc and toolbar.
  self.identityDiscView.translatesAutoresizingMaskIntoConstraints = NO;
  CGFloat dimension =
      ntp_home::kIdentityAvatarDimension + 2 * ntp_home::kIdentityAvatarMargin;
  [NSLayoutConstraint activateConstraints:@[
    [self.identityDiscView.heightAnchor constraintEqualToConstant:dimension],
    [self.identityDiscView.widthAnchor constraintEqualToConstant:dimension],
    [self.identityDiscView.trailingAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.trailingAnchor],
    [self.identityDiscView.topAnchor
        constraintEqualToAnchor:self.toolBarView.topAnchor],
  ]];
}

- (void)addViewsToSearchField:(UIView*)searchField {
  // Fake Toolbar.
  self.fakeToolbar = [[UIView alloc] init];
  [searchField insertSubview:self.fakeToolbar atIndex:0];
  self.fakeToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  // Fake location bar.
  [self.fakeToolbar addSubview:self.fakeLocationBar];

  // Omnibox, used for animations.
  // TODO(crbug.com/936811): See if it is possible to share some initialization
  // code with the real Omnibox.
  UIColor* color = [UIColor colorNamed:kTextfieldPlaceholderColor];
  OmniboxContainerView* omnibox =
      [[OmniboxContainerView alloc] initWithFrame:CGRectZero
                                        textColor:color
                                    textFieldTint:color
                                         iconTint:color];
  omnibox.textField.placeholderTextColor = color;
  omnibox.textField.placeholder =
      l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  [omnibox.textField setText:@""];
  omnibox.translatesAutoresizingMaskIntoConstraints = NO;
  [searchField addSubview:omnibox];
  AddSameConstraints(omnibox, self.fakeLocationBar);
  omnibox.textField.userInteractionEnabled = NO;
  omnibox.hidden = YES;
  self.omnibox = omnibox;

  // Cancel button, used in animation.
  ToolbarButtonFactory* factory =
      [[ToolbarButtonFactory alloc] initWithStyle:ToolbarStyle::kNormal];
  self.cancelButton = [factory cancelButton];
  [searchField addSubview:self.cancelButton];
  self.cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.cancelButton.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
    [self.cancelButton.leadingAnchor
        constraintEqualToAnchor:self.fakeLocationBar.trailingAnchor],
  ]];

  // Hint label.
  self.searchHintLabel = [[UILabel alloc] init];
  content_suggestions::ConfigureSearchHintLabel(self.searchHintLabel,
                                                searchField);
  self.searchHintLabel.font = [self hintLabelFont];

  if (base::FeatureList::IsEnabled(kNewNTPOmniboxLayout)) {
    // Enable the leading-edge-alignment hint label constraints.
    self.hintLabelLeadingMarginConstraint = [self.searchHintLabel.leadingAnchor
        constraintEqualToAnchor:[searchField leadingAnchor]];
    self.hintLabelLeadingMarginConstraint.priority =
        UILayoutPriorityDefaultHigh + 1;
    self.hintLabelLeadingConstraint = [self.searchHintLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.fakeLocationBar.leadingAnchor
                                    constant:HintLabelFakeboxLeadingSpace()];
    [self.hintLabelLeadingMarginConstraint setActive:YES];
  } else {
    // The old omnibox layout has the label centered horizontally in the
    // fakebox.
    self.hintLabelLeadingConstraint = [self.searchHintLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:[searchField leadingAnchor]
                                    constant:ntp_header::
                                                 kCenteredHintLabelSidePadding];
    [[self.searchHintLabel.centerXAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerXAnchor]
        setActive:YES];
  }
  [NSLayoutConstraint activateConstraints:@[
    self.hintLabelLeadingConstraint,
    [self.searchHintLabel.heightAnchor
        constraintEqualToAnchor:self.fakeLocationBar.heightAnchor
                       constant:-ntp_header::kHintLabelHeightMargin],
    [self.searchHintLabel.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
  ]];
  // Set a button the same size as the fake omnibox as the accessibility
  // element. If the hint is the only accessible element, when the fake omnibox
  // is taking the full width, there are few points that are not accessible and
  // allow to select the content below it.
  self.searchHintLabel.isAccessibilityElement = NO;
  [self.searchHintLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];

  // Voice search.
  self.voiceSearchButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  content_suggestions::ConfigureVoiceSearchButton(self.voiceSearchButton,
                                                  searchField);
  UIButton* endButton = self.voiceSearchButton;

  // Lens.
  const BOOL useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::NewTabPage, self.isGoogleDefaultSearchEngine);
  if (useLens) {
    self.lensButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
    content_suggestions::ConfigureLensButton(self.lensButton, searchField);
    endButton = self.lensButton;
  }

  // Constraints.
  AddSameConstraints(self.fakeToolbar, searchField);

  self.fakeLocationBarTopConstraint = [self.fakeLocationBar.topAnchor
      constraintEqualToAnchor:searchField.topAnchor];
  self.fakeLocationBarLeadingConstraint = [self.fakeLocationBar.leadingAnchor
      constraintEqualToAnchor:searchField.leadingAnchor];
  self.fakeLocationBarTrailingConstraint = [self.fakeLocationBar.trailingAnchor
      constraintEqualToAnchor:searchField.trailingAnchor];
  self.fakeLocationBarHeightConstraint = [self.fakeLocationBar.heightAnchor
      constraintEqualToConstant:content_suggestions::FakeOmniboxHeight()];
  [NSLayoutConstraint activateConstraints:@[
    self.fakeLocationBarTopConstraint,
    self.fakeLocationBarLeadingConstraint,
    self.fakeLocationBarTrailingConstraint,
    self.fakeLocationBarHeightConstraint,
  ]];

  // If the Lens button was created, layout the header with the Lens button on
  // the end.
  if (self.lensButton) {
    [NSLayoutConstraint activateConstraints:@[
      // Lens button constraints.
      [self.lensButton.leadingAnchor
          constraintEqualToAnchor:self.voiceSearchButton.trailingAnchor
                         constant:kEndButtonSeparation],
      [self.lensButton.centerYAnchor
          constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
    ]];
  }

  self.endButtonTrailingMarginConstraint = [endButton.trailingAnchor
      constraintEqualToAnchor:[searchField trailingAnchor]];
  self.endButtonTrailingMarginConstraint.priority =
      UILayoutPriorityDefaultHigh + 1;
  self.endButtonTrailingConstraint = [endButton.trailingAnchor
      constraintLessThanOrEqualToAnchor:self.fakeLocationBar.trailingAnchor
                               constant:-kEndButtonFakeboxTrailingSpace];

  // The voice search button is always on the leading side, even if the Lens
  // button is visible.
  self.hintLabelTrailingConstraint = [self.searchHintLabel.trailingAnchor
      constraintLessThanOrEqualToAnchor:self.voiceSearchButton.leadingAnchor];
  self.hintLabelTrailingConstraint.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint activateConstraints:@[
    [self.voiceSearchButton.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
    self.hintLabelTrailingConstraint,
    self.endButtonTrailingMarginConstraint,
    self.endButtonTrailingConstraint,
  ]];
}

- (void)addSeparatorToSearchField:(UIView*)searchField {
  DCHECK(searchField.superview == self);

  self.separator = [[UIView alloc] init];
  self.separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  self.separator.alpha = 0;
  self.separator.translatesAutoresizingMaskIntoConstraints = NO;
  [searchField addSubview:self.separator];
  [NSLayoutConstraint activateConstraints:@[
    [self.separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [self.separator.topAnchor constraintEqualToAnchor:searchField.bottomAnchor],
    [self.separator.heightAnchor
        constraintEqualToConstant:content_suggestions::HeaderSeparatorHeight()],
  ]];
}

- (CGFloat)searchFieldProgressForOffset:(CGFloat)offset {
  // The scroll offset at which point searchField's frame should stop growing.
  CGFloat maxScaleOffset = [self offsetToBeginFakeOmniboxExpansion];

  // The scroll offset at which point searchField's frame should start
  // growing.
  CGFloat startScaleOffset = maxScaleOffset - ntp_header::kAnimationDistance;
  CGFloat percent = 0;
  if (offset && offset > startScaleOffset) {
    CGFloat animatingOffset = offset - startScaleOffset;
    percent = std::clamp<CGFloat>(
        animatingOffset / ntp_header::kAnimationDistance, 0, 1);
  }
  return percent;
}

- (void)updateSearchFieldWidth:(NSLayoutConstraint*)widthConstraint
                        height:(NSLayoutConstraint*)heightConstraint
                     topMargin:(NSLayoutConstraint*)topMarginConstraint
                     forOffset:(CGFloat)offset
                   screenWidth:(CGFloat)screenWidth
                safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  CGFloat contentWidth = std::max<CGFloat>(
      0, screenWidth - safeAreaInsets.left - safeAreaInsets.right);
  if (screenWidth == 0 || contentWidth == 0) {
    return;
  }

  CGFloat searchFieldNormalWidth =
      content_suggestions::SearchFieldWidth(contentWidth, self.traitCollection);

  CGFloat percent = [self searchFieldProgressForOffset:offset];
  _lastAnimationPercent = percent;

  // Update the opacity of the header background color as the user scrolls so
  // that content does not appear beneath it. Since the NTP background might be
  // a gradient, the opacity must be 0 by default.
  self.backgroundColor =
      [HeaderBackgroundColor() colorWithAlphaComponent:percent];

  if (IsIOSLargeFakeboxEnabled()) {
    [self setFakeboxBackgroundWithProgress:percent];
  }

  // Offset the hint label constraints with half of the change in width
  // from the original scale, since constraints are calculated before
  // transformations are applied. This prevents the label from overlapping
  // with other UI elements.
  CGFloat hintLabelScalingExtraOffset =
      (content_suggestions::kHintTextScale * (1 - percent)) *
      self.searchHintLabel.bounds.size.width * 0.5;
  self.hintLabelTrailingConstraint.constant = -hintLabelScalingExtraOffset;

  CGFloat fakeOmniboxHeight = content_suggestions::FakeOmniboxHeight();
  // Use UIApplication preferredContentSizeCategory as this VC has a weird trait
  // collection from times to times.
  CGFloat locationBarHeight = LocationBarHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);

  if (!IsSplitToolbarMode(self)) {
    // When Voiceover is running, if the header's alpha is set to 0, voiceover
    // can't scroll back to it, and it will never come back into view. To
    // prevent that, set the alpha to non-zero when the header is fully
    // offscreen. It will still not be seen, but it will be accessible to
    // Voiceover.
    self.alpha = std::max(1 - percent, 0.01);

    widthConstraint.constant = searchFieldNormalWidth;
    self.fakeLocationBarHeightConstraint.constant =
        fakeOmniboxHeight - kFakeLocationBarHeightMargin;
    self.fakeLocationBar.layer.cornerRadius =
        self.fakeLocationBarHeightConstraint.constant / 2;
    [self scaleHintLabelForPercent:percent];

    self.fakeLocationBarLeadingConstraint.constant = 0;
    self.fakeLocationBarTrailingConstraint.constant = 0;
    self.fakeLocationBarTopConstraint.constant = 0;

    // Reset the view horizontal constraints.
    if (base::FeatureList::IsEnabled(kNewNTPOmniboxLayout)) {
      self.hintLabelLeadingMarginConstraint.constant =
          HintLabelFakeboxLeadingSpace() + hintLabelScalingExtraOffset;
    } else {
      self.hintLabelLeadingConstraint.constant =
          ntp_header::kCenteredHintLabelSidePadding;
    }
    self.endButtonTrailingMarginConstraint.constant = 0;

    self.separator.alpha = 0;

    return;
  }

  self.alpha = 1;
  self.separator.alpha = percent;

  // Calculate the amount to grow the width and height of searchField so that
  // its frame covers the entire toolbar area.
  CGFloat maxXInset =
      ui::AlignValueToUpperPixel((searchFieldNormalWidth - screenWidth) / 2);
  widthConstraint.constant =
      Interpolate(searchFieldNormalWidth, screenWidth, percent);
  CGFloat maxTopMarginDiff = fakeOmniboxHeight - locationBarHeight -
                             kAdaptiveLocationBarVerticalMargin;
  topMarginConstraint.constant =
      -content_suggestions::SearchFieldTopMargin() - maxTopMarginDiff * percent;
  heightConstraint.constant =
      ntp_header::kFakeLocationBarTopConstraint -
      content_suggestions::HeaderSeparatorHeight() +
      Interpolate(fakeOmniboxHeight,
                  locationBarHeight + kAdaptiveLocationBarVerticalMargin,
                  percent);

  // Calculate the amount to shrink the width and height of background so that
  // it's where the focused adapative toolbar focuses.
  self.fakeLocationBarLeadingConstraint.constant = Interpolate(
      0, safeAreaInsets.left + kExpandedLocationBarHorizontalMargin, percent);
  self.fakeLocationBarTrailingConstraint.constant = -Interpolate(
      0, safeAreaInsets.right + kExpandedLocationBarHorizontalMargin, percent);

  self.fakeLocationBarTopConstraint.constant =
      ntp_header::kFakeLocationBarTopConstraint * percent;
  self.fakeLocationBarHeightConstraint.constant =
      Interpolate(fakeOmniboxHeight, locationBarHeight, percent);
  self.fakeLocationBar.layer.cornerRadius =
      self.fakeLocationBarHeightConstraint.constant / 2;

  // Scale the hintLabel and update the horizontal constraint constant.
  [self scaleHintLabelForPercent:percent];

  // Adjust the position of the search field's subviews by adjusting their
  // constraint constant value.
  CGFloat subviewsDiff = -maxXInset * percent;
  self.endButtonTrailingMarginConstraint.constant = -subviewsDiff;
  // The trailing space wanted is a linear scale between the two states of the
  // fakebox: 1) when centered in the NTP and 2) when pinned to the top,
  // emulating the the omnibox.
  self.endButtonTrailingConstraint.constant = -Interpolate(
      kEndButtonFakeboxTrailingSpace, kEndButtonOmniboxTrailingSpace, percent);

  if (base::FeatureList::IsEnabled(kNewNTPOmniboxLayout)) {
    // A similar positioning scheme is applied to the leading-edge-aligned
    // hint label as the trailing-edge-aligned buttons.
    self.hintLabelLeadingMarginConstraint.constant = subviewsDiff;
    self.hintLabelLeadingConstraint.constant =
        hintLabelScalingExtraOffset +
        Interpolate(HintLabelFakeboxLeadingSpace(),
                    HintLabelOmniboxLeadingSpace(), percent);
  } else {
    self.hintLabelLeadingConstraint.constant =
        subviewsDiff + ntp_header::kCenteredHintLabelSidePadding;
  }
}

- (void)setFakeboxHighlighted:(BOOL)highlighted {
  [UIView animateWithDuration:kFakeboxHighlightDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     CGFloat alpha = highlighted ? kFakeboxHighlightAlpha : 0;
                     self.fakeLocationBarHighlightView.backgroundColor =
                         [UIColor colorWithWhite:0 alpha:alpha];
                   }
                   completion:nil];
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    self.searchHintLabel.font = [self hintLabelFont];
  }

  if (previousTraitCollection.userInterfaceStyle !=
      self.traitCollection.userInterfaceStyle) {
    if (IsIOSLargeFakeboxEnabled()) {
      // The fakebox background can be a blended color, which will not
      // automatically update when dark/light mode is changed. It needs to be
      // manually updated here.
      [self setFakeboxBackgroundWithProgress:_lastAnimationPercent];
    }
  }
}

- (void)updateForTopSafeAreaInset:(CGFloat)topSafeAreaInset {
  self.invisibleOmniboxConstraint.constant = topSafeAreaInset;
}

#pragma mark - Property accessors

- (UIView*)fakeLocationBar {
  if (!_fakeLocationBar) {
    _fakeLocationBar =
        [[GradientView alloc] initWithTopColor:FakeboxTopColor()
                                   bottomColor:FakeboxBottomColor()];
    _fakeLocationBar.userInteractionEnabled = NO;
    _fakeLocationBar.clipsToBounds = YES;
    _fakeLocationBar.translatesAutoresizingMaskIntoConstraints = NO;
    _fakeLocationBarHighlightView = [[UIView alloc] init];
    _fakeLocationBarHighlightView.userInteractionEnabled = NO;
    _fakeLocationBarHighlightView.backgroundColor = UIColor.clearColor;
    _fakeLocationBarHighlightView.translatesAutoresizingMaskIntoConstraints =
        NO;
    [_fakeLocationBar addSubview:_fakeLocationBarHighlightView];
    AddSameConstraints(_fakeLocationBar, _fakeLocationBarHighlightView);
  }
  return _fakeLocationBar;
}

#pragma mark - Private

// Returns the font size for the hint label.
- (UIFont*)hintLabelFont {
  return LocationBarSteadyViewFont(
      self.traitCollection.preferredContentSizeCategory);
}

// Scale the the hint label down to at most content_suggestions::kHintTextScale.
- (void)scaleHintLabelForPercent:(CGFloat)percent {
  DCHECK(self.searchHintLabel);
  CGFloat scaleValue =
      1 + (content_suggestions::kHintTextScale * (1 - percent));
  self.searchHintLabel.transform =
      CGAffineTransformMakeScale(scaleValue, scaleValue);
}

// The positive offset value to begin the fake omnibox expansion animation.
- (CGFloat)offsetToBeginFakeOmniboxExpansion {
  CGFloat offset =
      self.frame.size.height -
      ToolbarExpandedHeight(
          [UIApplication sharedApplication].preferredContentSizeCategory);

  // For non-split toolbar, the fake omnibox goes beneath the toolbar.
  if (!IsSplitToolbarMode(self)) {
    // The animation should start when the primary toolbar is met, with an
    // additional 1/4 height so the fake omnibox text appears to fade into the
    // primary toolbar.
    offset += content_suggestions::FakeOmniboxHeight() +
              (content_suggestions::FakeOmniboxHeight() / 4);

    // iPads pin slightly earlier than landscape iPhones.
    if (IsRegularXRegularSizeClass(self)) {
      offset -= content_suggestions::SearchFieldTopMargin();
    }
  }
  return offset;
}

// Sets the fakebox's background gradient colors, based on the progress towards
// being pinned at the top.
- (void)setFakeboxBackgroundWithProgress:(CGFloat)progress {
  UIColor* pinnedColor = [UIColor colorNamed:kTextfieldBackgroundColor];

  // Use a quadratic curve interpolation.
  progress = progress * progress;
  [_fakeLocationBar
      setStartColor:BlendColors(FakeboxTopColor(), pinnedColor, progress)
           endColor:BlendColors(FakeboxBottomColor(), pinnedColor, progress)];
}

@end
