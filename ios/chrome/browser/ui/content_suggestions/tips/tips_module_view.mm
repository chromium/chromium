// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_view.h"

#import "base/check.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/icon_detail_view.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using segmentation_platform::TipIdentifier;

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

}  // namespace

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

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Private methods

- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = kTipsModuleViewID;

  TipIdentifier tip = _state.identifier;

  NSString* symbolName = [self symbolNameForTip:tip];

  // `kListBulletClipboardSymbol` and `kGlobeAmericasSymbol` are the only
  // default symbols used.
  BOOL isDefaultSymbol =
      [symbolName isEqualToString:kListBulletClipboardSymbol] ||
      [symbolName isEqualToString:kGlobeAmericasSymbol];

  // Determine the background color of the symbol based on the
  // name of the tip.
  UIColor* symbolBackgroundColor;
  switch (tip) {
    case segmentation_platform::TipIdentifier::kAddressBarPosition:
      symbolBackgroundColor = [UIColor colorNamed:kPurple500Color];
      break;
    case segmentation_platform::TipIdentifier::kSavePasswords:
    case segmentation_platform::TipIdentifier::kAutofillPasswords:
      symbolBackgroundColor = [UIColor colorNamed:kYellow500Color];
      break;
    case segmentation_platform::TipIdentifier::kEnhancedSafeBrowsing:
      symbolBackgroundColor = [UIColor colorNamed:kBlueColor];
      break;
    default:
      symbolBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  }

  // Determine how the Tip should be initialized based on if it has a Badge
  // Icon. If the Tip has a badge, pass additional parameters to customize the
  // Badge Icon.
  switch (tip) {
    case segmentation_platform::TipIdentifier::kLensShop:
      _contentView = [[IconDetailView alloc]
                    initWithTitle:[self titleText:tip]
                      description:[self descriptionTextForTip:tip]
                       layoutType:IconDetailViewLayoutType::kHero
                       symbolName:symbolName
               symbolColorPalette:@[ [UIColor whiteColor] ]
            symbolBackgroundColor:symbolBackgroundColor
                usesDefaultSymbol:isDefaultSymbol
                    showCheckmark:NO
                  badgeSymbolName:@"cart"
             badgeBackgroundColor:[UIColor colorNamed:kPink500Color]
           badgeUsesDefaultSymbol:YES
          accessibilityIdentifier:[self accessibilityIdentifierForTip:tip]];

      break;
    case segmentation_platform::TipIdentifier::kLensTranslate:
      _contentView = [[IconDetailView alloc]
                    initWithTitle:[self titleText:tip]
                      description:[self descriptionTextForTip:tip]
                       layoutType:IconDetailViewLayoutType::kHero
                       symbolName:symbolName
               symbolColorPalette:@[ [UIColor whiteColor] ]
            symbolBackgroundColor:symbolBackgroundColor
                usesDefaultSymbol:isDefaultSymbol
                    showCheckmark:NO
                  badgeSymbolName:kLanguageSymbol
             badgeBackgroundColor:[UIColor colorNamed:kBlue500Color]
           badgeUsesDefaultSymbol:NO
          accessibilityIdentifier:[self accessibilityIdentifierForTip:tip]];

      break;
    default:
      _contentView = [[IconDetailView alloc]
                    initWithTitle:[self titleText:tip]
                      description:[self descriptionTextForTip:tip]
                       layoutType:IconDetailViewLayoutType::kHero
                       symbolName:symbolName
               symbolColorPalette:@[ [UIColor whiteColor] ]
            symbolBackgroundColor:symbolBackgroundColor
                usesDefaultSymbol:isDefaultSymbol
                    showCheckmark:NO
          accessibilityIdentifier:[self accessibilityIdentifierForTip:tip]];

      break;
  }

  [self addSubview:_contentView];

  AddSameConstraints(_contentView, self);

  return;
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
- (NSString*)descriptionTextForTip:(TipIdentifier)tip {
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

// Returns the symbol name for the given `tip`.
- (NSString*)symbolNameForTip:(TipIdentifier)tip {
  switch (tip) {
    case TipIdentifier::kUnknown:
      return kListBulletClipboardSymbol;
    case TipIdentifier::kLensSearch:
    case TipIdentifier::kLensShop:
    case TipIdentifier::kLensTranslate:
      return kCameraLensSymbol;
    case TipIdentifier::kAddressBarPosition:
      return kGlobeAmericasSymbol;
    case TipIdentifier::kSavePasswords:
    case TipIdentifier::kAutofillPasswords:
      return kPasswordSymbol;
    case TipIdentifier::kEnhancedSafeBrowsing:
      return kPrivacySymbol;
  }
}

// Returns the accessibility identifier for the specified `tip`.
- (NSString*)accessibilityIdentifierForTip:(TipIdentifier)tip {
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
