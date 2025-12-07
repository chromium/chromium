// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_UI_SYNCED_SET_UP_ANIMATOR_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_UI_SYNCED_SET_UP_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Animator that implements the custom Synced Set Up animation for presentation
// and dismissal.
@interface SyncedSetUpAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// Designated initializer. `isPresenting` indicates the direction of the
// transition.
- (instancetype)initForPresenting:(BOOL)isPresenting NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_UI_SYNCED_SET_UP_ANIMATOR_H_
