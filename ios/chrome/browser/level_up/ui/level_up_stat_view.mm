// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_stat_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Spacing for layout margins and stacks.
const CGFloat kLayoutSpacing = 16.0;
// The size of the illustration container.
const CGFloat kIllustrationSize = 64.0;
// Spacing between the illustration and the text.
const CGFloat kIllustrationTextSpacing = 8.0;
// Spacing within the vertical text labels stack.
const CGFloat kTextStackSpacing = 4.0;

// The corner radius of the card.
const CGFloat kCardCornerRadius = 24.0;
// The opacity of the card shadow.
const CGFloat kCardShadowOpacity = 1.0;
// The blur radius of the card shadow.
const CGFloat kCardShadowRadius = 2.0;
// The vertical offset of the card shadow.
const CGFloat kCardShadowOffset = 1.0;
// The color alpha of the card shadow.
const CGFloat kCardShadowAlpha = 0.05;

}  // namespace

@implementation LevelUpStatView {
  // Label displaying the stat title/metric.
  UILabel* _titleLabel;
  // Label displaying the stat subtitle description.
  UILabel* _subtitleLabel;
  // Image view displaying the stat illustration.
  UIImageView* _imageView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.backgroundColor =
        [UIColor colorNamed:kPrimaryBackgroundColor];
    self.contentView.layer.cornerRadius = kCardCornerRadius;
    self.contentView.layer.masksToBounds = YES;
    self.translatesAutoresizingMaskIntoConstraints = NO;

    self.layer.shadowColor =
        [UIColor colorWithRed:0 green:0 blue:0 alpha:kCardShadowAlpha].CGColor;
    self.layer.shadowOpacity = kCardShadowOpacity;
    self.layer.shadowRadius = kCardShadowRadius;
    self.layer.shadowOffset = CGSizeMake(0, kCardShadowOffset);
    self.layer.masksToBounds = NO;

    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _imageView.contentMode = UIViewContentModeScaleAspectFit;
    [NSLayoutConstraint activateConstraints:@[
      [_imageView.widthAnchor constraintEqualToConstant:kIllustrationSize],
      [_imageView.heightAnchor constraintEqualToConstant:kIllustrationSize],
    ]];

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    UIFontDescriptor* bodyDescriptor = [UIFontDescriptor
        preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
    _titleLabel.font = [UIFont systemFontOfSize:bodyDescriptor.pointSize
                                         weight:UIFontWeightSemibold];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.numberOfLines = 0;

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitleLabel.numberOfLines = 0;

    UIStackView* textStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel ]];
    textStack.translatesAutoresizingMaskIntoConstraints = NO;
    textStack.axis = UILayoutConstraintAxisVertical;
    textStack.spacing = kTextStackSpacing;

    UIStackView* cardStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _imageView, textStack ]];
    cardStack.translatesAutoresizingMaskIntoConstraints = NO;
    cardStack.axis = UILayoutConstraintAxisHorizontal;
    cardStack.spacing = kIllustrationTextSpacing;
    cardStack.alignment = UIStackViewAlignmentCenter;

    [self.contentView addSubview:cardStack];

    AddSameConstraintsWithInsets(
        cardStack, self.contentView,
        NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing,
                                    kLayoutSpacing, kLayoutSpacing));
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.shadowPath =
      [UIBezierPath bezierPathWithRoundedRect:self.bounds
                                 cornerRadius:kCardCornerRadius]
          .CGPath;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleLabel.text = nil;
  _subtitleLabel.text = nil;
  _imageView.image = nil;
}

- (void)setStatTitle:(NSString*)title
            subtitle:(NSString*)subtitle
               image:(UIImage*)image {
  _titleLabel.text = title;
  _subtitleLabel.text = subtitle;
  _imageView.image = image;
}

@end
