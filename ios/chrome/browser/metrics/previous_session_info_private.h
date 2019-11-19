// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_PREVIOUS_SESSION_INFO_PRIVATE_H_
#define IOS_CHROME_BROWSER_METRICS_PREVIOUS_SESSION_INFO_PRIVATE_H_

@interface PreviousSessionInfo (TestingOnly)

// Redefined to be read-write.
@property(nonatomic, assign) BOOL didSeeMemoryWarningShortlyBeforeTerminating;
@property(nonatomic, assign) BOOL isFirstSessionAfterUpgrade;
@property(nonatomic, assign) float deviceBatteryLevel;
@property(nonatomic, assign)
    previous_session_info_constants::DeviceBatteryState deviceBatteryState;
@property(nonatomic, assign) BOOL OSRestartedAfterPreviousSession;
@property(nonatomic, strong) NSString* previousSessionVersion;

+ (void)resetSharedInstanceForTesting;

@end

#endif  // IOS_CHROME_BROWSER_METRICS_PREVIOUS_SESSION_INFO_PRIVATE_H_
