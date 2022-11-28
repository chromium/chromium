// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"

#import <ostream>

#import "base/ranges/algorithm.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// kDataExpirationWindow represents the number of days of usage history stored
// for a given user. Data older than kDataExpirationWindow days will be removed
// during the presentation of the overflow menu.
constexpr int kDataExpirationWindow = 365;  // days (inclusive)

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

  // User's very first time using Smart Sorting.
  if (history.size() == 0)
    [self injectDefaultNumClicksForAllDestinations];

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

    if (!ValidDay(day, kDataExpirationWindow))
      prunedHistory.Remove(day);
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

// Fetches the current ranking stored in Chrome Prefs and returns a sorted list
// of OverflowMenuDestination* which match the ranking.
- (NSArray<OverflowMenuDestination*>*)generateDestinationsList:
    (NSArray<OverflowMenuDestination*>*)unrankedDestinations {
  return [self destinationList:[self fetchCurrentRanking]
                       options:unrankedDestinations];
}

// Runs the ranking algorithm given a `previousRanking`. If `previousRanking` is
// invalid or doesn't exist, use the default ranking, based on statistical usage
// of the old overflow menu.
- (const base::Value::List)calculateNewRanking:
                               (const base::Value::List*)previousRanking
                      numAboveFoldDestinations:(int)numAboveFoldDestinations {
  if (!previousRanking) {
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
