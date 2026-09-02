// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_ANIMATION_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_ANIMATION_UTIL_H_

#import <UIKit/UIKit.h>

// Returns an animation group containing the animations in `animations` that has
// the shortest duration necessary for all the animations to finish.
CAAnimation* AnimationGroupMake(NSArray* animations);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_ANIMATION_UTIL_H_
