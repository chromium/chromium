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

// Ingests base::Value::List of destination names (strings) (`from` list),
// converts each string to an overflow_menu::Destination, then adds each
// destination to a set (`to` set). Skips over invalid or malformed list
// items.
void AddDestinationsToSet(const base::Value::List& from,
                          std::set<overflow_menu::Destination>& to) {
  for (const auto& value : from) {
    if (!value.is_string()) {
      continue;
    }

    to.insert(overflow_menu::DestinationForStringName(value.GetString()));
  }
}

// Inserts `destination` in `output` at kNewDestinationsInsertionIndex and
// clear `destination` from the `destinationsToAdd` set.
void InsertDestination(overflow_menu::Destination destination,
                       std::map<overflow_menu::Destination,
                                OverflowMenuDestination*>* destinationsToAdd,
                       NSMutableArray<OverflowMenuDestination*>* output) {
  const NSUInteger insertionIndex =
      std::min([output count] - 1,
               static_cast<NSUInteger>(kNewDestinationsInsertionIndex));

  [output insertObject:destinationsToAdd->at(destination)
               atIndex:insertionIndex];

  destinationsToAdd->erase(destination);
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

  // New destinations recently added to the overflow menu carousel that have not
  // yet been clicked by the user.
  std::set<overflow_menu::Destination> _untappedDestinations;
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
  _untappedDestinations.erase(destination);

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
  // Fetch the stored list of newly-added, unclicked destinations, then update
  // `_untappedDestinations` with its data.
  AddDestinationsToSet(
      _localStatePrefs->GetList(prefs::kOverflowMenuNewDestinations),
      _untappedDestinations);

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

  // Flush the new untapped destinations to Prefs.
  ScopedListPrefUpdate untappedDestinationsUpdate(
      _localStatePrefs, prefs::kOverflowMenuNewDestinations);

  untappedDestinationsUpdate->clear();

  for (overflow_menu::Destination untappedDestination : _untappedDestinations) {
    untappedDestinationsUpdate->Append(
        overflow_menu::StringNameForDestination(untappedDestination));
  }
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

  // Detect new destinations added to the carousel by feature teams. New
  // destinations (`newDestinations`) are those now found in the carousel
  // (`currentDestinations`), but not found in the ranking
  // (`existingDestinations`).
  std::set<overflow_menu::Destination> currentDestinations;

  for (OverflowMenuDestination* carouselDestination in carouselDestinations) {
    overflow_menu::Destination destination =
        static_cast<overflow_menu::Destination>(
            carouselDestination.destination);
    currentDestinations.insert(destination);

    destinations[destination] = carouselDestination;
  }

  std::set<overflow_menu::Destination> existingDestinations(_ranking.begin(),
                                                            _ranking.end());

  std::vector<overflow_menu::Destination> newDestinations;

  std::set_difference(currentDestinations.begin(), currentDestinations.end(),
                      existingDestinations.begin(), existingDestinations.end(),
                      std::inserter(newDestinations, newDestinations.end()));

  for (overflow_menu::Destination newDestination : newDestinations) {
    _untappedDestinations.insert(newDestination);
  }

  NSMutableArray<OverflowMenuDestination*>* sortedDestinations =
      [[NSMutableArray alloc] init];

  // Reconstruct carousel based on current ranking.
  //
  // Add all ranked destinations that don't need be re-sorted back-to-back
  // following their ranking.
  //
  // Destinations that need to be re-sorted for highlight are not added here
  // where they are re-inserted later. These destinations have a badge and a
  // position of kNewDestinationsInsertionIndex or worst.
  for (overflow_menu::Destination rankedDestination : _ranking) {
    if (destinations.contains(rankedDestination) &&
        !_untappedDestinations.contains(rankedDestination)) {
      const bool dontSort =
          destinations[rankedDestination].badge == BadgeTypeNone ||
          [sortedDestinations count] < kNewDestinationsInsertionIndex;

      if (dontSort) {
        [sortedDestinations addObject:destinations[rankedDestination]];

        destinations.erase(rankedDestination);
      }
    }
  }

  // `-calculateNewRanking` excludes any
  // destinations in `_untappedDestinations` from its result, so new, untapped
  // destinations must be added to `sortedDestinations` as a separate step. New,
  // untapped destinations are inserted into the carousel starting at position
  // `kNewDestinationsInsertionIndex`. Destinations that already have a badge
  // are inserted in another step where they are inserted before the untapped
  // destinations that don't have badges.
  if (!_untappedDestinations.empty()) {
    for (overflow_menu::Destination untappedDestination :
         _untappedDestinations) {
      if (destinations.contains(untappedDestination) &&
          destinations[untappedDestination].badge == BadgeTypeNone) {
        destinations[untappedDestination].badge = BadgeTypeNew;

        InsertDestination(untappedDestination, &destinations,
                          sortedDestinations);
      }
    }
  }

  std::vector<overflow_menu::Destination> allDestinations;

  // Merge all destinations by prioritizing untapped destinations over ranked
  // destinations in their order of insertion.
  std::merge(_ranking.begin(), _ranking.end(), _untappedDestinations.begin(),
             _untappedDestinations.end(), std::back_inserter(allDestinations));

  // Insert the destinations with a badge that is not for an error at
  // kNewDestinationsInsertionIndex before the untapped destinations.
  for (overflow_menu::Destination destination : allDestinations) {
    if (destinations.contains(destination) &&
        destinations[destination].badge != BadgeTypeError) {
      InsertDestination(destination, &destinations, sortedDestinations);
    }
  }

  // Insert the destinations with an error badge before the destinations with
  // other types of badges.
  for (overflow_menu::Destination destination : allDestinations) {
    if (destinations.contains(destination)) {
      InsertDestination(destination, &destinations, sortedDestinations);
    }
  }

  // Check that all the destinations to show in the carousel were added to the
  // sorted destinations output at this point.
  DCHECK(destinations.empty());

  // Set the new ranking.
  _ranking = {};
  for (OverflowMenuDestination* sortedDestination in sortedDestinations) {
    _ranking.push_back(
        static_cast<overflow_menu::Destination>(sortedDestination.destination));
  }

  [self flushToPrefs];

  return sortedDestinations;
}

@end
