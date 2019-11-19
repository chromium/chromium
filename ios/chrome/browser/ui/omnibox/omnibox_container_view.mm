// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/animation_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Size of the leading image view.
const CGFloat kLeadingImageSize = 30;
// Offset from the leading edge to the image view (used when the image is
// shown).
const CGFloat kleadingImageViewEdgeOffset = 7;
// Offset from the leading edge to the textfield when no image is shown.
const CGFloat kTextFieldLeadingOffsetNoImage = 16;
// Space between the leading button and the textfield when a button is shown.
const CGFloat kTextFieldLeadingOffsetImage = 14;
// Space between the clear button and the edge of the omnibox.
const CGFloat kTextFieldClearButtonTrailingOffset = 4;

}  // namespace

#pragma mark - OmniboxContainerView

@interface OmniboxContainerView ()
// Constraints the leading textfield side to the leading of |self|.
// Active when the |leadingView| is nil or hidden.
@property(nonatomic, strong) NSLayoutConstraint* leadingTextfieldConstraint;
// When the |leadingImageView| is not hidden, this is a constraint that links
// the leading edge of the button to self leading edge. Used for animations.
@property(nonatomic, strong)
    NSLayoutConstraint* leadingImageViewLeadingConstraint;
// The leading image view. Used for autocomplete icons.
@property(nonatomic, strong) UIImageView* leadingImageView;
// Redefined as readwrite.
@property(nonatomic, strong) OmniboxTextFieldIOS* textField;

@end

@implementation OmniboxContainerView
@synthesize textField = _textField;
@synthesize leadingImageView = _leadingImageView;
@synthesize leadingTextfieldConstraint = _leadingTextfieldConstraint;
@synthesize incognito = _incognito;
@synthesize leadingImageViewLeadingConstraint =
    _leadingImageViewLeadingConstraint;

#pragma mark - Public methods

- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                textFieldTint:(UIColor*)textFieldTint
                     iconTint:(UIColor*)iconTint {
  self = [super initWithFrame:frame];
  if (self) {
    _textField = [[OmniboxTextFieldIOS alloc] initWithFrame:frame
                                                  textColor:textColor
                                                  tintColor:textFieldTint];
    [self addSubview:_textField];

    _leadingTextfieldConstraint = [_textField.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kTextFieldLeadingOffsetNoImage];

    [NSLayoutConstraint activateConstraints:@[
      [_textField.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kTextFieldClearButtonTrailingOffset],
      [_textField.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_textField.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      _leadingTextfieldConstraint,
    ]];

    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    [_textField
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    [self createLeadingImageView];
    _leadingImageView.tintColor = iconTint;
  }
  return self;
}

- (void)attachLayoutGuides {
  [NamedGuide guideWithName:kOmniboxTextFieldGuide view:self].constrainedView =
      self.textField;

  // The leading image view can be not present, in which case the guide
  // shouldn't be attached.
  if (self.leadingImageView.superview) {
    [NamedGuide guideWithName:kOmniboxLeadingImageGuide view:self]
        .constrainedView = self.leadingImageView;
  }
}

- (void)setLeadingImageHidden:(BOOL)hidden {
  if (hidden) {
    [_leadingImageView removeFromSuperview];
    self.leadingTextfieldConstraint.active = YES;
  } else {
    [self addSubview:_leadingImageView];
    self.leadingTextfieldConstraint.active = NO;
    self.leadingImageViewLeadingConstraint = [self.leadingAnchor
        constraintEqualToAnchor:self.leadingImageView.leadingAnchor
                       constant:-kleadingImageViewEdgeOffset];

    NSLayoutConstraint* leadingImageViewToTextField = nil;
    leadingImageViewToTextField = [self.leadingImageView.trailingAnchor
        constraintEqualToAnchor:self.textField.leadingAnchor
                       constant:-kTextFieldLeadingOffsetImage];

    [NSLayoutConstraint activateConstraints:@[
      [_leadingImageView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      self.leadingImageViewLeadingConstraint,
      leadingImageViewToTextField,
    ]];
  }
}

- (void)setLeadingImage:(UIImage*)image {
  [self.leadingImageView setImage:image];
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  self.textField.incognito = incognito;
}

- (void)setLeadingImageAlpha:(CGFloat)alpha {
  self.leadingImageView.alpha = alpha;
}

#pragma mark - private

- (void)createLeadingImageView {
  _leadingImageView = [[UIImageView alloc] init];
  _leadingImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _leadingImageView.contentMode = UIViewContentModeCenter;

  // When the flag is enabled, the image view is always shown. Its width should
  // also be constant.
  if (base::FeatureList::IsEnabled(kNewOmniboxPopupLayout)) {
    [NSLayoutConstraint activateConstraints:@[
      [_leadingImageView.widthAnchor
          constraintEqualToConstant:kLeadingImageSize],
      [_leadingImageView.heightAnchor
          constraintEqualToAnchor:_leadingImageView.widthAnchor],
    ]];
  } else {
    [_leadingImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_leadingImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];
    [_leadingImageView
        setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];
    [_leadingImageView
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisVertical];

    // Sometimes the image view is not hidden and has no image. Then it doesn't
    // have an intrinsic size. In this case the omnibox should appear the same
    // as with hidden image view. Add a placeholder width constraint.
    CGFloat placeholderSize = kTextFieldLeadingOffsetNoImage -
                              kleadingImageViewEdgeOffset -
                              kTextFieldLeadingOffsetImage;
    NSLayoutConstraint* placeholderWidthConstraint =
        [_leadingImageView.widthAnchor
            constraintEqualToConstant:placeholderSize];
    // The priority must be higher than content hugging.
    placeholderWidthConstraint.priority = UILayoutPriorityDefaultLow + 1;
    placeholderWidthConstraint.active = YES;
  }
}

@end
