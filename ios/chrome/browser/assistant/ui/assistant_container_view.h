// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

@class AssistantGrabberButton;

// View that contains the visual elements of the Assistant Container.
//
// The layout is structured as follows:
//
// +----------------------------------+
// |             Grabber              |
// +----------------------------------+
// |           contentView            |
// |    (Child VC View goes here)     |
// +----------------------------------+
@interface AssistantContainerView : UIView

// The grabber button used to minimize and expand the sheet.
@property(nonatomic, strong, readonly) AssistantGrabberButton* grabberButton;

// The content view where subviews should be added.
@property(nonatomic, strong, readonly) UIView* contentView;

// Allows the controller to dynamically morph the container radius.
// Used to animate the container between the minimized and expanded states.
- (void)updateTopCornerRadius:(CGFloat)topCornerRadius
           bottomCornerRadius:(CGFloat)bottomCornerRadius;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_
