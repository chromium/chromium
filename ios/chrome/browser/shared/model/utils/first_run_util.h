// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FIRST_RUN_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FIRST_RUN_UTIL_H_

#import "base/time/time.h"

// Returns whether chrome is first run or forced first run.
BOOL IsFirstRun();

// Returns whether first run was more recent than `timeDelta`.
BOOL IsFirstRunRecent(const base::TimeDelta& timeDelta);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FIRST_RUN_UTIL_H_
