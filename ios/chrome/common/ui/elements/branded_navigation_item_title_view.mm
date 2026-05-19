// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"

#import "base/check.h"
#import "base/not_fatal_until.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface BrandedNavigationItemTitleView () {
  UILabel* _titleLabel;
  UIImageView* _logoImageView;
  UIStackView* _containerStackView;
  UIFont* _font;
}

@end

@implementation BrandedNavigationItemTitleView

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _titleLabel = [self createUnbrandedTitleLabel];
    [self initializeCommonAttributes];
  }
  return self;
}

- (instancetype)initWithFont:(UIFont*)font {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _font = font;
    _titleLabel = [self createBrandedTitleLabel];
    [self initializeCommonAttributes];
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = [title copy];
}

- (NSString*)title {
  return _titleLabel.text;
}

- (void)setImageLogo:(UIImage*)imageLogo {
  _logoImageView.image = imageLogo;
}

- (UIImage*)imageLogo {
  return _logoImageView.image;
}

- (void)setTitleLogoSpacing:(CGFloat)titleLogoSpacing {
  _containerStackView.spacing = titleLogoSpacing;
}

- (CGFloat)titleLogoSpacing {
  return _containerStackView.spacing;
}

#pragma mark - Private

// Initializes view properties and layout constraints common to both branded and
// unbranded view configurations.
- (void)initializeCommonAttributes {
  _logoImageView = [self createLogoImageView];
  _containerStackView = [self createContainerStackView];

  [self addSubview:_containerStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_containerStackView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_containerStackView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor],
    [_containerStackView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],
    [_containerStackView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
  ]];

  self.backgroundColor = UIColor.clearColor;

  self.translatesAutoresizingMaskIntoConstraints = NO;

  self.isAccessibilityElement = YES;
  self.accessibilityTraits |= UIAccessibilityTraitHeader;
}

// Returns a newly created title label. Defines common attributes for both the
// branded and unbranded configurations.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.adjustsFontForContentSizeCategory = YES;
  label.numberOfLines = 1;
  label.translatesAutoresizingMaskIntoConstraints = NO;

  return label;
}

// Returns a title label configured with the custom branded font and color.
- (UILabel*)createBrandedTitleLabel {
  UILabel* label = [self createTitleLabel];
  CHECK(_font, base::NotFatalUntil(base::NotFatalUntil::M150));
  label.font = [[[UIFontMetrics alloc] initForTextStyle:UIFontTextStyleHeadline]
      scaledFontForFont:_font];
  label.textColor = [UIColor colorNamed:kGrey700Color];
  return label;
}

// Returns a title label configured with the standard system headline font.
- (UILabel*)createUnbrandedTitleLabel {
  UILabel* label = [self createTitleLabel];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  return label;
}

// Returns a newly created logo image view.
- (UIImageView*)createLogoImageView {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [imageView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [imageView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  return imageView;
}

// Returns a horizontal stack view holding the logo and title views.
- (UIStackView*)createContainerStackView {
  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _logoImageView, _titleLabel ]];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;

  return stackView;
}

@end
