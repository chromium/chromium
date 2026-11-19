// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LIVE_ACTIVITY_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_LIVE_ACTIVITY_PUBLIC_FEATURES_H_

#import "base/feature_list.h"

// Enables the `IOSChromeLiveActivityManager` infrastructure.
BASE_DECLARE_FEATURE(kIOSLiveActivityManager);

// Returns true if `kIOSLiveActivityManager` is enabled.
bool IsIOSLiveActivityManagerEnabled();

#endif  // IOS_CHROME_BROWSER_LIVE_ACTIVITY_PUBLIC_FEATURES_H_
