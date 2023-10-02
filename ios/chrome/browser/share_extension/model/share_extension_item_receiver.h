// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_ITEM_RECEIVER_H_
#define IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_ITEM_RECEIVER_H_

#import <Foundation/Foundation.h>

namespace bookmarks {
class BookmarkModel;
}

class ReadingListModel;

// This class observes the Application group folder
// `app_group::ShareExtensionItemsFolder()` and process the files it contains
// when a new file is created or when application is put in foreground.
@interface ShareExtensionItemReceiver : NSObject

// Initialize the ShareExtensionItemReceiver with the bookmark and reading
// list models to use. `shutdown` must be called before the last reference
// to the object is released.
- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                     readingListModel:(ReadingListModel*)readingListModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Stops observers and pending operations.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_ITEM_RECEIVER_H_
