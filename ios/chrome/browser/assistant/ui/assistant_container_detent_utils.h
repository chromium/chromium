// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DETENT_UTILS_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DETENT_UTILS_H_

#import <UIKit/UIKit.h>

@class AssistantContainerDetent;

// Detent identifier for the medium detent (50%).
extern NSString* const kAssistantContainerMediumDetentIdentifier;

// Detent identifier for the large detent (100%).
extern NSString* const kAssistantContainerLargeDetentIdentifier;

// Detent identifier for a minimized fixed detent.
extern NSString* const kAssistantContainerMinimizedDetentIdentifier;

// Returns a detent that snaps to 50% of the base view's height.
AssistantContainerDetent* AssistantContainerMediumDetent(UIView* baseView);

// Returns a detent that snaps to 100% of the base view's height.
AssistantContainerDetent* AssistantContainerLargeDetent(UIView* baseView);

// Returns a detent with a fixed height.
AssistantContainerDetent* AssistantContainerFixedDetent(NSInteger height,
                                                        NSString* identifier);

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_DETENT_UTILS_H_
