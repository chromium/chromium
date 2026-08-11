// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_HELPERS_H_
#define IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_HELPERS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/symbols/symbol_enums.h"

/// *******
/// Import `symbols.h` and not this file directly.
/// *******

#ifdef __cplusplus
extern "C" {
#endif

// Returns a SF Symbol to be used in a toolbar to symbolize "close".
UIImage* DefaultCloseButtonForToolbar();

// Returns a SF Symbol to be used in a toolbar to symbolize "done".
UIImage* DefaultDoneButtonForToolbar();

// Returns the given `symbol`, making sure that it is preferring the monochrome
// version.
UIImage* MakeSymbolMonochrome(UIImage* symbol);

// Returns the given `symbol`, making sure that it is preferring the multicolor
// version.
UIImage* MakeSymbolMulticolor(UIImage* symbol);

// Returns the given `symbol`, with the palette of `colors` applied.
UIImage* SymbolWithPalette(UIImage* symbol, NSArray<UIColor*>* colors);

// Returns an accessory symbol configured with UIImageSymbolWeightRegular.
UIImage* DefaultAccessorySymbolConfigurationWithRegularWeight(Symbol symbol);

// Returns a symbol configured with the given `configuration`.
UIImage* SymbolWithConfiguration(Symbol symbol,
                                 UIImageConfiguration* configuration);

// Returns a symbol configured with the default configuration and the given
// `point_size`.
UIImage* SymbolWithPointSize(Symbol symbol, CGFloat point_size);

// Returns a symbol as a template image, configured with the default
// configuration and the given `point_size`.
UIImage* SymbolTemplateWithPointSize(Symbol symbol, CGFloat point_size);

// Returns a symbol configured for the Settings root screen.
UIImage* SettingsRootSymbol(Symbol symbol);

// Returns a symbol configured for the Settings root screen with multicolor
// enabled.
UIImage* SettingsRootMulticolorSymbol(Symbol symbol);

// Helper for What's New: as it cannot safely store the enum values, use the
// string directly. Do not use outside of what's new.
UIImage* WhatsNewSymbolHelper(NSString* symbol_name,
                              bool is_system,
                              bool is_multicolor);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_HELPERS_H_
