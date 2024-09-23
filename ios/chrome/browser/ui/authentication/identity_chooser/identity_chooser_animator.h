// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Animator, taking care of the animation of the IdentityChooser.
@interface IdentityChooserAnimator
    : NSObject<UIViewControllerAnimatedTransitioning>

// Whether the IdentityChooser is `appearing`.
@property(nonatomic, assign) BOOL appearing;

// Origin of the animation, in window coordinates. Only user if `appearing` is
// true. Not user if equals to CGPointZero.
@property(nonatomic, assign) CGPoint origin;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_ANIMATOR_H_
