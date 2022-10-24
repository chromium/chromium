// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ICONS_INCOGNITO_SYMBOL_H_
#define IOS_CHROME_BROWSER_UI_ICONS_INCOGNITO_SYMBOL_H_

#import <UIKit/UIKit.h>

// Custom symbol names.
extern NSString* const kIncognitoSymbol;
extern NSString* const kIncognitoCircleFilliOS14Symbol;

// Custom symbol names which can be configured a "palette".
extern NSString* const kIncognitoCircleFillSymbol;

// Returns the colors palette to be used on incognito symbol when it is small.
NSArray<UIColor*>* SmallIncognitoColorsPalette();

// Returns the colors palette to be used on incognito symbol when it is large.
NSArray<UIColor*>* LargeIncognitoColorsPalette();

#endif  // IOS_CHROME_BROWSER_UI_ICONS_INCOGNITO_SYMBOL_H_
