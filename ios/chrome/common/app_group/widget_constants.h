// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_APP_GROUP_WIDGET_CONSTANTS_H_
#define IOS_CHROME_COMMON_APP_GROUP_WIDGET_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Scheme used by the widget extension actions. It's important that this scheme
// is never defined as Custom URL Scheme for Chrome so only the widgets can use
// the actions on it.
extern NSString* const kWidgetKitSchemeChrome;

// Host used to identify Search (small) widget.
extern NSString* const kWidgetKitHostSearchWidget;
// Host used to identify Quick Actions (medium) widget.
extern NSString* const kWidgetKitHostQuickActionsWidget;
// Host used to identify Dino Game (small) widget.
extern NSString* const kWidgetKitHostDinoGameWidget;
// Host used to identify the Lockscreen Launcher widget.
extern NSString* const kWidgetKitHostLockscreenLauncherWidget;
// Host used to identify the Chrome Shortcuts widget.
extern NSString* const kWidgetKitHostShortcutsWidget;
// Host used to identify the Search Passwords widget.
extern NSString* const kWidgetKitHostSearchPasswordsWidget;

// Path for search action.
extern NSString* const kWidgetKitActionSearch;
// Path for incognito action.
extern NSString* const kWidgetKitActionIncognito;
// Path for Voice Search action.
extern NSString* const kWidgetKitActionVoiceSearch;
// Path for QR Reader action.
extern NSString* const kWidgetKitActionQRReader;
// Path for Lens action.
extern NSString* const kWidgetKitActionLens;
// Path for Game action.
extern NSString* const kWidgetKitActionGame;
// Path for open URL action.
extern NSString* const kWidgetKitActionOpenURL;
// Path for search passwords action.
extern NSString* const kWidgetKitActionSearchPasswords;

#endif  // IOS_CHROME_COMMON_APP_GROUP_WIDGET_CONSTANTS_H_
