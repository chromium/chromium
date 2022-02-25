// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@class UMATableViewController;
@protocol UMATableViewControllerModelDelegate;

// Delegate for UMATableViewController, to receive events when the view
// controller is dismissed.
@protocol UMATableViewControllerPresentationDelegate <NSObject>

// Called when the view controller is dismissed, using the "Done" button.
- (void)UMATableViewControllerDidDismiss:
    (UMATableViewController*)viewController;

@end

// View controller for the UMA dialog.
@interface UMATableViewController : ChromeTableViewController

@property(nonatomic, weak) id<UMATableViewControllerModelDelegate>
    modelDelegate;
@property(nonatomic, weak) id<UMATableViewControllerPresentationDelegate>
    presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_TABLE_VIEW_CONTROLLER_H_
