// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_COORDINATOR_H_
#define IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class CobrowseContext;

// Coordinator for the "AI Mode" flow.
@interface AssistantAIMCoordinator : ChromeCoordinator

// Designated initializer. The Cobrowse context provides the necessary state to
// initialize the assistant.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   context:(CobrowseContext*)context
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_COORDINATOR_H_
