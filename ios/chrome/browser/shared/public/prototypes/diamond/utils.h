// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_UTILS_H_

#import <UIKit/UIKit.h>

class Browser;

// Height of the app bar.
extern const CGFloat kChromeAppBarPrototypeHeight;
extern const CGFloat kChromeAppBarPrototypeSymbolSize;

// The corner radius of the browser container.
extern const CGFloat kDiamondBrowserCornerRadius;

// Height of the toolbar with diamond.
extern const CGFloat kDiamondToolbarHeight;
extern const CGFloat kDiamondCollapsedToolbarHeight;
extern const CGFloat kDiamondLocationBarHeight;

// Notification sent when entering the tab grid.
extern NSString* kDiamondEnterTabGridNotification;
// Notification sent when leaving the tab grid.
extern NSString* kDiamondLeaveTabGridNotification;
// Notification sent for long press on the button, passing the button.
extern NSString* kDiamondLongPressButton;

// The symbol for the app icon.
extern NSString* const kAppSymbol;
// The symbol for the filled app icon.
extern NSString* const kAppFillSymbol;

// Starts the gemini panel for Diamond prototype.
void DiamondPrototypeStartGemini(bool from_tab_grid,
                                 bool incognito_grid,
                                 Browser* regular_browser,
                                 Browser* incognito_browser,
                                 UIViewController* base_view_controller);

// Starts the new tab sheet.
void DiamondPrototypeStartNewTab(bool from_tab_grid,
                                 bool incognito_grid,
                                 Browser* regular_browser,
                                 Browser* incognito_browser,
                                 UIViewController* base_view_controller);

// Returns a default symbol configured for the app bar.
UIImage* GetDefaultAppBarSymbol(NSString* symbol_name);

// Returns a custom symbol configured for the app bar.
UIImage* GetCustomAppBarSymbol(NSString* symbol_name);

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_UTILS_H_
