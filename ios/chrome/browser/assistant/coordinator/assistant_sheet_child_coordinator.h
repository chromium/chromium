// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_SHEET_CHILD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_SHEET_CHILD_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/ui/assistant_bar_configuration.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AssistantCommands;

// Base coordinator for child coordinators of the assistant sheet.
@interface AssistantSheetChildCoordinator : ChromeCoordinator

// The view controller managed by this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

// The bar configuration for the child coordinator.
@property(nonatomic, strong, readonly)
    AssistantBarConfiguration* barConfiguration;

// Handler for assistant commands.
@property(nonatomic, weak) id<AssistantCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_SHEET_CHILD_COORDINATOR_H_
