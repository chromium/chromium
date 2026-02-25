// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DELEGATE_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AssistantSheetViewController;

// Delegate for the Assistant Container to notify embedders of state changes.
@protocol AssistantContainerDelegate <NSObject>
@optional

#pragma mark - Lifecycle Events

// Called before the container's view appears.
- (void)assistantContainer:(AssistantSheetViewController*)container
        willAppearAnimated:(BOOL)animated;

// Called after the container's view has appeared.
- (void)assistantContainer:(AssistantSheetViewController*)container
         didAppearAnimated:(BOOL)animated;

// Called before the container's view disappears.
- (void)assistantContainer:(AssistantSheetViewController*)container
     willDisappearAnimated:(BOOL)animated;

// Called after the container's view has disappeared.
- (void)assistantContainer:(AssistantSheetViewController*)container
      didDisappearAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DELEGATE_H_
