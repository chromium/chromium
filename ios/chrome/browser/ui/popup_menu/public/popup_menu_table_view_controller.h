// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_consumer.h"

@protocol PopupMenuItem;
@protocol PopupMenuMetricsHandler;
@protocol PopupMenuTableViewControllerDelegate;

// TableViewController for the popup menu.
@interface PopupMenuTableViewController
    : LegacyChromeTableViewController <PopupMenuConsumer>

// Delegate for this consumer.
@property(nonatomic, weak) id<PopupMenuTableViewControllerDelegate> delegate;

// The model of this controller.
@property(nonatomic, readonly, strong)
    TableViewModel<TableViewItem<PopupMenuItem>*>* tableViewModel;

// Presenting ViewController for the ViewController needing to be presented as
// result of an interaction with the popup.
@property(nonatomic, weak) UIViewController* baseViewController;

// Metrics handler for tracking events happening in this view controller.
@property(nonatomic, weak) id<PopupMenuMetricsHandler> metricsHandler;

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_TABLE_VIEW_CONTROLLER_H_
