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
extern NSString* const kPrivacySecuritySymbol;

// The size of the symbol image displayed in the Settings root screen.
extern const CGFloat kSettingsRootSymbolImagePointSize;

// Returns a SF symbol named `symbol_name` configured for the Settings root
// screen.
UIImage* DefaultSettingsRootSymbol(NSString* symbol_name);
// Returns a custom symbol named `symbol_name` configured for the Settings
// root screen.
UIImage* CustomSettingsRootSymbol(NSString* symbol_name);

#endif  // IOS_CHROME_BROWSER_UI_ICONS_SETTINGS_ICON_H_
