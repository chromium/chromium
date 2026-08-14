// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_TASK_BUTTON_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_TASK_BUTTON_H_

#import <UIKit/UIKit.h>

// A custom interactive button featuring a gradient background, configurable
// left favicon/logo, a fixed trailing action icon, and a title/subtitle pair.
@interface ActuationTaskButton : UIControl

// The title text. Supports 1 line.
@property(nonatomic, copy) NSString* title;

// The subtitle text. Supports 1 line.
@property(nonatomic, copy) NSString* subtitle;

// The icon image (e.g., website favicon).
@property(nonatomic, strong) UIImage* icon;

// Sets the background gradient start and end colors. Passing nil for both
// resets the button to the default Gemini blue gradient tint.
- (void)setBackgroundGradientStartColor:(UIColor*)startColor
                               endColor:(UIColor*)endColor;

// Designated initializer to configure primary properties on creation.
- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                         icon:(UIImage*)icon NS_DESIGNATED_INITIALIZER;

// Convenience initializer. Delegates to the designated initializer.
- (instancetype)init;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_TASK_BUTTON_H_
