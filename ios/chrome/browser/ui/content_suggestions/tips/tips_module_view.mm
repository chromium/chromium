// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_view.h"

#import <optional>
#import <string>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/icon_detail_view.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
      return {base::SysNSStringToUTF8(kListBulletClipboardSymbol), true};
    case TipIdentifier::kLensSearch:
    case TipIdentifier::kLensShop:
    case TipIdentifier::kLensTranslate:
      return {base::SysNSStringToUTF8(kCameraLensSymbol), false};
    case TipIdentifier::kAddressBarPosition:
      return {base::SysNSStringToUTF8(kGlobeAmericasSymbol), true};
    case TipIdentifier::kSavePasswords:
    case TipIdentifier::kAutofillPasswords:
#if BUILDFLAG(IS_IOS_MACCATALYST)
      return {base::SysNSStringToUTF8(kPasswordSymbol), false};
#else
      return {base::SysNSStringToUTF8(kMulticolorPasswordSymbol), false};
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
    case TipIdentifier::kEnhancedSafeBrowsing:
      return {base::SysNSStringToUTF8(kPrivacySymbol), false};
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
        SymbolConfig result = {base::SysNSStringToUTF8(kCameraLensSymbol),
                               false};

        return result;
      }

      SymbolConfig result = {base::SysNSStringToUTF8(kCartSymbol), true};

      return result;
    }
    case TipIdentifier::kLensTranslate: {
      SymbolConfig result = {base::SysNSStringToUTF8(kLanguageSymbol), false};

      return result;
    }
    default:
      return std::nullopt;
  }
}

}  // namespace

@interface TipsModuleView () <IconDetailViewTapDelegate>
@end

@implementation TipsModuleView {
  // The current state of the Tips module.
  TipsModuleState* _state;

  // The root view of the Tips module.
  UIView* _contentView;
}

- (instancetype)initWithState:(TipsModuleState*)state {
  if ((self = [super init])) {
    _state = state;
  }

  return self;
}

#pragma mark - TipsMagicStackConsumer

- (void)tipsStateDidChange:(TipsModuleState*)state {
  _state = state;

  // Determine whether the separator should be hidden.
  BOOL hideSeparator = state.productImageData.length > 0;
  [_contentViewDelegate updateSeparatorVisibility:hideSeparator];

  [_contentView removeFromSuperview];

  [self createSubviews];
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - IconDetailViewTapDelegate

- (void)didTapIconDetailView:(IconDetailView*)view {
  CHECK(view.identifier != nil);

  TipIdentifier tip =
      TipIdentifierForName(base::SysNSStringToUTF8(view.identifier));

  CHECK_NE(tip, TipIdentifier::kUnknown);

  [self.audience didSelectTip:tip];
}

#pragma mark - Private methods

- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;

  self.accessibilityIdentifier = kTipsModuleViewID;

  _contentView = [self iconDetailView:_state.identifier];

  [self addSubview:_contentView];

  AddSameConstraints(_contentView, self);

  return;
}

// Creates and returns an `IconDetailView` configured for the `tip`.
- (IconDetailView*)iconDetailView:(TipIdentifier)tip {
  SymbolConfig symbol = GetSymbolConfigForTip(tip);

  UIImage* productImage = [UIImage imageWithData:_state.productImageData
                                           scale:[UIScreen mainScreen].scale];

  BOOL hasProductImage = productImage != nil;

  std::optional<SymbolConfig> badgeSymbol =
      GetBadgeSymbolConfigForTip(tip, hasProductImage);

  if (badgeSymbol.has_value()) {
    SymbolConfig badgeConfig = badgeSymbol.value();

    NSArray<UIColor*>* badgeColorPalette =
        hasProductImage ? nil : @[ [UIColor whiteColor] ];

    BadgeShapeConfig badgeShapeConfig = {IconDetailViewBadgeShape::kCircle,
                                         kDefaultBadgeSize};

    if (hasProductImage) {
      badgeShapeConfig = {IconDetailViewBadgeShape::kSquare,
                          kProductImageBadgeSize,
                          kProductImageBadgeCornerRadius,
                          kProductImageBadgeCornerRadius,
                          kProductImageBadgeCornerRadius,
                          kProductImageBadgeBottomRightRadius};
    }

    IconDetailView* view = [[IconDetailView alloc]
                  initWithTitle:[self titleText:tip]
                    description:[self descriptionText:tip]
                     layoutType:IconDetailViewLayoutType::kHero
                backgroundImage:productImage
                     symbolName:base::SysUTF8ToNSString(symbol.name)
             symbolColorPalette:[self symbolColorPalette:tip]
          symbolBackgroundColor:[self symbolBackgroundColor:tip]
              usesDefaultSymbol:symbol.is_default_symbol
                    symbolWidth:kSymbolWidth
                  showCheckmark:NO
                badgeSymbolName:base::SysUTF8ToNSString(badgeConfig.name)
              badgeColorPalette:badgeColorPalette
               badgeShapeConfig:badgeShapeConfig
           badgeBackgroundColor:[self badgeBackgroundColor:tip
                                           hasProductImage:hasProductImage]
         badgeUsesDefaultSymbol:badgeConfig.is_default_symbol
        accessibilityIdentifier:[self accessibilityIdentifier:tip]];

    view.identifier = base::SysUTF8ToNSString(NameForTipIdentifier(tip));

    view.tapDelegate = self;

    return view;
  }

  IconDetailView* view =
      [[IconDetailView alloc] initWithTitle:[self titleText:tip]
                                description:[self descriptionText:tip]
                                 layoutType:IconDetailViewLayoutType::kHero
                            backgroundImage:productImage
                                 symbolName:base::SysUTF8ToNSString(symbol.name)
                         symbolColorPalette:[self symbolColorPalette:tip]
                      symbolBackgroundColor:[self symbolBackgroundColor:tip]
                          usesDefaultSymbol:symbol.is_default_symbol
                              showCheckmark:NO
                    accessibilityIdentifier:[self accessibilityIdentifier:tip]];

  view.identifier = base::SysUTF8ToNSString(NameForTipIdentifier(tip));

  view.tapDelegate = self;

  return view;
}

// Returns the title text for the given `tip`.
- (NSString*)titleText:(TipIdentifier)tip {
  switch (tip) {
    case TipIdentifier::kUnknown:
      // An unknown tip does not use a title.
      return @"";
    case TipIdentifier::kLensSearch:
      return l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_LENS_DEFAULT_TITLE);
    case TipIdentifier::kLensShop:
      return l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_LENS_SHOP_TITLE);
    case TipIdentifier::kLensTranslate:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_LENS_TRANSLATE_TITLE);
    case TipIdentifier::kAddressBarPosition:
      return l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_ADDRESS_BAR_TITLE);
    case TipIdentifier::kSavePasswords:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORD_TITLE);
    case TipIdentifier::kAutofillPasswords:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TITLE);
    case TipIdentifier::kEnhancedSafeBrowsing:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_SAFE_BROWSING_TITLE);
  }
}

// Returns the description text for the given `tip`.
- (NSString*)descriptionText:(TipIdentifier)tip {
  switch (tip) {
    case TipIdentifier::kUnknown:
      // An unknown tip does not use a description.
      return @"";
    case TipIdentifier::kLensSearch:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_LENS_DEFAULT_DESCRIPTION);
    case TipIdentifier::kLensShop:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_LENS_SHOP_DESCRIPTION);
    case TipIdentifier::kLensTranslate:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_LENS_TRANSLATE_DESCRIPTION);
    case TipIdentifier::kAddressBarPosition:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_ADDRESS_BAR_DESCRIPTION);
    case TipIdentifier::kSavePasswords:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORD_DESCRIPTION);
    case TipIdentifier::kAutofillPasswords:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_DESCRIPTION);
    case TipIdentifier::kEnhancedSafeBrowsing:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_SAFE_BROWSING_DESCRIPTION);
  }
}

// Returns the color palette for the symbol based on the `tip`.
//
// Note: If `nil` is returned, the icon will be shown in its default
// multi-color.
- (NSArray<UIColor*>*)symbolColorPalette:(TipIdentifier)tip {
  switch (tip) {
    case segmentation_platform::TipIdentifier::kAddressBarPosition:
    case segmentation_platform::TipIdentifier::kEnhancedSafeBrowsing:
      return @[ [UIColor whiteColor] ];
    case segmentation_platform::TipIdentifier::kSavePasswords:
    case segmentation_platform::TipIdentifier::kAutofillPasswords:
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

// Returns the symbol background color based on the `tip`.
- (UIColor*)symbolBackgroundColor:(TipIdentifier)tip {
  switch (tip) {
    case segmentation_platform::TipIdentifier::kAddressBarPosition:
      return [UIColor colorNamed:kPurple500Color];
    case segmentation_platform::TipIdentifier::kSavePasswords:
    case segmentation_platform::TipIdentifier::kAutofillPasswords:
#if BUILDFLAG(IS_IOS_MACCATALYST)
      return [UIColor colorNamed:kYellow500Color];
#else
      return [UIColor colorNamed:kSolidWhiteColor];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
    case segmentation_platform::TipIdentifier::kEnhancedSafeBrowsing:
      return [UIColor colorNamed:kBlue500Color];
    default:
      return [UIColor colorNamed:kBackgroundColor];
  }
}

// Returns the badge background color based on the `tip`, or `nil` if the tip
// doesn't have a badge. `hasProductImage` is used to determine the correct
// badge color for the shopping tip.
- (UIColor*)badgeBackgroundColor:(TipIdentifier)tip
                 hasProductImage:(BOOL)hasProductImage {
  switch (tip) {
    case segmentation_platform::TipIdentifier::kLensShop: {
      if (hasProductImage) {
        return [UIColor colorNamed:kBackgroundColor];
      }

      return [UIColor colorNamed:kPink500Color];
    }
    case segmentation_platform::TipIdentifier::kLensTranslate:
      return [UIColor colorNamed:kBlue500Color];
    default:
      return nil;
  }
}

// Returns the accessibility identifier for the specified `tip`.
- (NSString*)accessibilityIdentifier:(TipIdentifier)tip {
  switch (tip) {
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

@end
