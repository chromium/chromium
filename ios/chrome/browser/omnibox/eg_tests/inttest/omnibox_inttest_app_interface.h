// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

/// Contains the app-side implementation of omnibox integration test helpers.
@interface OmniboxInttestAppInterface : NSObject

/// Returns the last URL loaded by the omnibox.
+ (NSURL*)lastURLLoaded;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_APP_INTERFACE_H_
