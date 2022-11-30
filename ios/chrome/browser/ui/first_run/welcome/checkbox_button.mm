// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/checkbox_button.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Margins to apply between the button edges and the content (i.e. content
// inset), and between the label and the checkmark.
const CGFloat kContentMargin = 13;
// Default corner radius for the button shape.
const CGFloat kDefaultCornerRadius = 8;

// Tha alpha to apply when the button is highlighted.
const CGFloat kHighlightedAlpha = 0.8;
// The duration of the animation when adding / removing transparency.
const CGFloat kAnimationDuration = 0.25;

}  // namespace

@interface CheckboxButton ()

// The text for the checkbox button.
@property(nonatomic, strong) UILabel* label;
// The image view for the checkmark image.
@property(nonatomic, strong) UIImageView* checkmarkImageView;
// The UIImage for the checkmark.
@property(nonatomic, strong) UIImage* checkmarkImage;

@end

@implementation CheckboxButton

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Default style.
    self.layer.cornerRadius = kDefaultCornerRadius;
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

    // Custom button label.
    _label = [[UILabel alloc] init];
    _label.numberOfLines = 0;
    _label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _label.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    _label.adjustsFontForContentSizeCategory = YES;
    [self addSubview:_label];

    // Custom button image.
    _checkmarkImageView = [[UIImageView alloc] init];
    _checkmarkImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _checkmarkImageView.tintColor = [UIColor colorNamed:kBlueColor];
    UIImage* checkmarkImage = [UIImage imageNamed:@"welcome_metrics_checkmark"];
    _checkmarkImageView.image = [checkmarkImage
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [self addSubview:_checkmarkImageView];

    // Layout constraints for the custom button and image. The label is on the
    // left of the image, and both are centered vertically. The image is only
    // visible if the `selected` property is YES, otherwise it is hidden (but
    // still takes the same amount of space in the layout). For RTL, the layout
    // is flipped horizontally, meaning the label is to the right of the image.
    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor
          constraintGreaterThanOrEqualToAnchor:_label.heightAnchor
                                      constant:2 * kContentMargin],
      [self.heightAnchor
          constraintGreaterThanOrEqualToAnchor:_checkmarkImageView.heightAnchor
                                      constant:2 * kContentMargin],

      [_label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                           constant:kContentMargin],
      [_label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],

      [_checkmarkImageView.leadingAnchor
          constraintEqualToAnchor:_label.trailingAnchor
                         constant:kContentMargin],
      [_checkmarkImageView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kContentMargin],
      [_checkmarkImageView.widthAnchor
          constraintEqualToConstant:checkmarkImage.size.width],
      [_checkmarkImageView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
    ]];
  }
  return self;
}

#pragma mark - Accessors

- (void)setLabelText:(NSString*)text {
  [self.label setText:text];
}

- (NSString*)labelText {
  return self.label.text;
}

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];
  self.checkmarkImageView.hidden = !selected;
}

- (NSString*)accessibilityLabel {
  return self.label.text;
}

- (void)setHighlighted:(BOOL)highlighted {
  __weak __typeof(self) weakSelf = self;
  CGFloat targetAlpha = highlighted ? kHighlightedAlpha : 1.0;
  [UIView animateWithDuration:kAnimationDuration
                   animations:^{
                     weakSelf.alpha = targetAlpha;
                   }];
  [super setHighlighted:highlighted];
}

@end
