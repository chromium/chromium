// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_PRESENTER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_PRESENTER_H_

#import <Foundation/Foundation.h>

@class AssistantContainerViewController;

// Protocol for an object that can host and present the assistant container.
@protocol AssistantContainerPresenter <NSObject>

// Adds the assistant container view controller to the view hierarchy.
// This cannot be nil.
- (void)addAssistantContainerViewController:
    (AssistantContainerViewController*)assistantContainerViewController;

// Removes the assistant container view controller from the view hierarchy.
- (void)removeAssistantContainerViewController;

// Sets the assistant container visible or hidden.
- (void)setAssistantContainerVisible:(BOOL)visible;

// Sets the assistant panel to active or inactive state.
// Implementations should update visual styling and layout (constraints or
// frames) for the given state.
- (void)setAssistantPanelActive:(BOOL)active;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_PRESENTER_H_
