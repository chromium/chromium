// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_INFO_H_
#define IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_INFO_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/symbols/symbol_enums.h"

// The type of the symbol.
enum class SymbolType {
  kCustom,
  kSystem,
};

// A simple struct to hold symbol information.
struct SymbolInfo {
  NSString* name;
  SymbolType type;
};

// Returns symbol info (name and whether it's custom) for the given symbol enum
// value.
SymbolInfo InfoForSymbol(Symbol symbol);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_INFO_H_
