// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_STACK_VIEW_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_STACK_VIEW_TEST_UTIL_H_

#import <Foundation/Foundation.h>

@class StackViewController;

namespace chrome_test_util {

// Returns whether the MainController's Tab Switcher is active.
bool IsTabSwitcherActive();

// Returns the StackViewController.
StackViewController* GetStackViewController();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_STACK_VIEW_TEST_UTIL_H_
