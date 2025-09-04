// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_view.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// `AppBundlePromoView` accessibility ID.
NSString* const kAppBundlePromoViewID = @"kAppBundlePromoViewID";

// The spacing between the title and description.
constexpr CGFloat kTitleDescriptionSpacing = 2;

// The spacing between elements within the item.
constexpr CGFloat kContentStackSpacing = 14;

// Constants related to the icon container view.
constexpr CGFloat kIconSize = 40;
constexpr CGFloat kIconContainerSize = 56;
constexpr CGFloat kIconContainerCornerRadius = 12;

}  // namespace

@implementation AppBundlePromoView {
  // UI tap gesture recognizer.
  UITapGestureRecognizer* _tapGestureRecognizer;
  AppBundlePromoConfig* _config;
}

- (instancetype)initWithConfig:(AppBundlePromoConfig*)config {
  if ((self = [super init])) {
    _config = config;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];
  [self createSubviews];
}

#pragma mark - Private

- (void)createSubviews {
  if (!(self.subviews.count == 0)) {
    return;
  }

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_TITLE);
  NSString* description =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? l10n_util::GetNSString(
                IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_IPAD_DESCRIPTION)
          : l10n_util::GetNSString(
                IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_IPHONE_DESCRIPTION);

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = kAppBundlePromoViewID;
  self.isAccessibilityElement = YES;
  self.accessibilityTraits = UIAccessibilityTraitButton;
  self.accessibilityLabel =
      [NSString stringWithFormat:@"%@, %@", title, description];

  NSMutableArray* arrangedSubviews = [[NSMutableArray alloc] init];

  UIView* imageContainerView = [self imageInContainer];
  [arrangedSubviews addObject:imageContainerView];

  UILabel* titleLabel = [self createTitleLabel:title];
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];
  UILabel* descriptionLabel = [self createDescriptionLabel:description];
  [descriptionLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];

  UIStackView* textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, descriptionLabel ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.spacing = kTitleDescriptionSpacing;
  [textStack setContentHuggingPriority:UILayoutPriorityDefaultLow
                               forAxis:UILayoutConstraintAxisHorizontal];

  [arrangedSubviews addObject:textStack];

  UIStackView* contentStack =
      [[UIStackView alloc] initWithArrangedSubviews:arrangedSubviews];
  contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  contentStack.axis = UILayoutConstraintAxisHorizontal;
  contentStack.alignment = UIStackViewAlignmentCenter;
  contentStack.spacing = kContentStackSpacing;

  [self addSubview:contentStack];
  AddSameConstraints(contentStack, self);

  _tapGestureRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];
  [self addGestureRecognizer:_tapGestureRecognizer];
}

- (void)handleTap:(UITapGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateEnded) {
    [self.audience didSelectAppBundlePromo];
  }
}

- (UIView*)imageInContainer {
  UIView* iconContainer = [[UIView alloc] init];

  iconContainer.backgroundColor = [UIColor colorNamed:kGrey100Color];
  iconContainer.layer.cornerRadius = kIconContainerCornerRadius;

  UIImageView* imageView =
      [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"app_bundle"]];
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [imageView.heightAnchor constraintEqualToConstant:kIconSize],
    [imageView.widthAnchor constraintEqualToConstant:kIconSize],
  ]];
  [iconContainer addSubview:imageView];
  AddSameCenterConstraints(imageView, iconContainer);

  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor constraintEqualToConstant:kIconContainerSize],
    [iconContainer.widthAnchor
        constraintEqualToAnchor:iconContainer.heightAnchor],
  ]];

  return iconContainer;
}

- (UILabel*)createTitleLabel:(NSString*)title {
  UILabel* label = [[UILabel alloc] init];
  label.text = title;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.font =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightSemibold,
                                kMaxTextSizeForStyleFootnote);
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  return label;
}

- (UILabel*)createDescriptionLabel:(NSString*)description {
  UILabel* label = [[UILabel alloc] init];
  label.text = description;
  label.numberOfLines = 2;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.font = PreferredFontForTextStyle(UIFontTextStyleFootnote, std::nullopt,
                                         kMaxTextSizeForStyleFootnote);
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return label;
}

@end
