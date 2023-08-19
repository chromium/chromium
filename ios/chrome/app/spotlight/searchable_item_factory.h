// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_SEARCHABLE_ITEM_FACTORY_H_
#define IOS_CHROME_APP_SPOTLIGHT_SEARCHABLE_ITEM_FACTORY_H_

#import <CoreSpotlight/CoreSpotlight.h>
#import <Foundation/Foundation.h>

#import "ios/chrome/app/spotlight/spotlight_util.h"

namespace favicon {
class LargeIconService;
}

class GURL;
@class CSSearchableItem;

/// A factory class for creating searchable item.
/// It is designed to be called on the main thread, and will dispatch all the
/// callbacks on the main thread. It takes care of fetching a favicon icon and
/// creating a searchable item.
@interface SearchableItemFactory : NSObject

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                                  domain:(spotlight::Domain)domain
                   useTitleInIdentifiers:(BOOL)useTitleInIdentifiers;

// When set to NO, items with same URL but different titles will use the same ID
// in the spotlight index.
@property(nonatomic, assign) BOOL useTitleInIdentifiers;

// Fetch a favicon for a given URL and creates a searchable item that points to
// URLToRefresh and have a given title. The URL should be valid and the
// title should not be empty.
- (void)generateSearchableItem:(const GURL&)URLToRefresh
                         title:(NSString*)title
            additionalKeywords:(NSArray<NSString*>*)keywords
             completionHandler:(void (^)(CSSearchableItem*))completionHandler;

/// Returns a simple searchable item displaying a title, given a 'title'
/// 'itemID' and some 'additionalKeywords'
- (CSSearchableItem*)searchableItem:(NSString*)title
                             itemID:(NSString*)itemID
                 additionalKeywords:(NSArray<NSString*>*)keywords;

// Returns the spotlight ID for an item indexing `URL` and `title`.
- (NSString*)spotlightIDForURL:(const GURL&)URL title:(NSString*)title;

// Returns the spotlight ID for an item indexing URL.
- (NSString*)spotlightIDForURL:(const GURL&)URL;

// Cancel all item generations if there are any.
- (void)cancelItemsGeneration;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_SEARCHABLE_ITEM_FACTORY_H_
