// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_COORDINATOR_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_COORDINATOR_PRIVATE_H_

#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"

@class NewTabPageCoordinator;

// Coordinator for BrowserViewController. This private class extension should
// only be used by the implementation of BrowserCoordinator and tests.
@interface BrowserCoordinator () <ActivityServiceCommands>

// The coordinator used for the New Tab Page.
@property(nonatomic, strong) NewTabPageCoordinator* NTPCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_COORDINATOR_PRIVATE_H_
