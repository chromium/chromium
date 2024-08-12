// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// FirstRunAppInterface contains the app-side implementation for helpers. These
// helpers are compiled into the app binary and can be called from either app or
// test code.
@interface FirstRunAppInterface : NSObject

// Returns whether UMA collection is enabled.
+ (BOOL)isUMACollectionEnabled;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_APP_INTERFACE_H_
