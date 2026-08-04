// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/symbol_helpers.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbol_configurations.h"
#import "ios/chrome/browser/shared/ui/symbols/symbol_info.h"
#import "ios/chrome/browser/shared/ui/symbols/symbol_names.h"

namespace {

constexpr CGFloat kCloseSymbolSize = 22;
constexpr CGFloat kDoneSymbolSize = 22;

// Returns the default configuration with the given `point_size`.
UIImageConfiguration* DefaultSymbolConfigurationWithPointSize(
    CGFloat point_size) {
  return [UIImageSymbolConfiguration
      configurationWithPointSize:point_size
                          weight:UIImageSymbolWeightMedium
                           scale:UIImageSymbolScaleMedium];
}

// Returns a symbol named `symbol.name` configured with the given
// `configuration`. `symbol.type` is used to specify if it is a system symbol or
// a custom symbol.
UIImage* SymbolWithConfiguration(SymbolInfo symbol,
                                 UIImageConfiguration* configuration) {
  UIImage* image;
  switch (symbol.type) {
    case SymbolType::kSystem:
      image = [UIImage systemImageNamed:symbol.name
                      withConfiguration:configuration];
      break;
    case SymbolType::kCustom:
      image = [UIImage imageNamed:symbol.name
                         inBundle:nil
                withConfiguration:configuration];
      break;
  }
  DCHECK(image) << " symbol_name: " << base::SysNSStringToUTF8(symbol.name)
                << " type: " << static_cast<int>(symbol.type);
  return image;
}

}  // namespace

extern "C" {

UIImage* DefaultCloseButtonForToolbar() {
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseSymbolSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  return SymbolWithConfiguration(SymbolXMark, configuration);
}

UIImage* DefaultDoneButtonForToolbar() {
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kDoneSymbolSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  return SymbolWithConfiguration(SymbolCheckmark, configuration);
}

UIImage* DefaultSymbolWithConfiguration(NSString* symbol_name,
                                        UIImageConfiguration* configuration) {
  return SymbolWithConfiguration({symbol_name, SymbolType::kSystem},
                                 configuration);
}

UIImage* CustomSymbolWithConfiguration(NSString* symbol_name,
                                       UIImageConfiguration* configuration) {
  return SymbolWithConfiguration({symbol_name, SymbolType::kCustom},
                                 configuration);
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

UIImage* MakeSymbolMonochrome(UIImage* symbol) {
  return [symbol
      imageByApplyingSymbolConfiguration:
          [UIImageSymbolConfiguration configurationPreferringMonochrome]];
}

UIImage* MakeSymbolMulticolor(UIImage* symbol) {
  return [symbol
      imageByApplyingSymbolConfiguration:
          [UIImageSymbolConfiguration configurationPreferringMulticolor]];
}

UIImage* SymbolWithPalette(UIImage* symbol, NSArray<UIColor*>* colors) {
  return [symbol
      imageByApplyingSymbolConfiguration:
          [UIImageSymbolConfiguration configurationWithPaletteColors:colors]];
}

UIImage* DefaultSettingsRootSymbol(NSString* symbol_name) {
  return DefaultSymbolWithPointSize(symbol_name,
                                    kSettingsRootSymbolImagePointSize);
}

UIImage* CustomSettingsRootSymbol(NSString* symbol_name) {
  return CustomSymbolWithPointSize(symbol_name,
                                   kSettingsRootSymbolImagePointSize);
}

UIImage* CustomSettingsRootMulticolorSymbol(NSString* symbol_name) {
  return MakeSymbolMulticolor(CustomSymbolWithPointSize(
      symbol_name, kSettingsRootSymbolImagePointSize));
}

UIImage* DefaultAccessorySymbolConfigurationWithRegularWeight(Symbol symbol) {
  return SymbolWithConfiguration(
      symbol, [UIImageSymbolConfiguration
                  configurationWithPointSize:kSymbolAccessoryPointSize
                                      weight:UIImageSymbolWeightRegular
                                       scale:UIImageSymbolScaleMedium]);
}

UIImage* SymbolWithConfiguration(Symbol symbol,
                                 UIImageConfiguration* configuration) {
  return SymbolWithConfiguration(InfoForSymbol(symbol), configuration);
}

UIImage* SymbolWithPointSize(Symbol symbol, CGFloat point_size) {
  return SymbolWithConfiguration(
      symbol, DefaultSymbolConfigurationWithPointSize(point_size));
}

UIImage* SymbolTemplateWithPointSize(Symbol symbol, CGFloat point_size) {
  return [SymbolWithPointSize(symbol, point_size)
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

UIImage* SettingsRootSymbol(Symbol symbol) {
  return SymbolWithPointSize(symbol, kSettingsRootSymbolImagePointSize);
}

UIImage* SettingsRootMulticolorSymbol(Symbol symbol) {
  return MakeSymbolMulticolor(SettingsRootSymbol(symbol));
}

}  // extern "C"
