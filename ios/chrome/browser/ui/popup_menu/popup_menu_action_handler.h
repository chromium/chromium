// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller_delegate.h"

@protocol ApplicationCommands;
@protocol BookmarksCommands;
@protocol BrowserCommands;
@protocol BrowserCoordinatorCommands;
@protocol FindInPageCommands;
@protocol HelpCommands;
@protocol LoadQueryCommands;
@protocol PageInfoCommands;
@protocol PopupMenuActionHandlerDelegate;
@protocol PopupMenuCommands;
@protocol PriceNotificationsCommands;
@protocol QRScannerCommands;
@protocol TextZoomCommands;
class WebNavigationBrowserAgent;

// Handles user interactions with the popup menu.
@interface PopupMenuActionHandler
    : NSObject <PopupMenuTableViewControllerDelegate>

// The view controller that presents the popup menu.
@property(nonatomic, weak) UIViewController* baseViewController;

// Command handler.
@property(nonatomic, weak) id<PopupMenuActionHandlerDelegate> delegate;

// Dispatcher.
@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCommands,
                              FindInPageCommands,
                              LoadQueryCommands,
                              PriceNotificationsCommands,
                              TextZoomCommands>
    dispatcher;

@property(nonatomic, weak) id<BookmarksCommands> bookmarksCommandsHandler;
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorCommandsHandler;
@property(nonatomic, weak) id<PageInfoCommands> pageInfoCommandsHandler;
@property(nonatomic, weak) id<PopupMenuCommands> popupMenuCommandsHandler;
@property(nonatomic, weak) id<QRScannerCommands> qrScannerCommandsHandler;
@property(nonatomic, weak) id<HelpCommands> helpHandler;
@property(nonatomic, assign) WebNavigationBrowserAgent* navigationAgent;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_H_
