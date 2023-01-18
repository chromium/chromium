// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"

#import <algorithm>
#import <ostream>
#import <set>

#import "base/ranges/algorithm.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// kDataExpirationWindow represents the number of days of usage history stored
// for a given user. Data older than kDataExpirationWindow days will be removed
// during the presentation of the overflow menu.
constexpr int kDataExpirationWindow = 365;  // days (inclusive)

// kNewDestinationsInsertionIndex represents the index new destinations are
// inserted into the current ranking. Assumes the overflow menu carousel always
// has at least four items in it.
constexpr int kNewDestinationsInsertionIndex = 3;

// kRecencyWindow represents the number of days before the present where usage
// is considered recent.
constexpr int kRecencyWindow = 7;  // days (inclusive)

// The percentage by which an app must overtake another app for them to swap
// places.
constexpr double kDampening = 1.1;

// kInitialBufferNumClicks represents the minimum number of clicks a destination
// must have before the user notices a visible change in the carousel sort.
constexpr int kInitialBufferNumClicks = 3;  // clicks

// The dictionary key used for storing rankings.
const char kRankingKey[] = "ranking";

// TODO(crbug.com/989694): Remove `kDefaultRanking` below after feature
// `kSmartSortingPriceTrackingDestination` fully launches.

// The default destinations ranking, based on statistical usage of the old
// overflow menu.
const overflow_menu::Destination kDefaultRanking[] = {
    overflow_menu::Destination::Bookmarks,
    overflow_menu::Destination::History,
    overflow_menu::Destination::ReadingList,
    overflow_menu::Destination::Passwords,
    overflow_menu::Destination::Downloads,
    overflow_menu::Destination::RecentTabs,
    overflow_menu::Destination::SiteInfo,
    overflow_menu::Destination::Settings,
};

// The number of days since the Unix epoch; one day, in this context, runs from
// UTC midnight to UTC midnight.
int TodaysDay() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

// Returns whether `day` is within a valid window of the present day and
// `window` days ago, inclusive.
bool ValidDay(int day, int window) {
  int windowEnd = TodaysDay();
  int windowStart = (windowEnd - window) + 1;

  return day >= windowStart && day <= windowEnd;
}

// Helper method. Converts day (string) to int, then calls ValidDay(int day,
// int window).
bool ValidDay(const std::string day, int window) {
  int dayNumber;
  if (base::StringToInt(day, &dayNumber))
    return ValidDay(dayNumber, window);

  return false;
}

// Returns the number of clicks stored in `history` for a given `destination`.
int NumClicks(overflow_menu::Destination destination,
              base::Value::Dict& history) {
  return history.FindInt(overflow_menu::StringNameForDestination(destination))
      .value_or(0);
}

// Destructively sort `ranking` in ascending or descending (indicated by
// `ascending`) and corresponding number of clicks stored in `flatHistory`.
std::vector<overflow_menu::Destination> SortByUsage(
    const std::vector<overflow_menu::Destination>& ranking,
    base::Value::Dict& flatHistory,
    bool ascending) {
  std::vector<overflow_menu::Destination> ordered_ranking(ranking.begin(),
                                                          ranking.end());
  std::sort(
      ordered_ranking.begin(), ordered_ranking.end(),
      [&](overflow_menu::Destination a, overflow_menu::Destination b) -> bool {
        return ascending
                   ? NumClicks(a, flatHistory) < NumClicks(b, flatHistory)
                   : NumClicks(a, flatHistory) > NumClicks(b, flatHistory);
      });
  return ordered_ranking;
}

// Returns above-the-fold destination with the lowest usage in `flatHistory`.
overflow_menu::Destination LowestShown(
    const std::vector<overflow_menu::Destination>& ranking,
    int numVisibleDestinations,
    base::Value::Dict& flatHistory) {
  std::vector<overflow_menu::Destination> shown(
      ranking.begin(), ranking.begin() + (numVisibleDestinations - 1));
  return SortByUsage(shown, flatHistory, true).front();
}

// Returns below-the-fold destination with the highest usage in `flatHistory`
overflow_menu::Destination HighestUnshown(
    const std::vector<overflow_menu::Destination>& ranking,
    int numVisibleDestinations,
    base::Value::Dict& flatHistory) {
  std::vector<overflow_menu::Destination> unshown(
      ranking.begin() + (numVisibleDestinations - 1), ranking.end());
  return SortByUsage(unshown, flatHistory, false).front();
}

// Swaps `from` and `to` in `ranking`.
void Swap(std::vector<overflow_menu::Destination>& ranking,
          overflow_menu::Destination from,
          overflow_menu::Destination to) {
  auto from_loc = base::ranges::find(ranking, from);
  auto to_loc = base::ranges::find(ranking, to);
  *from_loc = to;
  *to_loc = from;
}

// Converts base::Value::List* ranking into
// std::vector<overflow_menu::Destination> ranking.
std::vector<overflow_menu::Destination> Vector(
    const base::Value::List* ranking) {
  std::vector<overflow_menu::Destination> vec;

  if (!ranking)
    return vec;

  for (auto&& rank : *ranking) {
    if (!rank.is_string())
      NOTREACHED();

    vec.push_back(overflow_menu::DestinationForStringName(rank.GetString()));
  }

  return vec;
}

// Converts base::Value::List ranking into std::set ranking.
std::set<overflow_menu::Destination> Set(const base::Value::List& ranking) {
  std::set<overflow_menu::Destination> set;

  for (auto&& rank : ranking) {
    if (!rank.is_string()) {
      NOTREACHED();
    }

    set.insert(overflow_menu::DestinationForStringName(rank.GetString()));
  }

  return set;
}

// Converts iterable of overflow_menu::Destination `ranking` into
// base::Value::List ranking.
template <typename Range>
base::Value::List List(Range&& ranking) {
  base::Value::List list;

  for (overflow_menu::Destination destination : ranking) {
    list.Append(overflow_menu::StringNameForDestination(destination));
  }

  return list;
}

// Returns the difference between two vectors. Given vectors `x` and `y`,
// returns all elements present in `x`, but not in `y`.
template <typename T>
std::vector<T> FindDiff(std::vector<T> x, std::vector<T> y) {
  std::vector<T> diff;

  std::sort(x.begin(), x.end());
  std::sort(y.begin(), y.end());

  std::set_difference(x.begin(), x.end(), y.begin(), y.end(),
                      std::back_inserter(diff));

  return diff;
}

}  // namespace

// Tracks destination usage from the new overflow menu and implements a frecency
// algorithm to sort destinations shown in the overflow menu carousel. The
// algorithm, at a high-level, works as follows:
//
// (1) Divide the destinations carousel into two groups: (A) visible
// "above-the-fold" destinations and (B) non-visible "below-the-fold"
// destinations; "below-the-fold" destinations are made visible to the user when
// they scroll the carousel.
//
// (2) Get each destination's numClicks.
//
// (3) Compare destination with highest numClicks in [group B] to destination
// with lowest numClicks in [group A]
//
// (4) Swap (i.e. "promote") the [group B] destination with the [group A] one if
// B's numClicks exceeds A's.
@implementation DestinationUsageHistory

#pragma mark - Initializers

- (instancetype)initWithPrefService:(PrefService*)prefService {
  if (self = [super init])
    _prefService = prefService;

  return self;
}

- (void)dealloc {
  DCHECK(!self.prefService) << "-disconnect needs to be called before -dealloc";
}

#pragma mark - Disconnect

- (void)disconnect {
  self.prefService = nullptr;
}

#pragma mark - Public

- (std::vector<overflow_menu::Destination>)
    updatedRankWithCurrentRanking:
        (std::vector<overflow_menu::Destination>&)ranking
         numAboveFoldDestinations:(int)numAboveFoldDestinations {
  // Delete expired usage data older than `kDataExpirationWindow` days before
  // running the ranking algorithm.
  [self deleteExpiredData];

  base::Value::Dict allHistory =
      [self flattenedHistoryWithinWindow:kDataExpirationWindow];
  base::Value::Dict recentHistory =
      [self flattenedHistoryWithinWindow:kRecencyWindow];

  std::vector<overflow_menu::Destination> prevRanking = ranking;

  overflow_menu::Destination lowestShownAll =
      LowestShown(prevRanking, numAboveFoldDestinations, allHistory);
  overflow_menu::Destination lowestShownRecent =
      LowestShown(prevRanking, numAboveFoldDestinations, recentHistory);
  overflow_menu::Destination highestUnshownAll =
      HighestUnshown(prevRanking, numAboveFoldDestinations, allHistory);
  overflow_menu::Destination highestUnshownRecent =
      HighestUnshown(prevRanking, numAboveFoldDestinations, recentHistory);

  std::vector<overflow_menu::Destination> newRanking = prevRanking;

  if (NumClicks(highestUnshownRecent, recentHistory) >
      NumClicks(lowestShownRecent, recentHistory) * kDampening) {
    Swap(newRanking, lowestShownRecent, highestUnshownRecent);
  } else if (NumClicks(highestUnshownAll, allHistory) >
             NumClicks(lowestShownAll, allHistory) * kDampening) {
    Swap(newRanking, lowestShownAll, highestUnshownAll);
  }

  DCHECK_EQ(newRanking.size(), prevRanking.size());

  return newRanking;
}

// Track click for `destination` and associate it with TodaysDay().
- (void)trackDestinationClick:(overflow_menu::Destination)destination
     numAboveFoldDestinations:(int)numAboveFoldDestinations {
  // Exit early if there's no pref service. May happen during the application
  // shutdown.
  if (!self.prefService)
    return;

  const base::Value::Dict& history =
      self.prefService->GetDict(prefs::kOverflowMenuDestinationUsageHistory);

  const std::string path = base::NumberToString(TodaysDay()) + "." +
                           overflow_menu::StringNameForDestination(destination);

  int numClicks = history.FindIntByDottedPath(path).value_or(0) + 1;

  ScopedDictPrefUpdate update(self.prefService,
                              prefs::kOverflowMenuDestinationUsageHistory);

  update->SetByDottedPath(path, numClicks);

  if (IsSmartSortingPriceTrackingDestinationEnabled()) {
    ScopedListPrefUpdate newDestinationsUpdate(
        self.prefService, prefs::kOverflowMenuNewDestinations);

    newDestinationsUpdate->EraseValue(
        base::Value(overflow_menu::StringNameForDestination(destination)));
  } else {
    // TODO(crbug.com/989694): Remove this code and surrounding else-check after
    // feature `kSmartSortingPriceTrackingDestination` fully launches.

    // User's very first time using Smart Sorting.
    if (history.size() == 0) {
      [self injectDefaultNumClicksForAllDestinations];
    }
  }

  // Calculate new ranking and store to prefs; Calculate the new ranking
  // ahead of time so overflow menu presentation needn't run ranking algorithm
  // each time it presents.
  const base::Value::List* currentRanking = [self fetchCurrentRanking];
  base::Value::List newRanking =
      [self calculateNewRanking:currentRanking
          numAboveFoldDestinations:numAboveFoldDestinations];
  update->Set(kRankingKey, std::move(newRanking));
}

#pragma mark - Private

// TODO(crbug.com/989694): Remove `injectDefaultNumClicksForAllDestinations`
// below after feature `kSmartSortingPriceTrackingDestination` fully launches.

// Injects a default number of clicks for all destinations in the history
// dictonary.
- (void)injectDefaultNumClicksForAllDestinations {
  // Exit early if there's no pref service. May happen during the application
  // shutdown.
  if (!self.prefService)
    return;

  DCHECK_GT(kDampening, 1.0);
  DCHECK_GT(kInitialBufferNumClicks, 1);

  int defaultNumClicks =
      (kInitialBufferNumClicks - 1) * (kDampening - 1.0) * 100.0;
  std::string today = base::NumberToString(TodaysDay());
  ScopedDictPrefUpdate update(self.prefService,
                              prefs::kOverflowMenuDestinationUsageHistory);
  const base::Value::Dict& history =
      self.prefService->GetDict(prefs::kOverflowMenuDestinationUsageHistory);

  for (overflow_menu::Destination destination : kDefaultRanking) {
    const std::string path =
        today + "." + overflow_menu::StringNameForDestination(destination);
    update->SetByDottedPath(
        path, history.FindIntByDottedPath(path).value_or(0) + defaultNumClicks);
  }
}

// Adds `destinations` to the locally-stored usage history with a
// calculated number of initial clicks.
//
// NOTE: This method skips seeding history for destinations that already exist
// in the usage history.
- (void)seedHistoryWithDestinations:
    (std::vector<overflow_menu::Destination>&)destinations {
  // Exit early if there's no pref service. May happen during the application
  // shutdown.
  if (!self.prefService) {
    return;
  }

  DCHECK_GT(kDampening, 1.0);
  DCHECK_GT(kInitialBufferNumClicks, 1);

  int defaultNumClicks =
      (kInitialBufferNumClicks - 1) * (kDampening - 1.0) * 100.0;

  std::string today = base::NumberToString(TodaysDay());

  ScopedDictPrefUpdate update(self.prefService,
                              prefs::kOverflowMenuDestinationUsageHistory);

  const base::Value::Dict& history =
      self.prefService->GetDict(prefs::kOverflowMenuDestinationUsageHistory);

  const base::Value::Dict flattenedHistory =
      [self flattenedHistoryWithinWindow:kDataExpirationWindow];

  for (overflow_menu::Destination destination : destinations) {
    std::string destinationName =
        overflow_menu::StringNameForDestination(destination);

    // Does not seed history for destinations that already exist in the usage
    // history.
    if (!flattenedHistory.Find(destinationName)) {
      const std::string path = today + "." + destinationName;
      update->SetByDottedPath(
          path,
          history.FindIntByDottedPath(path).value_or(0) + defaultNumClicks);
    }
  }
}

// Updates the locally-stored ranking to `ranking`.
- (void)updateStoredRanking:(std::vector<overflow_menu::Destination>&)ranking {
  // Exit early if there's no pref service. May happen during the application
  // shutdown.
  if (!self.prefService) {
    return;
  }

  ScopedDictPrefUpdate update(self.prefService,
                              prefs::kOverflowMenuDestinationUsageHistory);

  base::Value::List newRanking = List(ranking);

  update->Set(kRankingKey, std::move(newRanking));
}

// Delete expired usage data (data older than `kDataExpirationWindow` days) and
// saves back to prefs. Returns true if expired usage data was found/removed,
// false otherwise.
- (void)deleteExpiredData {
  // Exit early if there's no pref service. May happen during the application
  // shutdown.
  if (!self.prefService)
    return;

  const base::Value::Dict& history =
      self.prefService->GetDict(prefs::kOverflowMenuDestinationUsageHistory);

  base::Value::Dict prunedHistory = history.Clone();

  for (auto&& [day, dayHistory] : history) {
    // Skip over entry corresponding to previous ranking.
    if (day == kRankingKey)
      continue;

    if (!ValidDay(day, kDataExpirationWindow)) {
      prunedHistory.Remove(day);
    }
  }

  self.prefService->SetDict(prefs::kOverflowMenuDestinationUsageHistory,
                            std::move(prunedHistory));
}

// Fetches the current ranking saved in prefs and returns it.
- (const base::Value::List*)fetchCurrentRanking {
  // Exit early if there's no pref service. May happen during the application
  // shutdown.
  if (!self.prefService)
    return nullptr;

  const base::Value::Dict& history =
      self.prefService->GetDict(prefs::kOverflowMenuDestinationUsageHistory);

  return history.FindList(kRankingKey);
}

// Compares the current list of carousel items to the current ranking.
//
// When the carousel has destinations not found in the current ranking, those
// destinations are considered new. New destinations are inserted into the
// current ranking starting at `kNewDestinationsInsertionIndex` and seeded with
// usage history.
//
// When the current ranking has destinations not found in carousel, those
// destinations are considered removed. Removed destinations are removed from
// the current ranking.
//
// This method tracks new and removed destinations via Pref lists.
- (void)runHistoryDiagnostic:
    (std::vector<overflow_menu::Destination>&)currentDestinations {
  // Exit early if there's no pref service. May happen during the application
  // shutdown.
  if (!self.prefService) {
    return;
  }
  const base::Value::List* storedRanking = [self fetchCurrentRanking];

  // If `currentRanking` is invalid, this is the user's first time using Smart
  // Sorting. This means no ranking or usage history exist on the device, yet,
  // so a ranking and usage history must be created and seeded.
  if (!storedRanking) {
    [self updateStoredRanking:currentDestinations];
    [self seedHistoryWithDestinations:currentDestinations];

    return;
  }

  std::vector<overflow_menu::Destination> currentRanking =
      Vector(storedRanking);

  std::vector<overflow_menu::Destination> newDestinations =
      FindDiff(currentDestinations, currentRanking);

  [self seedHistoryWithDestinations:newDestinations];

  ScopedListPrefUpdate newDestinationsUpdate(
      self.prefService, prefs::kOverflowMenuNewDestinations);

  // Make other parts of Smart Sorting infrastructure aware of the newly added
  // destinations. Newly added destinations are those that exist in the
  // carousel, but don't exist in the current ranking.
  for (overflow_menu::Destination newDestination : newDestinations) {
    newDestinationsUpdate->EraseValue(
        base::Value(overflow_menu::StringNameForDestination(newDestination)));
    newDestinationsUpdate->Append(
        base::Value(overflow_menu::StringNameForDestination(newDestination)));
  }

  std::vector<overflow_menu::Destination> removedDestinations =
      FindDiff(currentRanking, currentDestinations);

  std::set<overflow_menu::Destination> removed(removedDestinations.begin(),
                                               removedDestinations.end());

  // Newly-added and newly-removed destinations need to be added to, and removed
  // from, the current ranking, respectively. This ensures the logic for
  // detecting returning destinations works in future evaluations.
  std::vector<overflow_menu::Destination> updatedRanking;

  for (overflow_menu::Destination destination : currentRanking) {
    if (!removed.count(destination)) {
      updatedRanking.push_back(destination);
    }
  }

  DCHECK_GE(currentDestinations.size(), size_t(4));

  auto pos =
      updatedRanking.begin() +
      std::max(0, std::min(kNewDestinationsInsertionIndex,
                           static_cast<int>(currentDestinations.size()) - 1));
  updatedRanking.insert(pos, newDestinations.begin(), newDestinations.end());

  [self updateStoredRanking:updatedRanking];
}

// Fetches the current ranking stored in Chrome Prefs and returns a sorted list
// of OverflowMenuDestination* which match the ranking.
- (NSArray<OverflowMenuDestination*>*)generateDestinationsList:
    (NSArray<OverflowMenuDestination*>*)unrankedDestinations {
  if (IsSmartSortingPriceTrackingDestinationEnabled()) {
    std::vector<overflow_menu::Destination> destinations;

    for (OverflowMenuDestination* destination : unrankedDestinations) {
      overflow_menu::Destination currDestination =
          overflow_menu::DestinationForNSStringName(
              destination.destinationName);

      destinations.push_back(currDestination);
    }

    [self runHistoryDiagnostic:destinations];

    NSMutableArray<OverflowMenuDestination*>* carouselItems =
        [[self destinationList:[self fetchCurrentRanking]
                       options:unrankedDestinations] mutableCopy];

    std::set<overflow_menu::Destination> newDestinations =
        Set(self.prefService->GetList(prefs::kOverflowMenuNewDestinations));

    for (OverflowMenuDestination* carouselItem : carouselItems) {
      if (newDestinations.count(overflow_menu::DestinationForNSStringName(
              carouselItem.destinationName))) {
        carouselItem.badge = BadgeTypeNewLabel;
      }
    }

    return carouselItems;
  } else {
    return [self destinationList:[self fetchCurrentRanking]
                         options:unrankedDestinations];
  }
}

// Runs the ranking algorithm given a `previousRanking`. If `previousRanking` is
// invalid or doesn't exist, use the default ranking, based on statistical usage
// of the old overflow menu.
- (const base::Value::List)calculateNewRanking:
                               (const base::Value::List*)previousRanking
                      numAboveFoldDestinations:(int)numAboveFoldDestinations {
  if (IsSmartSortingPriceTrackingDestinationEnabled()) {
    DCHECK_NE(previousRanking, nullptr);
  }

  // TODO(crbug.com/1405245): Remove if-else check below after feature
  // `kSmartSortingPriceTrackingDestination` fully launches.
  if (!IsSmartSortingPriceTrackingDestinationEnabled() && !previousRanking) {
    return List(kDefaultRanking);
  }

  if (numAboveFoldDestinations >= static_cast<int>(previousRanking->size()))
    return previousRanking->Clone();

  std::vector<overflow_menu::Destination> prevRanking = Vector(previousRanking);
  std::vector<overflow_menu::Destination> newRanking =
      [self updatedRankWithCurrentRanking:prevRanking
                 numAboveFoldDestinations:numAboveFoldDestinations];

  return List(newRanking);
}

// Returns the flattened destination usage history as a dictionary of the
// following shape: destinationName (std::string) -> total number of clicks
// (int). Only usage data within previous `window` days will be included in the
// returned result.
- (base::Value::Dict)flattenedHistoryWithinWindow:(int)window {
  base::Value::Dict flatHistory;

  // Exit early if there's no pref service. May happen during the application
  // shutdown.
  if (!self.prefService)
    return flatHistory;

  const base::Value::Dict& history =
      self.prefService->GetDict(prefs::kOverflowMenuDestinationUsageHistory);

  for (auto&& [day, dayHistory] : history) {
    // Skip over entry corresponding to previous ranking.
    if (day == kRankingKey) {
      continue;
    }

    // Skip over expired day history; this `continue` is not expected to be
    // called, as expired data should be already pruned.
    if (!ValidDay(day, window)) {
      continue;
    }

    const base::Value::Dict* dayHistoryDict = dayHistory.GetIfDict();
    // Skip over malformed day history; this `continue` is not expected to be
    // called.
    if (!dayHistoryDict) {
      NOTREACHED();
      continue;
    }

    for (auto&& [destination, numClicks] : *dayHistoryDict) {
      int totalNumClicks = numClicks.GetIfInt().value_or(0) +
                           flatHistory.FindInt(destination).value_or(0);
      flatHistory.Set(destination, totalNumClicks);
    }
  }

  return flatHistory;
}

// Constructs OverflowMenuDestination* lookup-by-name dictionary of the
// following shape: destinationName (NSString*) -> destination
// (OverflowMenuDestination*).
- (NSDictionary<NSString*, OverflowMenuDestination*>*)destinationsByName:
    (NSArray<OverflowMenuDestination*>*)destinations {
  NSMutableDictionary<NSString*, OverflowMenuDestination*>* dictionary =
      [[NSMutableDictionary alloc] init];

  for (OverflowMenuDestination* destination : destinations) {
    dictionary[destination.destinationName] = destination;
  }

  return dictionary;
}

// Converts base::Value::List* ranking to
// NSArray<OverflowMenuDestination*>* ranking given a list, `options`, of
// OverflowMenuDestination* options.
- (NSArray<OverflowMenuDestination*>*)
    destinationList:(const base::Value::List*)ranking
            options:(NSArray<OverflowMenuDestination*>*)options {
  if (!ranking)
    // If no valid ranking, return unsorted options.
    return options;

  NSMutableArray<OverflowMenuDestination*>* result =
      [[NSMutableArray alloc] init];

  NSDictionary<NSString*, OverflowMenuDestination*>* destinations =
      [self destinationsByName:options];

  for (auto&& destinationName : *ranking) {
    // If at any point a stored ranking is invalid, return unsorted options.
    if (!destinationName.is_string())
      return options;

    NSString* name = base::SysUTF8ToNSString(destinationName.GetString());

    if (destinations[name])
      [result addObject:destinations[name]];
  }

  return result;
}

@end
