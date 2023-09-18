// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CONSUMER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CONSUMER_DELEGATE_H_

namespace web {
class WebStateID;
}  // namespace web

// Protocol that the tabstrip UI uses to update the model.
@protocol TabStripConsumerDelegate

// Tells the receiver to insert a new item in the tabstrip.
- (void)addNewItem;

// Tells the receiver to show to the selected tab.
- (void)selectTab:(int)index;

// Tells the receiver to close the item with identifier `itemID`. If there is
// no item with that identifier, no item is closed.
- (void)closeItemWithID:(web::WebStateID)itemID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CONSUMER_DELEGATE_H_
