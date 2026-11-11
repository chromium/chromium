// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/ntp_quick_action_button.h"

#import <optional>

#import "base/check.h"
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

using l10n_util::GetNSString;

// The border radius for a quick action button.
constexpr CGFloat kButtonCornerRadius = 24.0;

// The padding between a quick action's symbol and title, if it has a title.
constexpr CGFloat kSymbolPaddingRegularLayout = 8.0;

// The size of a quick action symbol when its button has no title.
constexpr CGFloat kSymbolPointSizeCompactLayout = 16.0;

// The size of `SymbolImageCreate` in a Quick Action button.
constexpr CGFloat kImageCreateSymbolPointSize = 19.0;

// The maximum font size for the quick actions button.
constexpr CGFloat kMaximumFontSize = 20.0;

// The color used to match the fakebox background.
NSString* const kFakeboxMatchingBackgroundColor =
    @"fake_omnibox_bottom_gradient_color";

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

// Returns the configuration update handler for the button.
UIButtonConfigurationUpdateHandler ButtonConfigurationUpdateHandler() {
  UIColor* base_tint_color =
      content_suggestions::DefaultIconTintColorWithAIMAllowed(YES);
  return CreateThemedButtonConfigurationUpdateHandler(
      /*unthemedTintColor=*/base_tint_color,
      ^(NewTabPageColorPalette* color_palette) {
        return ButtonBackgroundColor(color_palette);
      },
      UIBlurEffectStyleSystemThickMaterial);
}

// Returns the `UIImageSymbolConfiguration` for `symbol` and `layout_type`.
// `symbol` is optional because some buttons (such as the AI Mode button) are
// provided a `UIImage` rather than a `Symbol`. When `std::nullopt` is passed,
// the default point size and weight for `layout_type` are used.
UIImageSymbolConfiguration* SymbolConfigurationForLayoutType(
    std::optional<Symbol> symbol,
    NTPQuickActionButtonLayout layout_type) {
  CGFloat symbol_point_size;
  if (symbol == SymbolImageCreate) {
    symbol_point_size = kImageCreateSymbolPointSize;
  } else {
    switch (layout_type) {
      case NTPQuickActionButtonLayout::kCompact:
        symbol_point_size = kSymbolPointSizeCompactLayout;
        break;
      case NTPQuickActionButtonLayout::kRegular:
        symbol_point_size = IsNewTabPageUICleanupEnabled()
                                ? kQuickActionsSymbolPointSizeUICleanup
                                : kQuickActionsSymbolPointSize;
        break;
    }
  }

  if (IsNewTabPageUICleanupEnabled()) {
    UIImageSymbolWeight weight = (symbol == SymbolImageCreate)
                                     ? UIImageSymbolWeightLight
                                     : UIImageSymbolWeightSemibold;
    return
        [UIImageSymbolConfiguration configurationWithPointSize:symbol_point_size
                                                        weight:weight];
  }
  return [UIImageSymbolConfiguration
      configurationWithPointSize:symbol_point_size
                          weight:UIImageSymbolWeightMedium
                           scale:UIImageSymbolScaleMedium];
}

// Returns a monochrome `UIImage` for `symbol` configured for `layout_type`.
UIImage* IconForSymbolWithLayoutType(Symbol symbol,
                                     NTPQuickActionButtonLayout layout_type) {
  UIImageSymbolConfiguration* symbol_configuration =
      SymbolConfigurationForLayoutType(symbol, layout_type);
  return MakeSymbolMonochrome(
      SymbolWithConfiguration(symbol, symbol_configuration));
}

}  // namespace

@implementation NTPQuickActionButton {
  // UIImageSymbolConfiguration for the icon displayed in this button when in
  // compact layout mode.
  UIImageSymbolConfiguration* _compactSymbolConfiguration;
  // UIImageSymbolConfiguration for the icon displayed in this button when in
  // regular layout mode.
  UIImageSymbolConfiguration* _regularSymbolConfiguration;
}

#pragma mark - Convenience Initializers

+ (instancetype)aimButtonWithTitle:(NSString*)title
                              icon:(UIImage*)icon
                accessibilityLabel:(NSString*)accessibilityLabel
                        layoutType:(NTPQuickActionButtonLayout)layoutType {
  return [[NTPQuickActionButton alloc]
                   initWithTitle:[title copy]
                            icon:icon
              accessibilityLabel:[accessibilityLabel copy]
         accessibilityIdentifier:kNTPAIMQuickActionIdentifier
                      layoutType:layoutType
      compactSymbolConfiguration:SymbolConfigurationForLayoutType(
                                     /*symbol=*/std::nullopt,
                                     NTPQuickActionButtonLayout::kCompact)
      regularSymbolConfiguration:SymbolConfigurationForLayoutType(
                                     /*symbol=*/std::nullopt,
                                     NTPQuickActionButtonLayout::kRegular)];
}

+ (instancetype)aimImageGenerationButton:
    (NTPQuickActionButtonLayout)layoutType {
  CHECK_EQ(GetAimButtonRefactorArm(),
           AimButtonRefactorArm::kImageGenerationQuickAction);
  return [[NTPQuickActionButton alloc]
                   initWithTitle:GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_CREATE)
                            icon:IconForSymbolWithLayoutType(SymbolImageCreate,
                                                             layoutType)
              accessibilityLabel:GetNSString(IDS_IOS_ACCNAME_NTP_CREATE_IMAGE)
         accessibilityIdentifier:kNTPAIMImageGenerationQuickActionIdentifier
                      layoutType:layoutType
      compactSymbolConfiguration:SymbolConfigurationForLayoutType(
                                     SymbolImageCreate,
                                     NTPQuickActionButtonLayout::kCompact)
      regularSymbolConfiguration:SymbolConfigurationForLayoutType(
                                     SymbolImageCreate,
                                     NTPQuickActionButtonLayout::kRegular)];
}

+ (instancetype)aimAttachImageButton:(NTPQuickActionButtonLayout)layoutType {
  AimButtonRefactorArm arm = GetAimButtonRefactorArm();
  CHECK(arm == AimButtonRefactorArm::kImageGenerationQuickAction ||
        arm == AimButtonRefactorArm::kAttachImageQuickAction);
  return [[NTPQuickActionButton alloc]
                   initWithTitle:GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_ADD)
                            icon:IconForSymbolWithLayoutType(
                                     SymbolPhotoBadgePlus, layoutType)
              accessibilityLabel:GetNSString(IDS_IOS_ACCNAME_NTP_ATTACH_IMAGE)
         accessibilityIdentifier:kNTPAIMAttachImageQuickActionIdentifier
                      layoutType:layoutType
      compactSymbolConfiguration:SymbolConfigurationForLayoutType(
                                     SymbolPhotoBadgePlus,
                                     NTPQuickActionButtonLayout::kCompact)
      regularSymbolConfiguration:SymbolConfigurationForLayoutType(
                                     SymbolPhotoBadgePlus,
                                     NTPQuickActionButtonLayout::kRegular)];
}

+ (instancetype)incognitoSearchButton:(NTPQuickActionButtonLayout)layoutType {
  return [[NTPQuickActionButton alloc]
                   initWithTitle:GetNSString(
                                     IDS_IOS_NTP_QUICK_ACTIONS_INCOGNITO)
                            icon:IconForSymbolWithLayoutType(SymbolIncognito,
                                                             layoutType)
              accessibilityLabel:GetNSString(IDS_IOS_ACCNAME_NEW_INCOGNITO_TAB)
         accessibilityIdentifier:kNTPIncognitoQuickActionIdentifier
                      layoutType:layoutType
      compactSymbolConfiguration:SymbolConfigurationForLayoutType(
                                     SymbolIncognito,
                                     NTPQuickActionButtonLayout::kCompact)
      regularSymbolConfiguration:SymbolConfigurationForLayoutType(
                                     SymbolIncognito,
                                     NTPQuickActionButtonLayout::kRegular)];
}

#pragma mark - Private Initializers

- (instancetype)initWithTitle:(NSString*)title
                          icon:(UIImage*)icon
            accessibilityLabel:(NSString*)accessibilityLabel
       accessibilityIdentifier:(NSString*)accessibilityIdentifier
                    layoutType:(NTPQuickActionButtonLayout)layoutType
    compactSymbolConfiguration:
        (UIImageSymbolConfiguration*)compactSymbolConfiguration
    regularSymbolConfiguration:
        (UIImageSymbolConfiguration*)regularSymbolConfiguration {
  self = [super init];
  if (self) {
    _titleText = title;
    _icon = icon;
    _layoutType = layoutType;
    _compactSymbolConfiguration = compactSymbolConfiguration;
    _regularSymbolConfiguration = regularSymbolConfiguration;
    self.accessibilityLabel = accessibilityLabel;
    self.accessibilityIdentifier = [accessibilityIdentifier copy];

    self.translatesAutoresizingMaskIntoConstraints = NO;

    UIButtonConfiguration* configuration =
        [UIButtonConfiguration plainButtonConfiguration];
    configuration.background.backgroundColor =
        ButtonBackgroundColor(/*color_palette=*/nil);
    configuration.background.cornerRadius = kButtonCornerRadius;
    configuration.baseForegroundColor = [UIColor colorNamed:kGrey700Color];
    self.configuration = configuration;
    self.configurationUpdateHandler = ButtonConfigurationUpdateHandler();

    [self updateLayoutConfiguration];
  }
  return self;
}

#pragma mark - Public

- (void)updateTitle:(NSString*)title
                  icon:(UIImage*)icon
    accessibilityLabel:(NSString*)accessibilityLabel {
  _titleText = [title copy];
  _icon = icon;
  self.accessibilityLabel = [accessibilityLabel copy];
  [self updateLayoutConfiguration];
}

#pragma mark - Accessors & Mutators

- (void)setLayoutType:(NTPQuickActionButtonLayout)layoutType {
  if (layoutType == _layoutType) {
    return;
  }
  _layoutType = layoutType;
  [self updateLayoutConfiguration];
}

- (void)setTitleText:(NSString*)titleText {
  if (titleText == _titleText || [titleText isEqualToString:_titleText]) {
    return;
  }
  _titleText = [titleText copy];
  [self updateLayoutConfiguration];
}

- (void)setIcon:(UIImage*)icon {
  if (icon == _icon) {
    return;
  }
  _icon = icon;
  [self updateLayoutConfiguration];
}

#pragma mark - Private

// Updates `self.configuration` to match the current `layoutType`, `icon`, and
// `titleText`.
- (void)updateLayoutConfiguration {
  UIButtonConfiguration* configuration = self.configuration;
  UIImageSymbolConfiguration* symbolConfiguration =
      (self.layoutType == NTPQuickActionButtonLayout::kCompact)
          ? _compactSymbolConfiguration
          : _regularSymbolConfiguration;
  configuration.image =
      self.icon.isSymbolImage
          ? [self.icon imageByApplyingSymbolConfiguration:symbolConfiguration]
          : self.icon;

  BOOL showTitle = self.layoutType == NTPQuickActionButtonLayout::kRegular &&
                   self.titleText && self.titleText.length > 0;

  if (showTitle) {
    UIFont* font = PreferredFontForTextStyle(
        UIFontTextStyleSubheadline, UIFontWeightRegular, kMaximumFontSize);
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSAttributedString* attributedTitle =
        [[NSAttributedString alloc] initWithString:self.titleText
                                        attributes:attributes];
    configuration.attributedTitle = attributedTitle;
    configuration.titleLineBreakMode = NSLineBreakByTruncatingTail;
    configuration.imagePadding = kSymbolPaddingRegularLayout;
  } else {
    configuration.attributedTitle = nil;
    configuration.imagePadding = 0;
  }

  self.configuration = configuration;
}

@end
