// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/symbol_helpers.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbol_configurations.h"

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

UIImage* MakeSymbolMonochrome(UIImage* symbol) {
#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
  if (@available(iOS 16, *)) {
    return [symbol
        imageByApplyingSymbolConfiguration:
            [UIImageSymbolConfiguration configurationPreferringMonochrome]];
  }
#endif  // defined(__IPHONE_16_0)
  return symbol;
}

UIImage* MakeSymbolMulticolor(UIImage* symbol) {
  if (@available(iOS 15, *)) {
    return [symbol
        imageByApplyingSymbolConfiguration:
            [UIImageSymbolConfiguration configurationPreferringMulticolor]];
  }
  return [symbol imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
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
