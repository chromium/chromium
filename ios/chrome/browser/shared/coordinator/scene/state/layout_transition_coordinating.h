// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_TRANSITION_COORDINATING_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_TRANSITION_COORDINATING_H_

#import <UIKit/UIKit.h>

// This protocol defines focused methods for layout transitions.
@protocol LayoutTransitionCoordinating <NSObject>

// This method runs the animation alongside the transition.
- (void)animateAlongsideTransition:(void (^)(void))animation
                        completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_TRANSITION_COORDINATING_H_
