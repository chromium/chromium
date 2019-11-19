// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_TAB_USAGE_RECORDER_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_METRICS_TAB_USAGE_RECORDER_TEST_UTIL_H_

#include "base/compiler_specific.h"

@class NSError;

namespace tab_usage_recorder_test_util {

// Opens a new incognito tab using the UI and evicts any main tab model tabs.
// Returns false on failure.
bool OpenNewIncognitoTabUsingUIAndEvictMainTabs() WARN_UNUSED_RESULT;

// Switches to normal mode using the tab switcher and selects the
// previously-selected normal tab. Assumes current mode is Incognito.
// Induces EG assert on failure.
void SwitchToNormalMode();

}  // namespace tab_usage_recorder_test_util

#endif  // IOS_CHROME_BROWSER_METRICS_TAB_USAGE_RECORDER_TEST_UTIL_H_
