// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEVICE_ORIENTATION_PORTAIT_ORIENTATION_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_DEVICE_ORIENTATION_PORTAIT_ORIENTATION_MANAGER_H_

#import <Foundation/Foundation.h>

// Manager in charge to block and unblock to portrait mode only.
@protocol PortraitOrientationManager <NSObject>

// Call this when the UI should be blocked in portrait mode.
- (void)incrementForcePortraitOrientationCounter;

// Call this when the UI can be unblocked from portrait mode.
- (void)decrementForcePortraitOrientationCounter;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEVICE_ORIENTATION_PORTAIT_ORIENTATION_MANAGER_H_
