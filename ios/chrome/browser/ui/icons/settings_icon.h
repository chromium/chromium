// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ICONS_SETTINGS_ICON_H_
#define IOS_CHROME_BROWSER_UI_ICONS_SETTINGS_ICON_H_

#import <UIKit/UIKit.h>

// Custom symbol names.
extern NSString* const kSyncDisabledSymbol;

// Default symbol names.
extern NSString* const kSyncErrorSymbol;
extern NSString* const kSyncEnabledSymbol;
extern NSString* const kDefaultBrowserSymbol;

// The size of the symbol image displayed in the Settings root screen.
extern NSInteger kSettingsRootSymbolImagePointSize;

// Returns a SF symbol named `symbolName` configured for the Settings root
// screen.
UIImage* DefaultSettingsRootSymbol(NSString* symbolName);

// Returns a custom symbol named `symbolName` configured for the Settings root
// screen.
UIImage* CustomSettingsRootSymbol(NSString* symbolName);

#endif  // IOS_CHROME_BROWSER_UI_ICONS_SETTINGS_ICON_H_
