// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "ios/chrome/browser/assistant/ui/assistant_container_animatable.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_presentation_context.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"

enum class AssistantContainerDetent : NSInteger;
@protocol AssistantContainerDelegate;
@class BrowserLayoutState;
@class LayoutGuideCenter;
@class SceneLayoutState;

// View Controller for the Assistant Container.
@interface AssistantContainerViewController
    : UIViewController <AssistantContainerAnimatable>

// The available detents for the container. Can't be empty.
@property(nonatomic, assign) std::vector<AssistantContainerDetent> detents;

// The presentation context of the container.
@property(nonatomic, assign) AssistantPresentationContext presentationContext;

// The layout guide name to anchor to. If nil, falls back to the bottom of the
// parent view.
@property(nonatomic, copy) GuideName* guideName;

// The layout guide center used to resolve the guide name.
@property(nonatomic, weak) LayoutGuideCenter* layoutGuideCenter;

// The height to use for the minimized detent. Defaults to
// kAssistantContainerMinimizedDetentHeight.
@property(nonatomic, assign) NSInteger minimizedDetentHeight;

// The delegate for the container events.
@property(nonatomic, weak) id<AssistantContainerDelegate> delegate;

// The browser layout state.
@property(nonatomic, weak) BrowserLayoutState* browserLayoutState;

// The scene layout state.
@property(nonatomic, weak) SceneLayoutState* sceneLayoutState;

// Accessibility property. Whether to only announce the arrival of the assistant
// instead of moving VoiceOver focus to it.
@property(nonatomic, assign) BOOL announceArrivalOnly;

// Sets whether the grabber button is hidden with optional animation.
- (void)setGrabberHidden:(BOOL)hidden animated:(BOOL)animated;

// Returns the height of a given detent, or kInvalidDetentHeight if the detent
// is invalid or height is not yet calculated.
- (NSInteger)heightForDetent:(AssistantContainerDetent)detent;

// Animates the container to a specific detent.
// If the detent is not found, acts as a no-op.
- (void)animateToDetent:(AssistantContainerDetent)detent
               duration:(NSTimeInterval)duration
                  curve:(UIViewAnimationCurve)curve;

// Lays out the container anchored to the active toolbar within the parent view.
- (void)layoutInParentView:(UIView*)parentView;

// Animates the layout state cutout radius alongside sheet transitions.
// `presented` indicates whether the transition is a presentation or a
// dismissal.
- (void)animateAlongsideTransitionPresented:(BOOL)presented;

// Default initializer.
- (instancetype)initWithViewController:(UIViewController*)viewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_CONTROLLER_H_
