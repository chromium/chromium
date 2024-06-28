// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/enhanced_safe_browsing_inline_promo_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/enhanced_safe_browsing_inline_promo_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Height of the close button.
constexpr CGFloat kCloseButtonHeightSize = 24;

// The padding to the right of the close button.
constexpr CGFloat kCloseButtonHorizontalOffset = 12;

// The close button's padding to its top.
constexpr CGFloat kCloseButtonVerticalOffset = 16;

// Height and width of the shield image.
constexpr CGFloat kShieldImageSize = 36;

// The top padding for the inline promo.
constexpr CGFloat kInlinePromoVerticalPadding = 24;

// The side padding for the inline promo.
constexpr CGFloat kInlinePromoHorizontalPadding = 30;

// The UIStack spacing.
constexpr CGFloat kStackSpacing = 14;

// Creates the UIButton that dismisses the inline promo from view.
UIButton* CreateCloseButton() {
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ICON_CLOSE);

  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonHeightSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleSmall];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.image =
      DefaultSymbolWithConfiguration(kXMarkSymbol, symbolConfiguration);
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kSolidBlackColor];
  closeButton.configuration = buttonConfiguration;

  return closeButton;
}

// Creates a UIImageView containing either branded or unbranded shield image.
UIImageView* CreateShieldImage() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  NSString* shieldSymbol = kGoogleShieldSymbol;
#else
  NSString* shieldSymbol = kShieldSymbol;
#endif

  UIImage* image = [UIImage imageNamed:shieldSymbol];

  return [[UIImageView alloc] initWithImage:image];
}

// Creates a UIButton that redirects the user to the Safe Browsing settings
// page.
UIButton* CreatePrimaryButton() {
  UIButton* primaryButton = [UIButton buttonWithType:UIButtonTypeSystem];
  primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  primaryButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_INLINE_PROMO_BUTTON_TEXT);
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.attributedTitle = [[NSAttributedString alloc]
      initWithString:
          l10n_util::GetNSString(
              IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_INLINE_PROMO_BUTTON_TEXT)
          attributes:attributes];
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kSolidWhiteColor];
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kBlue600Color];
  primaryButton.configuration = buttonConfiguration;

  return primaryButton;
}

// Creates a UILabel containing the description of the inline promo.
UILabel* CreateLabel() {
  UILabel* textLabel = [[UILabel alloc] init];
  textLabel.text = l10n_util::GetNSString(
      IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_INLINE_PROMO_DESCRIPTION);
  textLabel.lineBreakMode = NSLineBreakByWordWrapping;
  textLabel.numberOfLines = 0;
  textLabel.textAlignment = NSTextAlignmentCenter;
  textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];

  return textLabel;
}
}  // namespace

@implementation EnhancedSafeBrowsingInlinePromoItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [EnhancedSafeBrowsingInlinePromoCell class];
    self.accessibilityTraits |= UIAccessibilityTraitNone;
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(EnhancedSafeBrowsingInlinePromoCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.delegate = self.delegate;
  [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
}

@end

@implementation EnhancedSafeBrowsingInlinePromoCell

#pragma mark - EnhancedSafeBrowsingInlinePromoCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIButton* closeButton = CreateCloseButton();
    [closeButton addTarget:self
                    action:@selector(dismissPromo)
          forControlEvents:UIControlEventTouchUpInside];
    UIImageView* shield = CreateShieldImage();
    UIButton* primaryButton = CreatePrimaryButton();
    [primaryButton addTarget:self
                      action:@selector(showSafeBrowsingSettings)
            forControlEvents:UIControlEventTouchUpInside];
    UILabel* textLabel = CreateLabel();
    [self.contentView addSubview:closeButton];

    UIStackView* stack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ shield, textLabel, primaryButton ]];
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    stack.axis = UILayoutConstraintAxisVertical;
    stack.distribution = UIStackViewDistributionEqualSpacing;
    stack.alignment = UIStackViewAlignmentCenter;
    stack.spacing = kStackSpacing;

    [self.contentView addSubview:stack];

    NSUInteger verticalPadding = 2 * kInlinePromoVerticalPadding;
    [NSLayoutConstraint activateConstraints:@[
      [self.contentView.heightAnchor constraintEqualToAnchor:stack.heightAnchor
                                                    constant:verticalPadding],
      [stack.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kInlinePromoHorizontalPadding],
      [stack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kInlinePromoHorizontalPadding],
      [stack.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                      constant:kInlinePromoVerticalPadding],
      [shield.heightAnchor constraintEqualToConstant:kShieldImageSize],
      [shield.widthAnchor constraintEqualToAnchor:shield.heightAnchor],
      [primaryButton.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
      [closeButton.heightAnchor
          constraintEqualToConstant:kCloseButtonHeightSize],
      [closeButton.widthAnchor
          constraintEqualToAnchor:closeButton.heightAnchor],
      [closeButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kCloseButtonHorizontalOffset],
      [closeButton.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kCloseButtonVerticalOffset],
    ]];
  }
  return self;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.delegate = nil;
}

#pragma mark - Private

// Invokes the delegate's promo dismissal functionality when the close button is
// tapped.
- (void)dismissPromo {
  [self.delegate dismissEnhancedSafeBrowsingInlinePromo];
}

// Displays the Safe Browsing Settings UI via the `delegate`.
- (void)showSafeBrowsingSettings {
  [self.delegate showSafeBrowsingSettingsMenu];
}

@end
