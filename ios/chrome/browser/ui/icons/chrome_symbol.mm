// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/icons/chrome_symbol.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the default configuration with the given `point_size`.
UIImageConfiguration* DefaultSymbolConfigurationWithPointSize(
    CGFloat point_size) {
  return [UIImageSymbolConfiguration
      configurationWithPointSize:point_size
                          weight:UIImageSymbolWeightMedium
                           scale:UIImageSymbolScaleMedium];
}

// Returns a symbol named `symbol_name` configured with the given
// `configuration`. `system_symbol` is used to specify if it is a SFSymbol or a
// custom symbol.

UIImage* SymbolWithConfiguration(NSString* symbol_name,
                                 UIImageConfiguration* configuration,
                                 BOOL system_symbol) {
  UIImage* symbol;
  if (system_symbol) {
    symbol = [UIImage systemImageNamed:symbol_name
                     withConfiguration:configuration];
  } else {
    symbol = [UIImage imageNamed:symbol_name
                        inBundle:nil
               withConfiguration:configuration];
  }
  DCHECK(symbol);
  return symbol;
}

}  // namespace

// Custom symbol names.
NSString* const kArrowClockWiseSymbol = @"arrow_clockwise";
NSString* const kIncognitoSymbol = @"incognito";
NSString* const kSquareNumberSymbol = @"square_number";
NSString* const kTranslateSymbol = @"translate";
NSString* const kCameraSymbol = @"camera";
NSString* const kCameraFillSymbol = @"camera_fill";
NSString* const kPasswordManagerSymbol = @"password_manager";
NSString* const kPlusCircleFillSymbol = @"plus_circle_fill";
NSString* const kPopupBadgeMinusSymbol = @"popup_badge_minus";
NSString* const kPhotoBadgePlusSymbol = @"photo_badge_plus";
NSString* const kPhotoBadgeMagnifyingglassSymbol =
    @"photo_badge_magnifyingglass";
NSString* const kReadingListSymbol = @"square_bullet_square";
NSString* const kRecentTabsSymbol = @"laptopcomputer_and_phone";
NSString* const kLanguageSymbol = @"language";
NSString* const kPasswordSymbol = @"password";
NSString* const kCameraLensSymbol = @"camera_lens";
NSString* const kDownTrendSymbol = @"line_downtrend";
NSString* const kIncognitoCircleFilliOS14Symbol =
    @"incognito_circle_fill_ios14";

// Custom symbol names which can be configured a "palette".
NSString* const kIncognitoCircleFillSymbol = @"incognito_circle_fill";

// Default symbol names.
NSString* const kCreditCardSymbol = @"creditcard";
NSString* const kMicrophoneFillSymbol = @"mic.fill";
NSString* const kMicrophoneSymbol = @"mic";
NSString* const kEllipsisCircleFillSymbol = @"ellipsis.circle.fill";
NSString* const kPinSymbol = @"pin";
NSString* const kPinFillSymbol = @"pin.fill";
NSString* const kIPhoneSymbol = @"iphone";
NSString* const kIPadSymbol = @"ipad";
NSString* const kLaptopSymbol = @"laptopcomputer";
NSString* const kSettingsSymbol = @"gearshape";
NSString* const kSettingsFilledSymbol = @"gearshape.fill";
NSString* const kShareSymbol = @"square.and.arrow.up";
NSString* const kXMarkSymbol = @"xmark";
NSString* const kPlusSymbol = @"plus";
NSString* const kSearchSymbol = @"magnifyingglass";
NSString* const kCheckmarkSymbol = @"checkmark";
NSString* const kDownloadSymbol = @"arrow.down.circle";
NSString* const kSecureSymbol = @"lock";
NSString* const kWarningSymbol = @"exclamationmark.triangle";
NSString* const kWarningFillSymbol = @"exclamationmark.triangle.fill";
NSString* const kHelpSymbol = @"questionmark.circle";
NSString* const kCheckMarkCircleSymbol = @"checkmark.circle";
NSString* const kCheckMarkCircleFillSymbol = @"checkmark.circle.fill";
NSString* const kFailMarkCircleFillSymbol = @"exclamationmark.circle.fill";
NSString* const kTrashSymbol = @"trash";
NSString* const kInfoCircleSymbol = @"info.circle";
NSString* const kHistorySymbol = @"clock.arrow.circlepath";
NSString* const kCheckmarkSealSymbol = @"checkmark.seal";
NSString* const kWifiSymbol = @"wifi";
NSString* const kBookmarksSymbol = @"star.fill";
NSString* const kSyncErrorSymbol =
    @"exclamationmark.arrow.triangle.2.circlepath";
NSString* const kMenuSymbol = @"ellipsis";

const CGFloat kColorfulBackgroundSymbolCornerRadius = 7;

UIImage* DefaultSymbolWithConfiguration(NSString* symbol_name,
                                        UIImageConfiguration* configuration) {
  return SymbolWithConfiguration(symbol_name, configuration, true);
}

UIImage* CustomSymbolWithConfiguration(NSString* symbol_name,
                                       UIImageConfiguration* configuration) {
  return SymbolWithConfiguration(symbol_name, configuration, false);
}

UIImage* DefaultSymbolWithPointSize(NSString* symbol_name, CGFloat point_size) {
  return DefaultSymbolWithConfiguration(
      symbol_name, DefaultSymbolConfigurationWithPointSize(point_size));
}

UIImage* CustomSymbolWithPointSize(NSString* symbol_name, CGFloat point_size) {
  return CustomSymbolWithConfiguration(
      symbol_name, DefaultSymbolConfigurationWithPointSize(point_size));
}

UIImage* DefaultSymbolTemplateWithPointSize(NSString* symbol_name,
                                            CGFloat point_size) {
  return [DefaultSymbolWithPointSize(symbol_name, point_size)
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

UIImage* CustomSymbolTemplateWithPointSize(NSString* symbol_name,
                                           CGFloat point_size) {
  return [CustomSymbolWithPointSize(symbol_name, point_size)
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

UIImage* CustomMulticolorSymbol(NSString* symbol_name, CGFloat point_size) {
  UIImageConfiguration* configuration =
      DefaultSymbolConfigurationWithPointSize(point_size);
  if (@available(iOS 15, *)) {
    configuration = [configuration
        configurationByApplyingConfiguration:
            [UIImageSymbolConfiguration configurationPreferringMulticolor]];
  }
  UIImage* symbol = CustomSymbolWithConfiguration(symbol_name, configuration);
  if (@available(iOS 15, *)) {
    return symbol;
  }
  return [symbol imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
}

UIImage* CustomPaletteSymbol(NSString* symbol_name,
                             CGFloat point_size,
                             UIImageSymbolWeight weight,
                             UIImageSymbolScale scale,
                             NSArray<UIColor*>* colors) {
  UIImageConfiguration* conf =
      [UIImageSymbolConfiguration configurationWithPointSize:point_size
                                                      weight:weight
                                                       scale:scale];
  conf = [conf
      configurationByApplyingConfiguration:
          [UIImageSymbolConfiguration configurationWithPaletteColors:colors]];

  return CustomSymbolWithConfiguration(symbol_name, conf);
}

bool UseSymbols() {
  return base::FeatureList::IsEnabled(kUseSFSymbols);
}
