// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_mediator_consumer.h"

@protocol WhatsNewTableViewDelegate;
@protocol WhatsNewTableViewActionHandler;

// View controller that displays What's New features and chrome tips in a table
// view.
@interface WhatsNewTableViewController
    : LegacyChromeTableViewController <WhatsNewMediatorConsumer>

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// The delegate object that manages interactions with What's New table view.
@property(nonatomic, weak) id<WhatsNewTableViewActionHandler> actionHandler;

// The delegate object to the parent coordinator (`WhatsNewCoordinator`).
@property(nonatomic, weak) id<WhatsNewTableViewDelegate> delegate;

- (void)reloadData;

@end
#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_CONTROLLER_H_
