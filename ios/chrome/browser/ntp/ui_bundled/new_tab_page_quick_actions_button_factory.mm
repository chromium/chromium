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
  return IsNewTabPageUICleanupEnabled() ? kQuickActionsSymbolPointSizeUICleanup
                                        : kQuickActionsSymbolPointSize;
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

// Converts a symbol to appropriately sized UIImage.
UIImage* QuickActionButtonIconWithSymbol(Symbol symbol, bool has_title) {
  UIImage* icon;
  CGFloat symbolPointSize = SymbolPointSizeForButton(has_title);
  if (IsNewTabPageUICleanupEnabled()) {
    UIImageSymbolConfiguration* symbolConfiguration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:symbolPointSize
                                weight:UIImageSymbolWeightSemibold];
    icon = SymbolWithConfiguration(symbol, symbolConfiguration);
  } else {
    icon = SymbolWithPointSize(symbol, symbolPointSize);
  }
  return MakeSymbolMonochrome(icon);
}

// Applies `image` and optional `title` to `configuration`.
void ConfigureButtonTitleAndIcon(UIButtonConfiguration* configuration,
                                 UIImage* image,
                                 NSString* title) {
  configuration.image = image;

  if (title) {
    UIFont* font = PreferredFontForTextStyle(
        UIFontTextStyleSubheadline, UIFontWeightRegular, kMaximumFontSize);
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSAttributedString* attributedTitle =
        [[NSAttributedString alloc] initWithString:title attributes:attributes];
    configuration.attributedTitle = attributedTitle;
    configuration.titleLineBreakMode = NSLineBreakByTruncatingTail;
    configuration.imagePadding = kSymbolPadding;
  } else {
    configuration.attributedTitle = nil;
    configuration.imagePadding = 0;
  }
}

// Creates a new quick action button with the given `image` and optional
// `title`.
UIButton* CreateQuickActionButton(UIImage* image, NSString* title) {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.background.backgroundColor =
      ButtonBackgroundColor(/*color_palette=*/nil);
  configuration.background.cornerRadius = kButtonCornerRadius;
  configuration.baseForegroundColor = [UIColor colorNamed:kGrey700Color];
  ConfigureButtonTitleAndIcon(configuration, image, title);

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

+ (UIButton*)aimButtonWithTitle:(NSString*)title
                           icon:(UIImage*)icon
             accessibilityLabel:(NSString*)accessibilityLabel {
  UIButton* aimButton = CreateQuickActionButton(icon, title);
  aimButton.accessibilityIdentifier = kNTPAIMQuickActionIdentifier;
  aimButton.accessibilityLabel = accessibilityLabel;
  return aimButton;
}

+ (void)updateButton:(UIButton*)button
             withTitle:(NSString*)title
                  icon:(UIImage*)icon
    accessibilityLabel:(NSString*)accessibilityLabel {
  if (!button) {
    return;
  }
  UIButtonConfiguration* configuration = button.configuration;
  ConfigureButtonTitleAndIcon(configuration, icon, title);
  button.configuration = configuration;
  button.accessibilityLabel = accessibilityLabel;
}

+ (UIButton*)aimImageGenerationButton {
  CHECK_EQ(ntp_tiles::GetAimButtonRefactorArm(),
           ntp_tiles::AimButtonRefactorArm::kImageGenerationQuickAction);
  // TODO(crbug.com/549020046): Add an accessibility label to this button.
  UIImage* icon =
      QuickActionButtonIconWithSymbol(SymbolImageCreate, /*has_title=*/false);
  UIButton* aimImageGenerationButton =
      CreateQuickActionButton(icon, /*title=*/nil);
  aimImageGenerationButton.accessibilityIdentifier =
      kNTPAIMImageGenerationQuickActionIdentifier;
  return aimImageGenerationButton;
}

+ (UIButton*)aimAttachImageButton {
  CHECK_EQ(ntp_tiles::GetAimButtonRefactorArm(),
           ntp_tiles::AimButtonRefactorArm::kAttachImageQuickAction);
  // TODO(crbug.com/549020046): Add an accessibility label to this button.
  UIImage* icon = QuickActionButtonIconWithSymbol(SymbolPhotoBadgePlus,
                                                  /*has_title=*/false);
  UIButton* aimAttachImageButton = CreateQuickActionButton(icon, /*title=*/nil);
  aimAttachImageButton.accessibilityIdentifier =
      kNTPAIMAttachImageQuickActionIdentifier;
  return aimAttachImageButton;
}

+ (UIButton*)incognitoSearchButtonWithTitle:(BOOL)hasTitle {
  NSString* title =
      hasTitle ? l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_INCOGNITO)
               : nil;
  UIImage* icon = QuickActionButtonIconWithSymbol(SymbolIncognito, hasTitle);
  UIButton* incognitoButton = CreateQuickActionButton(icon, title);
  incognitoButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_NEW_INCOGNITO_TAB);
  incognitoButton.accessibilityIdentifier = kNTPIncognitoQuickActionIdentifier;
  return incognitoButton;
}

@end
