// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ICONS_CHROME_SYMBOL_H_
#define IOS_CHROME_BROWSER_UI_ICONS_CHROME_SYMBOL_H_

#import <UIKit/UIKit.h>

// Custom symbol names.
extern NSString* const kArrowClockWiseSymbol;
extern NSString* const kIncognitoSymbol;
extern NSString* const kSquareNumberSymbol;
extern NSString* const kTranslateSymbol;

// Returns a custom symbol named |symbolName| configured with the given
// |configuration|.
UIImage* CustomSymbolWithConfiguration(NSString* symbolName,
                                       UIImageConfiguration* configuration);

#endif  // IOS_CHROME_BROWSER_UI_ICONS_CHROME_SYMBOL_H_
