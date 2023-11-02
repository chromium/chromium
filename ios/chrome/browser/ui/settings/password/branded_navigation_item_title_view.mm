// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/public/provider/chrome/browser/font/font_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Horizontal spacing between the logo and the title label.
const CGFloat kHorizontalSpacing = 9.0;
}  // namespace

@interface BrandedNavigationItemTitleView () {
  UILabel* _titleLabel;
  UIImageView* _logoImageView;
  UIStackView* _containerStackView;
}

@end

@implementation BrandedNavigationItemTitleView

- (instancetype)init {
  self = [super init];

  if (self) {
    _titleLabel = [self createTitleLabel];
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

#pragma mark - Private

// Returns a newly created title label.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];

  UIFont* productFont =
      ios::provider::GetBrandedProductRegularFont(UIFont.labelFontSize);
  label.font = [[[UIFontMetrics alloc] initForTextStyle:UIFontTextStyleHeadline]
      scaledFontForFont:productFont];
  label.adjustsFontForContentSizeCategory = YES;

  label.textColor = [UIColor colorNamed:kGrey700Color];
  label.numberOfLines = 1;

  label.translatesAutoresizingMaskIntoConstraints = NO;

  return label;
}

// Returns a newly created logo image view.
- (UIImageView*)createLogoImageView {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;

  return imageView;
}

// Returns a horizontal stack view holding the logo and title views.
- (UIStackView*)createContainerStackView {
  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _logoImageView, _titleLabel ]];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.spacing = kHorizontalSpacing;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;

  return stackView;
}

@end
