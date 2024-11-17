// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_LAST_VISITED_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_LAST_VISITED_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/page_info/page_info_last_visited_view_controller_delegate.h"

@protocol PageInfoCommands;

@interface PageInfoLastVisitedViewController : BaseHistoryViewController

// Delegate for this HistoryTableView.
@property(nonatomic, weak) id<PageInfoLastVisitedViewControllerDelegate>
    lastVisitedDelegate;
// Handler for actions related to the entire Page Info UI such as showing or
// dismissing the entire UI.
@property(nonatomic, weak) id<PageInfoCommands> pageInfoCommandsHandler;

// Initializers.
- (instancetype)initWithHostName:(NSString*)hostName NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_LAST_VISITED_VIEW_CONTROLLER_H_
