// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_BOOKMARK_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_BOOKMARK_ACTIVITY_H_

#import <UIKit/UIKit.h>

namespace bookmarks {
class BookmarkModel;
}

@protocol BookmarksCommands;
class GURL;
class PrefService;

// Activity that adds the page to bookmarks.
@interface BookmarkActivity : UIActivity

// Initializes the bookmark activity with a page's `URL` and `title`. The
// `bookmarkModel` to verify if the page has already been bookmarked or not. The
// `handler` is used to add the page to the bookmarks. The `prefService` is used
// to verify if the user can edit their bookmarks or not.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                    handler:(id<BookmarksCommands>)handler
                prefService:(PrefService*)prefService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_BOOKMARK_ACTIVITY_H_
