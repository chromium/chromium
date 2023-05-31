// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"

#import <limits.h>
#import <algorithm>
#import <ostream>
#import <set>
#import <vector>

#import "base/ranges/algorithm.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// `kDataExpirationWindow` is the window of time (inclusive) that usage history
// is stored for a given user. Usage history older than `kDataExpirationWindow`
// will be removed during the presentation of the overflow menu.
constexpr base::TimeDelta kDataExpirationWindow = base::Days(365);

// `kDataRecencyWindow` is the window of time (inclusive) that new usage history
// is considered recent.
constexpr base::TimeDelta kDataRecencyWindow = base::Days(7);

// The percentage by which an app must overtake another app for them to swap
// places.
constexpr double kDampening = 1.1;

// `kInitialUsageThreshold` represents the minimum number of clicks a
// destination must have before the user notices a visible change in the
// carousel sort.
constexpr int kInitialUsageThreshold = 3;  // clicks

// The dictionary key used for storing rankings.
const char kRankingKey[] = "ranking";

// A time delta from the Unix epoch to the beginning of the current day.
base::TimeDelta TodaysDay() {
  return base::Days(
      (base::Time::Now() - base::Time::UnixEpoch()).InDaysFloored());
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

// Sort `ranking` in ascending or descending (indicated by
// `ascending`) and corresponding number of clicks stored in
// `aggregate_history`.
DestinationRanking SortByUsage(
    const DestinationRanking& ranking,
    std::map<overflow_menu::Destination, int>& aggregate_history,
    bool ascending) {
  DestinationRanking ordered_ranking(ranking.begin(), ranking.end());
  std::sort(
      ordered_ranking.begin(), ordered_ranking.end(),
      [&](overflow_menu::Destination a, overflow_menu::Destination b) -> bool {
        return ascending ? aggregate_history[a] < aggregate_history[b]
                         : aggregate_history[a] > aggregate_history[b];
      });

  return ordered_ranking;
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

@implementation DestinationUsageHistory {
  // Pref service to retrieve/store preference values.
  PrefService* _prefService;

  // Nested dictionary containing the device's destination usage history. Has
  // the following shape:
  //
  // [ day (TimeDelta) : [ destination (overflow_menu::Destination) : clickCount
  // (int) ] ]
  std::map<base::TimeDelta, std::map<overflow_menu::Destination, int>>
      _usageHistory;

  // New destinations recently added to the overflow menu carousel that have not
  // yet been clicked by the user.
  std::set<overflow_menu::Destination> _untappedDestinations;
}

#pragma mark - Public methods

- (instancetype)initWithPrefService:(PrefService*)prefService {
  if (self = [super init]) {
    _prefService = prefService;
  }

  return self;
}

- (void)dealloc {
  DCHECK(!_prefService) << "-stop needs to be called before -dealloc";
}

- (void)start {
  if (!_prefService) {
    return;
  }

  ScopedDictPrefUpdate historyUpdate(
      _prefService, prefs::kOverflowMenuDestinationUsageHistory);

  const base::Value::Dict& storedUsageHistory = historyUpdate.Get();

  // Fetch the stored usage history, then update `_usageHistory` with its data.
  for (const auto&& [key, value] : storedUsageHistory) {
    if (key == kRankingKey) {
      // Ranking is now handled by `OverflowMenuOrderer` so ignore it here.
      continue;
    }

    int storedDay;

    // If, for some reason, `key` cannot be converted to a valid day (int),
    // consider the entry malformed, and skip over it. This effectively erases
    // the entry from the destination usage history.
    if (!base::StringToInt(key, &storedDay)) {
      continue;
    }

    base::TimeDelta entryDay = base::Days(storedDay);

    base::TimeDelta entryAge = TodaysDay() - entryDay;

    // Skip over expired entries. This effectively erases
    // the expired entry from the destination usage history.
    if (entryAge > kDataExpirationWindow) {
      continue;
    }

    const base::Value::Dict* dayHistory = value.GetIfDict();

    // If, for some reason, `dayHistory` cannot be converted to a valid
    // dictionary (base::Value::Dict), consider the entry malformed, and skip
    // over it. This effectively erases the entry from the destination usage
    // history.
    if (!dayHistory) {
      continue;
    }

    for (auto&& [destinationName, clicks] : *dayHistory) {
      _usageHistory[entryDay]
                   [overflow_menu::DestinationForStringName(destinationName)] =
                       clicks.GetIfInt().value_or(0);
    }
  }

  // Fetch the stored list of newly-added, unclicked destinations, then update
  // `_untappedDestinations` with its data.
  AddDestinationsToSet(
      _prefService->GetList(prefs::kOverflowMenuNewDestinations),
      _untappedDestinations);
}

- (void)stop {
  _prefService = nullptr;
}

- (DestinationRanking)
    sortedDestinationsFromCurrentRanking:(DestinationRanking)currentRanking
                    carouselDestinations:(NSArray<OverflowMenuDestination*>*)
                                             carouselDestinations {
  [self seedUsageHistoryForNewDestinations:carouselDestinations];

  // Exit early if there's no `currentRanking`, which only happens if the device
  // hasn't used Smart Sorting before.
  if (currentRanking.empty()) {
    // Given there's no existing `currentRanking`, the current carousel sort
    // order will be used as the default ranking.
    for (OverflowMenuDestination* destination in carouselDestinations) {
      currentRanking.push_back(
          static_cast<overflow_menu::Destination>(destination.destination));
    }
  }

  DestinationRanking sortedRanking =
      [self calculateNewRankingFromCurrentRanking:currentRanking];

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

  std::set<overflow_menu::Destination> existingDestinations(
      sortedRanking.begin(), sortedRanking.end());

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
  for (overflow_menu::Destination rankedDestination : sortedRanking) {
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
  std::merge(sortedRanking.begin(), sortedRanking.end(),
             _untappedDestinations.begin(), _untappedDestinations.end(),
             std::back_inserter(allDestinations));

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
  sortedRanking = {};
  for (OverflowMenuDestination* sortedDestination in sortedDestinations) {
    sortedRanking.push_back(
        static_cast<overflow_menu::Destination>(sortedDestination.destination));
  }

  return sortedRanking;
}

// Track click for `destination` and associate it with TodaysDay().
- (void)recordClickForDestination:(overflow_menu::Destination)destination {
  _usageHistory[TodaysDay()][destination] += 1;

  _untappedDestinations.erase(destination);

  [self flushToPrefs];
}

#pragma mark - Private methods

// Applies the frecency algorithm described in the class header comment to
// calculate the new ranking.
//
// NOTE: Destinations in `_untappedDestinations` will be completely ignored
// during the ranking calculation because they're artificially inserted into the
// carousel/ranking starting at position `kNewDestinationsInsertionIndex`.
// This is custom business logic that doesn't respect the Smart Sorting
// algorithm defined below, so it will happen outside of this method.
- (DestinationRanking)calculateNewRankingFromCurrentRanking:
    (DestinationRanking)currentRanking {
  if (!self.visibleDestinationsCount ||
      self.visibleDestinationsCount > static_cast<int>(currentRanking.size())) {
    return currentRanking;
  }

  // Dictionary with aggregate destination usage history that's occurred over
  // the last `kDataExpirationWindow` days.
  std::map<overflow_menu::Destination, int> aggregateUsageHistory =
      [self flattenedUsageHistoryWithinWindow:kDataExpirationWindow];

  // Dictionary with aggregate destination usage history that's occurred over
  // the last `kDataRecencyWindow` days.
  std::map<overflow_menu::Destination, int> aggregateRecentUsageHistory =
      [self flattenedUsageHistoryWithinWindow:kDataRecencyWindow];

  DestinationRanking aboveFoldRanking(
      currentRanking.begin(),
      currentRanking.begin() + (self.visibleDestinationsCount - 1));

  DestinationRanking belowFoldRanking(
      currentRanking.begin() + (self.visibleDestinationsCount - 1),
      currentRanking.end());

  overflow_menu::Destination lowestShownAll =
      SortByUsage(aboveFoldRanking, aggregateUsageHistory, true).front();
  overflow_menu::Destination lowestShownRecent =
      SortByUsage(aboveFoldRanking, aggregateRecentUsageHistory, true).front();
  overflow_menu::Destination highestUnshownAll =
      SortByUsage(belowFoldRanking, aggregateUsageHistory, false).front();
  overflow_menu::Destination highestUnshownRecent =
      SortByUsage(belowFoldRanking, aggregateRecentUsageHistory, false).front();

  DestinationRanking newRanking = currentRanking;

  if (aggregateRecentUsageHistory[highestUnshownRecent] >
      aggregateRecentUsageHistory[lowestShownRecent] * kDampening) {
    std::iter_swap(
        std::find(newRanking.begin(), newRanking.end(), lowestShownRecent),
        std::find(newRanking.begin(), newRanking.end(), highestUnshownRecent));
  } else if (aggregateUsageHistory[highestUnshownAll] >
             aggregateUsageHistory[lowestShownAll] * kDampening) {
    std::iter_swap(
        std::find(newRanking.begin(), newRanking.end(), lowestShownAll),
        std::find(newRanking.begin(), newRanking.end(), highestUnshownAll));
  }

  DCHECK_EQ(newRanking.size(), currentRanking.size());

  return newRanking;
}

// Constructs aggregated usage history dictionary of the following shape:
// destination (overflow_menu::Destination) :  clickCount (int)
//
// Excludes destinations in `_untappedDestinations`.
- (std::map<overflow_menu::Destination, int>)flattenedUsageHistoryWithinWindow:
    (base::TimeDelta)window {
  std::map<overflow_menu::Destination, int> flattenedUsageHistory;

  for (const auto& [day, dayHistory] : _usageHistory) {
    for (const auto& [destination, clickCount] : dayHistory) {
      if (_untappedDestinations.contains(destination)) {
        continue;
      }

      base::TimeDelta entryAge = TodaysDay() - day;

      if (entryAge <= window) {
        flattenedUsageHistory[destination] += _usageHistory[day][destination];
      }
    }
  }

  return flattenedUsageHistory;
}

// Adds `destinations` to `_usageHistory` with a calculated number of initial
// clicks. This method skips seeding history for any `destinations` that
// already exist in `_usageHistory`.
- (void)seedUsageHistoryForNewDestinations:
    (NSArray<OverflowMenuDestination*>*)destinations {
  DCHECK_GT(kDampening, 1.0);
  DCHECK_GT(kInitialUsageThreshold, 1);

  std::set<overflow_menu::Destination> newDestinations;

  for (OverflowMenuDestination* destination in destinations) {
    newDestinations.insert(
        static_cast<overflow_menu::Destination>(destination.destination));
  }

  std::set<overflow_menu::Destination> existingDestinations;

  for (const auto& [day, dayHistory] : _usageHistory) {
    for (const auto& [destination, clickCount] : dayHistory) {
      existingDestinations.insert(destination);
    }
  }

  // The difference between `newDestinations` and `existingDestinations` are
  // destinations without usage history; destinations which need their history
  // seeded.
  std::vector<overflow_menu::Destination> destinationsWithoutHistory;

  std::set_difference(newDestinations.begin(), newDestinations.end(),
                      existingDestinations.begin(), existingDestinations.end(),
                      std::inserter(destinationsWithoutHistory,
                                    destinationsWithoutHistory.end()));

  for (overflow_menu::Destination destination : destinationsWithoutHistory) {
    _usageHistory[TodaysDay()][destination] =
        ((kInitialUsageThreshold - 1) * (kDampening - 1.0) * 100.0);
  }

  [self flushToPrefs];
}

// Flushes the latest usage history, ranking, and untapped destinations
// information to Prefs.
- (void)flushToPrefs {
  if (!_prefService) {
    return;
  }

  ScopedDictPrefUpdate historyUpdate(
      _prefService, prefs::kOverflowMenuDestinationUsageHistory);

  base::Value::List* possibleStoredRanking =
      historyUpdate->FindList(kRankingKey);
  bool hasStoredRanking = possibleStoredRanking != nullptr;
  base::Value::List storedRanking;
  if (hasStoredRanking) {
    storedRanking = possibleStoredRanking->Clone();
  }

  // Clear the existing usage history and ranking stored in Prefs.
  historyUpdate->clear();

  // Keep stored ranking if it still exists.
  if (hasStoredRanking) {
    historyUpdate->Set(kRankingKey, storedRanking.Clone());
  }

  // Flush the new usage history to Prefs.
  for (const auto& dayHistory : _usageHistory) {
    for (const auto& dayUsage : dayHistory.second) {
      int day = dayHistory.first.InDays();
      overflow_menu::Destination destination = dayUsage.first;
      int clickCount = dayUsage.second;

      const std::string dottedPathForPrefEntry =
          base::NumberToString(day) + "." +
          overflow_menu::StringNameForDestination(destination);

      historyUpdate->SetByDottedPath(dottedPathForPrefEntry, clickCount);
    }
  }

  // Flush the new untapped destinations to Prefs.
  ScopedListPrefUpdate untappedDestinationsUpdate(
      _prefService, prefs::kOverflowMenuNewDestinations);

  untappedDestinationsUpdate->clear();

  for (overflow_menu::Destination untappedDestination : _untappedDestinations) {
    untappedDestinationsUpdate->Append(
        overflow_menu::StringNameForDestination(untappedDestination));
  }
}

@end
