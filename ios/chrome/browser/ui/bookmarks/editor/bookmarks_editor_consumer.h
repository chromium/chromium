// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_CONSUMER_H_

#import <UIKit/UIKit.h>

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Consumer allowing the Bookmarks Editor View Controller to be
// updated when its bookmark is updated.
@protocol BookmarksEditorConsumer <NSObject>

// Update the UI name and URL using current `bookmark` value.
- (void)updateUIFromBookmark;

// Update the UI’s folder using current `folder` value.
- (void)updateFolderLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_CONSUMER_H_
