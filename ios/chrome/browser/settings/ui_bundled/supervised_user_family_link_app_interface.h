// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SUPERVISED_USER_FAMILY_LINK_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SUPERVISED_USER_FAMILY_LINK_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Manages Family Link states for supervised user tests.
@interface SupervisedUserFamilyLinkAppInterface : NSObject

// Returns YES if the managed Family Link BrowserState managed by the
// TestFamilyLinkBrowserStateHelper singleton is successfully seeded.
+ (BOOL)isBrowserStateSeeded;

// Sets up and seeds the BrowserState that resets Family Link settings.
+ (void)resetFamilyLinkStateWithCompletion:(ProceduralBlock)completion;

// Tears down the TestFamilyLinkBrowserStateHelper singleton.
+ (void)tearDownTestFamilyLinkBrowserStateHelper;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SUPERVISED_USER_FAMILY_LINK_APP_INTERFACE_H_
