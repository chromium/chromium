// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_config.h"

#import <optional>
#import <string>

#import "base/strings/sys_string_conversions.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_audience.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view_configuration.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using base::SysNSStringToUTF8;
using base::SysUTF8ToNSString;
using l10n_util::GetNSString;
using segmentation_platform::NameForTipIdentifier;
using segmentation_platform::TipIdentifier;
using segmentation_platform::TipIdentifierForName;

namespace {

// `TipsModuleView` Accessibility ID.
NSString* const kTipsModuleViewID = @"kTipsModuleViewID";

// Accessibility ID for an unknown tip.
NSString* const kUnknownAccessibilityID = @"kUnknownAccessibilityID";

// Accessibility ID for the Lens Search tip.
NSString* const kLensSearchAccessibilityID = @"kLensSearchAccessibilityID";

// Accessibility ID for the Lens Shop tip.
NSString* const kLensShopAccessibilityID = @"kLensShopAccessibilityID";

// Accessibility ID for the Lens Translate tip.
NSString* const kLensTranslateAccessibilityID =
    @"kLensTranslateAccessibilityID";

// Accessibility ID for the Address Bar Position tip.
NSString* const kAddressBarPositionAccessibilityID =
    @"kAddressBarPositionAccessibilityID";

// Accessibility ID for the Save Passwords tip.
NSString* const kSavePasswordsAccessibilityID =
    @"kSavePasswordsAccessibilityID";

// Accessibility ID for the Autofill Passwords tip.
NSString* const kAutofillPasswordsAccessibilityID =
    @"kAutofillPasswordsAccessibilityID";

// Accessibility ID for the Enhanced Safe Browsing tip.
NSString* const kEnhancedSafeBrowsingAccessibilityID =
    @"kEnhancedSafeBrowsingAccessibilityID";

// Constants for the default badge shape configuration (circle).
const CGFloat kDefaultBadgeSize = 20;

// Constants for the product image badge shape configuration (square).
const CGFloat kProductImageBadgeSize = 24;
const CGFloat kProductImageBadgeCornerRadius = 4;  // Single corner radius
const CGFloat kProductImageBadgeBottomRightRadius = 8;

// Constant for the symbol width.
const CGFloat kSymbolWidth = 22;

// This struct represents configuration information for a Tips-related symbol.
struct SymbolConfig {
  const std::string name;
  bool is_default_symbol;
};

// Returns the `SymbolConfig` for the given `tip`.
SymbolConfig GetSymbolConfigForTip(TipIdentifier tip) {
  switch (tip) {
    case TipIdentifier::kUnknown:
      return {SysNSStringToUTF8(kListBulletClipboardSymbol), true};
    case TipIdentifier::kLensSearch:
    case TipIdentifier::kLensShop:
    case TipIdentifier::kLensTranslate:
      return {SysNSStringToUTF8(kCameraLensSymbol), false};
    case TipIdentifier::kAddressBarPosition:
      return {SysNSStringToUTF8(kGlobeAmericasSymbol), true};
    case TipIdentifier::kSavePasswords:
    case TipIdentifier::kAutofillPasswords:
#if BUILDFLAG(IS_IOS_MACCATALYST)
      return {SysNSStringToUTF8(kPasswordSymbol), false};
#else
      return {SysNSStringToUTF8(kMulticolorPasswordSymbol), false};
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
    case TipIdentifier::kEnhancedSafeBrowsing:
      return {SysNSStringToUTF8(kPrivacySymbol), false};
  }
}

// Returns the `SymbolConfig` for the badge symbol of the given `tip`, or
// `std::nullopt` if the tip doesn't have a badge. `has_product_image` is used
// to determine the correct badge for the shopping tip.
std::optional<SymbolConfig> GetBadgeSymbolConfigForTip(TipIdentifier tip,
                                                       bool has_product_image) {
  switch (tip) {
    case TipIdentifier::kLensShop: {
      if (has_product_image) {
        SymbolConfig result = {SysNSStringToUTF8(kCameraLensSymbol), false};

        return result;
      }

      SymbolConfig result = {SysNSStringToUTF8(kCartSymbol), true};

      return result;
    }
    case TipIdentifier::kLensTranslate: {
      SymbolConfig result = {SysNSStringToUTF8(kLanguageSymbol), false};

      return result;
    }
    default:
      return std::nullopt;
  }
}

}  // namespace

@interface TipsModuleConfig ()

// Config for the shape and size of the SF Symbol used for this Tips module.
@property(nonatomic, readonly) SymbolConfig symbolConfig;

// Config for the shape and size of the SF Symbol used for the badge on this
// Tips module.
@property(nonatomic, readonly) std::optional<SymbolConfig> badgeSymbolConfig;

@end

@implementation TipsModuleConfig

- (instancetype)initWithTipIdentifier:(TipIdentifier)identifier {
  if ((self = [super init])) {
    _identifier = identifier;
  }
  return self;
}

#pragma mark - Accessors

- (SymbolConfig)symbolConfig {
  return GetSymbolConfigForTip(self.identifier);
}

- (std::optional<SymbolConfig>)badgeSymbolConfig {
  return GetBadgeSymbolConfigForTip(self.identifier, [self productImage]);
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  TipsModuleConfig* config =
      [[super copyWithZone:zone] initWithTipIdentifier:self.identifier];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  config.productImageData = self.productImageData;
  config.audience = self.audience;
  // LINT.ThenChange(tips_module_config.h:Copy)
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  if (self.identifier == TipIdentifier::kLensShop &&
      self.productImageData.length > 0) {
    return ContentSuggestionsModuleType::kTipsWithProductImage;
  }
  return ContentSuggestionsModuleType::kTips;
}

#pragma mark - IconDetailViewConfig

- (NSString*)titleText {
  switch (self.identifier) {
    case TipIdentifier::kUnknown:
      // An unknown tip does not use a title.
      return @"";
    case TipIdentifier::kLensSearch:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_LENS_DEFAULT_TITLE);
    case TipIdentifier::kLensShop:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_LENS_SHOP_TITLE);
    case TipIdentifier::kLensTranslate:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_LENS_TRANSLATE_TITLE);
    case TipIdentifier::kAddressBarPosition:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_ADDRESS_BAR_TITLE);
    case TipIdentifier::kSavePasswords:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORD_TITLE);
    case TipIdentifier::kAutofillPasswords:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TITLE);
    case TipIdentifier::kEnhancedSafeBrowsing:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_SAFE_BROWSING_TITLE);
  }
}

- (NSString*)descriptionText {
  switch (self.identifier) {
    case TipIdentifier::kUnknown:
      // An unknown tip does not use a description.
      return @"";
    case TipIdentifier::kLensSearch:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_LENS_DEFAULT_DESCRIPTION);
    case TipIdentifier::kLensShop:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_LENS_SHOP_DESCRIPTION);
    case TipIdentifier::kLensTranslate:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_LENS_TRANSLATE_DESCRIPTION);
    case TipIdentifier::kAddressBarPosition:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_ADDRESS_BAR_DESCRIPTION);
    case TipIdentifier::kSavePasswords:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORD_DESCRIPTION);
    case TipIdentifier::kAutofillPasswords:
      return GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_DESCRIPTION);
    case TipIdentifier::kEnhancedSafeBrowsing:
      return GetNSString(IDS_IOS_MAGIC_STACK_TIP_SAFE_BROWSING_DESCRIPTION);
  }
}

- (IconDetailViewLayoutType)layoutType {
  return IconDetailViewLayoutType::kHero;
}

- (NSString*)accessibilityIdentifier {
  switch (self.identifier) {
    case TipIdentifier::kUnknown:
      return kUnknownAccessibilityID;
    case TipIdentifier::kLensSearch:
      return kLensSearchAccessibilityID;
    case TipIdentifier::kLensShop:
      return kLensShopAccessibilityID;
    case TipIdentifier::kLensTranslate:
      return kLensTranslateAccessibilityID;
    case TipIdentifier::kAddressBarPosition:
      return kAddressBarPositionAccessibilityID;
    case TipIdentifier::kSavePasswords:
      return kSavePasswordsAccessibilityID;
    case TipIdentifier::kAutofillPasswords:
      return kAutofillPasswordsAccessibilityID;
    case TipIdentifier::kEnhancedSafeBrowsing:
      return kEnhancedSafeBrowsingAccessibilityID;
  }
}

- (UIImage*)backgroundImage {
  return [self productImage];
}

- (NSString*)iconName {
  return SysUTF8ToNSString(self.symbolConfig.name);
}

- (IconViewSourceType)iconSource {
  return IconViewSourceType::kSymbol;
}

- (NSArray<UIColor*>*)symbolColorPalette {
  switch (self.identifier) {
    case TipIdentifier::kAddressBarPosition:
    case TipIdentifier::kEnhancedSafeBrowsing:
      return @[ [UIColor whiteColor] ];
    case TipIdentifier::kSavePasswords:
    case TipIdentifier::kAutofillPasswords:
#if BUILDFLAG(IS_IOS_MACCATALYST)
      return @[ [UIColor whiteColor] ];
#else
      return nil;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
    default:
      // `nil` indicates that the icon should be shown in its default color.
      return nil;
  }
}

- (UIColor*)symbolBackgroundColor {
  switch (self.identifier) {
    case TipIdentifier::kAddressBarPosition:
      return [UIColor colorNamed:kPurple500Color];
    case TipIdentifier::kSavePasswords:
    case TipIdentifier::kAutofillPasswords:
#if BUILDFLAG(IS_IOS_MACCATALYST)
      return [UIColor colorNamed:kYellow500Color];
#else
      return [UIColor colorNamed:kSolidWhiteColor];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
    case TipIdentifier::kEnhancedSafeBrowsing:
      return [UIColor colorNamed:kBlue500Color];
    default:
      return [UIColor colorNamed:kBackgroundColor];
  }
}

- (BOOL)usesDefaultSymbol {
  return self.symbolConfig.is_default_symbol;
}

- (CGFloat)iconWidth {
  return kSymbolWidth;
}

- (NSString*)badgeSymbolName {
  if (!self.badgeSymbolConfig.has_value()) {
    return nil;
  }
  return SysUTF8ToNSString(self.badgeSymbolConfig.value().name);
}

- (NSArray<UIColor*>*)badgeColorPalette {
  if (!self.badgeSymbolConfig.has_value()) {
    return nil;
  }
  return [self productImage] ? nil : @[ [UIColor whiteColor] ];
}

- (BadgeShapeConfig)badgeShapeConfig {
  if (!self.badgeSymbolConfig.has_value()) {
    return {};
  }

  if ([self productImage]) {
    return {
        IconDetailViewBadgeShape::kSquare, kProductImageBadgeSize,
        kProductImageBadgeCornerRadius,    kProductImageBadgeCornerRadius,
        kProductImageBadgeCornerRadius,    kProductImageBadgeBottomRightRadius};
  }

  return {IconDetailViewBadgeShape::kCircle, kDefaultBadgeSize};
}

- (UIColor*)badgeBackgroundColor {
  switch (self.identifier) {
    case TipIdentifier::kLensShop: {
      if ([self productImage]) {
        return [UIColor colorNamed:kBackgroundColor];
      }
      return [UIColor colorNamed:kPink500Color];
    }
    case TipIdentifier::kLensTranslate:
      return [UIColor colorNamed:kBlue500Color];
    default:
      return nil;
  }
}

- (BOOL)badgeUsesDefaultSymbol {
  if (!self.badgeSymbolConfig.has_value()) {
    return NO;
  }
  return self.badgeSymbolConfig.value().is_default_symbol;
}

#pragma mark - IconDetailViewTapDelegate

- (void)didTapIconDetailView:(IconDetailView*)view {
  CHECK_NE(view.identifier, nil);

  TipIdentifier tip =
      TipIdentifierForName(base::SysNSStringToUTF8(view.identifier));

  CHECK_NE(tip, TipIdentifier::kUnknown);

  [self.audience didSelectTip:tip];
}

#pragma mark - Private

- (UIImage*)productImage {
  return [UIImage imageWithData:self.productImageData
                          scale:[UIScreen mainScreen].scale];
}

@end
