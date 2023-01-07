// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/cxx17_backports.h"
#import "base/feature_list.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"
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
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Landscape inset for fake omnibox background container
const CGFloat kBackgroundLandscapeInset = 169;

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

// The constants for the constraints affecting the vertical separator that
// appears between the Lens and Voice Search buttons.
const CGFloat kLensButtonSeparatorWidth = 1;
const CGFloat kLensButtonSeparatorHeight = 12;
const CGFloat kLensButtonSeparatorLeftMargin = 8;
const CGFloat kLensButtonSeparatorRightMargin = 13;

// Returns the height of the toolbar based on the preferred content size of the
// application.
CGFloat ToolbarHeight() {
  // Use UIApplication preferredContentSizeCategory as this VC has a weird trait
  // collection from times to times.
  return ToolbarExpandedHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

}  // namespace

@interface ContentSuggestionsHeaderView ()

// The Lens button. May be null if Lens is not available.
@property(nonatomic, strong, readwrite) ExtendedTouchTargetButton* lensButton;
// The view that seperates the Lens and voice search buttons. May be null if
// Lens is not available.
@property(nonatomic, strong) UIView* lensButtonSeparator;

@property(nonatomic, strong, readwrite)
    ExtendedTouchTargetButton* voiceSearchButton;

@property(nonatomic, strong) UIView* separator;

// Layout constraints for fake omnibox background image and blur.
@property(nonatomic, strong) NSLayoutConstraint* fakeLocationBarTopConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeToolbarTopConstraint;
@property(nonatomic, strong) NSLayoutConstraint* hintLabelLeadingConstraint;
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

@implementation ContentSuggestionsHeaderView

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
    [toolbarView.heightAnchor constraintEqualToConstant:ToolbarHeight()],
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
  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:NORMAL];
  self.fakeToolbar = [[UIView alloc] init];
  self.fakeToolbar.backgroundColor =
      IsContentSuggestionsUIModuleRefreshEnabled()
          ? [UIColor clearColor]
          : buttonFactory.toolbarConfiguration.backgroundColor;
  [searchField insertSubview:self.fakeToolbar atIndex:0];
  self.fakeToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  // Fake location bar.
  [self.fakeToolbar addSubview:self.fakeLocationBar];

  // Omnibox, used for animations.
  // TODO(crbug.com/936811): See if it is possible to share some initialization
  // code with the real Omnibox.
  UIColor* color = IsContentSuggestionsUIModuleRefreshEnabled()
                       ? [UIColor colorNamed:kGrey700Color]
                       : [UIColor colorNamed:kTextfieldPlaceholderColor];
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
      [[ToolbarButtonFactory alloc] initWithStyle:NORMAL];
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
  self.hintLabelLeadingConstraint = [self.searchHintLabel.leadingAnchor
      constraintGreaterThanOrEqualToAnchor:[searchField leadingAnchor]
                                  constant:ntp_header::kHintLabelSidePadding];
  [NSLayoutConstraint activateConstraints:@[
    [self.searchHintLabel.centerXAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerXAnchor],
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

  // Voice search.
  self.voiceSearchButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  content_suggestions::ConfigureVoiceSearchButton(self.voiceSearchButton,
                                                  searchField);
  UIButton* endButton = self.voiceSearchButton;

  // Lens.
  const BOOL lensEnabled = ios::provider::IsLensSupported() &&
                           base::FeatureList::IsEnabled(kEnableLensInNTP);
  const BOOL useLens = lensEnabled && self.isGoogleDefaultSearchEngine;
  if (useLens) {
    self.lensButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
    content_suggestions::ConfigureLensButton(self.lensButton, searchField);
    endButton = self.lensButton;

    self.lensButtonSeparator = [[UIView alloc] init];
    self.lensButtonSeparator.backgroundColor =
        [UIColor colorNamed:kToolbarShadowColor];
    self.lensButtonSeparator.translatesAutoresizingMaskIntoConstraints = NO;
    [searchField addSubview:self.lensButtonSeparator];
  }

  // Constraints.
  self.fakeToolbarTopConstraint = [self.fakeToolbar.topAnchor
      constraintEqualToAnchor:searchField.topAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [self.fakeToolbar.leadingAnchor
        constraintEqualToAnchor:searchField.leadingAnchor],
    [self.fakeToolbar.trailingAnchor
        constraintEqualToAnchor:searchField.trailingAnchor],
    self.fakeToolbarTopConstraint,
    [self.fakeToolbar.bottomAnchor
        constraintEqualToAnchor:searchField.bottomAnchor]
  ]];

  self.fakeLocationBarTopConstraint = [self.fakeLocationBar.topAnchor
      constraintEqualToAnchor:searchField.topAnchor];
  self.fakeLocationBarLeadingConstraint = [self.fakeLocationBar.leadingAnchor
      constraintEqualToAnchor:searchField.leadingAnchor];
  self.fakeLocationBarTrailingConstraint = [self.fakeLocationBar.trailingAnchor
      constraintEqualToAnchor:searchField.trailingAnchor];
  self.fakeLocationBarHeightConstraint = [self.fakeLocationBar.heightAnchor
      constraintEqualToConstant:ToolbarHeight()];
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
      // Separator constraints.
      [self.lensButtonSeparator.leadingAnchor
          constraintEqualToAnchor:self.voiceSearchButton.trailingAnchor
                         constant:kLensButtonSeparatorLeftMargin],
      [self.lensButtonSeparator.trailingAnchor
          constraintEqualToAnchor:self.lensButton.leadingAnchor
                         constant:-kLensButtonSeparatorRightMargin],
      [self.lensButtonSeparator.centerYAnchor
          constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
      [self.lensButtonSeparator.widthAnchor
          constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                        kLensButtonSeparatorWidth)],
      [self.lensButtonSeparator.heightAnchor
          constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                        kLensButtonSeparatorHeight)],
      // Lens button constraints.
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

  [NSLayoutConstraint activateConstraints:@[
    [self.voiceSearchButton.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
    // The voice search button is always on the left, even if the Lens button is
    // visible.
    [self.searchHintLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.voiceSearchButton.leadingAnchor],
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
        constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                      kToolbarSeparatorHeight)],
  ]];
}

- (CGFloat)searchFieldProgressForOffset:(CGFloat)offset
                         safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  // The scroll offset at which point searchField's frame should stop growing.
  CGFloat maxScaleOffset = self.frame.size.height - ToolbarHeight() -
                           ntp_header::kFakeOmniboxScrolledToTopMargin -
                           safeAreaInsets.top;
  // If the Shrunk logo for the Start Surface is being shown, the searchField
  // expansion should start later so that its background does not cut off the
  // logo. This mainly impacts notched devices that have a large top Safe Area
  // inset. Instead of ensuring the expansion finishes by the time the omnibox
  // reaches the bottom of the toolbar, wait until the logo is in the safe area
  // before expanding so it is out of view.
  if (ShouldShrinkLogoForStartSurface()) {
    maxScaleOffset += safeAreaInsets.top;
  }
  // If it is not in SplitMode the search field should scroll under the toolbar.
  if (!IsSplitToolbarMode(self)) {
    maxScaleOffset += ToolbarHeight();
  }

  // The scroll offset at which point searchField's frame should start
  // growing.
  CGFloat startScaleOffset = maxScaleOffset - ntp_header::kAnimationDistance;
  CGFloat percent = 0;
  if (offset && offset > startScaleOffset) {
    CGFloat animatingOffset = offset - startScaleOffset;
    percent = base::clamp<CGFloat>(
        animatingOffset / ntp_header::kAnimationDistance, 0, 1);
  }
  if (IsContentSuggestionsUIModuleRefreshEnabled()) {
    // Update background color of fake toolbar if stuck to top of NTP so that it
    // has a non-clear background. Otherwise, return to clear background.
    self.fakeToolbar.backgroundColor =
        percent == 1.0f ? [UIColor colorNamed:kBackgroundColor]
                        : [UIColor clearColor];
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
  if (screenWidth == 0 || contentWidth == 0)
    return;

  CGFloat searchFieldNormalWidth =
      content_suggestions::SearchFieldWidth(contentWidth, self.traitCollection);

  CGFloat percent =
      [self searchFieldProgressForOffset:offset safeAreaInsets:safeAreaInsets];

  CGFloat toolbarExpandedHeight = ToolbarHeight();

  if (!IsSplitToolbarMode(self)) {
    // When Voiceover is running, if the header's alpha is set to 0, voiceover
    // can't scroll back to it, and it will never come back into view. To
    // prevent that, set the alpha to non-zero when the header is fully
    // offscreen. It will still not be seen, but it will be accessible to
    // Voiceover.
    self.alpha = std::max(1 - percent, 0.01);

    widthConstraint.constant = searchFieldNormalWidth;
    self.fakeLocationBarHeightConstraint.constant =
        toolbarExpandedHeight - kFakeLocationBarHeightMargin;
    self.fakeLocationBar.layer.cornerRadius =
        self.fakeLocationBarHeightConstraint.constant / 2;
    [self scaleHintLabelForPercent:percent];
    self.fakeToolbarTopConstraint.constant = 0;

    self.fakeLocationBarLeadingConstraint.constant = 0;
    self.fakeLocationBarTrailingConstraint.constant = 0;
    self.fakeLocationBarTopConstraint.constant = 0;

    self.hintLabelLeadingConstraint.constant =
        ntp_header::kHintLabelSidePadding;
    self.endButtonTrailingMarginConstraint.constant = 0;

    self.separator.alpha = 0;

    return;
  } else {
    self.alpha = 1;
    self.separator.alpha = percent;
  }

  // Grow the background to cover the safeArea top.
  self.fakeToolbarTopConstraint.constant = -safeAreaInsets.top * percent;

  // Calculate the amount to grow the width and height of searchField so that
  // its frame covers the entire toolbar area.
  CGFloat maxXInset =
      ui::AlignValueToUpperPixel((searchFieldNormalWidth - screenWidth) / 2);
  widthConstraint.constant = searchFieldNormalWidth - 2 * maxXInset * percent;
  topMarginConstraint.constant = -content_suggestions::SearchFieldTopMargin() -
                                 ntp_header::kMaxTopMarginDiff * percent;
  heightConstraint.constant = toolbarExpandedHeight;

  // Calculate the amount to shrink the width and height of background so that
  // it's where the focused adapative toolbar focuses.
  CGFloat inset = !IsSplitToolbarMode(self) ? kBackgroundLandscapeInset : 0;
  CGFloat leadingMargin = IsOmniboxActionsEnabled()
                              ? kExpandedLocationBarLeadingMarginRefreshedPopup
                              : kExpandedLocationBarHorizontalMargin;
  self.fakeLocationBarLeadingConstraint.constant =
      (safeAreaInsets.left + leadingMargin + inset) * percent;
  self.fakeLocationBarTrailingConstraint.constant =
      -(safeAreaInsets.right + kExpandedLocationBarHorizontalMargin + inset) *
      percent;

  self.fakeLocationBarTopConstraint.constant =
      ntp_header::kFakeLocationBarTopConstraint * percent;
  // Use UIApplication preferredContentSizeCategory as this VC has a weird trait
  // collection from times to times.
  CGFloat kLocationBarHeight = LocationBarHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
  CGFloat minHeightDiff =
      kLocationBarHeight + kFakeLocationBarHeightMargin - toolbarExpandedHeight;
  self.fakeLocationBarHeightConstraint.constant = toolbarExpandedHeight -
                                                  kFakeLocationBarHeightMargin +
                                                  minHeightDiff * percent;
  self.fakeLocationBar.layer.cornerRadius =
      self.fakeLocationBarHeightConstraint.constant / 2;

  // Scale the hintLabel, and make sure the frame stays left aligned.
  [self scaleHintLabelForPercent:percent];

  // Adjust the position of the search field's subviews by adjusting their
  // constraint constant value.
  CGFloat subviewsDiff = -maxXInset * percent;
  self.endButtonTrailingMarginConstraint.constant = -subviewsDiff;
  // The trailing space wanted is a linear scale between the two states of the
  // fakebox: 1) when centered in the NTP and 2) when pinned to the top,
  // emulating the the omnibox.
  self.endButtonTrailingConstraint.constant =
      -kEndButtonFakeboxTrailingSpace +
      (kEndButtonFakeboxTrailingSpace - kEndButtonOmniboxTrailingSpace) *
          percent;
  self.hintLabelLeadingConstraint.constant =
      subviewsDiff + ntp_header::kHintLabelSidePadding;
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
}

- (void)updateForTopSafeAreaInset:(CGFloat)topSafeAreaInset {
  self.invisibleOmniboxConstraint.constant = topSafeAreaInset;
}

#pragma mark - Property accessors

- (UIView*)fakeLocationBar {
  if (!_fakeLocationBar) {
    _fakeLocationBar = [[UIView alloc] init];
    _fakeLocationBar.userInteractionEnabled = NO;
    _fakeLocationBar.clipsToBounds = YES;
    _fakeLocationBar.backgroundColor =
        IsContentSuggestionsUIModuleRefreshEnabled()
            ? [UIColor clearColor]
            : [UIColor colorNamed:kTextfieldBackgroundColor];
    _fakeLocationBar.translatesAutoresizingMaskIntoConstraints = NO;
    if (IsContentSuggestionsUIModuleRefreshEnabled()) {
      GradientView* gradientView = [[GradientView alloc]
          initWithTopColor:[UIColor
                               colorNamed:@"fake_omnibox_top_gradient_color"]
               bottomColor:
                   [UIColor colorNamed:@"fake_omnibox_bottom_gradient_color"]];
      gradientView.translatesAutoresizingMaskIntoConstraints = NO;
      [_fakeLocationBar addSubview:gradientView];
      AddSameConstraints(_fakeLocationBar, gradientView);
    }

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
  CGFloat scaleValue =
      1 + (content_suggestions::kHintTextScale * (1 - percent));
  self.searchHintLabel.transform =
      CGAffineTransformMakeScale(scaleValue, scaleValue);
}

@end
