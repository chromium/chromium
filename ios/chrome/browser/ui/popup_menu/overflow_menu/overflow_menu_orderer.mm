// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu//overflow_menu/overflow_menu_orderer.h"

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Sorts badged destinations using a local heuristic when the usage history
// isn't available (e.g. when on an incognito tab). Destionations that need
// highlight and that are at a position of kNewDestinationsInsertionIndex
// or worst are re-inserted at kNewDestinationsInsertionIndex. Destionations
// that are at a better position than kNewDestinationsInsertionIndex aren't
// moved.
NSArray<OverflowMenuDestination*>* SortBadgedDestinations(
    NSArray<OverflowMenuDestination*>* carouselDestinations) {
  NSMutableSet<OverflowMenuDestination*>* destinationsToSort =
      [NSMutableSet setWithArray:carouselDestinations];
  NSMutableArray<OverflowMenuDestination*>* sortedDestinations =
      [NSMutableArray array];

  // Keep the ranking of badged destination as is up to
  // kNewDestinationsInsertionIndex and keep the ranking as is for all
  // destinations without a badge.
  for (OverflowMenuDestination* destination in carouselDestinations) {
    const bool dontSort =
        [sortedDestinations count] < kNewDestinationsInsertionIndex ||
        destination.badge == BadgeTypeNone;
    if (dontSort) {
      [destinationsToSort removeObject:destination];
      [sortedDestinations addObject:destination];
    }
  }

  // Put the destinations with non-error badges in the middle.
  for (OverflowMenuDestination* destination in carouselDestinations) {
    if ([destinationsToSort containsObject:destination] &&
        destination.badge != BadgeTypeError) {
      [destinationsToSort removeObject:destination];
      [sortedDestinations insertObject:destination
                               atIndex:kNewDestinationsInsertionIndex];
    }
  }

  // Put the destinations with error badges in the middle before the
  // destinations with non-error badges.
  for (OverflowMenuDestination* destination in carouselDestinations) {
    if ([destinationsToSort containsObject:destination]) {
      [destinationsToSort removeObject:destination];
      [sortedDestinations insertObject:destination
                               atIndex:kNewDestinationsInsertionIndex];
    }
  }

  // Verify that all the carousel destinations are in the sorted result.
  DCHECK_EQ([destinationsToSort count], 0u);
  DCHECK_EQ([sortedDestinations count], [carouselDestinations count]);

  return sortedDestinations;
}
}  // namespace

@interface OverflowMenuOrderer ()

// The destination usage history, which (1) tracks which items from the carousel
// are clicked, and (2) suggests a sorted order for carousel menu items.
@property(nonatomic, strong) DestinationUsageHistory* destinationUsageHistory;

@end

@implementation OverflowMenuOrderer {
  BOOL _isIncognito;
}

- (instancetype)initWithIsIncognito:(BOOL)isIncognito {
  if (self = [super init]) {
    _isIncognito = isIncognito;
  }
  return self;
}

- (void)disconnect {
  [self.destinationUsageHistory stop];
  self.destinationUsageHistory = nil;
}

#pragma mark - Property Setters/Getters

- (void)setLocalStatePrefs:(PrefService*)localStatePrefs {
  _localStatePrefs = localStatePrefs;

  if (!_isIncognito) {
    self.destinationUsageHistory =
        [[DestinationUsageHistory alloc] initWithPrefService:localStatePrefs];
    self.destinationUsageHistory.visibleDestinationsCount =
        self.visibleDestinationsCount;
    [self.destinationUsageHistory start];
  }
}

- (void)setVisibleDestinationsCount:(int)visibleDestinationsCount {
  _visibleDestinationsCount = visibleDestinationsCount;
  self.destinationUsageHistory.visibleDestinationsCount =
      self.visibleDestinationsCount;
}

#pragma mark - Public

- (void)recordClickForDestination:(overflow_menu::Destination)destination {
  [self.destinationUsageHistory recordClickForDestination:destination];
}

- (NSArray<OverflowMenuDestination*>*)
    sortedDestinationsFromCarouselDestinations:
        (NSArray<OverflowMenuDestination*>*)carouselDestinations {
  if (self.destinationUsageHistory) {
    return [self.destinationUsageHistory
        sortedDestinationsFromCarouselDestinations:carouselDestinations];
  } else {
    return SortBadgedDestinations(carouselDestinations);
  }
}

@end
