// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_SHEET_CHILD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_SHEET_CHILD_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/ui/assistant_navbar_configuration.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Base coordinator for child coordinators of the Assistant Sheet.
@interface AssistantSheetChildCoordinator : ChromeCoordinator

// The view controller managed by this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

// The navigation bar configuration for the child coordinator.
@property(nonatomic, strong, readonly)
    AssistantNavbarConfiguration* navbarConfiguration;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_SHEET_CHILD_COORDINATOR_H_
