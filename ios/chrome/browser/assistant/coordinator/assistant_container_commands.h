// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_CONTAINER_COMMANDS_H_
#define IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_CONTAINER_COMMANDS_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@protocol AssistantContainerDelegate;
@class AssistantContainerDetent;

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

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_CONTAINER_COMMANDS_H_
