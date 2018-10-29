// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol BookmarkHomeConsumer;
@class BookmarkHomeSharedState;

namespace ios {
class ChromeBrowserState;
}

// BookmarkHomeMediator manages model interactions for the
// BookmarkHomeViewController.
@interface BookmarkHomeMediator : NSObject

@property(nonatomic, weak) id<BookmarkHomeConsumer> consumer;

- (instancetype)initWithSharedState:(BookmarkHomeSharedState*)sharedState
                       browserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Starts this mediator. Populates the table view model with current data and
// begins listening for backend model updates.
- (void)startMediating;

// Stops mediating and disconnects from backend models.
- (void)disconnect;

// Rebuilds the table view model data for the Bookmarks section.  Deletes any
// existing data first.
- (void)computeBookmarkTableViewData;

// Rebuilds the table view model data for the bookmarks matching the given text.
// Deletes any existing data first.  If no items found, an entry with
// |noResults' message is added to the table.
- (void)computeBookmarkTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults;

// Updates promo cell based on its current visibility.
- (void)computePromoTableViewData;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_MEDIATOR_H_
