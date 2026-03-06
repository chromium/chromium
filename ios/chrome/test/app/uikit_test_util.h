// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_UIKIT_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_UIKIT_TEST_UTIL_H_

#import <UIKit/UIKit.h>

namespace chrome_test_util {

// Returns the UIWindowScene found from the list of connected scenes. Used only
// for testing.
UIWindowScene* GetAnyWindowScene();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_UIKIT_TEST_UTIL_H_
