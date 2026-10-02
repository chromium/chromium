// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_BRICKS_ANIMATION_BRICK_H_
#define IOS_CHROME_BROWSER_SHARED_UI_BRICKS_ANIMATION_BRICK_H_

#import <UIKit/UIKit.h>

@class AnimationBrickConfiguration;

// A pre-configured Lottie animation view.
@interface AnimationBrick : UIView

- (instancetype)initWithConfiguration:(AnimationBrickConfiguration*)config
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_BRICKS_ANIMATION_BRICK_H_
