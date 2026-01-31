// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_BREADCRUMBS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_BREADCRUMBS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/debugger/composebox_debugger_event.h"

// The UI of the breadcrumbs debugging view.
@interface ComposeboxDebuggerBreadcrumbsViewController : UITableViewController

// Instantiates a new breadcrumbs interface with the given events to be
// displayed.
- (instancetype)initWithEvents:(NSArray<ComposeboxDebuggerEvent*>*)breadcrumbs;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_BREADCRUMBS_VIEW_CONTROLLER_H_
