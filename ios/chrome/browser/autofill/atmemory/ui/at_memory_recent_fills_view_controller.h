// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_RECENT_FILLS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_RECENT_FILLS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class AtMemoryRecentFillsViewController;
@class AtMemorySearchItem;

@protocol AtMemoryRecentFillsViewControllerDelegate <NSObject>

// Notifies that the info button was tapped.
- (void)recentFillsViewController:
            (AtMemoryRecentFillsViewController*)viewController
                didTapInfoForItem:(AtMemorySearchItem*)item;

// Notifies that an item was selected.
- (void)recentFillsViewController:
            (AtMemoryRecentFillsViewController*)viewController
                 didSelectContent:(NSString*)content;

@end

@interface AtMemoryRecentFillsViewController : ChromeTableViewController

// Delegate to handle events.
@property(nonatomic, weak) id<AtMemoryRecentFillsViewControllerDelegate>
    delegate;

// The items to display.
@property(nonatomic, copy) NSArray<AtMemorySearchItem*>* items;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_RECENT_FILLS_VIEW_CONTROLLER_H_
