// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "ios/chrome/browser/assistant/ui/assistant_container_animatable.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_presentation_context.h"

@protocol AssistantContainerDelegate;
enum class AssistantContainerDetent : NSInteger;
@class LayoutState;

// View Controller for the Assistant Container.
@interface AssistantContainerViewController
    : UIViewController <AssistantContainerAnimatable>

// The available detents for the container. Can't be empty.
@property(nonatomic, assign) std::vector<AssistantContainerDetent> detents;

// The presentation context of the container.
@property(nonatomic, assign) AssistantPresentationContext presentationContext;

// The view to anchor to. If nil, falls back to the bottom of the parent view.
@property(nonatomic, weak) UIView* anchorView;

// The height to use for the minimized detent. Defaults to
// kAssistantContainerMinimizedDetentHeight.
@property(nonatomic, assign) NSInteger minimizedDetentHeight;

// The delegate for the container events.
@property(nonatomic, weak) id<AssistantContainerDelegate> delegate;

// The layout state.
@property(nonatomic, weak) LayoutState* layoutState;

// Accessibility property. Whether to only announce the arrival of the assistant
// instead of moving VoiceOver focus to it.
@property(nonatomic, assign) BOOL announceArrivalOnly;

// Animates the container to a specific detent.
// If the detent is not found, acts as a no-op.
- (void)animateToDetent:(AssistantContainerDetent)detent
               duration:(NSTimeInterval)duration
                  curve:(UIViewAnimationCurve)curve;

// Default initializer.
- (instancetype)initWithViewController:(UIViewController*)viewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_CONTROLLER_H_
