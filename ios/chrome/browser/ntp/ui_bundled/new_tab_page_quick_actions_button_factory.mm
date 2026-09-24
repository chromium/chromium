// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_button_factory.h"

#import "base/check.h"
#import "components/ntp_tiles/features.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using ntp_tiles::AimButtonRefactorArm;

// The border radius for a quick action button.
constexpr CGFloat kButtonCornerRadius = 24.0;

// The size of the quick actions symbols.
constexpr CGFloat kSymbolPointSize = 18.0;
constexpr CGFloat kSymbolPointSizeUICleanup = 14.0;

// The size of a quick action symbol when its button has no title.
constexpr CGFloat kSymbolPointSizeNoTitle = 16.0;

// The maximum font size for the quick actions button.
constexpr CGFloat kMaximumFontSize = 20.0;

// The padding between a quick action's symbol and title, if it has a title.
constexpr CGFloat kSymbolPadding = 8.0;

// The color used to match the fakebox background.
NSString* const kFakeboxMatchingBackgroundColor =
    @"fake_omnibox_bottom_gradient_color";

// Returns the point size for the symbol in a Quick Action button.
CGFloat SymbolPointSizeForButton(BOOL has_title) {
  AimButtonRefactorArm arm = ntp_tiles::GetAimButtonRefactorArm();
  BOOL ai_merchandising_chips_enabled =
      arm == AimButtonRefactorArm::kImageGenerationQuickAction ||
      arm == AimButtonRefactorArm::kAttachImageQuickAction;
  if (ai_merchandising_chips_enabled && !has_title) {
    return kSymbolPointSizeNoTitle;
  }
  return IsNewTabPageUICleanupEnabled() ? kSymbolPointSizeUICleanup
                                        : kSymbolPointSize;
}

// Returns the color needed for the background of the button.
UIColor* ButtonBackgroundColor(NewTabPageColorPalette* color_palette) {
  if (color_palette) {
    return color_palette.omniboxColor;
  }
  if (IsNewTabPageUICleanupEnabled()) {
    return [UIColor colorNamed:kNTPQuickActionChipColor];
  }
  return [UIColor colorNamed:kFakeboxMatchingBackgroundColor];
}

// Creates a new quick action button with the given `icon` and optional `title`.
UIButton* CreateQuickActionButton(Symbol symbol, NSString* title) {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.background.backgroundColor =
      ButtonBackgroundColor(/*color_palette=*/nil);
  configuration.background.cornerRadius = kButtonCornerRadius;
  configuration.baseForegroundColor = [UIColor colorNamed:kGrey700Color];
  UIImage* icon;
  CGFloat symbolPointSize =
      SymbolPointSizeForButton(/*has_title=*/title != nil);
  if (IsNewTabPageUICleanupEnabled()) {
    UIImageSymbolConfiguration* symbolConfiguration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:symbolPointSize
                                weight:UIImageSymbolWeightSemibold];
    icon = SymbolWithConfiguration(symbol, symbolConfiguration);
  } else {
    icon = SymbolWithPointSize(symbol, symbolPointSize);
  }
  configuration.image = MakeSymbolMonochrome(icon);

  if (title) {
    UIFont* font = PreferredFontForTextStyle(
        UIFontTextStyleSubheadline, UIFontWeightRegular, kMaximumFontSize);
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSAttributedString* attributedTitle =
        [[NSAttributedString alloc] initWithString:title attributes:attributes];
    configuration.attributedTitle = attributedTitle;
    configuration.titleLineBreakMode = NSLineBreakByTruncatingTail;
    configuration.imagePadding = kSymbolPadding;
  }

  UIButton* button = [[UIButton alloc] init];
  UIColor* base_tint_color =
      content_suggestions::DefaultIconTintColorWithAIMAllowed(YES);
  button.configurationUpdateHandler =
      CreateThemedButtonConfigurationUpdateHandler(
          base_tint_color,
          ^(NewTabPageColorPalette* color_palette) {
            return ButtonBackgroundColor(color_palette);
          },
          UIBlurEffectStyleSystemThickMaterial);

  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.configuration = configuration;
  return button;
}

}  // namespace

@implementation NewTabPageQuickActionsButtonFactory

+ (UIButton*)aimButtonWithTitle:(BOOL)hasTitle {
  // TODO(crbug.com/549020046): Add an accessibility label to this button.
  NSString* title =
      hasTitle ? l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM) : nil;
  UIButton* aimButton =
      CreateQuickActionButton(SymbolMagnifyingglassSpark, title);
  aimButton.accessibilityIdentifier = kNTPAIMQuickActionIdentifier;
  return aimButton;
}

+ (UIButton*)aimImageGenerationButton {
  CHECK_EQ(ntp_tiles::GetAimButtonRefactorArm(),
           ntp_tiles::AimButtonRefactorArm::kImageGenerationQuickAction);
  // TODO(crbug.com/549020046): Add an accessibility label to this button.
  UIButton* aimImageGenerationButton =
      CreateQuickActionButton(SymbolImageCreate, /*title=*/nil);
  aimImageGenerationButton.accessibilityIdentifier =
      kNTPAIMImageGenerationQuickActionIdentifier;
  return aimImageGenerationButton;
}

+ (UIButton*)aimAttachImageButton {
  CHECK_EQ(ntp_tiles::GetAimButtonRefactorArm(),
           ntp_tiles::AimButtonRefactorArm::kAttachImageQuickAction);
  // TODO(crbug.com/549020046): Add an accessibility label to this button.
  UIButton* aimAttachImageButton =
      CreateQuickActionButton(SymbolPhotoBadgePlus, /*title=*/nil);
  aimAttachImageButton.accessibilityIdentifier =
      kNTPAIMAttachImageQuickActionIdentifier;
  return aimAttachImageButton;
}

+ (UIButton*)incognitoSearchButtonWithTitle:(BOOL)hasTitle {
  NSString* title =
      hasTitle ? l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_INCOGNITO)
               : nil;
  UIButton* incognitoButton = CreateQuickActionButton(SymbolIncognito, title);
  incognitoButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_NEW_INCOGNITO_TAB);
  incognitoButton.accessibilityIdentifier = kNTPIncognitoQuickActionIdentifier;
  return incognitoButton;
}

@end
