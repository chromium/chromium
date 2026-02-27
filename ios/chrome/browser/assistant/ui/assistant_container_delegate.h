// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DELEGATE_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AssistantContainerViewController;
@class AssistantContainerDetent;

// Describes the presentation context of the Assistant Container.
enum class AssistantPresentationContext {
  // Standard compact-width presentation (e.g., iPhone portrait).
  // The container behaves as a traditional bottom sheet.
  kSheet,
  // Regular-width presentation (e.g., iPad full screen).
  // The container is presented as a side panel.
  kPanel,
};

// Delegate for the Assistant Container to notify embedders of state changes.
@protocol AssistantContainerDelegate <NSObject>
@optional

#pragma mark - Lifecycle Events

// Called before the container's view appears.
- (void)assistantContainer:(AssistantContainerViewController*)container
        willAppearAnimated:(BOOL)animated;

// Called after the container's view has appeared.
- (void)assistantContainer:(AssistantContainerViewController*)container
         didAppearAnimated:(BOOL)animated;

// Called before the container's view disappears.
- (void)assistantContainer:(AssistantContainerViewController*)container
     willDisappearAnimated:(BOOL)animated;

// Called after the container's view has disappeared.
- (void)assistantContainer:(AssistantContainerViewController*)container
      didDisappearAnimated:(BOOL)animated;

#pragma mark - Sizing and Detents

// Called when the container successfully settles on a new detent.
- (void)assistantContainer:(AssistantContainerViewController*)container
           didChangeDetent:(AssistantContainerDetent*)newDetent;

// Called when the container updates its internal height dynamically.
- (void)assistantContainer:(AssistantContainerViewController*)container
           didUpdateHeight:(NSInteger)newHeight;

#pragma mark - Context Changes

// Called when the host environment's presentation context changes.
- (void)assistantContainer:(AssistantContainerViewController*)container
          didChangeContext:(AssistantPresentationContext)newContext;

#pragma mark - Gesture Handling

// Asks the delegate if it should intercept the container's resizing pan
// gesture. This is mainly used to avoid gesture conflicts with the embedder. If
// this returns YES, the container will ignore the gesture. Defaults to NO.
- (BOOL)assistantContainer:(AssistantContainerViewController*)container
    shouldInterceptPanGesture:(UIPanGestureRecognizer*)gesture;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DELEGATE_H_
