// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_HELPERS_H_
#define IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_HELPERS_H_

#import <UIKit/UIKit.h>

/// *******
/// Import `symbols.h` and not this file directly.
/// *******

#ifdef __cplusplus
extern "C" {
#endif

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

// Returns the given `symbol`, making sure that it is preferring the monochrome
// version.
UIImage* MakeSymbolMonochrome(UIImage* symbol);

// Returns the given `symbol`, making sure that it is preferring the multicolor
// version.
UIImage* MakeSymbolMulticolor(UIImage* symbol);

// Returns the given `symbol`, with the palette of `colors` applied.
UIImage* SymbolWithPalette(UIImage* symbol, NSArray<UIColor*>* colors);

// Returns a SF symbol named `symbol_name` configured for the Settings root
// screen.
UIImage* DefaultSettingsRootSymbol(NSString* symbol_name);

// Returns a custom symbol named `symbol_name` configured for the Settings
// root screen.
UIImage* CustomSettingsRootSymbol(NSString* symbol_name);

// Returns a custom symbol named `symbol_name` configured for the Settings
// root screen, with multicolor enabled.
UIImage* CustomSettingsRootMulticolorSymbol(NSString* symbol_name);

// Returns a custom accessory symbol named `symbol_name` configured with
// UIImageSymbolWeightRegular.
UIImage* DefaultAccessorySymbolConfigurationWithRegularWeight(
    NSString* symbol_name);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_HELPERS_H_
