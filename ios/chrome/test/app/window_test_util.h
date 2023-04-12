// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_WINDOW_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_WINDOW_TEST_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}

namespace chrome_test_util {

// Gets current active WebState, in window with given number.
web::WebState* GetCurrentWebStateForWindowWithNumber(int windowNumber);

// Returns the number of main tabs, in window with given number.
NSUInteger GetMainTabCountForWindowWithNumber(int windowNumber);

// Returns the number of incognito tabs, in window with given number.
NSUInteger GetIncognitoTabCountForWindowWithNumber(int windowNumber);

// Opens a new tab, in window with given number.
void OpenNewTabInWindowWithNumber(int windowNumber);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_WINDOW_TEST_UTIL_H_
