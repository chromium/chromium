// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_

#include "base/feature_list.h"

// Feature to choose between the legacy new tab page or the refactored one.
// Use IsRefactoredNTP() instead of this constant directly.
extern const base::Feature kRefactoredNTP;

// Parameter to enable/disable the logging of the refactoredNTP.
extern const char kRefactoredNTPLoggingEnabled[];

// Whether the refactored NTP is used instead of the legacy one.
bool IsRefactoredNTP();

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_
