// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ANIMATABLE_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ANIMATABLE_H_

#import <UIKit/UIKit.h>

// Protocol exposing properties of the AssistantContainerViewController
// necessary for custom transition animations.
@protocol AssistantContainerAnimatable <NSObject>

// Whether the container is currently being animated.
@property(nonatomic, assign) BOOL isAnimating;

// The background dimming view. Used for alpha transitions.
@property(nonatomic, readonly) UIView* dimmingView;

// The main view that holds the child. Used for translating the sheet.
@property(nonatomic, readonly) UIView* assistantContainerView;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ANIMATABLE_H_
