// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_PASSKEY_CREATION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_PASSKEY_CREATION_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/passwords/bottom_sheet/ui/passkey_creation_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"

class FaviconLoader;
@protocol BrowserCoordinatorCommands;

// View controller for the passkey creation bottom sheet.
@interface PasskeyCreationBottomSheetViewController
    : BottomSheetViewController <PasskeyCreationBottomSheetConsumer>

// Initializes the view controller with the `handler` for user actions and
// `faviconLoader` for loading favicons.
- (instancetype)initWithHandler:(id<BrowserCoordinatorCommands>)handler
                  faviconLoader:(FaviconLoader*)faviconLoader;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_PASSKEY_CREATION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
