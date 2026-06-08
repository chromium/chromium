// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_BREADCRUMBS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_BREADCRUMBS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class AimSRPDebuggerEvent;

// The UI of the AIM SRP message logs debugging view.
@interface AimSRPDebuggerBreadcrumbsViewController : UITableViewController

// Instantiates a new logs interface with the given events to be displayed.
- (instancetype)initWithEvents:(NSArray<AimSRPDebuggerEvent*>*)events;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_BREADCRUMBS_VIEW_CONTROLLER_H_
