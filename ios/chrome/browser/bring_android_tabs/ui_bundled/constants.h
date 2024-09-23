// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_CONSTANTS_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the "Bring Android Tabs" confirmation alert
// prompt view controller.
extern NSString* const kBringAndroidTabsPromptConfirmationAlertAXId;

// Accessibility identifier for the "Bring Android Tabs" tab list
// view controller.
extern NSString* const kBringAndroidTabsPromptTabListAXId;

// Accessibility identifier for the "cancel" button of the "Bring Android Tabs"
// tab list.
extern NSString* const kBringAndroidTabsPromptTabListCancelButtonAXId;

// Accessibility identifier for the "open" button of the "Bring Android Tabs"
// tab list.
extern NSString* const kBringAndroidTabsPromptTabListOpenButtonAXId;

// Size of the favicons in the "Bring Android Tabs" tab list.
extern CGFloat const kBringAndroidTabsFaviconSize;

// Height of the table rows in the "Bring Android Tabs" tab list.
extern CGFloat const kTabListFromAndroidCellHeight;

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_CONSTANTS_H_
