// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol BrowserCommands;

// Displays the send tab to self UI for all device form factors. Will show a
// modal dialog popup on both platforms. Once this coordinator is stopped, the
// underlying dialog is dismissed.
@interface SendTabToSelfCoordinator : ChromeCoordinator

// Dispatcher
@property(nonatomic, weak) id<BrowserCommands> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_COORDINATOR_H_
