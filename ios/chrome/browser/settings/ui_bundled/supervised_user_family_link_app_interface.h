// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SUPERVISED_USER_FAMILY_LINK_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SUPERVISED_USER_FAMILY_LINK_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Manages Family Link states for supervised user tests.
@interface SupervisedUserFamilyLinkAppInterface : NSObject

// Returns YES if the managed FamilyLinkSettingsState managed by the
// TestFamilyLinkSettingsStateHelper singleton is successfully seeded.
+ (BOOL)isFamilyLinkSettingsStateSeeded;

// Manages Family Link settings.
// For subsequent calls to any of these methods, callers must wait for
// the BrowserState singleton to be seeded
// (i.e., [SupervisedUserFamilyLinkAppInterface isBrowserStateSeeded]
// returns YES).
+ (void)seedDefaultFamilyLinkSettings;
+ (void)seedSafeSitesFiltering;
+ (void)seedAllowSite:(NSString*)url;
+ (void)seedBlockSite:(NSString*)url;

// Manually triggers sync service refresh as a fallback to ensure that
// Family Link settings updates are applied.
+ (void)triggerSyncServiceRefresh;

// Tears down the TestFamilyLinkSettingsStateHelper singleton.
+ (void)tearDownTestFamilyLinkSettingsStateHelper;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SUPERVISED_USER_FAMILY_LINK_APP_INTERFACE_H_
