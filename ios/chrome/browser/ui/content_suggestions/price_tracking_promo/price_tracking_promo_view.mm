// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing between items stacked vertically (title, description and allow label)
const CGFloat kVerticalStackSpacing = 15.0f;

// Spacing between items stacked horizontally (product image and text stack
// (which contains title, description and allow label)).
const CGFloat kHorizontalStackSpacing = 16.0f;

// Inset for product image fallback from the UIImageView boundary.
const CGFloat kProductImageFallbackInset = 10.0f;

// Radius of background circle of product image fallback.
const CGFloat kProductImageFallbackCornerRadius = 25.0;

// Height and width of product image fallback.
const CGFloat kProductImageFallbackSize = 28.0;

// Point size of product image fallback.
const CGFloat kProductImageFallbackPointSize = 10.0;

// Separator height.
const CGFloat kSeparatorHeight = 0.5;

}  // namespace

@implementation PriceTrackingPromoModuleView {
  UILabel* _titleLabel;
  UILabel* _descriptionLabel;
  UIButton* _allowButton;
  UIImageView* _fallbackProductImageView;
  // To create a background circle around the fallback product image.
  UIView* _fallbackProductImageBackgroundCircle;
  UIStackView* _contentStack;
  UIStackView* _textStack;
  UIView* _separator;
  UITapGestureRecognizer* _tapRecognizer;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self constructView];
    self.isAccessibilityElement = YES;
  }
  return self;
}

- (void)configureView:(PriceTrackingPromoItem*)config {
}

- (void)constructView {
  [self createSubviews];
}

- (void)createSubviews {
  if (!(self.subviews.count == 0)) {
    return;
  }
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = kPriceTrackingPromoViewID;
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.numberOfLines = 1;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;

  _titleLabel.font =
      CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold, self);
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.text = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_TITLE);

  _descriptionLabel = [[UILabel alloc] init];
  _descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _descriptionLabel.numberOfLines = 2;
  _descriptionLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _descriptionLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _descriptionLabel.adjustsFontForContentSizeCategory = YES;
  _descriptionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _descriptionLabel.text = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_DESCRIPTION);

  _fallbackProductImageView = [[UIImageView alloc] init];
  _fallbackProductImageView.image = CustomSymbolWithPointSize(
      kDownTrendSymbol, kProductImageFallbackPointSize);
  _fallbackProductImageView.contentMode = UIViewContentModeScaleAspectFit;
  _fallbackProductImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _fallbackProductImageView.layer.borderWidth = 0;

  [NSLayoutConstraint activateConstraints:@[
    [_fallbackProductImageView.widthAnchor
        constraintEqualToConstant:kProductImageFallbackSize],
    [_fallbackProductImageView.widthAnchor
        constraintEqualToAnchor:_fallbackProductImageView.heightAnchor],
  ]];

  _fallbackProductImageBackgroundCircle = [[UIView alloc] init];
  _fallbackProductImageBackgroundCircle.layer.cornerRadius =
      kProductImageFallbackCornerRadius;
  _fallbackProductImageBackgroundCircle.backgroundColor =
      [UIColor colorNamed:kBlueHaloColor];

  [_fallbackProductImageBackgroundCircle addSubview:_fallbackProductImageView];

  AddSameConstraintsWithInset(_fallbackProductImageView,
                              _fallbackProductImageBackgroundCircle,
                              kProductImageFallbackInset);

  _allowButton = [[UIButton alloc] init];
  _allowButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_allowButton
      setTitle:l10n_util::GetNSString(
                   IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_ALLOW)
      forState:UIControlStateNormal];
  [_allowButton setTitleColor:[UIColor colorNamed:kBlueColor]
                     forState:UIControlStateNormal];
  _allowButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(allowPriceTrackingTapped:)];

  [_allowButton addGestureRecognizer:_tapRecognizer];

  _separator = [[UIView alloc] init];
  _separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];

  _textStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _titleLabel, _descriptionLabel, _separator, _allowButton
  ]];
  _textStack.axis = UILayoutConstraintAxisVertical;
  _textStack.translatesAutoresizingMaskIntoConstraints = NO;
  _textStack.alignment = UIStackViewAlignmentLeading;
  _textStack.spacing = kVerticalStackSpacing;

  [NSLayoutConstraint activateConstraints:@[
    [_separator.heightAnchor
        constraintEqualToConstant:AlignValueToPixel(kSeparatorHeight)],
    [_separator.leadingAnchor constraintEqualToAnchor:_textStack.leadingAnchor],
    [_separator.trailingAnchor
        constraintEqualToAnchor:_textStack.trailingAnchor],
  ]];

  _contentStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _fallbackProductImageBackgroundCircle, _textStack
  ]];
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  _contentStack.spacing = kHorizontalStackSpacing;
  _contentStack.alignment = UIStackViewAlignmentTop;
  [self addSubview:_contentStack];
  AddSameConstraints(_contentStack, self);
}

- (void)allowPriceTrackingTapped:(UIGestureRecognizer*)sender {
  [self.commandHandler allowPriceTrackingNotifications];
}

#pragma mark - Testing category methods

- (NSString*)titleLabelTextForTesting {
  return self->_titleLabel.text;
}

- (NSString*)descriptionLabelTextForTesting {
  return self->_descriptionLabel.text;
}

- (NSString*)allowLabelTextForTesting {
  return self->_allowButton.currentTitle;
}

@end
