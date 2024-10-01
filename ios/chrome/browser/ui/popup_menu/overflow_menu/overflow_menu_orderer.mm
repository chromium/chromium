// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_orderer.h"

#import <unordered_set>

#import "base/containers/contains.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_action_provider.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_metrics.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

namespace {
// The dictionary key used for storing rankings.
const char kRankingKey[] = "ranking";

// The dictionary key used for storing the shown action ordering.
const char kShownActionsKey[] = "shown";

// The dictionary key used for storing the hidden action ordering.
const char kHiddenActionsKey[] = "hidden";

// The dictionary key used for storing the impressions remaining.
const char kImpressionsRemainingKey[] = "impressions_remaining";

// The dictionary key used for storing the badge type.
const char kBadgeTypeKey[] = "badge_type";

// The dictionary key used for storing whether the badge is feature driven.
const char kIsFeatureDrivenBadgeKey[] = "is_feature_driven_badge";

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
                       DestinationRanking& output,
                       bool insertAtEnd) {
  if (insertAtEnd) {
    output.push_back(destination);
  } else {
    const int insertionIndex = std::min(
        output.size() - 1, static_cast<size_t>(kNewDestinationsInsertionIndex));

    output.insert(output.begin() + insertionIndex, destination);
  }

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

struct BadgeData {
  int impressionsRemaining;
  BadgeType badgeType;
  bool isFeatureDrivenBadge;
};

// Creates a `BadgeData` from the provided dict.
std::optional<BadgeData> BadgeDataFromDict(const base::Value::Dict& dict) {
  BadgeData badgeData;

  std::optional<int> impressionsRemaining =
      dict.FindInt(kImpressionsRemainingKey);
  if (!impressionsRemaining) {
    return std::nullopt;
  }
  badgeData.impressionsRemaining = impressionsRemaining.value();

  std::optional<bool> isFeatureDrivenBadge =
      dict.FindBool(kIsFeatureDrivenBadgeKey);
  if (!isFeatureDrivenBadge) {
    return std::nullopt;
  }
  badgeData.isFeatureDrivenBadge = isFeatureDrivenBadge.value();

  const std::string* badgeType = dict.FindString(kBadgeTypeKey);
  if (!badgeType) {
    return std::nullopt;
  }
  badgeData.badgeType = [OverflowMenuDestination
      badgeTypeFromString:base::SysUTF8ToNSString(*badgeType)];

  return badgeData;
}

// Creates a `base::Value::Dict` from the provided `BadgeData`.
base::Value::Dict DictFromBadgeData(const BadgeData badgeData) {
  std::string badgeTypeString = base::SysNSStringToUTF8(
      [OverflowMenuDestination stringFromBadgeType:badgeData.badgeType]);
  return base::Value::Dict()
      .Set(kImpressionsRemainingKey, badgeData.impressionsRemaining)
      .Set(kIsFeatureDrivenBadgeKey, badgeData.isFeatureDrivenBadge)
      .Set(kBadgeTypeKey, badgeTypeString);
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

  // New destinations recently added to the overflow menu carousel that have not
  // yet been clicked by the user.
  std::set<overflow_menu::Destination> _untappedDestinations;

  // The data for the current destinations ordering and show/hide state.
  DestinationOrderData _destinationOrderData;

  // The data for the current actions ordering and show/hide state.
  ActionOrderData _actionOrderData;

  PrefBackedBoolean* _destinationUsageHistoryEnabled;

  // The data for which destinations currently have badges and how many
  // impressions they have remaining.
  std::map<overflow_menu::Destination, BadgeData> _destinationBadgeData;
}

@synthesize actionCustomizationModel = _actionCustomizationModel;
@synthesize destinationCustomizationModel = _destinationCustomizationModel;

- (instancetype)initWithIsIncognito:(BOOL)isIncognito {
  if ((self = [super init])) {
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

- (BOOL)isDestinationCustomizationInProgress {
  return _destinationCustomizationModel != nil;
}

#pragma mark - Public

- (void)recordClickForDestination:(overflow_menu::Destination)destination {
  _untappedDestinations.erase(destination);

  if (base::Contains(_destinationBadgeData, destination) &&
      !_destinationBadgeData[destination].isFeatureDrivenBadge) {
    _destinationBadgeData.erase(destination);
  }

  [self flushDestinationsToPrefs];

  [self.destinationUsageHistory recordClickForDestination:destination];
}

- (void)reorderDestinationsForInitialMenu {
  [self initializeDestinationOrderDataIfEmpty];

  DestinationRanking availableDestinations =
      [self.destinationProvider baseDestinations];

  DestinationRanking badgedRanking =
      [self customizationRankingAfterBadgingWithAvailableDestinations:
                availableDestinations];
  _destinationOrderData.shownDestinations = badgedRanking;
  [self flushDestinationsToPrefs];

  // If customization is enabled, then skip destination usage history if there
  // are current badges, as those have more important positions.
  BOOL hasBadgeWithImpressions = NO;
  for (const auto& [key, value] : _destinationBadgeData) {
    if (value.impressionsRemaining > 0) {
      hasBadgeWithImpressions = YES;
      break;
    }
  }
  BOOL skipDestinationUsageHistory =
      (hasBadgeWithImpressions || !_destinationUsageHistoryEnabled.value);

  if (!skipDestinationUsageHistory && self.destinationUsageHistory) {
    _destinationOrderData.shownDestinations = [self.destinationUsageHistory
        sortedDestinationsFromCurrentRanking:_destinationOrderData
                                                 .shownDestinations
                       availableDestinations:availableDestinations];

    [self flushDestinationsToPrefs];
  }

  self.model.destinations = [self destinationsFromCurrentRanking];
}

- (void)updateDestinations {
  [self initializeDestinationOrderDataIfEmpty];
  self.model.destinations = [self destinationsFromCurrentRanking];
}

- (void)updatePageActions {
  [self.pageActionsGroup setActionsWithAnimation:[self pageActions]];
}

- (void)updateForMenuDisappearance {
  // If spotlight debugging is enabled, an extra destination is auto-inserted
  // at the beginning.
  NSUInteger badgeImpressionLastIndex =
      (experimental_flags::IsSpotlightDebuggingEnabled())
          ? kNewDestinationsInsertionIndex + 1
          : kNewDestinationsInsertionIndex;

  NSRange impressedRange = NSMakeRange(
      0, MIN(badgeImpressionLastIndex + 1, self.model.destinations.count));
  for (OverflowMenuDestination* menuDestination :
       [self.model.destinations subarrayWithRange:impressedRange]) {
    overflow_menu::Destination destination =
        static_cast<overflow_menu::Destination>(menuDestination.destination);
    auto it = _destinationBadgeData.find(destination);
    if (it == _destinationBadgeData.end()) {
      continue;
    }
    // If the badge is feature-driven, just decrease its impression count
    // until it hits 0. Otherwise, remove it when it hits 0.
    if (it->second.isFeatureDrivenBadge) {
      it->second.impressionsRemaining =
          std::max(0, it->second.impressionsRemaining - 1);
    } else {
      it->second.impressionsRemaining = it->second.impressionsRemaining - 1;
      if (it->second.impressionsRemaining <= 0) {
        _destinationBadgeData.erase(destination);
      }
    }
  }
}

- (void)commitActionsUpdate {
  if (!_actionCustomizationModel.hasChanged) {
    [self cancelActionsUpdate];
    return;
  }

  ActionOrderData actionOrderData;
  for (OverflowMenuAction* action in self.actionCustomizationModel
           .shownActions) {
    actionOrderData.shownActions.push_back(
        static_cast<overflow_menu::ActionType>(action.actionType));
  }

  for (OverflowMenuAction* action in self.actionCustomizationModel
           .hiddenActions) {
    actionOrderData.hiddenActions.push_back(
        static_cast<overflow_menu::ActionType>(action.actionType));
  }

  [self recordMetricsForActionCustomizationWithNewOrderData:actionOrderData];

  _actionOrderData = actionOrderData;
  [self flushActionsToPrefs];

  [self updatePageActions];

  // Reset customization model so next customization can start fresh.
  _actionCustomizationModel = nil;
}

- (void)commitDestinationsUpdate {
  if (!_destinationCustomizationModel.hasChanged) {
    [self cancelDestinationsUpdate];
    return;
  }

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

  [self recordMetricsForDestinationCustomizationWithNewOrderData:orderData];

  [self eraseBadgesAfterDestinationCustomization];

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

  [self.model
      setDestinationsWithAnimation:[self destinationsFromCurrentRanking]];

  // Reset customization model so next customization can start fresh.
  _destinationCustomizationModel = nil;
}

- (void)cancelActionsUpdate {
  _actionCustomizationModel = nil;
}

- (void)cancelDestinationsUpdate {
  _destinationCustomizationModel = nil;
}

- (void)customizationUpdateToggledShown:(BOOL)shown
                    forLinkedActionType:(overflow_menu::ActionType)actionType
                         actionSubtitle:(NSString*)actionSubtitle {
  if (!_actionCustomizationModel) {
    return;
  }

  OverflowMenuAction* correspondingAction;
  for (OverflowMenuAction* action in _actionCustomizationModel.actionsGroup
           .actions) {
    if (action.actionType == static_cast<int>(actionType)) {
      correspondingAction = action;
      break;
    }
  }

  if (!correspondingAction) {
    return;
  }

  if (shown) {
    correspondingAction.subtitle = nil;
  } else {
    correspondingAction.highlighted = YES;
    correspondingAction.subtitle = actionSubtitle;
  }
}

#pragma mark - Private

// Loads the stored destinations data from local prefs/disk.
- (void)loadDestinationsFromPrefs {
  // Fetch the stored list of newly-added, unclicked destinations, then update
  // `_untappedDestinations` with its data.
  AddDestinationsToSet(
      _localStatePrefs->GetList(prefs::kOverflowMenuNewDestinations),
      _untappedDestinations);

  const base::Value::List& storedHiddenDestinations =
      _localStatePrefs->GetList(prefs::kOverflowMenuHiddenDestinations);
  AppendDestinationsToVector(storedHiddenDestinations,
                             _destinationOrderData.hiddenDestinations);

  const base::Value::Dict& storedBadgeData =
      _localStatePrefs->GetDict(prefs::kOverflowMenuDestinationBadgeData);

  for (const auto&& [key, value] : storedBadgeData) {
    if (!value.is_dict()) {
      continue;
    }

    std::optional<BadgeData> badgeData = BadgeDataFromDict(value.GetDict());
    if (!badgeData) {
      continue;
    }

    overflow_menu::Destination destination =
        overflow_menu::DestinationForStringName(key);
    _destinationBadgeData[destination] = badgeData.value();
  }

  [self loadShownDestinationsPref];
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
  base::Value::List hiddenDestinations;

  for (overflow_menu::Destination destination :
       _destinationOrderData.hiddenDestinations) {
    hiddenDestinations.Append(
        overflow_menu::StringNameForDestination(destination));
  }

  _localStatePrefs->SetList(prefs::kOverflowMenuHiddenDestinations,
                            std::move(hiddenDestinations));

  // Flush dict of badge data to Prefs.
  base::Value::Dict badgeDataPref;
  for (const auto& [destination, badgeData] : _destinationBadgeData) {
    std::string destinationKey =
        overflow_menu::StringNameForDestination(destination);
    badgeDataPref.Set(destinationKey, DictFromBadgeData(badgeData));
  }

  _localStatePrefs->SetDict(prefs::kOverflowMenuDestinationBadgeData,
                            std::move(badgeDataPref));

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
  if (!_localStatePrefs) {
    return;
  }
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

// Returns the current pageActions in order.
- (NSArray<OverflowMenuAction*>*)pageActions {
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
    OverflowMenuDestination* overflowMenuDestination =
        [self.destinationProvider destinationForDestinationType:destination];
    if (overflowMenuDestination) {
      // If the orderer has stored badge data about this destination, the
      // badge type may need to be upgraded. However, don't replace badges
      // with less important ones. Specifically, error badges are most
      // important.
      auto it = _destinationBadgeData.find(destination);
      if (it != _destinationBadgeData.end()) {
        if (it->second.badgeType == BadgeTypeError ||
            overflowMenuDestination.badge == BadgeTypeNone) {
          overflowMenuDestination.badge = it->second.badgeType;
        }
      }
      [sortedDestinations addObject:overflowMenuDestination];
    }
  }

  return sortedDestinations;
}

#pragma mark - Badging Helpers

// Rerank the destinations, handling any badges and new items, using the new
// rules introduced during the customization project.
- (DestinationRanking)customizationRankingAfterBadgingWithAvailableDestinations:
    (DestinationRanking)availableDestinations {
  // First, update the stored badge data.
  [self updateBadgeDataWithAvailableDestinations:availableDestinations];

  // Make sure all destinations that should end up in the final ranking do.
  std::set<overflow_menu::Destination> remainingDestinations =
      [self shownDestinationsFromAvailableDestinations:availableDestinations];

  DestinationRanking newDestinationRanking;

  // Start by adding all items that don't have new positions. This is items with
  // no badge and items that appear in the first few spots already.
  for (overflow_menu::Destination destination :
       _destinationOrderData.shownDestinations) {
    if (!remainingDestinations.contains(destination)) {
      continue;
    }

    // Initial items are always added to the ranking, regardless of badge state.
    if (newDestinationRanking.size() < kNewDestinationsInsertionIndex) {
      InsertDestination(destination, remainingDestinations,
                        newDestinationRanking, true);
      continue;
    }

    // If item is badged with impressions remaining, it should be reordered to
    // a specific position and will be added later.
    if (base::Contains(_destinationBadgeData, destination) &&
        _destinationBadgeData[destination].impressionsRemaining > 0) {
      continue;
    }

    InsertDestination(destination, remainingDestinations, newDestinationRanking,
                      true);
  }

  // Iterate over the list of destinations with badges three times, inserting
  // them into the ranking. First, add items with new badges that aren't feature
  // driven, then add items with new badges that are feature driven, then add
  // items with error badges, so the final order is: error badges -> new feature
  // driven badges -> new items.
  for (auto& pair : _destinationBadgeData) {
    if (!remainingDestinations.contains(pair.first)) {
      continue;
    }

    if (pair.second.isFeatureDrivenBadge) {
      continue;
    }

    if (pair.second.badgeType == BadgeTypeNew ||
        pair.second.badgeType == BadgeTypePromo) {
      InsertDestination(pair.first, remainingDestinations,
                        newDestinationRanking, false);
    }
  }
  for (auto& pair : _destinationBadgeData) {
    if (!remainingDestinations.contains(pair.first)) {
      continue;
    }

    if (pair.second.badgeType == BadgeTypeNew ||
        pair.second.badgeType == BadgeTypePromo) {
      InsertDestination(pair.first, remainingDestinations,
                        newDestinationRanking, false);
    }
  }
  for (auto& pair : _destinationBadgeData) {
    if (!remainingDestinations.contains(pair.first)) {
      continue;
    }

    InsertDestination(pair.first, remainingDestinations, newDestinationRanking,
                      false);
  }

  // Check that all the destinations that were supposed to appear have been
  // added to the output at this point.
  DCHECK(remainingDestinations.empty());

  [self recordMetricsForBadgeReorderingWithNewDestinationRanking:
            newDestinationRanking];

  return newDestinationRanking;
}

// Modifies an updated ranking after re-ordering it based on the current badge
// status of the various destinations.
- (DestinationRanking)rankingAfterBadgingWithAvailableDestinations:
    (DestinationRanking)availableDestinations {
  DestinationRanking newDestinations =
      [self newDestinationsFromAvailableDestinations:availableDestinations];

  for (overflow_menu::Destination newDestination : newDestinations) {
    _untappedDestinations.insert(newDestination);
  }

  // Make sure that all destinations that should end up in the final ranking do.
  std::set<overflow_menu::Destination> remainingDestinations =
      [self shownDestinationsFromAvailableDestinations:availableDestinations];

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
        InsertDestination(rankedDestination, remainingDestinations,
                          sortedDestinations, true);
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
                          sortedDestinations, false);
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
      InsertDestination(destination, remainingDestinations, sortedDestinations,
                        false);
    }
  }

  // Insert the destinations with an error badge before the destinations with
  // other types of badges.
  for (overflow_menu::Destination destination : allDestinations) {
    if (remainingDestinations.contains(destination) &&
        [self.destinationProvider destinationForDestinationType:destination]) {
      InsertDestination(destination, remainingDestinations, sortedDestinations,
                        false);
    }
  }

  // Check that all the destinations to show in the carousel were added to the
  // sorted destinations output at this point.
  DCHECK(remainingDestinations.empty());

  return sortedDestinations;
}

// Erases any necessary badges after the user customizes their destination list.
// This completely removes all non-feature driven badges and removes all
// impressions from feature driven ones.
- (void)eraseBadgesAfterDestinationCustomization {
  _untappedDestinations.clear();

  for (auto it = _destinationBadgeData.begin();
       it != _destinationBadgeData.end();) {
    if (it->second.isFeatureDrivenBadge) {
      it->second.impressionsRemaining = 0;
      it++;
    } else {
      it = _destinationBadgeData.erase(it);
    }
  }

  [self.destinationProvider destinationCustomizationCompleted];
}

// Detects new destinations added to the carousel by feature teams. New
// destinations (`newDestinations`) are those now found in the carousel
// (`availableDestinations`), but not found in the ranking
// (`_destinationOrderData`).
- (DestinationRanking)newDestinationsFromAvailableDestinations:
    (DestinationRanking)availableDestinations {
  std::set<overflow_menu::Destination> currentDestinations(
      availableDestinations.begin(), availableDestinations.end());

  std::set<overflow_menu::Destination> existingDestinations(
      _destinationOrderData.shownDestinations.begin(),
      _destinationOrderData.shownDestinations.end());
  existingDestinations.insert(_destinationOrderData.hiddenDestinations.begin(),
                              _destinationOrderData.hiddenDestinations.end());

  DestinationRanking newDestinations;

  std::set_difference(currentDestinations.begin(), currentDestinations.end(),
                      existingDestinations.begin(), existingDestinations.end(),
                      std::back_inserter(newDestinations));
  return newDestinations;
}

// Returns the set of destinations that should be shown, given the current
// available ones. This is different from the stored
// `_destinationOrderData.shownDestinations` because it can include new
// destinations added in code and not stored in the ranking yet.
- (std::set<overflow_menu::Destination>)
    shownDestinationsFromAvailableDestinations:
        (DestinationRanking)availableDestinations {
  std::set<overflow_menu::Destination> shownDestinations(
      availableDestinations.begin(), availableDestinations.end());

  for (overflow_menu::Destination hiddenDestination :
       _destinationOrderData.hiddenDestinations) {
    shownDestinations.erase(hiddenDestination);
  }
  return shownDestinations;
}

// Updates the stored `_destinationBadgeData`, adding any new badges due to new
// destinations or new feature-driven badges, and removing any feature-driven
// badges that are no longer present.
- (void)updateBadgeDataWithAvailableDestinations:
    (DestinationRanking)availableDestinations {
  DestinationRanking newDestinations =
      [self newDestinationsFromAvailableDestinations:availableDestinations];

  for (overflow_menu::Destination newDestination : newDestinations) {
    _untappedDestinations.insert(newDestination);

    if (!base::Contains(_destinationBadgeData, newDestination)) {
      _destinationBadgeData[newDestination].badgeType = BadgeTypeNew;
      _destinationBadgeData[newDestination].impressionsRemaining = 3;
    }
  }

  std::map<overflow_menu::Destination, BadgeType> providerBadgeTypes;

  for (overflow_menu::Destination destination : availableDestinations) {
    BadgeType badgeType =
        [self.destinationProvider destinationForDestinationType:destination]
            .badge;
    // If `destination` is hidden and has an error badge, propagate that down to
    // a visible badge.
    if (badgeType == BadgeTypeError &&
        std::find(_destinationOrderData.hiddenDestinations.begin(),
                  _destinationOrderData.hiddenDestinations.end(),
                  destination) !=
            _destinationOrderData.hiddenDestinations.end()) {
      providerBadgeTypes[overflow_menu::Destination::Settings] = BadgeTypeError;
    } else {
      // Don't override a previously propagated error badge.
      providerBadgeTypes[destination] =
          (providerBadgeTypes[destination] == BadgeTypeError) ? BadgeTypeError
                                                              : badgeType;
    }
  }

  // Make sure all badges from the provider end up in the stored badge data.
  for (const auto& [destination, badgeType] : providerBadgeTypes) {
    if (badgeType != BadgeTypeNone) {
      // If this is a new badge, the current badge is not feature driven, or the
      // badge from the provider is different than the current, then update the
      // data. Otherwise, the badge is already known about.
      if (!base::Contains(_destinationBadgeData, destination) ||
          !_destinationBadgeData[destination].isFeatureDrivenBadge ||
          badgeType != _destinationBadgeData[destination].badgeType) {
        _destinationBadgeData[destination].badgeType = badgeType;
        _destinationBadgeData[destination].impressionsRemaining = 3;
        _destinationBadgeData[destination].isFeatureDrivenBadge = true;
      }
    }
  }

  // Clear any orderer-driven badges that have finished all impressions or
  // feature-driven badges that no longer have a badge from the destination
  // provider.
  for (auto it = _destinationBadgeData.begin();
       it != _destinationBadgeData.end();) {
    if (it->second.isFeatureDrivenBadge &&
        providerBadgeTypes[it->first] == BadgeTypeNone) {
      it = _destinationBadgeData.erase(it);
    } else if (!it->second.isFeatureDrivenBadge &&
               it->second.impressionsRemaining <= 0) {
      it = _destinationBadgeData.erase(it);
    } else {
      it++;
    }
  }
}

#pragma mark - Metrics helpers

// Records any necessary metrics for when destination customization takes place,
// given the new order data the user edited.
- (void)recordMetricsForDestinationCustomizationWithNewOrderData:
    (DestinationOrderData)orderData {
  BOOL smartSortingNewlyEnabled =
      !_destinationUsageHistoryEnabled.value &&
      _destinationCustomizationModel.destinationUsageEnabled;
  BOOL smartSortingNewlyDisabled =
      _destinationUsageHistoryEnabled.value &&
      !_destinationCustomizationModel.destinationUsageEnabled;
  if (smartSortingNewlyEnabled || smartSortingNewlyDisabled) {
    IOSOverflowMenuSmartSortingChange changeType;
    if (smartSortingNewlyEnabled) {
      changeType = IOSOverflowMenuSmartSortingChange::kNewlyEnabled;
    } else {
      changeType = IOSOverflowMenuSmartSortingChange::kNewlyDisabled;
    }
    base::UmaHistogramEnumeration("IOS.OverflowMenu.SmartSortingStateChange",
                                  changeType);
  }

  std::set<overflow_menu::Destination> oldShownDestinations(
      _destinationOrderData.shownDestinations.begin(),
      _destinationOrderData.shownDestinations.end());
  std::set<overflow_menu::Destination> newShownDestinations(
      orderData.shownDestinations.begin(), orderData.shownDestinations.end());

  DestinationRanking removedDestinations;
  std::set_difference(oldShownDestinations.begin(), oldShownDestinations.end(),
                      newShownDestinations.begin(), newShownDestinations.end(),
                      std::back_inserter(removedDestinations));
  DestinationRanking addedDestinations;
  std::set_difference(newShownDestinations.begin(), newShownDestinations.end(),
                      oldShownDestinations.begin(), oldShownDestinations.end(),
                      std::back_inserter(addedDestinations));

  for (overflow_menu::Destination destination : removedDestinations) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.DestinationRemoved",
        HistogramDestinationFromDestination(destination));
  }
  for (overflow_menu::Destination destination : addedDestinations) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.DestinationAdded",
        HistogramDestinationFromDestination(destination));
  }

  if (orderData.shownDestinations.size() >= 1) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.DestinationsReordered.FirstPosition",
        HistogramDestinationFromDestination(orderData.shownDestinations[0]));
  }

  if (orderData.shownDestinations.size() >= 2) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.DestinationsReordered.SecondPosition",
        HistogramDestinationFromDestination(orderData.shownDestinations[1]));
  }

  if (orderData.shownDestinations.size() >= 3) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.DestinationsReordered.ThirdPosition",
        HistogramDestinationFromDestination(orderData.shownDestinations[2]));
  }

  if (orderData.shownDestinations.size() >= 4) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.DestinationsReordered.FourthPosition",
        HistogramDestinationFromDestination(orderData.shownDestinations[3]));
  }

  DestinationsCustomizationEvent event;
  event.PutOrRemove(
      DestinationsCustomizationEventFields::kSmartSortingTurnedOff,
      smartSortingNewlyDisabled);
  event.PutOrRemove(DestinationsCustomizationEventFields::kSmartSortingTurnedOn,
                    smartSortingNewlyEnabled);
  event.PutOrRemove(DestinationsCustomizationEventFields::kSmartSortingIsOn,
                    _destinationCustomizationModel.destinationUsageEnabled);
  event.PutOrRemove(
      DestinationsCustomizationEventFields::kDestinationWasRemoved,
      removedDestinations.size() > 0);
  event.PutOrRemove(DestinationsCustomizationEventFields::kDestinationWasAdded,
                    addedDestinations.size() > 0);

  // Loop through initial and new shown destinations lists to see if a
  // reordering occurred.
  for (unsigned long oldIndex = 0, newIndex = 0;
       oldIndex < _destinationOrderData.shownDestinations.size() &&
       newIndex < orderData.shownDestinations.size();) {
    overflow_menu::Destination oldDestination =
        _destinationOrderData.shownDestinations[oldIndex];
    overflow_menu::Destination newDestination =
        orderData.shownDestinations[newIndex];

    bool destinationRemoved =
        std::find(removedDestinations.begin(), removedDestinations.end(),
                  oldDestination) != removedDestinations.end();
    // If either destination was added or removed, skip it.
    if (destinationRemoved) {
      oldIndex++;
      continue;
    }

    bool destinationAdded =
        std::find(addedDestinations.begin(), addedDestinations.end(),
                  newDestination) != addedDestinations.end();
    if (destinationAdded) {
      newIndex++;
      continue;
    }

    if (oldDestination == newDestination) {
      oldIndex++;
      newIndex++;
      continue;
    }
    event.Put(DestinationsCustomizationEventFields::kDestinationWasReordered);
    break;
  }
  RecordDestinationsCustomizationEvent(event);
}

// Records any necessary metrics for when action customization takes place,
// given the new order data the user edited.
- (void)recordMetricsForActionCustomizationWithNewOrderData:
    (ActionOrderData)orderData {
  std::set<overflow_menu::ActionType> oldShownActions(
      _actionOrderData.shownActions.begin(),
      _actionOrderData.shownActions.end());
  std::set<overflow_menu::ActionType> newShownActions(
      orderData.shownActions.begin(), orderData.shownActions.end());

  ActionRanking removedActions;
  std::set_difference(oldShownActions.begin(), oldShownActions.end(),
                      newShownActions.begin(), newShownActions.end(),
                      std::back_inserter(removedActions));
  ActionRanking addedActions;
  std::set_difference(newShownActions.begin(), newShownActions.end(),
                      oldShownActions.begin(), oldShownActions.end(),
                      std::back_inserter(addedActions));

  for (overflow_menu::ActionType action : removedActions) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.ActionRemoved",
        HistogramActionFromActionType(action));
  }

  for (overflow_menu::ActionType action : addedActions) {
    base::UmaHistogramEnumeration("IOS.OverflowMenu.Customization.ActionAdded",
                                  HistogramActionFromActionType(action));
  }

  if (orderData.shownActions.size() >= 1) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.ActionsReordered.FirstPosition",
        HistogramActionFromActionType(orderData.shownActions[0]));
  }

  if (orderData.shownActions.size() >= 2) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.ActionsReordered.SecondPosition",
        HistogramActionFromActionType(orderData.shownActions[1]));
  }

  if (orderData.shownActions.size() >= 3) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.ActionsReordered.ThirdPosition",
        HistogramActionFromActionType(orderData.shownActions[2]));
  }

  if (orderData.shownActions.size() >= 4) {
    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.Customization.ActionsReordered.FourthPosition",
        HistogramActionFromActionType(orderData.shownActions[3]));
  }

  ActionsCustomizationEvent event;
  event.PutOrRemove(ActionsCustomizationEventFields::kActionWasRemoved,
                    removedActions.size() > 0);
  event.PutOrRemove(ActionsCustomizationEventFields::kActionWasAdded,
                    addedActions.size() > 0);
  // Loop through initial and new shown actions lists to see if a reordering
  // occurred.
  for (unsigned long oldIndex = 0, newIndex = 0;
       oldIndex < _actionOrderData.shownActions.size() &&
       newIndex < orderData.shownActions.size();) {
    overflow_menu::ActionType oldAction =
        _actionOrderData.shownActions[oldIndex];
    overflow_menu::ActionType newAction = orderData.shownActions[newIndex];

    // If either action was added or removed, skip it.
    bool actionRemoved = std::find(removedActions.begin(), removedActions.end(),
                                   oldAction) != removedActions.end();
    if (actionRemoved) {
      oldIndex++;
      continue;
    }

    bool actionAdded = std::find(addedActions.begin(), addedActions.end(),
                                 newAction) != addedActions.end();
    if (actionAdded) {
      newIndex++;
      continue;
    }

    if (oldAction == newAction) {
      oldIndex++;
      newIndex++;
    } else {
      event.Put(ActionsCustomizationEventFields::kActionWasReordered);
      break;
    }
  }
  RecordActionsCustomizationEvent(event);
}

// Records any necessary metrics for when Chrome automatically reorders the menu
// due to badges on the items.
- (void)recordMetricsForBadgeReorderingWithNewDestinationRanking:
    (DestinationRanking)newDestinationRanking {
  // Check if the menu was reordered due to a badge specifically. The assumption
  // here is that if a reordered or newly added item has a badge, then the
  // reordering was due to the badge
  BOOL reorderedDueToNewBadge = NO;
  BOOL reorderedDueToErrorBadge = NO;
  std::unordered_set<overflow_menu::Destination> movedDestinations;
  unsigned long finalIndex = 0;
  for (unsigned long initialIndex = 0;
       initialIndex < _destinationOrderData.shownDestinations.size() &&
       finalIndex < newDestinationRanking.size();) {
    overflow_menu::Destination initialDestination =
        _destinationOrderData.shownDestinations[initialIndex];
    overflow_menu::Destination finalDestination =
        newDestinationRanking[finalIndex];

    // If initial destination is not in newDestinationRanking, then it was
    // removed.
    if (std::find(newDestinationRanking.begin(), newDestinationRanking.end(),
                  initialDestination) == newDestinationRanking.end()) {
      initialIndex++;
      continue;
    }

    // If final destination is not in the current ranking, then it was added.
    if (std::find(_destinationOrderData.shownDestinations.begin(),
                  _destinationOrderData.shownDestinations.end(),
                  finalDestination) ==
        _destinationOrderData.shownDestinations.end()) {
      CHECK(_destinationBadgeData[finalDestination].badgeType != BadgeTypeNone);
      if (_destinationBadgeData[finalDestination].badgeType == BadgeTypeError) {
        reorderedDueToErrorBadge = YES;
      } else {
        reorderedDueToNewBadge = YES;
      }
      finalIndex++;
      continue;
    }

    if (initialDestination == finalDestination) {
      initialIndex++;
      finalIndex++;
    } else if (movedDestinations.contains(initialDestination)) {
      initialIndex++;
    } else {
      CHECK(_destinationBadgeData[finalDestination].badgeType != BadgeTypeNone);

      // Store that this destination was moved, so when it is encountered in the
      // intial list later on, it can be skipped over.
      movedDestinations.insert(finalDestination);
      if (_destinationBadgeData[finalDestination].badgeType == BadgeTypeError) {
        reorderedDueToErrorBadge = YES;
      } else {
        reorderedDueToNewBadge = YES;
      }

      // In this step, badges only move items forward in the list, so assume
      // that the final ordering side is the one that moved.
      finalIndex++;
    }
  }

  // Check if any remaining destinations in the final list are new.
  for (; finalIndex < newDestinationRanking.size(); finalIndex++) {
    overflow_menu::Destination finalDestination =
        newDestinationRanking[finalIndex];

    bool destinationAdded =
        std::find(_destinationOrderData.shownDestinations.begin(),
                  _destinationOrderData.shownDestinations.end(),
                  finalDestination) ==
        _destinationOrderData.shownDestinations.end();
    if (destinationAdded) {
      CHECK(_destinationBadgeData[finalDestination].badgeType != BadgeTypeNone);
      if (_destinationBadgeData[finalDestination].badgeType == BadgeTypeError) {
        reorderedDueToErrorBadge = YES;
      } else {
        reorderedDueToNewBadge = YES;
      }
      finalIndex++;
      continue;
    }
  }

  if (reorderedDueToNewBadge || reorderedDueToErrorBadge) {
    IOSOverflowMenuReorderingReason reason;
    if (reorderedDueToNewBadge && reorderedDueToErrorBadge) {
      reason = IOSOverflowMenuReorderingReason::kBothBadges;
    } else if (reorderedDueToErrorBadge) {
      reason = IOSOverflowMenuReorderingReason::kErrorBadge;
    } else {
      reason = IOSOverflowMenuReorderingReason::kNewBadge;
    }

    base::UmaHistogramEnumeration(
        "IOS.OverflowMenu.DestinationsOrderChangedProgrammatically", reason);
  }
}

@end
