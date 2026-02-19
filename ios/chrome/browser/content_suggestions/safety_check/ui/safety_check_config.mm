// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_config.h"

#import "base/strings/string_number_conversions.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/safety_check/model/safety_check_utils.h"
#import "ios/chrome/browser/content_suggestions/safety_check/public/safety_check_constants.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_audience.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_item_type.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view_configuration.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Width of the icon in a Safety Check item's view.
constexpr CGFloat kIconWidth = 22;

}  // namespace

using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

@implementation SafetyCheckConfig

#pragma mark - Public

- (instancetype)init {
  if ((self = [super init])) {
    _updateChromeState = UpdateChromeSafetyCheckState::kDefault;
    _passwordState = PasswordSafetyCheckState::kDefault;
    _safeBrowsingState = SafeBrowsingSafetyCheckState::kDefault;
    _runningState = RunningSafetyCheckState::kDefault;
    _itemType = SafetyCheckItemType::kDefault;
  }
  return self;
}

- (instancetype)
    initWithUpdateChromeState:(UpdateChromeSafetyCheckState)updateChromeState
                passwordState:(PasswordSafetyCheckState)passwordState
            safeBrowsingState:(SafeBrowsingSafetyCheckState)safeBrowsingState
                 runningState:(RunningSafetyCheckState)runningState {
  if ((self = [self init])) {
    _updateChromeState = updateChromeState;
    _passwordState = passwordState;
    _safeBrowsingState = safeBrowsingState;
    _runningState = runningState;
  }
  return self;
}

- (BOOL)isRunning {
  return self.runningState == RunningSafetyCheckState::kRunning ||
         self.updateChromeState == UpdateChromeSafetyCheckState::kRunning ||
         self.passwordState == PasswordSafetyCheckState::kRunning ||
         self.safeBrowsingState == SafeBrowsingSafetyCheckState::kRunning;
}

- (BOOL)isDefault {
  return self.runningState == RunningSafetyCheckState::kDefault &&
         self.updateChromeState == UpdateChromeSafetyCheckState::kDefault &&
         (self.passwordState == PasswordSafetyCheckState::kDefault ||
          self.passwordState == PasswordSafetyCheckState::kSignedOut) &&
         self.safeBrowsingState == SafeBrowsingSafetyCheckState::kDefault;
}

- (NSUInteger)numberOfIssues {
  NSUInteger invalidCheckCount = 0;

  if (InvalidUpdateChromeState(self.updateChromeState)) {
    invalidCheckCount++;
  }

  if (InvalidPasswordState(self.passwordState)) {
    invalidCheckCount++;
  }

  if (InvalidSafeBrowsingState(self.safeBrowsingState)) {
    invalidCheckCount++;
  }

  return invalidCheckCount;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  SafetyCheckConfig* config = [[super copyWithZone:zone]
      initWithUpdateChromeState:self.updateChromeState
                  passwordState:self.passwordState
              safeBrowsingState:self.safeBrowsingState
                   runningState:self.runningState];
  config.weakPasswordsCount = self.weakPasswordsCount;
  config.reusedPasswordsCount = self.reusedPasswordsCount;
  config.compromisedPasswordsCount = self.compromisedPasswordsCount;
  config.lastRunTime = self.lastRunTime;
  config.audience = self.audience;
  config.itemType = self.itemType;
  // LINT.ThenChange(safety_check_config.h:Copy)
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kSafetyCheck;
}

#pragma mark - IconDetailViewConfig

- (NSString*)titleText {
  switch (self.itemType) {
    case SafetyCheckItemType::kAllSafe:
      return GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_ALL_SAFE);
    case SafetyCheckItemType::kRunning:
      return GetNSString(IDS_IOS_SAFETY_CHECK_RUNNING);
    case SafetyCheckItemType::kUpdateChrome:
      return GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_UPDATE_CHROME);
    case SafetyCheckItemType::kPassword:
      return GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_PASSWORD);
    case SafetyCheckItemType::kSafeBrowsing:
      return GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_SAFE_BROWSING);
    case SafetyCheckItemType::kDefault:
      return GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_DEFAULT);
  }
}

- (NSString*)descriptionText {
  switch (self.layoutType) {
    case IconDetailViewLayoutType::kHero:
      return [self heroLayoutDescriptionText];
    case IconDetailViewLayoutType::kCompact:
      return [self compactLayoutDescriptionText];
  }
}

- (IconDetailViewLayoutType)layoutType {
  return ([self isRunning] || [self isDefault] || [self numberOfIssues] <= 1)
             ? IconDetailViewLayoutType::kHero
             : IconDetailViewLayoutType::kCompact;
}

- (NSString*)accessibilityIdentifier {
  switch (self.itemType) {
    case SafetyCheckItemType::kAllSafe:
      return safety_check::kAllSafeItemID;
    case SafetyCheckItemType::kRunning:
      return safety_check::kRunningItemID;
    case SafetyCheckItemType::kUpdateChrome:
      return safety_check::kUpdateChromeItemID;
    case SafetyCheckItemType::kPassword:
      return safety_check::kPasswordItemID;
    case SafetyCheckItemType::kSafeBrowsing:
      return safety_check::kSafeBrowsingItemID;
    case SafetyCheckItemType::kDefault:
      return safety_check::kDefaultItemID;
  }
}

- (BOOL)showCheckmark {
  return self.itemType == SafetyCheckItemType::kAllSafe;
}

- (NSString*)iconName {
  switch (self.itemType) {
    case SafetyCheckItemType::kUpdateChrome:
      return kInfoCircleSymbol;
    case SafetyCheckItemType::kPassword:
      return kPasswordSymbol;
    case SafetyCheckItemType::kSafeBrowsing:
      return kPrivacySymbol;
    case SafetyCheckItemType::kAllSafe:
    case SafetyCheckItemType::kRunning:
    case SafetyCheckItemType::kDefault:
      return kSafetyCheckSymbol;
    default:
      return nil;
  }
}

- (IconViewSourceType)iconSource {
  return IconViewSourceType::kSymbol;
}

- (NSArray<UIColor*>*)symbolColorPalette {
  if (self.layoutType == IconDetailViewLayoutType::kHero) {
    return @[ [UIColor whiteColor] ];
  } else if (self.ntpBackgroundColorPalette) {
    return @[ self.ntpBackgroundColorPalette.tintColor ];
  } else {
    return @[ [UIColor colorNamed:kBlue500Color] ];
  }
}

- (UIColor*)symbolBackgroundColor {
  if (self.layoutType == IconDetailViewLayoutType::kHero) {
    return [UIColor colorNamed:kBlue500Color];
  } else if (self.ntpBackgroundColorPalette) {
    return self.ntpBackgroundColorPalette.tertiaryColor;
  } else {
    return [UIColor colorNamed:kBlueHaloColor];
  }
}

- (BOOL)usesDefaultSymbol {
  return [self.iconName isEqualToString:kInfoCircleSymbol];
}

- (CGFloat)iconWidth {
  return kIconWidth;
}

#pragma mark - Private

// Returns the detailed description text for the Update Chrome item, considering
// the current channel.
- (NSString*)updateChromeItemDescriptionText {
  switch (::GetChannel()) {
    case version_info::Channel::STABLE:
    case version_info::Channel::DEV:
      return GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_UPDATE_CHROME);
    case version_info::Channel::BETA:
      return GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_BETA_DESC);
    case version_info::Channel::CANARY:
      return GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_CANARY_DESC);
    default:
      return GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_UPDATE_CHROME);
  }
}

// Returns the detailed description text for the Password item, based on the
// number of compromised, reused, and weak passwords.
- (NSString*)passwordItemDescriptionText {
  if (self.compromisedPasswordsCount > 1) {
    return GetNSStringF(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_COMPROMISED_PASSWORDS,
        base::NumberToString16(self.compromisedPasswordsCount));
  }

  if (self.compromisedPasswordsCount == 1) {
    return GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_COMPROMISED_PASSWORD);
  }

  if (self.reusedPasswordsCount > 1) {
    return GetNSStringF(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_REUSED_PASSWORDS,
        base::NumberToString16(self.reusedPasswordsCount));
  }

  if (self.reusedPasswordsCount == 1) {
    return GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_REUSED_PASSWORD);
  }

  if (self.weakPasswordsCount > 1) {
    return GetNSStringF(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_WEAK_PASSWORDS,
        base::NumberToString16(self.weakPasswordsCount));
  }

  if (self.weakPasswordsCount == 1) {
    return GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_WEAK_PASSWORD);
  }

  return GetNSString(
      IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_MULTIPLE_PASSWORD_ISSUES);
}

// Returns the description text for the current `itemType` in the Hero layout.
- (NSString*)heroLayoutDescriptionText {
  switch (self.itemType) {
    case SafetyCheckItemType::kAllSafe:
      return GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_ALL_SAFE);
    case SafetyCheckItemType::kRunning:
      // The running state has no description text.
      return @"";
    case SafetyCheckItemType::kUpdateChrome:
      return [self updateChromeItemDescriptionText];
    case SafetyCheckItemType::kPassword:
      return [self passwordItemDescriptionText];
    case SafetyCheckItemType::kSafeBrowsing:
      return GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_SAFE_BROWSING);
    case SafetyCheckItemType::kDefault:
      return GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_DEFAULT);
  }
}

// Returns the description text for the current `itemType` in the compact
// layout.
- (NSString*)compactLayoutDescriptionText {
  switch (self.itemType) {
    case SafetyCheckItemType::kAllSafe:
      return GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_ALL_SAFE);
    case SafetyCheckItemType::kRunning:
      // The running state has no description text.
      return @"";
    case SafetyCheckItemType::kUpdateChrome:
      return [self updateChromeItemCompactDescriptionText];
    case SafetyCheckItemType::kPassword:
      return [self passwordItemCompactDescriptionText];
    case SafetyCheckItemType::kSafeBrowsing:
      return GetNSString(
          IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_SAFE_BROWSING);
    case SafetyCheckItemType::kDefault:
      return GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_DEFAULT);
  }
}

// Returns the compact description text for the Update Chrome item, considering
// the current channel.
- (NSString*)updateChromeItemCompactDescriptionText {
  switch (::GetChannel()) {
    case version_info::Channel::STABLE:
    case version_info::Channel::DEV:
      return GetNSString(
          IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_UPDATE_CHROME);
    case version_info::Channel::BETA:
      return GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_BETA_DESC);
    case version_info::Channel::CANARY:
      return GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_CANARY_DESC);
    default:
      return GetNSString(
          IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_UPDATE_CHROME);
  }
}

// Returns the compact description text for the Password item, based on the
// presence of compromised, reused, or weak passwords.
- (NSString*)passwordItemCompactDescriptionText {
  if (self.compromisedPasswordsCount >= 1) {
    return GetNSString(
        IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_COMPROMISED_PASSWORD);
  }

  if (self.reusedPasswordsCount >= 1) {
    return GetNSString(
        IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_REUSED_PASSWORD);
  }

  if (self.weakPasswordsCount >= 1) {
    return GetNSString(IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_WEAK_PASSWORD);
  }

  return GetNSString(
      IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_MULTIPLE_PASSWORD_ISSUES);
}

#pragma mark - IconDetailViewTapDelegate

- (void)didTapIconDetailView:(IconDetailView*)view {
  CHECK_NE(view.identifier, nil);
  SafetyCheckItemType itemType = SafetyCheckItemTypeForName(view.identifier);
  [self.audience didSelectSafetyCheckItem:itemType];
}

@end
