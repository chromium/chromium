// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/animation_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_experimental.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_legacy.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/gfx/color_palette.h"
#import "ui/gfx/image/image.h"

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
// Constraints the leading textfield side to the leading of `self`.
// Active when the `leadingView` is nil or hidden.
@property(nonatomic, strong) NSLayoutConstraint* leadingTextfieldConstraint;
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

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                textFieldTint:(UIColor*)textFieldTint
                     iconTint:(UIColor*)iconTint {
  self = [super initWithFrame:frame];
  if (self) {
    if (base::FeatureList::IsEnabled(kIOSNewOmniboxImplementation)) {
      _textField =
          [[OmniboxTextFieldExperimental alloc] initWithFrame:frame
                                                    textColor:textColor
                                                    tintColor:textFieldTint];
    } else {
      _textField = [[OmniboxTextFieldLegacy alloc] initWithFrame:frame
                                                       textColor:textColor
                                                       tintColor:textFieldTint];
    }
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

    [self setupLeadingImageViewWithTint:iconTint];
  }
  return self;
}

- (void)setLeadingImage:(UIImage*)image
    withAccessibilityIdentifier:(NSString*)accessibilityIdentifier {
  [self.leadingImageView setImage:image];
  [self.leadingImageView setAccessibilityIdentifier:accessibilityIdentifier];
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  self.textField.incognito = incognito;
}

- (void)setLeadingImageAlpha:(CGFloat)alpha {
  self.leadingImageView.alpha = alpha;
}

- (void)setLeadingImageScale:(CGFloat)scaleValue {
  self.leadingImageView.transform =
      CGAffineTransformMakeScale(scaleValue, scaleValue);
}

- (void)setLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  _layoutGuideCenter = layoutGuideCenter;
  [_layoutGuideCenter referenceView:_leadingImageView
                          underName:kOmniboxLeadingImageGuide];
  [_layoutGuideCenter referenceView:_textField
                          underName:kOmniboxTextFieldGuide];
}

#pragma mark - Private

- (void)setupLeadingImageViewWithTint:(UIColor*)iconTint {
  _leadingImageView = [[UIImageView alloc] init];
  _leadingImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _leadingImageView.contentMode = UIViewContentModeCenter;

  // The image view is always shown. Its width should be constant.
  [NSLayoutConstraint activateConstraints:@[
    [_leadingImageView.widthAnchor constraintEqualToConstant:kLeadingImageSize],
    [_leadingImageView.heightAnchor
        constraintEqualToAnchor:_leadingImageView.widthAnchor],
  ]];

  _leadingImageView.tintColor = iconTint;
  [self addSubview:_leadingImageView];
  self.leadingTextfieldConstraint.active = NO;

  NSLayoutConstraint* leadingImageViewToTextField =
      [self.leadingImageView.trailingAnchor
          constraintEqualToAnchor:self.textField.leadingAnchor
                         constant:-kTextFieldLeadingOffsetImage];

  [NSLayoutConstraint activateConstraints:@[
    [_leadingImageView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
    [self.leadingAnchor
        constraintEqualToAnchor:self.leadingImageView.leadingAnchor
                       constant:-kleadingImageViewEdgeOffset],
    leadingImageViewToTextField,
  ]];
}

@end
