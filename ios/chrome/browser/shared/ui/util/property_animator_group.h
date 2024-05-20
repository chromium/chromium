// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_PROPERTY_ANIMATOR_GROUP_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_PROPERTY_ANIMATOR_GROUP_H_

#import <UIKit/UIKit.h>

// A group of UIViewPropertyAnimators, which can be run (and stopped) together.
// This class is functionally convenience wrappers around the
// UIViewImplicitlyAnimating API that will map those method calls to all of the
// animators in a group. Since instances of this class conform to that protocol,
// in most cases an animator group can be used where a single property animator
// would be (provided that the call sites want a id<UIViewImplicitlyAnimating>).
// Protocol methods that mutate the receiver are sequentially appiled to all of
// the animators in the group (this include -startAnimation, -stopAnimation:,
// and so on). -addAnimations:, -addCompletion:, and similar methods are just
// applied to the first animator in the group. Methods (and property getters)
// that return a value return the value of the first animator in the group, on
// the assumption that all of them have the same value.
// It is assumed that all of the animators in the group have the same duration
// and delay. This is enforced by -addAnimator:
@interface PropertyAnimatorGroup : NSObject <UIViewImplicitlyAnimating>

// The animators in this group.
@property(nonatomic, readonly) NSArray<UIViewPropertyAnimator*>* animators;

// Adds `animator`, checking that it matches duration and delay with the other
// animators in the group.
- (void)addAnimator:(UIViewPropertyAnimator*)animator;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_PROPERTY_ANIMATOR_GROUP_H_
