// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/icons/symbol_helpers.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/icons/symbol_configurations.h"
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

UIImage* DefaultSettingsRootSymbol(NSString* symbol_name) {
  return DefaultSymbolWithPointSize(symbol_name,
                                    kSettingsRootSymbolImagePointSize);
}

UIImage* CustomSettingsRootSymbol(NSString* symbol_name) {
  return CustomSymbolWithPointSize(symbol_name,
                                   kSettingsRootSymbolImagePointSize);
}

UIImage* CustomSettingsRootMulticolorSymbol(NSString* symbol_name) {
  return CustomMulticolorSymbol(symbol_name, kSettingsRootSymbolImagePointSize);
}
