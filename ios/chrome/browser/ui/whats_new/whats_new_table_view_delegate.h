// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_DELEGATE_H_

@class WhatsNewItem;
@class WhatsNewTableViewController;

// Delegate protocol to handle communication from the table view to the
// parent coordinator.
@protocol WhatsNewTableViewDelegate

// Invoked when a user interacts with a cell item to create the
// detail view from the main coordinator.
- (void)detailViewController:
            (WhatsNewTableViewController*)whatsNewTableviewController
    openDetailViewControllerForItem:(WhatsNewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_DELEGATE_H_
