// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

// View that contains the visual elements of the Assistant Container.
//
// The layout is structured as follows:
//
// +----------------------------------+
// |             Grabber              |
// +----------------------------------+
// |           scrollView             |
// |  +----------------------------+  |
// |  |        contentView         |  |
// |  |  (Child VC View goes here) |  |
// |  +----------------------------+  |
// +----------------------------------+
@interface AssistantContainerView : UIView

// The content view where subviews should be added.
@property(nonatomic, strong, readonly) UIView* contentView;

// Allows the controller to dynamically morph the container radius.
// Used to animate the container between the minimized and expanded states.
- (void)updateCornerRadius:(CGFloat)cornerRadius
             maskedCorners:(CACornerMask)maskedCorners;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_
