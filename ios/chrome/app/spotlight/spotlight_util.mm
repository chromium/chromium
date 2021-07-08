// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_util.h"

#import <CoreSpotlight/CoreSpotlight.h>

#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/spotlight/spotlight_provider.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// This enum is used for Histogram. Items should not be removed or reordered and
// this enum should be kept synced with histograms.xml.
// The three states correspond to:
// - SPOTLIGHT_UNSUPPORTED: Framework CoreSpotlight is not found and
// [CSSearchableIndex class] returns nil.
// - SPOTLIGHT_UNAVAILABLE: Framework is loaded but [CSSearchableIndex
// isIndexingAvailable] return NO. Note: It is unclear if this state is
// reachable (I could not find configuration where CoreSpotlight was loaded but
// [CSSearchableIndex isIndexingAvailable] returned NO.
// - SPOTLIGHT_AVAILABLE: Framework is loaded and [CSSearchableIndex
// isIndexingAvailable] returns YES. Note: This does not mean the actual
// indexing will happen. If the user disables Spotlight in the system settings,
// [CSSearchableIndex isIndexingAvailable] still returns YES.
enum Availability {
  SPOTLIGHT_UNSUPPORTED = 0,
  SPOTLIGHT_UNAVAILABLE,
  SPOTLIGHT_AVAILABLE,
  SPOTLIGHT_AVAILABILITY_COUNT
};

// Documentation says that failed deletion should be retried. Set a maximum
// value to avoid infinite loop.
const int kMaxDeletionAttempts = 5;

// Execute blockName block with up to retryCount retries on error. Execute
// callback when done.
void DoWithRetry(BlockWithError callback,
                 NSUInteger retryCount,
                 void (^blockName)(BlockWithError error)) {
  BlockWithError retryCallback = ^(NSError* error) {
    if (error && retryCount > 0) {
      DoWithRetry(callback, retryCount - 1, blockName);
    } else {
      if (callback) {
        callback(error);
      }
    }
  };
  blockName(retryCallback);
}

// Execute blockName block with up to kMaxDeletionAttempts retries on error.
// Execute callback when done.
void DoWithRetry(BlockWithError completion,
                 void (^blockName)(BlockWithError error)) {
  DoWithRetry(completion, kMaxDeletionAttempts, blockName);
}

}  // namespace

namespace spotlight {

// NSUserDefaults key of entry containing date of the latest bookmarks indexing.
const char kSpotlightLastIndexingDateKey[] = "SpotlightLastIndexingDate";

// NSUserDefault key of entry containing Chrome version of the latest bookmarks
// indexing.
const char kSpotlightLastIndexingVersionKey[] = "SpotlightLastIndexingVersion";

// The current version of the Spotlight index format.
// Change this value if there are change int the information indexed in
// Spotlight. This will force reindexation on next startup.
// Value is stored in |kSpotlightLastIndexingVersionKey|.
const int kCurrentSpotlightIndexVersion = 3;

Domain SpotlightDomainFromString(NSString* domain) {
  SpotlightProvider* provider =
      ios::GetChromeBrowserProvider().GetSpotlightProvider();
  if ([domain hasPrefix:[provider->GetBookmarkDomain()
                            stringByAppendingString:@"."]]) {
    return DOMAIN_BOOKMARKS;
  } else if ([domain hasPrefix:[provider->GetTopSitesDomain()
                                   stringByAppendingString:@"."]]) {
    return DOMAIN_TOPSITES;
  } else if ([domain hasPrefix:[provider->GetActionsDomain()
                                   stringByAppendingString:@"."]]) {
    return DOMAIN_ACTIONS;
  }
  // On normal flow, it is not possible to reach this point. When testing the
  // app, it may be possible though if the app is downgraded.
  NOTREACHED();
  return DOMAIN_UNKNOWN;
}

NSString* StringFromSpotlightDomain(Domain domain) {
  SpotlightProvider* provider =
      ios::GetChromeBrowserProvider().GetSpotlightProvider();
  switch (domain) {
    case DOMAIN_BOOKMARKS:
      return provider->GetBookmarkDomain();
    case DOMAIN_TOPSITES:
      return provider->GetTopSitesDomain();
    case DOMAIN_ACTIONS:
      return provider->GetActionsDomain();
    default:
      // On normal flow, it is not possible to reach this point. When testing
      // the app, it may be possible though if the app is downgraded.
      NOTREACHED();
      return nil;
  }
}

void DeleteItemsWithIdentifiers(NSArray* items, BlockWithError callback) {
  void (^deleteItems)(BlockWithError) = ^(BlockWithError errorBlock) {
    [[CSSearchableIndex defaultSearchableIndex]
        deleteSearchableItemsWithIdentifiers:items
                           completionHandler:errorBlock];
  };

  DoWithRetry(callback, deleteItems);
}

void DeleteSearchableDomainItems(Domain domain, BlockWithError callback) {
  void (^deleteItems)(BlockWithError) = ^(BlockWithError errorBlock) {
    [[CSSearchableIndex defaultSearchableIndex]
        deleteSearchableItemsWithDomainIdentifiers:@[ StringFromSpotlightDomain(
                                                       domain) ]
                                 completionHandler:errorBlock];
  };

  DoWithRetry(callback, deleteItems);
}

void ClearAllSpotlightEntries(BlockWithError callback) {
  BlockWithError augmentedCallback = ^(NSError* error) {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:@(kSpotlightLastIndexingDateKey)];
    if (callback) {
      callback(error);
    }
  };

  void (^deleteItems)(BlockWithError) = ^(BlockWithError errorBlock) {
    [[CSSearchableIndex defaultSearchableIndex]
        deleteAllSearchableItemsWithCompletionHandler:errorBlock];
  };

  DoWithRetry(augmentedCallback, deleteItems);
}

bool IsSpotlightAvailable() {
  bool provided = ios::GetChromeBrowserProvider()
                      .GetSpotlightProvider()
                      ->IsSpotlightEnabled();
  if (!provided) {
    // The product does not support Spotlight, do not go further.
    return false;
  }
  bool loaded = !![CSSearchableIndex class];
  bool available = loaded && [CSSearchableIndex isIndexingAvailable];
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    Availability availability = SPOTLIGHT_UNSUPPORTED;
    if (loaded) {
      availability = SPOTLIGHT_UNAVAILABLE;
    }
    if (available) {
      availability = SPOTLIGHT_AVAILABLE;
    }
    UMA_HISTOGRAM_ENUMERATION("IOS.Spotlight.Availability", availability,
                              SPOTLIGHT_AVAILABILITY_COUNT);
  });
  return loaded && available;
}

void ClearSpotlightIndexWithCompletion(BlockWithError completion) {
  DCHECK(IsSpotlightAvailable());
  ClearAllSpotlightEntries(completion);
}

NSString* GetSpotlightCustomAttributeItemID() {
  return ios::GetChromeBrowserProvider()
      .GetSpotlightProvider()
      ->GetCustomAttributeItemID();
}

void GetURLForSpotlightItemID(NSString* itemID, BlockWithNSURL completion) {
  NSString* queryString =
      [NSString stringWithFormat:@"%@ == \"%@\"",
                                 GetSpotlightCustomAttributeItemID(), itemID];

  CSSearchQuery* query =
      [[CSSearchQuery alloc] initWithQueryString:queryString
                                      attributes:@[ @"contentURL" ]];

  [query setFoundItemsHandler:^(NSArray<CSSearchableItem*>* items) {
    if ([items count] == 1) {
      CSSearchableItem* searchableItem = [items objectAtIndex:0];
      if (searchableItem) {
        completion([[searchableItem attributeSet] contentURL]);
        return;
      }
    }
    completion(nil);

  }];

  [query start];
}

}  // namespace spotlight
