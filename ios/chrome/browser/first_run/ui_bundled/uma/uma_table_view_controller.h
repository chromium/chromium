// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_UMA_UMA_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_UMA_UMA_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

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
@interface UMATableViewController : LegacyChromeTableViewController

@property(nonatomic, weak) id<UMATableViewControllerPresentationDelegate>
    presentationDelegate;
// Value of the UMA reporting toggle.
@property(nonatomic, assign) BOOL UMAReportingUserChoice;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_UMA_UMA_TABLE_VIEW_CONTROLLER_H_
