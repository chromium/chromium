// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_PUBLIC_FIRST_RUN_LIVE_ACTIVITY_CONSTANTS_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_PUBLIC_FIRST_RUN_LIVE_ACTIVITY_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Identifies the current standard FRE step by the number of completed
// milestones out of 5 total milestones (where App Install/Launch is the first
// completed milestone; iPad uses values 1-3 since
// `ScreenType::kLensInteractivePromo` is omitted on tablet).
// Serialized into `FirstRunActivityAttributes.ContentState`. Entries should
// not be renumbered and numeric values should never be reused.
enum class FirstRunLiveActivityStep : NSInteger {
  kNone = 0,
  kSignIn = 1,                // 20% progress
  kHistorySync = 2,           // 40% progress
  kDefaultBrowserPromo = 3,   // 60% progress
  kLensInteractivePromo = 4,  // 80% progress
  kMaxValue = kLensInteractivePromo,
};

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_PUBLIC_FIRST_RUN_LIVE_ACTIVITY_CONSTANTS_H_
