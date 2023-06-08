// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu//overflow_menu/overflow_menu_orderer.h"

#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The dictionary key used for storing rankings.
const char kRankingKey[] = "ranking";

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

// Ingests base::Value::List of destination names (strings) (`from` list),
// converts each string to an overflow_menu::Destination, then appends each
// destination to a vector (`to` vector). Skips over invalid or malformed list
// items.
void AppendDestinationsToVector(const base::Value::List& from,
                                std::vector<overflow_menu::Destination>& to) {
  for (const auto& value : from) {
    if (!value.is_string()) {
      continue;
    }

    to.push_back(overflow_menu::DestinationForStringName(value.GetString()));
  }
}
}  // namespace

@interface OverflowMenuOrderer ()

// The destination usage history, which (1) tracks which items from the carousel
// are clicked, and (2) suggests a sorted order for carousel menu items.
@property(nonatomic, strong) DestinationUsageHistory* destinationUsageHistory;

@end

@implementation OverflowMenuOrderer {
  // Whether the current menu is for an incognito page.
  BOOL _isIncognito;

  // The current ranking of the destinations.
  DestinationRanking _ranking;
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
    [self loadDataFromPrefs];
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
    return [self destinationHistorySortedDestinationsFromCarouselDestinations:
                     carouselDestinations];
  } else {
    return SortBadgedDestinations(carouselDestinations);
  }
}

#pragma mark - Private

- (void)loadDataFromPrefs {
  // First try to load new pref.
  const base::Value::List& storedRanking =
      _localStatePrefs->GetList(prefs::kOverflowMenuDestinationsOrder);
  if (storedRanking.size() > 0) {
    AppendDestinationsToVector(storedRanking, _ranking);
    return;
  }

  // Fall back to old key.
  ScopedDictPrefUpdate storedUsageHistoryUpdate(
      _localStatePrefs, prefs::kOverflowMenuDestinationUsageHistory);
  base::Value::List* oldRanking =
      storedUsageHistoryUpdate->FindList(kRankingKey);
  if (!oldRanking) {
    return;
  }
  base::Value::List& oldRankingRef = *oldRanking;
  _localStatePrefs->SetList(prefs::kOverflowMenuDestinationsOrder,
                            oldRankingRef.Clone());

  AppendDestinationsToVector(oldRankingRef, _ranking);
  storedUsageHistoryUpdate->Remove(kRankingKey);
}

- (void)flushToPrefs {
  if (!_localStatePrefs) {
    return;
  }

  // Flush the new ranking to Prefs.
  base::Value::List ranking;

  for (overflow_menu::Destination destination : _ranking) {
    ranking.Append(overflow_menu::StringNameForDestination(destination));
  }

  _localStatePrefs->SetList(prefs::kOverflowMenuDestinationsOrder,
                            std::move(ranking));
}

- (NSArray<OverflowMenuDestination*>*)
    destinationHistorySortedDestinationsFromCarouselDestinations:
        (NSArray<OverflowMenuDestination*>*)carouselDestinations {
  _ranking = [self.destinationUsageHistory
      sortedDestinationsFromCurrentRanking:_ranking
                      carouselDestinations:carouselDestinations];

  [self flushToPrefs];

  // Maintain a map from overflow_menu::Destination : OverflowMenuDestination*
  // for fast retrieval of a given overflow_menu::Destination's corresponding
  // Objective-C class.
  std::map<overflow_menu::Destination, OverflowMenuDestination*> destinations;
  for (OverflowMenuDestination* carouselDestination in carouselDestinations) {
    overflow_menu::Destination destination =
        static_cast<overflow_menu::Destination>(
            carouselDestination.destination);
    destinations[destination] = carouselDestination;
  }

  // Reconstruct the correct array to return from `_ranking`.
  NSMutableArray<OverflowMenuDestination*>* sortedDestinations =
      [[NSMutableArray alloc] init];
  for (overflow_menu::Destination destination : _ranking) {
    [sortedDestinations addObject:destinations.at(destination)];
    destinations.erase(destination);
  }

  return sortedDestinations;
}

@end
