// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_QUERY_UNSUPPORTED_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_QUERY_UNSUPPORTED_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class AtMemoryQueryUnsupportedViewController;

@protocol AtMemoryQueryUnsupportedViewControllerDelegate <NSObject>
// Notifies that the user tapped the unsupported query cell.
- (void)queryUnsupportedViewControllerDidTapCell:
    (AtMemoryQueryUnsupportedViewController*)viewController;
@end

// View controller for the "Query Unsupported" state of the AtMemory screen.
@interface AtMemoryQueryUnsupportedViewController : ChromeTableViewController

// Delegate for this view controller.
@property(nonatomic, weak) id<AtMemoryQueryUnsupportedViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_QUERY_UNSUPPORTED_VIEW_CONTROLLER_H_
