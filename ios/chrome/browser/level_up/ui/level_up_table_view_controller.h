// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/level_up/ui/level_up_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class LevelUpTask;
@class LevelUpTableViewController;

// Delegate protocol for table view controller actions.
@protocol LevelUpTableViewControllerDelegate <NSObject>

// Called when the user taps the "See All" button.
- (void)didTapSeeAllTasks:(LevelUpTableViewController*)controller;

@end

// Table View Controller managing the Level Up task cells.
@interface LevelUpTableViewController
    : ChromeTableViewController <LevelUpConsumer>

// The delegate for action notifications.
@property(nonatomic, weak) id<LevelUpTableViewControllerDelegate> delegate;

// Initializes this controller.
- (instancetype)initWithHeaderTitle:(NSString*)headerTitle
                  showsSeeAllButton:(BOOL)showsSeeAllButton
                              style:(UITableViewStyle)style;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TABLE_VIEW_CONTROLLER_H_
