// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_CONTAINER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_CONTAINER_COORDINATOR_H_

#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the Assistant Container.
@interface AssistantContainerCoordinator
    : ChromeCoordinator <AssistantContainerCommands>

// Stops the coordinator with optional animation and completion handler.
- (void)stopAnimated:(BOOL)animated completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_CONTAINER_COORDINATOR_H_
