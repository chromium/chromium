// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_ephemeral_theme_promo_mediator.h"

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_ephemeral_theme_promo_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

// JSON dictionary keys for `animation-promo-colormapping`.
constexpr std::string_view kLightModeColorsKey = "light";
constexpr std::string_view kDarkModeColorsKey = "dark";

// Parses a hex color string (e.g. "#1A73E8" or "1A73E8") into a `UIColor*`.
// Returns nil if the string is not a valid 6-digit hex color.
UIColor* ColorFromHexString(std::string_view hex_string) {
  std::string_view trimmed =
      base::TrimString(hex_string, "#", base::TRIM_LEADING);
  if (trimmed.length() != 6) {
    return nil;
  }
  uint32_t rgb_value = 0;
  if (!base::HexStringToUInt(trimmed, &rgb_value)) {
    return nil;
  }
  return UIColorFromRGB(rgb_value);
}

// Converts a `base::DictValue` mapping keypath strings to hex color strings
// into an `NSDictionary<NSString*, UIColor*>*`.
NSDictionary<NSString*, UIColor*>* ColorProviderDictionaryFromDict(
    const base::DictValue* dict) {
  if (!dict) {
    return nil;
  }
  NSMutableDictionary<NSString*, UIColor*>* result =
      [[NSMutableDictionary alloc] initWithCapacity:dict->size()];
  for (const auto [key, value] : *dict) {
    if (!value.is_string()) {
      continue;
    }
    UIColor* color = ColorFromHexString(value.GetString());
    if (color) {
      result[base::SysUTF8ToNSString(key)] = color;
    }
  }
  return result.count > 0 ? [result copy] : nil;
}

}  // namespace

@implementation HomeCustomizationEphemeralThemePromoMediator {
  raw_ptr<PrefService> _prefService;
  raw_ptr<HomeBackgroundCustomizationService> _backgroundCustomizationService;
  base::FilePath _promoDataDirectory;
}

#pragma mark - Initializers

- (instancetype)initWithPrefService:(PrefService*)prefService
     backgroundCustomizationService:
         (HomeBackgroundCustomizationService*)backgroundCustomizationService
                 promoDataDirectory:(const base::FilePath&)promoDataDirectory {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _backgroundCustomizationService = backgroundCustomizationService;
    _promoDataDirectory = promoDataDirectory;
  }
  return self;
}

#pragma mark - Properties

- (void)setConsumer:(id<HomeCustomizationEphemeralThemePromoConsumer>)consumer {
  _consumer = consumer;
  if (!_consumer || !_prefService || _promoDataDirectory.empty()) {
    return;
  }

  const base::DictValue& savedThemeData =
      _prefService->GetDict(prefs::kIosNtpEphemeralThemeData);
  const std::string* savedPromoPath =
      savedThemeData.FindString(kEphemeralThemeAnimationPromoPathKey);
  if (!savedPromoPath || savedPromoPath->empty()) {
    return;
  }

  base::FilePath promoFilePath(*savedPromoPath);
  if (!promoFilePath.IsAbsolute()) {
    promoFilePath = _promoDataDirectory.Append(promoFilePath);
  }

  base::FilePath bundleDir = promoFilePath.DirName();
  std::string animationFileStem =
      promoFilePath.BaseName().RemoveExtension().value();
  NSString* bundlePath = base::SysUTF8ToNSString(bundleDir.value());
  NSBundle* animationBundle = [NSBundle bundleWithPath:bundlePath];
  NSString* animationAssetName = base::SysUTF8ToNSString(animationFileStem);
  if (![animationBundle pathForResource:animationAssetName ofType:@"json"]) {
    return;
  }

  NSDictionary<NSString*, UIColor*>* lightColors = nil;
  NSDictionary<NSString*, UIColor*>* darkColors = nil;
  const base::DictValue* savedColorMappingDict =
      savedThemeData.FindDict(kEphemeralThemeAnimationPromoColorMappingKey);
  if (savedColorMappingDict) {
    lightColors = ColorProviderDictionaryFromDict(
        savedColorMappingDict->FindDict(kLightModeColorsKey));
    darkColors = ColorProviderDictionaryFromDict(
        savedColorMappingDict->FindDict(kDarkModeColorsKey));
  }

  [_consumer setAnimationAssetName:animationAssetName bundle:animationBundle];
  [_consumer setLightModeColorProvider:lightColors
                 darkModeColorProvider:darkColors];
}

#pragma mark - Public

- (void)applyEphemeralTheme {
  if (!_prefService || !_backgroundCustomizationService) {
    return;
  }

  const base::DictValue& savedThemeData =
      _prefService->GetDict(prefs::kIosNtpEphemeralThemeData);
  const std::string* seedHex =
      savedThemeData.FindString(kEphemeralThemeSeedColorKey);
  if (!seedHex) {
    return;
  }

  std::string_view trimmed =
      base::TrimString(*seedHex, "#", base::TRIM_LEADING);
  uint32_t rgbValue = 0;
  if (trimmed.length() != 6 || !base::HexStringToUInt(trimmed, &rgbValue)) {
    return;
  }

  _backgroundCustomizationService->SetCurrentEphemeralTheme(
      SkColorSetA(rgbValue, 0xFF), sync_pb::UserColorTheme::TONAL_SPOT);
  _backgroundCustomizationService->StoreCurrentTheme();
}

- (void)disconnect {
  _prefService = nullptr;
  _backgroundCustomizationService = nullptr;
  _consumer = nil;
}

@end
