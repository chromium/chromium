// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTEXT_MENU_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTEXT_MENU_DELEGATE_H_

#import <Foundation/Foundation.h>

class GURL;

// Methods used to create context menu actions for tab strip items.
@protocol TabStripContextMenuDelegate <NSObject>

// Tells the delegate to add `URL` and `title` to the reading list.
- (void)addToReadingListURL:(const GURL&)URL title:(NSString*)title;

// Tells the delegate to create a bookmark for `URL` with `title`.
- (void)bookmarkURL:(const GURL&)URL title:(NSString*)title;

// Tells the delegate to edit the bookmark for `URL`.
- (void)editBookmarkWithURL:(const GURL&)URL;

// Tells the delegate to pin a tab with the item identifier `identifier`.
- (void)pinTabWithIdentifier:(NSString*)identifier;

// Tells the delegate to unpin a tab with the item identifier `identifier`.
- (void)unpinTabWithIdentifier:(NSString*)identifier;

// Tells the delegate to close the tab with the item identifier `identifier`.
- (void)closeTabWithIdentifier:(NSString*)identifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTEXT_MENU_DELEGATE_H_
