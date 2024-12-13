// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_CONFIGURATIONS_H_
#define IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_CONFIGURATIONS_H_

#import <UIKit/UIKit.h>

/// *******
/// Import `symbols.h` and not this file directly.
/// *******

// The size of the symbol image used in UIActions.
extern const CGFloat kSymbolActionPointSize;

// The corner radius of the symbol with a colorful background.
extern const CGFloat kColorfulBackgroundSymbolCornerRadius;

// The size of the symbol image used in the download toolbar.
extern const CGFloat kSymbolDownloadInfobarPointSize;
extern const CGFloat kSymbolDownloadSmallInfobarPointSize;

// The size of the symbol image displayed in infobars.
extern const CGFloat kInfobarSymbolPointSize;

// The size of accessory symbol images.
extern const CGFloat kSymbolAccessoryPointSize;

// Size of the icons in the root screen of the settings.
extern const CGFloat kSettingsRootSymbolImagePointSize;

// The size of the cloud slash icon.
extern const CGFloat kCloudSlashSymbolPointSize;

// Returns the palette to be used on incognito symbol when it is small.
NSArray<UIColor*>* SmallIncognitoPalette();

// Returns the palette to be used on incognito symbol when it is large.
NSArray<UIColor*>* LargeIncognitoPalette();

// Returns the cloud slash tint color.
UIColor* CloudSlashTintColor();

#endif  // IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_CONFIGURATIONS_H_
