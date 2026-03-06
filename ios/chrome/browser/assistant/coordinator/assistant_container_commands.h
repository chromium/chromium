// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_CONTAINER_COMMANDS_H_
#define IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_CONTAINER_COMMANDS_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"

enum class AssistantContainerDetent : NSInteger;

// Commands to control the Assistant Container.
@protocol AssistantContainerCommands <NSObject>

// Requests the container to present the given view controller.
// The container will strongly retain `viewController` while it is presented.
- (void)showAssistantContainerWithContent:(UIViewController*)viewController
                                 delegate:
                                     (id<AssistantContainerDelegate>)delegate;

// Programmatically requests the container to be closed.
- (void)dismissAssistantContainerAnimated:(BOOL)animated
                               completion:(ProceduralBlock)completion;

// Sets the available detents for the container.
// The container dynamically adjusts its height within the range defined by
// these values. Can't be empty.
- (void)setAssistantContainerDetents:
    (std::vector<AssistantContainerDetent>)detents;

// Animates the container to a specific detent.
// If the detent is not found, acts as a no-op.
- (void)animateAssistantContainerToDetent:(AssistantContainerDetent)detent
                                 duration:(NSTimeInterval)duration
                                    curve:(UIViewAnimationCurve)curve;

// Sets the minimized detent height of the Assistant Container.
- (void)setAssistantContainerMinimizedDetentHeight:(NSInteger)height;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_CONTAINER_COMMANDS_H_
