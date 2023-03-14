// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_OPTIONAL_PROPERTY_ANIMATOR_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_OPTIONAL_PROPERTY_ANIMATOR_H_

#import <UIKit/UIKit.h>

// UIViewPropertyAnimators throw exceptions if they're started before any
// animation blocks were added.  OptionalPropertyAnimators can be used in
// scenarios when it is not guaranteed that animation blocks will be added (i.e.
// animation blocks provided by observers).
@interface OptionalPropertyAnimator : UIViewPropertyAnimator

// Whether animations have been added to this animator.  `-startAnimation` and
// `-startAnimationAfterDelay:` are no-ops if this property is NO.
@property(nonatomic, readonly) BOOL hasAnimations;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_OPTIONAL_PROPERTY_ANIMATOR_H_
