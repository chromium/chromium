// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_NAVIGATION_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_NAVIGATION_TEST_UTIL_H_

#include "url/gurl.h"

namespace chrome_test_util {

// Loads `url` in the current WebState with transition of type
// ui::PAGE_TRANSITION_TYPED.
void LoadUrl(const GURL& url);

// Loads `url` in the current WebState with transition of type
// ui::PAGE_TRANSITION_TYPED in window given windowNumber.
void LoadUrlInWindowWithNumber(const GURL& url, int window_number);

// Returns true if the current page in the current WebState is loading.
bool IsLoading();

// Returns true if the current page in the current WebState is loading in window
// given windowNumber.
bool IsLoadingInWindowWithNumber(int window_number);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_NAVIGATION_TEST_UTIL_H_
