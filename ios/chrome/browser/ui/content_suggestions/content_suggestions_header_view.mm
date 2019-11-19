// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view.h"

#import <UIKit/UIKit.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ui/base/l10n/l10n_util.h"
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

// Returns the height of the toolbar based on the preferred content size of the
// application.
CGFloat ToolbarHeight() {
  // Use UIApplication preferredContentSizeCategory as this VC has a weird trait
  // collection from times to times.
  return ToolbarExpandedHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

// Returns the amount of vertical space to allow for the existence of a top
// toolbar when iPhone is in landscape orientation.
CGFloat IdentityDiscToolbarOffset(id<UITraitEnvironment> environment) {
  return IsCompactHeight(environment) ? ToolbarHeight() : 0;
}

}  // namespace

@interface ContentSuggestionsHeaderView ()

@property(nonatomic, strong, readwrite) UIButton* voiceSearchButton;

@property(nonatomic, strong) UIView* separator;

// Layout constraints for fake omnibox background image and blur.
@property(nonatomic, strong) NSLayoutConstraint* fakeLocationBarTopConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeToolbarTopConstraint;
@property(nonatomic, strong) NSLayoutConstraint* hintLabelLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* voiceSearchTrailingConstraint;
// Layout constraints for Identity Disc that need to be adjusted based on
// device size class changes.
@property(nonatomic, strong) NSLayoutConstraint* identityDiscTopConstraint;
// View used to add on-touch highlight to the fake omnibox.
@property(nonatomic, strong) UIView* fakeLocationBarHighlightView;

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
  id<LayoutGuideProvider> layoutGuide = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [toolbarView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [toolbarView.topAnchor constraintEqualToAnchor:layoutGuide.topAnchor],
    [toolbarView.heightAnchor constraintEqualToConstant:ToolbarHeight()],
    [toolbarView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor]
  ]];
}

- (void)setIdentityDiscView:(UIView*)identityDiscView {
  DCHECK(identityDiscView);
  _identityDiscView = identityDiscView;
  [self addSubview:_identityDiscView];

  // Sets the layout constraints for size of Identity Disc and the placement
  // based on whether there is a top toolbar or not.
  self.identityDiscView.translatesAutoresizingMaskIntoConstraints = NO;
  id<LayoutGuideProvider> layoutGuide = self.safeAreaLayoutGuide;
  self.identityDiscTopConstraint = [self.identityDiscView.topAnchor
      constraintEqualToAnchor:layoutGuide.topAnchor
                     constant:IdentityDiscToolbarOffset(self)];
  CGFloat dimension =
      ntp_home::kIdentityAvatarDimension + 2 * ntp_home::kIdentityAvatarMargin;
  [NSLayoutConstraint activateConstraints:@[
    [self.identityDiscView.heightAnchor constraintEqualToConstant:dimension],
    [self.identityDiscView.widthAnchor constraintEqualToConstant:dimension],
    [self.identityDiscView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor],
    self.identityDiscTopConstraint
  ]];
}

- (void)addViewsToSearchField:(UIView*)searchField {
  // Fake Toolbar.
  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:NORMAL];
  UIView* fakeToolbar = [[UIView alloc] init];
  fakeToolbar.backgroundColor =
      buttonFactory.toolbarConfiguration.backgroundColor;
  [searchField insertSubview:fakeToolbar atIndex:0];
  fakeToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  // Fake location bar.
  [fakeToolbar addSubview:self.fakeLocationBar];

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
  content_suggestions::configureSearchHintLabel(self.searchHintLabel,
                                                searchField);
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
  self.voiceSearchButton = [[UIButton alloc] init];
  content_suggestions::configureVoiceSearchButton(self.voiceSearchButton,
                                                  searchField);

  // Constraints.
  self.fakeToolbarTopConstraint =
      [fakeToolbar.topAnchor constraintEqualToAnchor:searchField.topAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [fakeToolbar.leadingAnchor
        constraintEqualToAnchor:searchField.leadingAnchor],
    [fakeToolbar.trailingAnchor
        constraintEqualToAnchor:searchField.trailingAnchor],
    self.fakeToolbarTopConstraint,
    [fakeToolbar.bottomAnchor constraintEqualToAnchor:searchField.bottomAnchor]
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

  // The voice search button should always be at least inside the fake omnibox.
  // When the fake omnibox is shrinked, the position from the trailing side of
  // the search field should yield.
  self.voiceSearchTrailingConstraint = [self.voiceSearchButton.trailingAnchor
      constraintEqualToAnchor:[searchField trailingAnchor]];
  self.voiceSearchTrailingConstraint.priority = UILayoutPriorityDefaultHigh + 1;

  [NSLayoutConstraint activateConstraints:@[
    [self.voiceSearchButton.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
    [self.searchHintLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.voiceSearchButton.leadingAnchor],
    self.voiceSearchTrailingConstraint,
    [self.voiceSearchButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.fakeLocationBar.trailingAnchor],
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

  // With RxR the search field should scroll under the toolbar.
  if (IsRegularXRegularSizeClass(self)) {
    maxScaleOffset += ToolbarHeight();
  }

  // The scroll offset at which point searchField's frame should start
  // growing.
  CGFloat startScaleOffset = maxScaleOffset - ntp_header::kAnimationDistance;
  CGFloat percent = 0;
  if (offset && offset > startScaleOffset) {
    CGFloat animatingOffset = offset - startScaleOffset;
    percent = MIN(1, MAX(0, animatingOffset / ntp_header::kAnimationDistance));
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
      content_suggestions::searchFieldWidth(contentWidth);

  CGFloat percent =
      [self searchFieldProgressForOffset:offset safeAreaInsets:safeAreaInsets];

  if (!IsSplitToolbarMode(self)) {
    // When Voiceover is running, if the header's alpha is set to 0, voiceover
    // can't scroll back to it, and it will never come back into view. To
    // prevent that, set the alpha to non-zero when the header is fully
    // offscreen. It will still not be seen, but it will be accessible to
    // Voiceover.
    self.alpha = std::max(1 - percent, 0.01);

    widthConstraint.constant = searchFieldNormalWidth;
    self.fakeLocationBarHeightConstraint.constant = ToolbarHeight();
    self.fakeLocationBar.layer.cornerRadius =
        self.fakeLocationBarHeightConstraint.constant / 2;
    [self scaleHintLabelForPercent:percent];
    self.fakeToolbarTopConstraint.constant = 0;

    self.fakeLocationBarLeadingConstraint.constant = 0;
    self.fakeLocationBarTrailingConstraint.constant = 0;
    self.fakeLocationBarTopConstraint.constant = 0;

    self.separator.alpha = 0;

    return;
  } else {
    self.alpha = 1;
    self.separator.alpha = percent;
  }

  // Grow the background to cover the safeArea top.
  self.fakeToolbarTopConstraint.constant = -safeAreaInsets.top * percent;

  CGFloat toolbarExpandedHeight = ToolbarHeight();

  // Calculate the amount to grow the width and height of searchField so that
  // its frame covers the entire toolbar area.
  CGFloat maxXInset =
      ui::AlignValueToUpperPixel((searchFieldNormalWidth - screenWidth) / 2);
  widthConstraint.constant = searchFieldNormalWidth - 2 * maxXInset * percent;
  topMarginConstraint.constant = -content_suggestions::searchFieldTopMargin() -
                                 ntp_header::kMaxTopMarginDiff * percent;
  heightConstraint.constant = toolbarExpandedHeight;

  // Calculate the amount to shrink the width and height of background so that
  // it's where the focused adapative toolbar focuses.
  CGFloat inset = !IsSplitToolbarMode() ? kBackgroundLandscapeInset : 0;
  self.fakeLocationBarLeadingConstraint.constant =
      (safeAreaInsets.left + kExpandedLocationBarHorizontalMargin + inset) *
      percent;
  self.fakeLocationBarTrailingConstraint.constant =
      -(safeAreaInsets.right + kExpandedLocationBarHorizontalMargin + inset) *
      percent;

  self.fakeLocationBarTopConstraint.constant =
      ntp_header::kFakeLocationBarTopConstraint * percent;
  // Use UIApplication preferredContentSizeCategory as this VC has a weird trait
  // collection from times to times.
  CGFloat kLocationBarHeight = LocationBarHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
  CGFloat minHeightDiff = kLocationBarHeight - toolbarExpandedHeight;
  self.fakeLocationBarHeightConstraint.constant =
      toolbarExpandedHeight + minHeightDiff * percent;
  self.fakeLocationBar.layer.cornerRadius =
      self.fakeLocationBarHeightConstraint.constant / 2;

  // Scale the hintLabel, and make sure the frame stays left aligned.
  [self scaleHintLabelForPercent:percent];

  // Adjust the position of the search field's subviews by adjusting their
  // constraint constant value.
  CGFloat subviewsDiff = -maxXInset * percent;
  self.voiceSearchTrailingConstraint.constant = -subviewsDiff;
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

// Adjusts the autolayout constraints for |identityDiscView| when view changes
// size. When an iPhone is rotated from portrait (no top toolbar) to landscape
// (with top toolbar), the placement of Identity Disc has to be shifted down
// below the top toolbar. Otherwise, the Identity Disc may be obscured by the
// top toolbar in landscape mode.
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // identityDiscView may not be set if feature is not enabled.
  if (!self.identityDiscView)
    return;
  if ((self.traitCollection.verticalSizeClass !=
       previousTraitCollection.verticalSizeClass) ||
      (self.traitCollection.horizontalSizeClass !=
       previousTraitCollection.horizontalSizeClass)) {
    self.identityDiscTopConstraint.constant = IdentityDiscToolbarOffset(self);
  }
}

#pragma mark - Property accessors

- (UIView*)fakeLocationBar {
  if (!_fakeLocationBar) {
    _fakeLocationBar = [[UIView alloc] init];
    _fakeLocationBar.userInteractionEnabled = NO;
    _fakeLocationBar.clipsToBounds = YES;
    _fakeLocationBar.backgroundColor =
        [UIColor colorNamed:kTextfieldBackgroundColor];
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

// Scale the the hint label down to at most content_suggestions::kHintTextScale.
- (void)scaleHintLabelForPercent:(CGFloat)percent {
  CGFloat scaleValue =
      1 + (content_suggestions::kHintTextScale * (1 - percent));
  self.searchHintLabel.transform =
      CGAffineTransformMakeScale(scaleValue, scaleValue);
}

@end
