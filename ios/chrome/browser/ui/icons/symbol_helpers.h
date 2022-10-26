// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ICONS_SYMBOL_HELPERS_H_
#define IOS_CHROME_BROWSER_UI_ICONS_SYMBOL_HELPERS_H_

#import <UIKit/UIKit.h>

/// *******
/// Import `symbols.h` and not this file directly.
/// *******

// Returns YES if the kUseSFSymbols flag is enabled.
bool UseSymbols();

// Returns a SF symbol named `symbol_name` configured with the given
// `configuration`.
UIImage* DefaultSymbolWithConfiguration(NSString* symbol_name,
                                        UIImageConfiguration* configuration);

// Returns a custom symbol named `symbol_name` configured with the given
// `configuration`.
UIImage* CustomSymbolWithConfiguration(NSString* symbol_name,
                                       UIImageConfiguration* configuration);

// Returns a SF symbol named `symbol_name` configured with the default
// configuration and the given `point_size`.
UIImage* DefaultSymbolWithPointSize(NSString* symbol_name, CGFloat point_size);

// Returns a custom symbol named `symbol_name` configured with the default
// configuration and the given `point_size`.
UIImage* CustomSymbolWithPointSize(NSString* symbol_name, CGFloat point_size);

// Returns a SF symbol named `symbol_name` as a template image, configured with
// the default configuration and the given `point_size`.
UIImage* DefaultSymbolTemplateWithPointSize(NSString* symbol_name,
                                            CGFloat point_size);

// Returns a custom symbol named `symbol_name` as a template image, configured
// with the default configuration and the given `point_size`.
UIImage* CustomSymbolTemplateWithPointSize(NSString* symbol_name,
                                           CGFloat point_size);

// Returns a custom symbol named `symbol_name`, configured with the default
// configuration and the given `point_size`.
UIImage* CustomMulticolorSymbol(NSString* symbol_name, CGFloat point_size);

// Returns a custom symbol named `symbol_name` configured with `point_size`,
// `weight`, `scale` and set the "Palette" configuration for `colors`.
UIImage* CustomPaletteSymbol(NSString* symbol_name,
                             CGFloat point_size,
                             UIImageSymbolWeight weight,
                             UIImageSymbolScale scale,
                             NSArray<UIColor*>* colors)
    API_AVAILABLE(ios(15.0));

// Returns a SF symbol named `symbol_name` configured for the Settings root
// screen.
UIImage* DefaultSettingsRootSymbol(NSString* symbol_name);

// Returns a custom symbol named `symbol_name` configured for the Settings
// root screen.
UIImage* CustomSettingsRootSymbol(NSString* symbol_name);

// Returns a custom symbol named `symbol_name` configured for the Settings
// root screen, with multicolor enabled.
UIImage* CustomSettingsRootMulticolorSymbol(NSString* symbol_name);

#endif  // IOS_CHROME_BROWSER_UI_ICONS_SYMBOL_HELPERS_H_
