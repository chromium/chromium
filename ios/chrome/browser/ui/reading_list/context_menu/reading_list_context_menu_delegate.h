// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_CONTEXT_MENU_READING_LIST_CONTEXT_MENU_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_CONTEXT_MENU_READING_LIST_CONTEXT_MENU_DELEGATE_H_

@class ReadingListContextMenuParams;

// Delegate to handle commands issued from the context menu shown for reading
// list items.
@protocol ReadingListContextMenuDelegate
// Opens |param|'s online URL in a new tab.
- (void)openURLInNewTabForContextMenuWithParams:
    (ReadingListContextMenuParams*)params;
// Opens |param|'s online in a new incognito tab.
- (void)openURLInNewIncognitoTabForContextMenuWithParams:
    (ReadingListContextMenuParams*)params;
// Opens |param|'s online URL in a new window.
- (void)openURLInNewWindowForContextMenuWithParams:
    (ReadingListContextMenuParams*)params;
// Copies |param|'s online URL to the pasteboard.
- (void)copyURLForContextMenuWithParams:(ReadingListContextMenuParams*)params;
// Opens the offline page at |offlineURL| in a new tab.
- (void)openOfflineURLInNewTabForContextMenuWithParams:
    (ReadingListContextMenuParams*)params;
@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_CONTEXT_MENU_READING_LIST_CONTEXT_MENU_DELEGATE_H_
