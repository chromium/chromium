// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
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

// Fakebox highlight background alpha increase.
const CGFloat kFakeboxHighlightIncrease = 0.06;

}  // namespace

@interface ContentSuggestionsHeaderView ()

// Layout constraints for fake omnibox background image and blur.
@property(nonatomic, strong) NSLayoutConstraint* backgroundHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* backgroundLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* backgroundTrailingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* blurTopConstraint;
@property(nonatomic, strong) UIView* backgroundContainer;

@end

@implementation ContentSuggestionsHeaderView

@synthesize backgroundContainer = _backgroundContainer;
@synthesize backgroundHeightConstraint = _backgroundHeightConstraint;
@synthesize backgroundLeadingConstraint = _backgroundLeadingConstraint;
@synthesize backgroundTrailingConstraint = _backgroundTrailingConstraint;
@synthesize blurTopConstraint = _blurTopConstraint;
@synthesize toolBarView = _toolBarView;

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
  id<LayoutGuideProvider> layoutGuide = SafeAreaLayoutGuideForView(self);
  [NSLayoutConstraint activateConstraints:@[
    [toolbarView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [toolbarView.topAnchor constraintEqualToAnchor:layoutGuide.topAnchor],
    [toolbarView.heightAnchor
        constraintEqualToConstant:ntp_header::ToolbarHeight()],
    [toolbarView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor]
  ]];
}

- (void)addViewsToSearchField:(UIView*)searchField {
  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:NORMAL];
  UIBlurEffect* blurEffect = buttonFactory.toolbarConfiguration.blurEffect;
  UIView* blur = nil;
  if (blurEffect) {
    blur = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  } else {
    blur = [[UIView alloc] init];
  }
  blur.backgroundColor = buttonFactory.toolbarConfiguration.blurBackgroundColor;
  [searchField insertSubview:blur atIndex:0];
  blur.translatesAutoresizingMaskIntoConstraints = NO;
  self.blurTopConstraint =
      [blur.topAnchor constraintEqualToAnchor:searchField.topAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [blur.leadingAnchor constraintEqualToAnchor:searchField.leadingAnchor],
    [blur.trailingAnchor constraintEqualToAnchor:searchField.trailingAnchor],
    self.blurTopConstraint,
    [blur.bottomAnchor constraintEqualToAnchor:searchField.bottomAnchor]
  ]];

  UIVisualEffect* vibrancy = [buttonFactory.toolbarConfiguration
      vibrancyEffectForBlurEffect:blurEffect];
  UIVisualEffectView* vibrancyView =
      [[UIVisualEffectView alloc] initWithEffect:vibrancy];
  [searchField insertSubview:vibrancyView atIndex:1];
  vibrancyView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(vibrancyView, searchField);

  self.backgroundContainer = [[UIView alloc] init];
  self.backgroundContainer.userInteractionEnabled = NO;
  self.backgroundContainer.backgroundColor =
      [UIColor colorWithWhite:0 alpha:kAdaptiveLocationBarBackgroundAlpha];
  self.backgroundContainer.layer.cornerRadius =
      kAdaptiveLocationBarCornerRadius;
  [vibrancyView.contentView addSubview:self.backgroundContainer];

  self.backgroundContainer.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundLeadingConstraint = [self.backgroundContainer.leadingAnchor
      constraintEqualToAnchor:searchField.leadingAnchor];
  self.backgroundTrailingConstraint = [self.backgroundContainer.trailingAnchor
      constraintEqualToAnchor:searchField.trailingAnchor];
  self.backgroundHeightConstraint = [self.backgroundContainer.heightAnchor
      constraintEqualToConstant:content_suggestions::kSearchFieldHeight];
  [NSLayoutConstraint activateConstraints:@[
    [self.backgroundContainer.centerYAnchor
        constraintEqualToAnchor:searchField.centerYAnchor],
    self.backgroundLeadingConstraint,
    self.backgroundTrailingConstraint,
    self.backgroundHeightConstraint,
  ]];
}

- (CGFloat)searchFieldProgressForOffset:(CGFloat)offset
                         safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  // The scroll offset at which point searchField's frame should stop growing.
  CGFloat maxScaleOffset = self.frame.size.height -
                           ntp_header::kMinHeaderHeight - safeAreaInsets.top;

  // With RxR the search field should scroll under the toolbar.
  if (IsRegularXRegularSizeClass(self)) {
    maxScaleOffset += kAdaptiveToolbarHeight;
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
                     hintLabel:(UILabel*)hintLabel
            subviewConstraints:(NSArray*)constraints
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
    self.alpha = 1 - percent;
    widthConstraint.constant = searchFieldNormalWidth;
    self.backgroundHeightConstraint.constant =
        content_suggestions::kSearchFieldHeight;
    self.backgroundContainer.layer.cornerRadius =
        content_suggestions::kSearchFieldHeight / 2;
    [self scaleHintLabel:hintLabel percent:percent];
    self.blurTopConstraint.constant = 0;

    return;
  } else {
    self.alpha = 1;
  }

  // Grow the blur to cover the safeArea top.
  self.blurTopConstraint.constant = -safeAreaInsets.top * percent;

  // Calculate the amount to grow the width and height of searchField so that
  // its frame covers the entire toolbar area.
  CGFloat maxXInset =
      ui::AlignValueToUpperPixel((searchFieldNormalWidth - screenWidth) / 2);
  CGFloat maxHeightDiff =
      ntp_header::ToolbarHeight() - content_suggestions::kSearchFieldHeight;

  widthConstraint.constant = searchFieldNormalWidth - 2 * maxXInset * percent;
  topMarginConstraint.constant = -content_suggestions::searchFieldTopMargin() -
                                 ntp_header::kMaxTopMarginDiff * percent;
  heightConstraint.constant =
      content_suggestions::kSearchFieldHeight + maxHeightDiff * percent;

  // Calculate the amount to shrink the width and height of background so that
  // it's where the focused adapative toolbar focuses.
  CGFloat inset = !IsSplitToolbarMode() ? kBackgroundLandscapeInset : 0;
  self.backgroundLeadingConstraint.constant =
      (safeAreaInsets.left + kExpandedLocationBarHorizontalMargin + inset) *
      percent;
  self.backgroundTrailingConstraint.constant =
      -(safeAreaInsets.right + kExpandedLocationBarHorizontalMargin + inset) *
      percent;

  CGFloat kLocationBarHeight =
      kAdaptiveToolbarHeight - 2 * kAdaptiveLocationBarVerticalMargin;
  CGFloat minHeightDiff =
      kLocationBarHeight - content_suggestions::kSearchFieldHeight;
  self.backgroundHeightConstraint.constant =
      content_suggestions::kSearchFieldHeight + minHeightDiff * percent;
  self.backgroundContainer.layer.cornerRadius =
      self.backgroundHeightConstraint.constant / 2;

  // Scale the hintLabel, and make sure the frame stays left aligned.
  [self scaleHintLabel:hintLabel percent:percent];

  // Adjust the position of the search field's subviews by adjusting their
  // constraint constant value.
  CGFloat constantDiff = -maxXInset * percent;
  for (NSLayoutConstraint* constraint in constraints) {
    if (constraint.constant > 0)
      constraint.constant = constantDiff + ntp_header::kHintLabelSidePadding;
    else
      constraint.constant = -constantDiff;
  }
}

- (void)setFakeboxHighlighted:(BOOL)highlighted {
  [UIView animateWithDuration:kFakeboxHighlightDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     CGFloat alpha = kAdaptiveLocationBarBackgroundAlpha;
                     if (highlighted)
                       alpha += kFakeboxHighlightIncrease;
                     self.backgroundContainer.backgroundColor =
                         [UIColor colorWithWhite:0 alpha:alpha];
                   }
                   completion:nil];
}

#pragma mark - Private

// Scale the the hint label down to at most content_suggestions::kHintTextScale.
- (void)scaleHintLabel:(UIView*)hintLabel percent:(CGFloat)percent {
  CGFloat scaleValue =
      1 + (content_suggestions::kHintTextScale * (1 - percent));
  hintLabel.transform = CGAffineTransformMakeScale(scaleValue, scaleValue);
}

@end
