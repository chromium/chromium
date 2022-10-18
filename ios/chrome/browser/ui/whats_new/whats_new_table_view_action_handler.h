// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_ACTION_HANDLER_H_

@class WhatsNewItem;

// Delegate protocol to handle communication from the table view to the
// mediator.
@protocol WhatsNewTableViewActionHandler

// Invoked when a user interacts with a cell item (`WhatsNewItem`) in What's
// New to record user actions.
- (void)recordWhatsNewInteraction:(WhatsNewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_ACTION_HANDLER_H_