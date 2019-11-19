// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// FirstRunAppInterface contains the app-side implementation for helpers. These
// helpers are compiled into the app binary and can be called from either app or
// test code.
@interface FirstRunAppInterface : NSObject

// Triggers the display of the first run UI.
+ (void)showFirstRunUI;

// Resets the UMA collection enabled pref to |enabled|.
+ (void)setUMACollectionEnabled:(BOOL)enabled;

// Returns whether UMA collection is enabled.
+ (BOOL)isUMACollectionEnabled;

// Resets the UMA collection enabled by default pref to UNKNOWN.
+ (void)resetUMACollectionEnabledByDefault;

// Returns whether the UMA collections checkbox is initially checked.
+ (BOOL)isUMACollectionEnabledByDefault;

// Returns whether sync has finished its first run setup.
+ (BOOL)isSyncFirstSetupComplete;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_APP_INTERFACE_H_
