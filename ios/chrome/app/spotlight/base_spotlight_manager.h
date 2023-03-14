// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_BASE_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_BASE_SPOTLIGHT_MANAGER_H_

#import <CoreSpotlight/CoreSpotlight.h>
#import <UIKit/UIKit.h>

#include "ios/chrome/app/spotlight/spotlight_util.h"

class GURL;

namespace favicon {
class LargeIconService;
}

@interface BaseSpotlightManager : NSObject

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                                  domain:(spotlight::Domain)domain
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Refreshes all items that point to `URLToRefresh`, using title `title`, by
// calling spotlightItemsWithURL on given URL. The values of `title` and `URL`
// will be passed to spotlightItemsWithURL.
- (void)refreshItemsWithURL:(const GURL&)URLToRefresh title:(NSString*)title;

// Creates a spotlight item with `itemID`, using the `attributeSet`.
- (CSSearchableItem*)spotlightItemWithItemID:(NSString*)itemID
                                attributeSet:
                                    (CSSearchableItemAttributeSet*)attributeSet;

// Creates spotlight items in the class's domain for `URL`,
// using `favicon` and `defaultTitle`
// Base implementation creates a single item directly using provided arguments
// and expects a non-nil title.
- (NSArray*)spotlightItemsWithURL:(const GURL&)URL
                          favicon:(UIImage*)favicon
                     defaultTitle:(NSString*)defaultTitle;

// Removes all items in the current manager's domain from the Spotlight
// index, then calls `callback` on completion
- (void)clearAllSpotlightItems:(BlockWithError)callback;

// Cancel all large icon pending tasks.
- (void)cancelAllLargeIconPendingTasks;

// Returns the spotlight ID for an item indexing `URL` and `title`.
- (NSString*)spotlightIDForURL:(const GURL&)URL title:(NSString*)title;

// Returns the number of pending large icon query tasks
- (NSUInteger)pendingLargeIconTasksCount;

// Called before the instance is deallocated. This method should be overridden
// by the subclasses and de-activate the instance.
- (void)shutdown NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_BASE_SPOTLIGHT_MANAGER_H_
