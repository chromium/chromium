// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_DEBUGGER_UI_AIM_DEBUGGER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AIM_DEBUGGER_UI_AIM_DEBUGGER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/aim/debugger/ui/aim_debugger_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol AimDebuggerMutator;

// View Controller for the AIM Eligibility Debugger page.
@interface AimDebuggerViewController
    : ChromeTableViewController <AimDebuggerConsumer>

// The mutator for user actions.
@property(nonatomic, weak) id<AimDebuggerMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_AIM_DEBUGGER_UI_AIM_DEBUGGER_VIEW_CONTROLLER_H_
