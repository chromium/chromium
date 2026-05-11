// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ACCESSIBILITY_MANAGER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ACCESSIBILITY_MANAGER_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_grabber_button.h"

// Delegate protocol to handle requests from the accessibility manager.
@protocol AssistantContainerAccessibilityManagerDelegate <NSObject>

// Called when the user triggers a custom action or adjustable action to change
// detent.
- (void)accessibilityManagerDidRequestDetentChange:
    (AssistantContainerDetent)detent;

@end

// Manages accessibility properties and actions for the Assistant Container.
@interface AssistantContainerAccessibilityManager
    : NSObject <AssistantGrabberButtonAccessibilityDelegate>

- (instancetype)
    initWithGrabberButton:(AssistantGrabberButton*)grabberButton
                 delegate:(id<AssistantContainerAccessibilityManagerDelegate>)
                              delegate;

// Updates the value and custom actions based on the current state.
- (void)updateAccessibilityPropertiesWithCurrentDetent:
            (AssistantContainerDetent)currentDetent
                                      availableDetents:
                                          (const std::vector<
                                              AssistantContainerDetent>&)
                                              detents;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_ACCESSIBILITY_MANAGER_H_
