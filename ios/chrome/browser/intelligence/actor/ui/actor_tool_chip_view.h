// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_TOOL_CHIP_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_TOOL_CHIP_VIEW_H_

#import <UIKit/UIKit.h>

// Customizable chip component representing an actor tool. The `tintColor`
// property is propagated to both the icon color and text color.
@interface ActorToolChipView : UIView

// Initializes the chip with label text and an icon.
- (instancetype)initWithText:(NSString*)text
                        icon:(UIImage*)icon NS_DESIGNATED_INITIALIZER;

- (instancetype)init;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Updates the chip's label text and icon.
- (void)updateText:(NSString*)text icon:(UIImage*)icon;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_TOOL_CHIP_VIEW_H_
