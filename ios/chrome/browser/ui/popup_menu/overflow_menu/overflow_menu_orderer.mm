// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_orderer.h"

#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The dictionary key used for storing rankings.
const char kRankingKey[] = "ranking";

// The dictionary key used for storing the shown action ordering.
const char kShownActionsKey[] = "shown";

// The dictionary key used for storing the hidden action ordering.
const char kHiddenActionsKey[] = "hidden";

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
                       std::set<overflow_menu::Destination>& destinationsToAdd,
                       DestinationRanking& output) {
  const int insertionIndex = std::min(
      output.size() - 1, static_cast<size_t>(kNewDestinationsInsertionIndex));

  output.insert(output.begin() + insertionIndex, destination);

  destinationsToAdd.erase(destination);
}

// Simple data struct to bundle the two lists of destinations together.
struct DestinationOrderData {
  DestinationRanking shownDestinations;
  DestinationRanking hiddenDestinations;

  bool empty() const {
    return shownDestinations.empty() && hiddenDestinations.empty();
  }
};

// Simple data struct to bundle the two lists of actions together.
struct ActionOrderData {
  ActionRanking shownActions;
  ActionRanking hiddenActions;

  bool empty() const { return shownActions.empty() && hiddenActions.empty(); }
};
}  // namespace

using DestinationLookup =
    std::map<overflow_menu::Destination, OverflowMenuDestination*>;

@interface OverflowMenuOrderer ()

// The destination usage history, which (1) tracks which items from the carousel
// are clicked, and (2) suggests a sorted order for carousel menu items.
@property(nonatomic, strong) DestinationUsageHistory* destinationUsageHistory;

@end

@implementation OverflowMenuOrderer {
  // Whether the current menu is for an incognito page.
  BOOL _isIncognito;

  // New destinations recently added to the overflow menu carousel that have not
  // yet been clicked by the user.
  std::set<overflow_menu::Destination> _untappedDestinations;

  // The data for the current destinations ordering and show/hide state.
  DestinationOrderData _destinationOrderData;

  // The data for the current actions ordering and show/hide state.
  ActionOrderData _actionOrderData;

  PrefBackedBoolean* _destinationUsageHistoryEnabled;
}

@synthesize actionCustomizationModel = _actionCustomizationModel;
@synthesize destinationCustomizationModel = _destinationCustomizationModel;

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

  _destinationUsageHistoryEnabled = [[PrefBackedBoolean alloc]
      initWithPrefService:_localStatePrefs
                 prefName:prefs::kOverflowMenuDestinationUsageHistoryEnabled];

  [self loadDestinationsFromPrefs];
  [self loadActionsFromPrefs];
}

- (void)setVisibleDestinationsCount:(int)visibleDestinationsCount {
  _visibleDestinationsCount = visibleDestinationsCount;
  self.destinationUsageHistory.visibleDestinationsCount =
      self.visibleDestinationsCount;
}

// Lazily create action customization model.
- (ActionCustomizationModel*)actionCustomizationModel {
  if (_actionCustomizationModel) {
    return _actionCustomizationModel;
  }

  [self updateActionOrderData];

  NSMutableArray<OverflowMenuAction*>* actions = [[NSMutableArray alloc] init];
  for (overflow_menu::ActionType action : _actionOrderData.shownActions) {
    if (OverflowMenuAction* overflowMenuAction =
            [self.actionProvider customizationActionForActionType:action]) {
      [actions addObject:overflowMenuAction];
    }
  }
  for (overflow_menu::ActionType action : _actionOrderData.hiddenActions) {
    if (OverflowMenuAction* overflowMenuAction =
            [self.actionProvider customizationActionForActionType:action]) {
      overflowMenuAction.shown = NO;
      [actions addObject:overflowMenuAction];
    }
  }

  _actionCustomizationModel =
      [[ActionCustomizationModel alloc] initWithActions:actions];
  return _actionCustomizationModel;
}

// Lazily create destination customization model.
- (DestinationCustomizationModel*)destinationCustomizationModel {
  if (_destinationCustomizationModel) {
    return _destinationCustomizationModel;
  }

  [self initializeDestinationOrderDataIfEmpty];

  NSMutableArray<OverflowMenuDestination*>* destinations =
      [[NSMutableArray alloc] init];
  for (overflow_menu::Destination destination :
       _destinationOrderData.shownDestinations) {
    if (OverflowMenuDestination* overflowMenuDestination =
            [self.destinationProvider
                customizationDestinationForDestinationType:destination]) {
      [destinations addObject:overflowMenuDestination];
    }
  }
  for (overflow_menu::Destination destination :
       _destinationOrderData.hiddenDestinations) {
    if (OverflowMenuDestination* overflowMenuDestination =
            [self.destinationProvider
                customizationDestinationForDestinationType:destination]) {
      overflowMenuDestination.shown = NO;
      [destinations addObject:overflowMenuDestination];
    }
  }

  _destinationCustomizationModel = [[DestinationCustomizationModel alloc]
         initWithDestinations:destinations
      destinationUsageEnabled:_destinationUsageHistoryEnabled.value];
  return _destinationCustomizationModel;
}

#pragma mark - Public

- (void)recordClickForDestination:(overflow_menu::Destination)destination {
  _untappedDestinations.erase(destination);

  [self.destinationUsageHistory recordClickForDestination:destination];
}

- (void)updateDestinations {
  self.model.destinations = [self sortedDestinations];
}

- (void)updatePageActions {
  self.pageActionsGroup.actions = [self pageActions];
}

- (void)commitActionsUpdate {
  ActionOrderData actionOrderData;
  for (OverflowMenuAction* action in self.actionCustomizationModel.shownActions
           .actions) {
    actionOrderData.shownActions.push_back(
        static_cast<overflow_menu::ActionType>(action.actionType));
  }

  for (OverflowMenuAction* action in self.actionCustomizationModel.hiddenActions
           .actions) {
    actionOrderData.hiddenActions.push_back(
        static_cast<overflow_menu::ActionType>(action.actionType));
  }

  _actionOrderData = actionOrderData;
  [self flushActionsToPrefs];

  [self updatePageActions];

  // Reset customization model so next customization can start fresh.
  _actionCustomizationModel = nil;
}

- (void)commitDestinationsUpdate {
  DestinationOrderData orderData;
  for (OverflowMenuDestination* destination in self
           .destinationCustomizationModel.shownDestinations) {
    orderData.shownDestinations.push_back(
        static_cast<overflow_menu::Destination>(destination.destination));
  }

  for (OverflowMenuDestination* destination in self
           .destinationCustomizationModel.hiddenDestinations) {
    orderData.hiddenDestinations.push_back(
        static_cast<overflow_menu::Destination>(destination.destination));
  }

  _destinationOrderData = orderData;
  // If Destination Usage History is being reenabled, add all hidden
  // destinations back to the shown list. Destination Usage History only acts on
  // all destinations at once.
  if (!_destinationUsageHistoryEnabled.value &&
      _destinationCustomizationModel.destinationUsageEnabled) {
    _destinationOrderData.shownDestinations.insert(
        _destinationOrderData.shownDestinations.end(),
        _destinationOrderData.hiddenDestinations.begin(),
        _destinationOrderData.hiddenDestinations.end());
    _destinationOrderData.hiddenDestinations.clear();
    [self.destinationUsageHistory clearStoredClickData];
  }
  _destinationUsageHistoryEnabled.value =
      _destinationCustomizationModel.destinationUsageEnabled;
  [self flushDestinationsToPrefs];

  self.model.destinations = [self destinationsFromCurrentRanking];

  // Reset customization model so next customization can start fresh.
  _destinationCustomizationModel = nil;
}

- (void)cancelActionsUpdate {
  _actionCustomizationModel = nil;
}

- (void)cancelDestinationsUpdate {
  _destinationCustomizationModel = nil;
}

#pragma mark - Private

// Loads the stored destinations data from local prefs/disk.
- (void)loadDestinationsFromPrefs {
  // Fetch the stored list of newly-added, unclicked destinations, then update
  // `_untappedDestinations` with its data.
  AddDestinationsToSet(
      _localStatePrefs->GetList(prefs::kOverflowMenuNewDestinations),
      _untappedDestinations);

  if (IsOverflowMenuCustomizationEnabled()) {
    const base::Value::List& storedHiddenDestinations =
        _localStatePrefs->GetList(prefs::kOverflowMenuHiddenDestinations);
    AppendDestinationsToVector(storedHiddenDestinations,
                               _destinationOrderData.hiddenDestinations);
  }

  [self loadShownDestinationsPref];

  // If the customization flag was enabled in the past and users hid
  // destinations make sure to add those back to the shown list, if the flag
  // becomes disabled.
  if (!IsOverflowMenuCustomizationEnabled()) {
    const base::Value::List& storedHiddenDestinations =
        _localStatePrefs->GetList(prefs::kOverflowMenuHiddenDestinations);
    AppendDestinationsToVector(storedHiddenDestinations,
                               _destinationOrderData.shownDestinations);
    _localStatePrefs->ClearPref(prefs::kOverflowMenuHiddenDestinations);

    // If Destination Usage History needs to be reenabled, then clear any stored
    // data.
    if (!_destinationUsageHistoryEnabled.value) {
      _destinationUsageHistoryEnabled.value = YES;
      [self.destinationUsageHistory clearStoredClickData];
    }

    [self flushDestinationsToPrefs];
  }
}

// Loads and migrates the shown destinations pref from disk.
- (void)loadShownDestinationsPref {
  // First try to load new pref.
  const base::Value::List& storedRanking =
      _localStatePrefs->GetList(prefs::kOverflowMenuDestinationsOrder);
  if (storedRanking.size() > 0) {
    AppendDestinationsToVector(storedRanking,
                               _destinationOrderData.shownDestinations);
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

  AppendDestinationsToVector(oldRankingRef,
                             _destinationOrderData.shownDestinations);
  storedUsageHistoryUpdate->Remove(kRankingKey);
}

// Load the stored actions data from local prefs/disk.
- (void)loadActionsFromPrefs {
  const base::Value::Dict& storedActions =
      _localStatePrefs->GetDict(prefs::kOverflowMenuActionsOrder);
  ActionOrderData actionOrderData;

  const base::Value::List* shownActions =
      storedActions.FindList(kShownActionsKey);
  if (shownActions) {
    for (const auto& value : *shownActions) {
      if (!value.is_string()) {
        continue;
      }

      actionOrderData.shownActions.push_back(
          overflow_menu::ActionTypeForStringName(value.GetString()));
    }
  }

  const base::Value::List* hiddenActions =
      storedActions.FindList(kHiddenActionsKey);
  if (hiddenActions) {
    for (const auto& value : *hiddenActions) {
      if (!value.is_string()) {
        continue;
      }

      actionOrderData.hiddenActions.push_back(
          overflow_menu::ActionTypeForStringName(value.GetString()));
    }
  }
  _actionOrderData = actionOrderData;
}

// Write stored destination data back to local prefs/disk.
- (void)flushDestinationsToPrefs {
  if (!_localStatePrefs) {
    return;
  }

  // Flush the new destinations ranking to Prefs.
  base::Value::List ranking;

  for (overflow_menu::Destination destination :
       _destinationOrderData.shownDestinations) {
    ranking.Append(overflow_menu::StringNameForDestination(destination));
  }

  _localStatePrefs->SetList(prefs::kOverflowMenuDestinationsOrder,
                            std::move(ranking));

  // Flush list of hidden destinations to Prefs.
  if (IsOverflowMenuCustomizationEnabled()) {
    base::Value::List hiddenDestinations;

    for (overflow_menu::Destination destination :
         _destinationOrderData.hiddenDestinations) {
      hiddenDestinations.Append(
          overflow_menu::StringNameForDestination(destination));
    }

    _localStatePrefs->SetList(prefs::kOverflowMenuHiddenDestinations,
                              std::move(hiddenDestinations));
  }

  // Flush the new untapped destinations to Prefs.
  ScopedListPrefUpdate untappedDestinationsUpdate(
      _localStatePrefs, prefs::kOverflowMenuNewDestinations);

  untappedDestinationsUpdate->clear();

  for (overflow_menu::Destination untappedDestination : _untappedDestinations) {
    untappedDestinationsUpdate->Append(
        overflow_menu::StringNameForDestination(untappedDestination));
  }
}

// Write stored action data back to local prefs/disk.
- (void)flushActionsToPrefs {
  base::Value::Dict storedActions;

  base::Value::List shownActions;
  for (overflow_menu::ActionType action : _actionOrderData.shownActions) {
    shownActions.Append(overflow_menu::StringNameForActionType(action));
  }

  base::Value::List hiddenActions;
  for (overflow_menu::ActionType action : _actionOrderData.hiddenActions) {
    hiddenActions.Append(overflow_menu::StringNameForActionType(action));
  }

  storedActions.Set(kShownActionsKey, std::move(shownActions));
  storedActions.Set(kHiddenActionsKey, std::move(hiddenActions));

  _localStatePrefs->SetDict(prefs::kOverflowMenuActionsOrder,
                            std::move(storedActions));
}

// Creates a map from overflow_menu::Destination : OverflowMenuDestination*
// for fast retrieval of a given overflow_menu::Destination's corresponding
// Objective-C class.
- (DestinationLookup)destinationLookupMapFromDestinations:
    (NSArray<OverflowMenuDestination*>*)destinations {
  std::map<overflow_menu::Destination, OverflowMenuDestination*>
      destinationLookup;

  for (OverflowMenuDestination* carouselDestination in destinations) {
    overflow_menu::Destination destination =
        static_cast<overflow_menu::Destination>(
            carouselDestination.destination);
    destinationLookup[destination] = carouselDestination;
  }
  return destinationLookup;
}

// Modifies `_ranking` to re-order it based on the current badge status of the
// various destinations
- (void)applyBadgeOrderingToRankingWithAvailableDestinations:
    (DestinationRanking)availableDestinations {
  // Detect new destinations added to the carousel by feature teams. New
  // destinations (`newDestinations`) are those now found in the carousel
  // (`availableDestinations`), but not found in the ranking
  // (`_destinationOrderData`).
  std::set<overflow_menu::Destination> currentDestinations(
      availableDestinations.begin(), availableDestinations.end());

  std::set<overflow_menu::Destination> existingDestinations(
      _destinationOrderData.shownDestinations.begin(),
      _destinationOrderData.shownDestinations.end());

  existingDestinations.insert(_destinationOrderData.hiddenDestinations.begin(),
                              _destinationOrderData.hiddenDestinations.end());

  std::vector<overflow_menu::Destination> newDestinations;

  std::set_difference(currentDestinations.begin(), currentDestinations.end(),
                      existingDestinations.begin(), existingDestinations.end(),
                      std::back_inserter(newDestinations));

  for (overflow_menu::Destination newDestination : newDestinations) {
    _untappedDestinations.insert(newDestination);
  }

  // Make sure that all destinations that should end up in the final ranking do.
  std::set<overflow_menu::Destination> remainingDestinations =
      currentDestinations;

  for (overflow_menu::Destination hiddenDestination :
       _destinationOrderData.hiddenDestinations) {
    remainingDestinations.erase(hiddenDestination);
  }

  DestinationRanking sortedDestinations;

  // Reconstruct carousel based on current ranking.
  //
  // Add all ranked destinations that don't need be re-sorted back-to-back
  // following their ranking.
  //
  // Destinations that need to be re-sorted for highlight are not added here
  // where they are re-inserted later. These destinations have a badge and a
  // position of kNewDestinationsInsertionIndex or worst.
  for (overflow_menu::Destination rankedDestination :
       _destinationOrderData.shownDestinations) {
    if (remainingDestinations.contains(rankedDestination) &&
        !_untappedDestinations.contains(rankedDestination)) {
      OverflowMenuDestination* overflowMenuDestination =
          [self.destinationProvider
              destinationForDestinationType:rankedDestination];
      const bool dontSort =
          overflowMenuDestination.badge == BadgeTypeNone ||
          sortedDestinations.size() < kNewDestinationsInsertionIndex;

      if (dontSort) {
        sortedDestinations.push_back(rankedDestination);

        remainingDestinations.erase(rankedDestination);
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
      if (remainingDestinations.contains(untappedDestination)) {
        OverflowMenuDestination* overflowMenuDestination =
            [self.destinationProvider
                destinationForDestinationType:untappedDestination];
        if (overflowMenuDestination.badge != BadgeTypeNone) {
          continue;
        }
        overflowMenuDestination.badge = BadgeTypeNew;

        InsertDestination(untappedDestination, remainingDestinations,
                          sortedDestinations);
      }
    }
  }

  std::vector<overflow_menu::Destination> allDestinations;

  // Merge all destinations by prioritizing untapped destinations over ranked
  // destinations in their order of insertion.
  std::merge(_destinationOrderData.shownDestinations.begin(),
             _destinationOrderData.shownDestinations.end(),
             _untappedDestinations.begin(), _untappedDestinations.end(),
             std::back_inserter(allDestinations));

  // Insert the destinations with a badge that is not for an error at
  // kNewDestinationsInsertionIndex before the untapped destinations.
  for (overflow_menu::Destination destination : allDestinations) {
    if (remainingDestinations.contains(destination)) {
      OverflowMenuDestination* overflowMenuDestination =
          [self.destinationProvider destinationForDestinationType:destination];
      if (overflowMenuDestination.badge == BadgeTypeError) {
        continue;
      }
      InsertDestination(destination, remainingDestinations, sortedDestinations);
    }
  }

  // Insert the destinations with an error badge before the destinations with
  // other types of badges.
  for (overflow_menu::Destination destination : allDestinations) {
    if (remainingDestinations.contains(destination) &&
        [self.destinationProvider destinationForDestinationType:destination]) {
      InsertDestination(destination, remainingDestinations, sortedDestinations);
    }
  }

  // Check that all the destinations to show in the carousel were added to the
  // sorted destinations output at this point.
  DCHECK(remainingDestinations.empty());

  // Set the new ranking.
  _destinationOrderData.shownDestinations = sortedDestinations;

  [self flushDestinationsToPrefs];
}

// Uses the current `actionProvider` to add any new actions to the shown list.
// This handles new users with no stored data and new actions added.
- (void)updateActionOrderData {
  ActionRanking availableActions = [self.actionProvider basePageActions];

  // Add any available actions not present in shown or hidden to the shown list.
  std::set<overflow_menu::ActionType> knownActions(
      _actionOrderData.shownActions.begin(),
      _actionOrderData.shownActions.end());
  knownActions.insert(_actionOrderData.hiddenActions.begin(),
                      _actionOrderData.hiddenActions.end());

  std::set_difference(availableActions.begin(), availableActions.end(),
                      knownActions.begin(), knownActions.end(),
                      std::back_inserter(_actionOrderData.shownActions));

  [self flushActionsToPrefs];
}

// Uses the current `destinationProvider` to get the initial order of
// destinations for new users without an ordering.
- (void)initializeDestinationOrderDataIfEmpty {
  if (_destinationOrderData.empty()) {
    _destinationOrderData.shownDestinations =
        [self.destinationProvider baseDestinations];
  }
}

// Returns the current destinations in order.
- (NSArray<OverflowMenuDestination*>*)sortedDestinations {
  [self initializeDestinationOrderDataIfEmpty];

  DestinationRanking availableDestinations =
      [self.destinationProvider baseDestinations];

  if (_destinationUsageHistoryEnabled.value && self.destinationUsageHistory) {
    _destinationOrderData.shownDestinations = [self.destinationUsageHistory
        sortedDestinationsFromCurrentRanking:_destinationOrderData
                                                 .shownDestinations
                       availableDestinations:availableDestinations];

    [self flushDestinationsToPrefs];
  }

  [self applyBadgeOrderingToRankingWithAvailableDestinations:
            availableDestinations];

  return [self destinationsFromCurrentRanking];
}

// Returns the current pageActions in order.
- (NSArray<OverflowMenuAction*>*)pageActions {
  if (!IsOverflowMenuCustomizationEnabled()) {
    ActionRanking availableActions = [self.actionProvider basePageActions];
    // Convert back to Objective-C array for returning. This step also filters
    // out any actions that are not supported on the current page.
    NSMutableArray<OverflowMenuAction*>* sortedActions =
        [[NSMutableArray alloc] init];
    for (overflow_menu::ActionType action : availableActions) {
      if (OverflowMenuAction* overflowMenuAction =
              [self.actionProvider actionForActionType:action]) {
        [sortedActions addObject:overflowMenuAction];
      }
    }

    return sortedActions;
  }

  [self updateActionOrderData];

  // Convert back to Objective-C array for returning. This step also filters out
  // any actions that are not supported on the current page.
  NSMutableArray<OverflowMenuAction*>* sortedActions =
      [[NSMutableArray alloc] init];
  for (overflow_menu::ActionType action : _actionOrderData.shownActions) {
    if (OverflowMenuAction* overflowMenuAction =
            [self.actionProvider actionForActionType:action]) {
      [sortedActions addObject:overflowMenuAction];
    }
  }

  return sortedActions;
}

// Converts the current `_destinationOrderData` into an array of actual
// `OverflowMenuDestination` objects.
- (NSArray<OverflowMenuDestination*>*)destinationsFromCurrentRanking {
  // Convert back to Objective-C array for returning. This step also filters out
  // any destinations that are not supported on the current page.
  NSMutableArray<OverflowMenuDestination*>* sortedDestinations =
      [[NSMutableArray alloc] init];

  // Manually inject spotlight destination if it's supported.
  if (experimental_flags::IsSpotlightDebuggingEnabled()) {
    if (OverflowMenuDestination* spotlightDestination =
            [self.destinationProvider
                destinationForDestinationType:overflow_menu::Destination::
                                                  SpotlightDebugger]) {
      [sortedDestinations addObject:spotlightDestination];
    }
  }
  for (overflow_menu::Destination destination :
       _destinationOrderData.shownDestinations) {
    if (OverflowMenuDestination* overflowMenuDestination =
            [self.destinationProvider
                destinationForDestinationType:destination]) {
      [sortedDestinations addObject:overflowMenuDestination];
    }
  }

  return sortedDestinations;
}

@end
