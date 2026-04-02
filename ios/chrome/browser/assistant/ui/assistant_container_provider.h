// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_PROVIDER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_PROVIDER_H_

#import <Foundation/Foundation.h>

@class AssistantContainerViewController;

// Protocol for an object that can provide the assistant container to the
// hierarchy.
@protocol AssistantContainerProvider <NSObject>

// Adds the assistant container view controller to the view hierarchy.
// This cannot be nil.
- (void)addAssistantContainerViewController:
    (AssistantContainerViewController*)assistantContainerViewController;

// Removes the assistant container view controller from the view hierarchy.
- (void)removeAssistantContainerViewController;

// Adjusts the side-panel's translation offset.
- (void)updateAssistantContainerOffset:(CGFloat)offset;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_PROVIDER_H_
