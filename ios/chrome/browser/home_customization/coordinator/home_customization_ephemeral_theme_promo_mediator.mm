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
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_ephemeral_theme_promo_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

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
        savedColorMappingDict->FindDict(kEphemeralThemeLightModeColorsKey));
    darkColors = ColorProviderDictionaryFromDict(
        savedColorMappingDict->FindDict(kEphemeralThemeDarkModeColorsKey));
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
