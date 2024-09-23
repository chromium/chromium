// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the User Education screen.
extern NSString* const kInactiveTabsUserEducationAccessibilityIdentifier;

// NSUserDefaults key to check whether the user education screen has ever been
// shown. The associated value in user defaults is a BOOL.
extern NSString* const kInactiveTabsUserEducationShownOnceKey;

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_CONSTANTS_H_
