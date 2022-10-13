// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ICONS_SETTINGS_ICON_H_
#define IOS_CHROME_BROWSER_UI_ICONS_SETTINGS_ICON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/icons/buildflags.h"

// Custom symbol names.
extern NSString* const kPrivacySymbol;
extern NSString* const kSyncDisabledSymbol;
extern NSString* const kSafetyCheckSymbol;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
extern NSString* const kGoogleIconSymbol;
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

// Default symbol names.
extern NSString* const kSyncEnabledSymbol;
extern NSString* const kDefaultBrowserSymbol;
extern NSString* const kDiscoverSymbol;
extern NSString* const kBellSymbol;

// Returns a SF symbol named `symbol_name` configured for the Settings root
// screen.
UIImage* DefaultSettingsRootSymbol(NSString* symbol_name);
// Returns a custom symbol named `symbol_name` configured for the Settings
// root screen.
UIImage* CustomSettingsRootSymbol(NSString* symbol_name);
// Returns a custom symbol named `symbol_name` configured for the Settings
// root screen, with multicolor enabled.
UIImage* CustomSettingsRootMulticolorSymbol(NSString* symbol_name);

#endif  // IOS_CHROME_BROWSER_UI_ICONS_SETTINGS_ICON_H_
