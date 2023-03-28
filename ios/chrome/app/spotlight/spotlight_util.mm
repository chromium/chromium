// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_util.h"

#import <CoreSpotlight/CoreSpotlight.h>

#import "base/metrics/histogram_macros.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "url/gurl.h"

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

// Strings corresponding to the domain/prefix for respectively bookmarks,
// top sites and actions items for spotlight.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
NSString* const kSpotlightBookmarkDomain = @"com.google.chrome.bookmarks";
NSString* const kSpotlightBookmarkPrefix = @"com.google.chrome.bookmarks.";

NSString* const kSpotlightTopSitesDomain = @"com.google.chrome.topsites";
NSString* const kSpotlightTopSitesPrefix = @"com.google.chrome.topsites.";

NSString* const kSpotlightActionsDomain = @"com.google.chrome.actions";
NSString* const kSpotlightActionsPrefix = @"com.google.chrome.actions.";

NSString* const kSpotlightReadingListDomain = @"org.chromium.readinglist";
NSString* const kSpotlightReadingListPrefix = @"org.chromium.readinglist.";
#else

NSString* const kSpotlightBookmarkDomain = @"org.chromium.bookmarks";
NSString* const kSpotlightBookmarkPrefix = @"org.chromium.bookmarks.";

NSString* const kSpotlightTopSitesDomain = @"org.chromium.topsites";
NSString* const kSpotlightTopSitesPrefix = @"org.chromium.topsites.";

NSString* const kSpotlightActionsDomain = @"org.chromium.actions";
NSString* const kSpotlightActionsPrefix = @"org.chromium.actions.";

NSString* const kSpotlightReadingListDomain = @"org.chromium.readinglist";
NSString* const kSpotlightReadingListPrefix = @"org.chromium.readinglist.";

#endif

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
// Value is stored in `kSpotlightLastIndexingVersionKey`.
const int kCurrentSpotlightIndexVersion = 3;

Domain SpotlightDomainFromString(NSString* domain) {
  if ([domain hasPrefix:kSpotlightBookmarkPrefix]) {
    return DOMAIN_BOOKMARKS;
  } else if ([domain hasPrefix:kSpotlightTopSitesPrefix]) {
    return DOMAIN_TOPSITES;
  } else if ([domain hasPrefix:kSpotlightActionsPrefix]) {
    return DOMAIN_ACTIONS;
  } else if ([domain hasPrefix:kSpotlightReadingListPrefix]) {
    return DOMAIN_READING_LIST;
  }
  // On normal flow, it is not possible to reach this point. When testing the
  // app, it may be possible though if the app is downgraded.
  NOTREACHED();
  return DOMAIN_UNKNOWN;
}

NSString* StringFromSpotlightDomain(Domain domain) {
  switch (domain) {
    case DOMAIN_BOOKMARKS:
      return kSpotlightBookmarkDomain;
    case DOMAIN_TOPSITES:
      return kSpotlightTopSitesDomain;
    case DOMAIN_ACTIONS:
      return kSpotlightActionsDomain;
    case DOMAIN_READING_LIST:
      return kSpotlightReadingListDomain;
    default:
      // On normal flow, it is not possible to reach this point. When testing
      // the app, it may be possible though if the app is downgraded.
      NOTREACHED();
      return nil;
  }
}

bool IsSpotlightAvailable() {
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

NSString* GetSpotlightCustomAttributeItemID() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return @"ComGoogleChromeItemID";
#else
  return @"OrgChromiumItemID";
#endif
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
