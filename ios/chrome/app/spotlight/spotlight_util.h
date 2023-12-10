// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_UTIL_H_
#define IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_UTIL_H_

#import <UIKit/UIKit.h>

typedef void (^BlockWithError)(NSError*);
typedef void (^BlockWithNSURL)(NSURL*);

namespace spotlight {

// This enum is used for Histogram. Domains should not be removed or reordered
// and this enum should be kept synced with histograms.xml.
// DOMAIN_UNKNOWN may be reported if Spotlight is not synced with chrome and
// a domain has been removed since last indexation (should not happen in stable
// channel).
enum Domain {
  DOMAIN_UNKNOWN = 0,
  DOMAIN_BOOKMARKS = 1,
  DOMAIN_TOPSITES = 2,
  DOMAIN_ACTIONS = 3,
  DOMAIN_READING_LIST = 4,
  DOMAIN_OPEN_TABS = 5,
  kMaxValue = DOMAIN_OPEN_TABS
};

// The key of a custom attribute containing the item ID so the item is
// searchable using CSSearchQuery.
NSString* GetSpotlightCustomAttributeItemID();

// NSUserDefaults key of entry containing date of the latest bookmarks indexing.
extern const char kSpotlightLastIndexingDateKey[];

// The current version of the Spotlight index format.
// Change this value if there are change int the information indexed in
// Spotlight. This will force reindexation on next startup.
// Value is stored in `kSpotlightLastIndexingVersionKey`.
extern const int kCurrentSpotlightIndexVersion;

// NSUserDefault key of entry containing Chrome version of the latest bookmarks
// indexing.
extern const char kSpotlightLastIndexingVersionKey[];

// Maximum retry attempts to delete/index a set of items.
const NSUInteger kMaxAttempts = 5;

// Converts the spotlight::Domain enum to Spotlight domain string
NSString* StringFromSpotlightDomain(Domain domain);

// Converts the Spotlight domain string to spotlight::Domain enum.
Domain SpotlightDomainFromString(NSString* domain);

// Return the source label for an item from the spotlight::Domain
NSString* SpotlightItemSourceLabelFromDomain(Domain domain);

// Returns whether Spotlight is available on the device. Must be tested before
// calling other methods of this class.
bool IsSpotlightAvailable();

// Finds the Spoglight itemID and calls `completion` with the corresponding URL.
// Calls `completion` with nil if none was found.
// `completion` is called on the Spotlight Thread.
void GetURLForSpotlightItemID(NSString* itemID, BlockWithNSURL completion);

}  // namespace spotlight

#endif  // IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_UTIL_H_
