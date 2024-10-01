// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_orderer.h"

#import "base/strings/string_number_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_action_provider.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// The number of destinations immediately visible in the carousel when the
// overflow menu is opened.
//
// For the purposes of these unit tests, this value is
// statically declared below. In practice, this value is dynamically calculated
// based on device size.
static constexpr int kVisibleDestinationsCount = 5;

// A time delta from the Unix epoch to the beginning of the current day.
base::TimeDelta TodaysDay() {
  return base::Days(
      (base::Time::Now() - base::Time::UnixEpoch()).InDaysFloored());
}

OverflowMenuDestination* CreateOverflowMenuDestination(
    overflow_menu::Destination destination) {
  OverflowMenuDestination* result =
      [[OverflowMenuDestination alloc] initWithName:@"Foobar"
                                         symbolName:kSettingsSymbol
                                       systemSymbol:YES
                                   monochromeSymbol:NO
                            accessibilityIdentifier:@"Foobar"
                                 enterpriseDisabled:NO
                                displayNewLabelIcon:NO
                                            handler:^{
                                                // Do nothing
                                            }];

  result.destination = static_cast<NSInteger>(destination);

  return result;
}

OverflowMenuAction* CreateOverflowMenuAction(
    overflow_menu::ActionType actionType) {
  OverflowMenuAction* result =
      [[OverflowMenuAction alloc] initWithName:@"Foobar"
                                    symbolName:kSettingsSymbol
                                  systemSymbol:YES
                              monochromeSymbol:NO
                       accessibilityIdentifier:@"Foobar"
                            enterpriseDisabled:NO
                           displayNewLabelIcon:NO
                                       handler:^{
                                           // Do nothing
                                       }];

  result.actionType = static_cast<NSInteger>(actionType);

  return result;
}

}  // namespace

// Fake destination provider for test purposes.
@interface FakeOverflowMenuDestinationProvider
    : NSObject <OverflowMenuDestinationProvider>

@property(nonatomic, assign) DestinationRanking baseDestinations;

@property(nonatomic, assign) BOOL badgesCleared;

// By default, the provider will create a standard `OverflowMenuDestination`
// and return that in `-destinationForDestinationType:showAll:`. This will
// override that to return a custom destination.
- (void)storeCustomDestination:(OverflowMenuDestination*)destination
            forDestinationType:(overflow_menu::Destination)destinationType;

@end

@implementation FakeOverflowMenuDestinationProvider {
  std::map<overflow_menu::Destination, OverflowMenuDestination*>
      _destinationMap;
}

- (void)storeCustomDestination:(OverflowMenuDestination*)destination
            forDestinationType:(overflow_menu::Destination)destinationType {
  _destinationMap[destinationType] = destination;
}

- (OverflowMenuDestination*)destinationForDestinationType:
    (overflow_menu::Destination)destinationType {
  if (_destinationMap.contains(destinationType)) {
    return _destinationMap[destinationType];
  }
  return CreateOverflowMenuDestination(destinationType);
}

- (OverflowMenuDestination*)customizationDestinationForDestinationType:
    (overflow_menu::Destination)destinationType {
  return [self destinationForDestinationType:destinationType];
}

- (void)destinationCustomizationCompleted {
  self.badgesCleared = YES;
}

@end

// Fake action provider for test purposes.
@interface FakeOverflowMenuActionProvider
    : NSObject <OverflowMenuActionProvider>

@property(nonatomic, assign) ActionRanking basePageActions;

// By default, the provider will create a standard `OverflowMenuAction`
// and return that in `-actionForActionType:`. This will override
// that to return a custom action.
- (void)storeCustomAction:(OverflowMenuAction*)action
            forActionType:(overflow_menu::ActionType)actionType;

@end

@implementation FakeOverflowMenuActionProvider {
  std::map<overflow_menu::ActionType, OverflowMenuAction*> _actionMap;
}

- (void)storeCustomAction:(OverflowMenuAction*)action
            forActionType:(overflow_menu::ActionType)actionType {
  _actionMap[actionType] = action;
}

- (OverflowMenuAction*)actionForActionType:
    (overflow_menu::ActionType)actionType {
  if (_actionMap.contains(actionType)) {
    return _actionMap[actionType];
  }
  return CreateOverflowMenuAction(actionType);
}

- (OverflowMenuAction*)customizationActionForActionType:
    (overflow_menu::ActionType)actionType {
  return [self actionForActionType:actionType];
}

@end

class OverflowMenuOrdererTest : public PlatformTest {
 public:
  OverflowMenuOrdererTest() {}

 protected:
  void SetUp() override {
    destination_provider_ = [[FakeOverflowMenuDestinationProvider alloc] init];
    action_provider_ = [[FakeOverflowMenuActionProvider alloc] init];
  }

  void TearDown() override {
    [overflow_menu_orderer_ disconnect];

    PlatformTest::TearDown();
  }

  void InitializeOverflowMenuOrderer(BOOL isIncognito) {
    if (!prefs_) {
      CreatePrefs();
    }

    overflow_menu_model_ = [[OverflowMenuModel alloc] initWithDestinations:@[]
                                                              actionGroups:@[]];

    overflow_menu_orderer_ =
        [[OverflowMenuOrderer alloc] initWithIsIncognito:isIncognito];

    overflow_menu_orderer_.model = overflow_menu_model_;
    overflow_menu_orderer_.localStatePrefs = prefs_.get();
    overflow_menu_orderer_.visibleDestinationsCount = kVisibleDestinationsCount;
    overflow_menu_orderer_.destinationProvider = destination_provider_;
    overflow_menu_orderer_.actionProvider = action_provider_;
  }

  void InitializeOverflowMenuOrdererWithRanking(BOOL isIncognito,
                                                DestinationRanking ranking) {
    InitializeOverflowMenuOrderer(isIncognito);
    destination_provider_.baseDestinations = ranking;
    [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  }

  // Create pref registry for tests.
  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuDestinationUsageHistory, PrefRegistry::LOSSY_PREF);
    prefs_->registry()->RegisterListPref(prefs::kOverflowMenuNewDestinations,
                                         PrefRegistry::LOSSY_PREF);
    prefs_->registry()->RegisterListPref(prefs::kOverflowMenuDestinationsOrder);
    prefs_->registry()->RegisterListPref(
        prefs::kOverflowMenuHiddenDestinations);
    prefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuDestinationBadgeData);
    prefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuActionsOrder);
    prefs_->registry()->RegisterBooleanPref(
        prefs::kOverflowMenuDestinationUsageHistoryEnabled, true);
  }

  DestinationRanking SampleDestinations() {
    return {
        overflow_menu::Destination::Bookmarks,
        overflow_menu::Destination::History,
        overflow_menu::Destination::ReadingList,
        overflow_menu::Destination::Passwords,
        overflow_menu::Destination::Downloads,
        overflow_menu::Destination::RecentTabs,
        overflow_menu::Destination::SiteInfo,
        overflow_menu::Destination::Settings,
    };
  }

  ActionRanking SampleActions() {
    return {
        overflow_menu::ActionType::Follow,
        overflow_menu::ActionType::Bookmark,
        overflow_menu::ActionType::ReadingList,
        overflow_menu::ActionType::ClearBrowsingData,
        overflow_menu::ActionType::Translate,
        overflow_menu::ActionType::DesktopSite,
        overflow_menu::ActionType::FindInPage,
        overflow_menu::ActionType::TextZoom,
    };
  }

  DestinationRanking RankingFromDestinationArray(
      NSArray<OverflowMenuDestination*>* array) {
    DestinationRanking ranking;
    for (OverflowMenuDestination* destination : array) {
      ranking.push_back(
          static_cast<overflow_menu::Destination>(destination.destination));
    }
    return ranking;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  OverflowMenuOrderer* overflow_menu_orderer_;
  OverflowMenuModel* overflow_menu_model_;
  FakeOverflowMenuDestinationProvider* destination_provider_;
  FakeOverflowMenuActionProvider* action_provider_;
};

// Tests that the destination ranking pref gets populated after sorting once.
TEST_F(OverflowMenuOrdererTest, StoresInitialDestinationRanking) {
  InitializeOverflowMenuOrderer(NO);
  DestinationRanking sample_destinations = SampleDestinations();
  destination_provider_.baseDestinations = sample_destinations;
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  const base::Value::List& stored_ranking =
      prefs_->GetList(prefs::kOverflowMenuDestinationsOrder);

  EXPECT_EQ(stored_ranking.size(), sample_destinations.size());
}

// Tests that the old pref format (kOverflowMenuDestinationUsageHistory as a
// dict containing both usage history and ranking) is correctly migrated to the
// new format (kOverflowMenuDestinationUsageHistory containing just usage
// history and kOverflowMenuDestinationsOrder containing ranking).
TEST_F(OverflowMenuOrdererTest, MigratesDestinationRanking) {
  CreatePrefs();

  base::Value::List old_ranking =
      base::Value::List()
          .Append(overflow_menu::StringNameForDestination(
              overflow_menu::Destination::Bookmarks))
          .Append(overflow_menu::StringNameForDestination(
              overflow_menu::Destination::History));
  base::Value::Dict old_usage_history =
      base::Value::Dict()
          .Set(base::NumberToString(TodaysDay().InDays()),
               base::Value::Dict().Set(
                   overflow_menu::StringNameForDestination(
                       overflow_menu::Destination::Bookmarks),
                   5))
          .Set("ranking", old_ranking.Clone());

  prefs_->SetDict(prefs::kOverflowMenuDestinationUsageHistory,
                  std::move(old_usage_history));

  overflow_menu_orderer_ = [[OverflowMenuOrderer alloc] initWithIsIncognito:NO];

  // Set prefs here to force orderer to load and migrate.
  overflow_menu_orderer_.localStatePrefs = prefs_.get();

  const base::Value::List& new_ranking =
      prefs_->GetList(prefs::kOverflowMenuDestinationsOrder);
  const base::Value::Dict& new_usage_history =
      prefs_->GetDict(prefs::kOverflowMenuDestinationUsageHistory);

  EXPECT_EQ(new_ranking, old_ranking);
  EXPECT_EQ(1ul, new_usage_history.size());
}

TEST_F(OverflowMenuOrdererTest, InsertsNewDestinationInMiddleOfRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
      all_destinations[6],
  };

  // Creates `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  // Same as `current_destinations`, but has a new element,
  // `all_destinations[7]`, which should eventually be inserted starting at
  // position 4 in the carousel (this is the expected behavior defined by
  // product).
  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      all_destinations[6],
      // New destination
      all_destinations[7],
  };

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[3].destination),
            all_destinations[7]);
}

TEST_F(OverflowMenuOrdererTest, InsertsNewDestinationsInMiddleOfRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  // Same as `current_destinations`, but has new elements (`all_destinations[6]`
  // and `all_destinations[7]`) inserted starting at position 4 in the carousel
  // (this is the expected behavior defined by product).
  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
      all_destinations[7],
  };

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[7]);

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[6]);
}

TEST_F(OverflowMenuOrdererTest, InsertsAndRemovesNewDestinationsInRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      // NOTE: all_destinations[0] was removed
      // NOTE: all_destinations[1] was removed
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
      all_destinations[7],
  };

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[0].destination),
            all_destinations[2]);

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[7]);

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[6]);
}

// Tests that the destinations that have a badge are moved in the middle of the
// ranking to get the user's attention; before the untapped destinations.
TEST_F(OverflowMenuOrdererTest, MoveBadgedDestinationsInRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
  };

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[4]);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[4]];

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[4]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[6]);
}

// Tests that the destinations that have an error badge have priority over the
// other badges when they are moved.
TEST_F(OverflowMenuOrdererTest, PriorityToErrorBadgeOverOtherBadges) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  OverflowMenuDestination* destination5 =
      CreateOverflowMenuDestination(all_destinations[5]);
  destination5.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination5
                             forDestinationType:all_destinations[5]];

  OverflowMenuDestination* destination3 =
      CreateOverflowMenuDestination(all_destinations[3]);
  destination3.badge = BadgeTypePromo;
  [destination_provider_ storeCustomDestination:destination3
                             forDestinationType:all_destinations[3]];

  destination_provider_.baseDestinations = current_destinations;

  // Initializes `OverflowMenuOrderer`.
  InitializeOverflowMenuOrderer(NO);

  // Set the initial ranking to `current_destinations`.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[5]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[3]);
}

// Tests that the destinations that have a badge but are in a better position
// than kNewDestinationsInsertionIndex won't be moved hence not demoted.
TEST_F(OverflowMenuOrdererTest, DontMoveBadgedDestinationWithGoodRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[0]);
  destination.badge = BadgeTypePromo;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[0]];

  destination_provider_.baseDestinations = current_destinations;

  // Initializes `OverflowMenuOrderer`.
  InitializeOverflowMenuOrderer(NO);

  // Set the initial ranking to `current_destinations`.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  // Verify that the destination with a badge and with a better ranking than
  // kNewDestinationsInsertionIndex wasn't moved.
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[0].destination),
            all_destinations[0]);
}

// Tests that if a destination is both new and has a badge, it will be inserted
// before the other destinations that are only new without a badge assigned.
TEST_F(OverflowMenuOrdererTest, PriorityToBadgeOverNewDestinationStatus) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      // New destinations
      all_destinations[5],
      all_destinations[6],
      all_destinations[7],
  };

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[6]);
  destination.badge = BadgeTypeNew;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[6]];

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[6]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[7]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 2]
              .destination),
      all_destinations[5]);
}

// Tests that if a destination is both new and has a badge, it will be inserted
// before the other destinations wtih a badge of the same priority that are not
// new.
TEST_F(OverflowMenuOrdererTest, PriorityToNewDestinationWithBadge) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
      all_destinations[7],
  };

  OverflowMenuDestination* destination4 =
      CreateOverflowMenuDestination(all_destinations[4]);
  destination4.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination4
                             forDestinationType:all_destinations[4]];

  OverflowMenuDestination* destination5 =
      CreateOverflowMenuDestination(all_destinations[5]);
  destination5.badge = BadgeTypePromo;
  [destination_provider_ storeCustomDestination:destination5
                             forDestinationType:all_destinations[5]];

  OverflowMenuDestination* destination7 =
      CreateOverflowMenuDestination(all_destinations[7]);
  destination7.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination7
                             forDestinationType:all_destinations[7]];

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[7]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[4]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 2]
              .destination),
      all_destinations[5]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 3]
              .destination),
      all_destinations[6]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 4]
              .destination),
      all_destinations[3]);
}

// Tests that the destinations are still promoted when there is no usage
// history ranking.
TEST_F(OverflowMenuOrdererTest, TestNewDestinationsWhenNoHistoryUsageRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
      all_destinations[6],
  };

  // Creates `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(YES, current_destinations);

  // Same as `current_destinations`, but has a new element,
  // `all_destinations[7]`, which should eventually be inserted starting at
  // position 4 in the carousel (this is the expected behavior defined by
  // product).
  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      all_destinations[6],
      // New destination
      all_destinations[7],
  };

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[3].destination),
            all_destinations[7]);
}

TEST_F(OverflowMenuOrdererTest, MovesBadgedDestinationsWithNoUsageHistory) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
  };

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[4]);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[4]];

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  DestinationRanking updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);

  ASSERT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            all_destinations[4]);
  ASSERT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            all_destinations[6]);
}

// Tests that the action ranking pref gets populated after sorting once.
TEST_F(OverflowMenuOrdererTest, StoresInitialActionRanking) {
  InitializeOverflowMenuOrderer(NO);
  ActionRanking sample_actions = SampleActions();
  action_provider_.basePageActions = sample_actions;

  [overflow_menu_orderer_ updatePageActions];

  const base::Value::Dict& stored_ranking =
      prefs_->GetDict(prefs::kOverflowMenuActionsOrder);

  EXPECT_EQ(stored_ranking.size(), 2u);
  EXPECT_EQ(stored_ranking.FindList("shown")->size(), sample_actions.size());
}

// Tests that reenabling destnation usage history clears the destination usage
// history and moves all destinations back to shown.
TEST_F(OverflowMenuOrdererTest, EnablingDestinationUsageHistory) {
  CreatePrefs();

  // Set the pref state to what it would be with destination usage history
  // disabled and some destinations hidden.
  base::Value::List shown_destinations =
      base::Value::List()
          .Append(overflow_menu::StringNameForDestination(
              overflow_menu::Destination::ReadingList))
          .Append(overflow_menu::StringNameForDestination(
              overflow_menu::Destination::Downloads));
  base::Value::List hidden_destinations =
      base::Value::List()
          .Append(overflow_menu::StringNameForDestination(
              overflow_menu::Destination::Bookmarks))
          .Append(overflow_menu::StringNameForDestination(
              overflow_menu::Destination::History));
  prefs_->SetList(prefs::kOverflowMenuDestinationsOrder,
                  std::move(shown_destinations));
  prefs_->SetList(prefs::kOverflowMenuHiddenDestinations,
                  std::move(hidden_destinations));

  prefs_->SetBoolean(prefs::kOverflowMenuDestinationUsageHistoryEnabled, false);

  // Destination Usage History prefs
  base::TimeDelta day = TodaysDay() - base::Days(1);
  std::string bookmarkName = overflow_menu::StringNameForDestination(
      overflow_menu::Destination::Bookmarks);
  base::Value::Dict day_history =
      base::Value::Dict().Set(base::NumberToString(day.InDays()),
                              base::Value::Dict().Set(bookmarkName, 1));
  prefs_->SetDict(prefs::kOverflowMenuDestinationUsageHistory,
                  std::move(day_history));

  overflow_menu_orderer_ = [[OverflowMenuOrderer alloc] initWithIsIncognito:NO];

  overflow_menu_model_ = [[OverflowMenuModel alloc] initWithDestinations:@[]
                                                            actionGroups:@[]];
  overflow_menu_orderer_.model = overflow_menu_model_;
  overflow_menu_orderer_.localStatePrefs = prefs_.get();
  overflow_menu_orderer_.visibleDestinationsCount = kVisibleDestinationsCount;
  overflow_menu_orderer_.destinationProvider = destination_provider_;
  overflow_menu_orderer_.actionProvider = action_provider_;

  // Reenable Destination Usage History.
  DestinationCustomizationModel* customizationModel =
      overflow_menu_orderer_.destinationCustomizationModel;
  customizationModel.destinationUsageEnabled = YES;
  [overflow_menu_orderer_ commitDestinationsUpdate];

  ASSERT_EQ([overflow_menu_model_.destinations count], 4u);
  EXPECT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[0].destination),
            overflow_menu::Destination::ReadingList);
  EXPECT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[1].destination),
            overflow_menu::Destination::Downloads);
  EXPECT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[2].destination),
            overflow_menu::Destination::Bookmarks);
  EXPECT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[3].destination),
            overflow_menu::Destination::History);

  const base::Value::Dict& new_history =
      prefs_->GetDict(prefs::kOverflowMenuDestinationUsageHistory);
  EXPECT_EQ(new_history.size(), 0u);
}

// Tests that new actions in code are added to the ranking
TEST_F(OverflowMenuOrdererTest, AddsNewActionsToRanking) {
  CreatePrefs();

  // Set the pref state what it would be with a few actions shown and hidden.
  base::Value::Dict actions_order =
      base::Value::Dict()
          .Set("shown", base::Value::List()
                            .Append(overflow_menu::StringNameForActionType(
                                overflow_menu::ActionType::Follow))
                            .Append(overflow_menu::StringNameForActionType(
                                overflow_menu::ActionType::Bookmark)))
          .Set("hidden",
               base::Value::List()
                   .Append(overflow_menu::StringNameForActionType(
                       overflow_menu::ActionType::ReadingList))
                   .Append(overflow_menu::StringNameForActionType(
                       overflow_menu::ActionType::ClearBrowsingData)));

  prefs_->SetDict(prefs::kOverflowMenuActionsOrder, std::move(actions_order));

  InitializeOverflowMenuOrderer(NO);
  ActionRanking sample_actions = SampleActions();
  action_provider_.basePageActions = sample_actions;

  OverflowMenuActionGroup* group =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"test"
                                                 actions:@[]
                                                  footer:nil];
  overflow_menu_orderer_.pageActionsGroup = group;

  [overflow_menu_orderer_ updatePageActions];

  ASSERT_EQ(group.actions.count, 6u);
  // The action order should first be shown actions, and then any new actions in
  // order.
  EXPECT_EQ(static_cast<overflow_menu::ActionType>(group.actions[0].actionType),
            overflow_menu::ActionType::Follow);
  EXPECT_EQ(static_cast<overflow_menu::ActionType>(group.actions[1].actionType),
            overflow_menu::ActionType::Bookmark);
  EXPECT_EQ(static_cast<overflow_menu::ActionType>(group.actions[2].actionType),
            overflow_menu::ActionType::Translate);
  EXPECT_EQ(static_cast<overflow_menu::ActionType>(group.actions[3].actionType),
            overflow_menu::ActionType::DesktopSite);
  EXPECT_EQ(static_cast<overflow_menu::ActionType>(group.actions[4].actionType),
            overflow_menu::ActionType::FindInPage);
  EXPECT_EQ(static_cast<overflow_menu::ActionType>(group.actions[5].actionType),
            overflow_menu::ActionType::TextZoom);
}

// Tests that when there is a badged item, the overflow menu orderer doesn't
// change the order via the destination usage history.
TEST_F(OverflowMenuOrdererTest, NoDestinationUsageHistoryWithBadge) {
  DestinationRanking all_destinations = SampleDestinations();

  // Destination 6 will be badged and destination 5 will be tapped many times,
  // so it typically would be reordered.
  overflow_menu::Destination badged_destination = all_destinations[6];
  overflow_menu::Destination tapped_destination = all_destinations[5];
  DestinationRanking initial_ranking = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], tapped_destination,
      badged_destination,
  };

  InitializeOverflowMenuOrdererWithRanking(NO, initial_ranking);
  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(badged_destination);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:badged_destination];

  // Tap the last destination many times, so it would typically be reordered.
  for (int i = 0; i < 5; i++) {
    [overflow_menu_orderer_ recordClickForDestination:tapped_destination];
  }

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  DestinationRanking updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  // The expected result is that the badged destination is moved, but the tapped
  // destination is not because there is an active badge.
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            badged_destination);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  // After 2 more impressions, the badge will no longer affect ordering.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            badged_destination);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            badged_destination);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  // Now, on the next reordering, destination usage history should take effect.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  [overflow_menu_orderer_ updateForMenuDisappearance];
  EXPECT_EQ(updated_ranking[0], tapped_destination);
}

// Tests that a newly added menu item only has a new badge for a short time.
TEST_F(OverflowMenuOrdererTest, NewItemOnlyHasBadgeForShortTime) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking initial_ranking = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4],
  };

  InitializeOverflowMenuOrdererWithRanking(NO, initial_ranking);

  overflow_menu::Destination new_destination = all_destinations[5];
  DestinationRanking ranking_with_new = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], new_destination,
  };
  destination_provider_.baseDestinations = ranking_with_new;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  // The new item should be moved up.
  EXPECT_EQ(
      RankingFromDestinationArray(
          overflow_menu_model_.destinations)[kNewDestinationsInsertionIndex],
      new_destination);
  EXPECT_EQ(
      overflow_menu_model_.destinations[kNewDestinationsInsertionIndex].badge,
      BadgeTypeNew);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  // For the next 2 impressions, the destination should still have a badge
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  EXPECT_EQ(
      overflow_menu_model_.destinations[kNewDestinationsInsertionIndex].badge,
      BadgeTypeNew);
  [overflow_menu_orderer_ updateForMenuDisappearance];
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  EXPECT_EQ(
      overflow_menu_model_.destinations[kNewDestinationsInsertionIndex].badge,
      BadgeTypeNew);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  // Now, on the next reordering, destination should no longer have a badge.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  EXPECT_EQ(
      overflow_menu_model_.destinations[kNewDestinationsInsertionIndex].badge,
      BadgeTypeNone);
  [overflow_menu_orderer_ updateForMenuDisappearance];
}

// Tests that if two items are badged, with one being below the threshold, its
// impression counter doesn't count down until the first item's counter hits 0.
TEST_F(OverflowMenuOrdererTest, TwoBadgesOnlyOneCountsImpressions) {
  DestinationRanking all_destinations = SampleDestinations();

  // Destinations 6 and 7 will be badged and destination 5 will be tapped many
  // times, so it typically would be reordered.
  overflow_menu::Destination badged_destination1 = all_destinations[6];
  overflow_menu::Destination badged_destination2 = all_destinations[7];
  overflow_menu::Destination tapped_destination = all_destinations[5];
  DestinationRanking initial_ranking = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], tapped_destination,
      badged_destination1, badged_destination2,
  };

  InitializeOverflowMenuOrdererWithRanking(NO, initial_ranking);
  OverflowMenuDestination* destination1 =
      CreateOverflowMenuDestination(badged_destination1);
  destination1.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination1
                             forDestinationType:badged_destination1];
  OverflowMenuDestination* destination2 =
      CreateOverflowMenuDestination(badged_destination2);
  destination2.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination2
                             forDestinationType:badged_destination2];

  // Tap the last destination many times, so it would typically be reordered.
  for (int i = 0; i < 5; i++) {
    [overflow_menu_orderer_ recordClickForDestination:tapped_destination];
  }

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  DestinationRanking updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  // The expected result is that the badged destinations are moved, but the
  // tapped destination is not because there is an active badge.
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            badged_destination2);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            badged_destination1);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  // After 2 more impressions, the first badge will no longer affect ordering.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            badged_destination2);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            badged_destination1);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            badged_destination2);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            badged_destination1);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  // Now, on the next reordering, the second badged item should be in prime
  // position.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            badged_destination1);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            badged_destination2);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            badged_destination1);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            badged_destination2);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            badged_destination1);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            badged_destination2);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  // Finally, now that 6 impressions have happened, the tapped item will move.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[0], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];
}

// Tests that if two items are badged at the front of the list, their impression
// counters count down simultaneously.
TEST_F(OverflowMenuOrdererTest, TwoBadgesAtBeginningCountTogether) {
  DestinationRanking all_destinations = SampleDestinations();

  // Destinations 0 and 1 will be badged and destination 7 will be tapped many
  // times, so it typically would be reordered.
  overflow_menu::Destination badged_destination1 = all_destinations[0];
  overflow_menu::Destination badged_destination2 = all_destinations[1];
  overflow_menu::Destination tapped_destination = all_destinations[7];
  DestinationRanking initial_ranking = {
      badged_destination1, badged_destination2, all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
      all_destinations[6], tapped_destination,
  };

  InitializeOverflowMenuOrdererWithRanking(NO, initial_ranking);
  OverflowMenuDestination* destination1 =
      CreateOverflowMenuDestination(badged_destination1);
  destination1.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination1
                             forDestinationType:badged_destination1];
  OverflowMenuDestination* destination2 =
      CreateOverflowMenuDestination(badged_destination2);
  destination2.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination2
                             forDestinationType:badged_destination2];

  // Tap the last destination many times, so it would typically be reordered.
  for (int i = 0; i < 5; i++) {
    [overflow_menu_orderer_ recordClickForDestination:tapped_destination];
  }

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  DestinationRanking updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  // The expected result is that the badged destinations have not moved because
  // they are already visible, and the tapped destination has not because there
  // is an active badge.
  EXPECT_EQ(updated_ranking[0], badged_destination1);
  EXPECT_EQ(updated_ranking[1], badged_destination2);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  // After 2 more impressions, the badges will no longer affect ordering.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[0], badged_destination1);
  EXPECT_EQ(updated_ranking[1], badged_destination2);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[0], badged_destination1);
  EXPECT_EQ(updated_ranking[1], badged_destination2);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  // Finally, now that all badges have run out of impressions, the tapped item
  // will move.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[0], tapped_destination);
  [overflow_menu_orderer_ updateForMenuDisappearance];
}

// Tests that if an item has a new badge and the destinations are customized,
// those badges are cleared
TEST_F(OverflowMenuOrdererTest, CustomizingDestinationsClearsBadgeImpressions) {
  DestinationRanking all_destinations = SampleDestinations();

  // Destination 0 will be badged and destination 6 will be tapped many times,
  // so it typically would be reordered. Destination 7 is new.
  overflow_menu::Destination badged_destination = all_destinations[0];
  overflow_menu::Destination tapped_destination = all_destinations[6];
  DestinationRanking initial_ranking = {
      badged_destination,  all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
      tapped_destination,
  };

  InitializeOverflowMenuOrdererWithRanking(NO, initial_ranking);
  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(badged_destination);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:badged_destination];

  // Tap the last destination many times, so it would typically be reordered.
  for (int i = 0; i < 5; i++) {
    [overflow_menu_orderer_ recordClickForDestination:tapped_destination];
  }

  overflow_menu::Destination new_destination = all_destinations[7];
  DestinationRanking ranking_with_new = {initial_ranking[0], initial_ranking[1],
                                         initial_ranking[2], initial_ranking[3],
                                         initial_ranking[4], initial_ranking[5],
                                         initial_ranking[6], new_destination};
  destination_provider_.baseDestinations = ranking_with_new;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  DestinationRanking updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  // The expected result is that the badged error destination has not moved
  // because it is already visible, the new destination has moved and has a
  // badge, and the tapped destination is not because there is an active badge.
  EXPECT_EQ(updated_ranking[0], badged_destination);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex], new_destination);
  EXPECT_EQ(updated_ranking[updated_ranking.size() - 1], tapped_destination);
  EXPECT_FALSE(destination_provider_.badgesCleared);

  // Edit the menu, which should clear the badge impressions remaining.
  DestinationCustomizationModel* customizationModel =
      overflow_menu_orderer_.destinationCustomizationModel;
  OverflowMenuDestination* destinationToHide =
      customizationModel.shownDestinations[6];
  ASSERT_EQ(destinationToHide.destination,
            static_cast<int>(initial_ranking[5]));
  destinationToHide.shown = NO;
  customizationModel.destinationUsageEnabled = YES;
  [overflow_menu_orderer_ commitDestinationsUpdate];

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  // With badges cleared, the tapped item should move again
  EXPECT_EQ(updated_ranking[0], tapped_destination);
  EXPECT_TRUE(destination_provider_.badgesCleared);
}

// Variant of `InsertsNewDestinationInMiddleOfRanking` with customization flag
// enabled.
TEST_F(OverflowMenuOrdererTest,
       Customization_InsertsNewDestinationInMiddleOfRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
      all_destinations[6],
  };

  // Creates `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  // Same as `current_destinations`, but has a new element,
  // `all_destinations[7]`, which should eventually be inserted starting at
  // position 4 in the carousel (this is the expected behavior defined by
  // product).
  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      all_destinations[6],
      // New destination
      all_destinations[7],
  };

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[3].destination),
            all_destinations[7]);
}

// Variant of `InsertsNewDestinationsInMiddleOfRanking` with customization flag
// enabled.
TEST_F(OverflowMenuOrdererTest,
       Customization_InsertsNewDestinationsInMiddleOfRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  // Same as `current_destinations`, but has new elements (`all_destinations[6]`
  // and `all_destinations[7]`) inserted starting at position 4 in the carousel
  // (this is the expected behavior defined by product).
  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
      all_destinations[7],
  };

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[7]);

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[6]);
}

// Variant of `InsertsAndRemovesNewDestinationsInRanking` with customization
// flag enabled.
TEST_F(OverflowMenuOrdererTest,
       Customization_InsertsAndRemovesNewDestinationsInRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      // NOTE: all_destinations[0] was removed
      // NOTE: all_destinations[1] was removed
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
      all_destinations[7],
  };

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[0].destination),
            all_destinations[2]);

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[7]);

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[6]);
}

// Variant of `MoveBadgedDestinationsInRanking` with customization flag enabled.
// Tests that the destinations that have a badge are moved in the middle of the
// ranking to get the user's attention; before the untapped destinations.
TEST_F(OverflowMenuOrdererTest, Customization_MoveBadgedDestinationsInRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
  };

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[4]);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[4]];

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[4]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[6]);
}

// Variant of `PriorityToErrorBadgeOverOtherBadges` with customization flag
// enabled. Tests that the destinations that have an error badge have priority
// over the other badges when they are moved.
TEST_F(OverflowMenuOrdererTest,
       Customization_PriorityToErrorBadgeOverOtherBadges) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  OverflowMenuDestination* destination5 =
      CreateOverflowMenuDestination(all_destinations[5]);
  destination5.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination5
                             forDestinationType:all_destinations[5]];

  OverflowMenuDestination* destination3 =
      CreateOverflowMenuDestination(all_destinations[3]);
  destination3.badge = BadgeTypePromo;
  [destination_provider_ storeCustomDestination:destination3
                             forDestinationType:all_destinations[3]];

  destination_provider_.baseDestinations = current_destinations;

  // Initializes `OverflowMenuOrderer`.
  InitializeOverflowMenuOrderer(NO);

  // Set the initial ranking to `current_destinations`.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex]
              .destination),
      all_destinations[5]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          overflow_menu_model_.destinations[kNewDestinationsInsertionIndex + 1]
              .destination),
      all_destinations[3]);
}

// Variant of `DontMoveBadgedDestinationWithGoodRanking` with customization flag
// enabled. Tests that the destinations that have a badge but are in a better
// position than kNewDestinationsInsertionIndex won't be moved hence not
// demoted.
TEST_F(OverflowMenuOrdererTest,
       Customization_DontMoveBadgedDestinationWithGoodRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[0]);
  destination.badge = BadgeTypePromo;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[0]];

  destination_provider_.baseDestinations = current_destinations;

  // Initializes `OverflowMenuOrderer`.
  InitializeOverflowMenuOrderer(NO);

  // Set the initial ranking to `current_destinations`.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  // Verify that the destination with a badge and with a better ranking than
  // kNewDestinationsInsertionIndex wasn't moved.
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[0].destination),
            all_destinations[0]);
}

// Variant of `TestNewDestinationsWhenNoHistoryUsageRanking` with customization
// flag enabled. Tests that the destinations are still promoted when there is no
// usage history ranking.
TEST_F(OverflowMenuOrdererTest,
       Customization_TestNewDestinationsWhenNoHistoryUsageRanking) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
      all_destinations[6],
  };

  // Creates `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(YES, current_destinations);

  // Same as `current_destinations`, but has a new element,
  // `all_destinations[7]`, which should eventually be inserted starting at
  // position 4 in the carousel (this is the expected behavior defined by
  // product).
  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      all_destinations[6],
      // New destination
      all_destinations[7],
  };

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[3].destination),
            all_destinations[7]);
}

// Variant of `MovesBadgedDestinationsWithNoUsageHistory` with customization
// flag enabled.
TEST_F(OverflowMenuOrdererTest,
       Customization_MovesBadgedDestinationsWithNoUsageHistory) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
  };

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[4]);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[4]];

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  DestinationRanking updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);

  ASSERT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            all_destinations[4]);
  ASSERT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            all_destinations[6]);
}

// Tests that calling the simple `-updateDestinations` method doesn't reorder
// the menu.
TEST_F(OverflowMenuOrdererTest, UpdateDoesntReorderMenu) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[5]);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[5]];

  [overflow_menu_orderer_ updateDestinations];

  // The badged destination should not have changed position.
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                overflow_menu_model_.destinations[5].destination),
            all_destinations[5]);
}

// Tests that if a hidden destination gets an error badge, that badge appears on
// settings instead.
TEST_F(OverflowMenuOrdererTest, HiddenDestinationPropagatesErrorBadge) {

  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      overflow_menu::Destination::Settings,
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
  };

  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  // Hide destination 5.
  DestinationCustomizationModel* destinationModel =
      overflow_menu_orderer_.destinationCustomizationModel;
  destinationModel.shownDestinations[5].shown = NO;
  [overflow_menu_orderer_ commitDestinationsUpdate];

  // Add error badge to destination 5
  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[5]);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[5]];

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  // The badge on the hidden destination 5 should propagate to the settings
  // destination.
  ASSERT_EQ(overflow_menu_model_.destinations[0].badge, BadgeTypeError);

  // Removing the badge on hidden destination 5 should also propagate to the
  // settings destination
  destination.badge = BadgeTypeNone;
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  ASSERT_EQ(overflow_menu_model_.destinations[0].badge, BadgeTypeNone);
}

// Tests that the overflow menu order loads badge data from prefs.
TEST_F(OverflowMenuOrdererTest, LoadBadgeDataFromPrefs) {
  CreatePrefs();

  base::Value::Dict badge_data =
      base::Value::Dict().Set(overflow_menu::StringNameForDestination(
                                  overflow_menu::Destination::Bookmarks),
                              base::Value::Dict()
                                  .Set("impressions_remaining", 2)
                                  .Set("badge_type", "error")
                                  .Set("is_feature_driven_badge", false));

  prefs_->SetDict(prefs::kOverflowMenuDestinationBadgeData,
                  std::move(badge_data));

  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4],
      all_destinations[5], overflow_menu::Destination::Bookmarks,
  };

  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            overflow_menu::Destination::Bookmarks);

  // Adding another new destination should cause Bookmarks to hold the 4th slot
  // until its impression timer is up
  DestinationRanking new_destinations = {
      all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4],
      all_destinations[5], overflow_menu::Destination::Bookmarks,
      all_destinations[6],
  };
  destination_provider_.baseDestinations = new_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            overflow_menu::Destination::Bookmarks);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            all_destinations[6]);
  [overflow_menu_orderer_ updateForMenuDisappearance];

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            overflow_menu::Destination::Bookmarks);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            all_destinations[6]);
  [overflow_menu_orderer_ updateForMenuDisappearance];
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  // After the 2 impressions, the new item should be promoted, while the
  // originally-badged item should fall back.
  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];
  updated_ranking =
      RankingFromDestinationArray(overflow_menu_model_.destinations);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex],
            all_destinations[6]);
  EXPECT_EQ(updated_ranking[kNewDestinationsInsertionIndex + 1],
            overflow_menu::Destination::Bookmarks);
  [overflow_menu_orderer_ updateForMenuDisappearance];
}

// Tests that the overflow menu order saves badge data to prefs.
TEST_F(OverflowMenuOrdererTest, SaveBadgeDataToPrefs) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
      all_destinations[6],
  };

  // Creates `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  // Same as `current_destinations`, but has a new element,
  // `all_destinations[7]`.
  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      all_destinations[6],
      // New destination
      all_destinations[7],
  };

  destination_provider_.baseDestinations = updated_destinations;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  const base::Value::Dict& badge_data =
      prefs_->GetDict(prefs::kOverflowMenuDestinationBadgeData);

  std::string new_destination_string =
      overflow_menu::StringNameForDestination(all_destinations[7]);

  ASSERT_TRUE(badge_data.contains(new_destination_string));

  EXPECT_EQ(3, badge_data.FindDict(new_destination_string)
                   ->FindInt("impressions_remaining"));
  EXPECT_EQ(
      "new",
      *badge_data.FindDict(new_destination_string)->FindString("badge_type"));
  EXPECT_EQ(false, badge_data.FindDict(new_destination_string)
                       ->FindBool("is_feature_driven_badge"));
}

// Tests that correct metrics are fired when the menu is reordered due to a new
// badge.
TEST_F(OverflowMenuOrdererTest, Customization_NewBadgeReorderingMetrics) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
  };

  destination_provider_.baseDestinations = updated_destinations;

  base::HistogramTester tester;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  // There was one new destination and one with an error badge.
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.DestinationsOrderChangedProgrammatically", 1, 1);
}

// Tests that correct metrics are fired when the menu is reordered due to an
// error badge.
TEST_F(OverflowMenuOrdererTest, Customization_ErrorBadgeReorderingMetrics) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[4]);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[4]];

  base::HistogramTester tester;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  // There was one destination with an error badge.
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.DestinationsOrderChangedProgrammatically", 0, 1);
}

// Tests that correct metrics are fired when the menu is reordered due to both a
// new and error badge.
TEST_F(OverflowMenuOrdererTest,
       Customization_NewAndErrorBadgeReorderingMetrics) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  DestinationRanking updated_destinations = {
      all_destinations[0],
      all_destinations[1],
      all_destinations[2],
      all_destinations[3],
      all_destinations[4],
      all_destinations[5],
      // New destinations
      all_destinations[6],
  };

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[4]);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[4]];

  destination_provider_.baseDestinations = updated_destinations;

  base::HistogramTester tester;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  // There was one new destination and one with an error badge.
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.DestinationsOrderChangedProgrammatically", 2, 1);
}

// Tests that no metrics are fired when there are badges, but they do not cause
// the menu to be re-ordered.
TEST_F(OverflowMenuOrdererTest, Customization_BadgeReorderingMetricsNotFired) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  // Initializes `OverflowMenuOrderer` with initial ranking
  // `current_destinations`.
  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  OverflowMenuDestination* destination =
      CreateOverflowMenuDestination(all_destinations[0]);
  destination.badge = BadgeTypeError;
  [destination_provider_ storeCustomDestination:destination
                             forDestinationType:all_destinations[0]];

  base::HistogramTester tester;

  [overflow_menu_orderer_ reorderDestinationsForInitialMenu];

  // The badge did not cause the menu to change order.
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.DestinationsOrderChangedProgrammatically", 0, 0);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.DestinationsOrderChangedProgrammatically", 1, 0);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.DestinationsOrderChangedProgrammatically", 2, 0);
}

// Tests that the proper metrics are recorded when destination customization
// completes.
TEST_F(OverflowMenuOrdererTest, DestinationCustomizationRecordsMetrics) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  // Hide destinations 4 and 5.
  DestinationCustomizationModel* destinationModel =
      overflow_menu_orderer_.destinationCustomizationModel;

  OverflowMenuDestination* destination4 = destinationModel.shownDestinations[4];
  OverflowMenuDestination* destination5 = destinationModel.shownDestinations[5];
  destination4.shown = NO;
  destination5.shown = NO;

  base::HistogramTester tester;

  [overflow_menu_orderer_ commitDestinationsUpdate];

  tester.ExpectBucketCount("IOS.OverflowMenu.SmartSortingStateChange", 1, 1);

  tester.ExpectTotalCount("IOS.OverflowMenu.Customization.DestinationRemoved",
                          2);
  tester.ExpectBucketCount("IOS.OverflowMenu.Customization.DestinationRemoved",
                           4, 1);
  tester.ExpectBucketCount("IOS.OverflowMenu.Customization.DestinationRemoved",
                           5, 1);

  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.FirstPosition", 0,
      1);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.SecondPosition", 1,
      1);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.ThirdPosition", 2,
      1);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.FourthPosition", 3,
      1);

  std::vector<base::Bucket> buckets = tester.GetAllSamples(
      "IOS.OverflowMenu.Customization.DestinationsCustomized");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & 1 << 0);
  EXPECT_FALSE(buckets[0].min & 1 << 1);
  EXPECT_FALSE(buckets[0].min & 1 << 2);
  EXPECT_TRUE(buckets[0].min & 1 << 3);
  EXPECT_FALSE(buckets[0].min & 1 << 4);
  EXPECT_FALSE(buckets[0].min & 1 << 5);

  // Now turn destination 4 (the first hidden destination) back on and also turn
  // destination usage history back on.
  destinationModel = overflow_menu_orderer_.destinationCustomizationModel;
  destinationModel.hiddenDestinations[0].shown = YES;
  destinationModel.destinationUsageEnabled = YES;

  [overflow_menu_orderer_ commitDestinationsUpdate];

  tester.ExpectBucketCount("IOS.OverflowMenu.SmartSortingStateChange", 0, 1);

  tester.ExpectTotalCount("IOS.OverflowMenu.Customization.DestinationAdded", 1);
  tester.ExpectBucketCount("IOS.OverflowMenu.Customization.DestinationAdded", 4,
                           1);

  // Destinations are added to the front of the list, so the new one now
  // occupies the first position.
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.FirstPosition", 4,
      1);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.SecondPosition", 0,
      1);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.ThirdPosition", 1,
      1);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.FourthPosition", 2,
      1);

  buckets = tester.GetAllSamples(
      "IOS.OverflowMenu.Customization.DestinationsCustomized");
  ASSERT_EQ(2U, buckets.size());
  EXPECT_FALSE(buckets[1].min & 1 << 0);
  EXPECT_TRUE(buckets[1].min & 1 << 1);
  EXPECT_TRUE(buckets[1].min & 1 << 2);
  EXPECT_FALSE(buckets[1].min & 1 << 3);
  EXPECT_TRUE(buckets[1].min & 1 << 4);
  EXPECT_FALSE(buckets[1].min & 1 << 5);
}

// Tests that no metrics are recorded if there were no actual changes.
TEST_F(OverflowMenuOrdererTest, DestinationCustomizationNoMetricsIfNoChange) {
  DestinationRanking all_destinations = SampleDestinations();
  DestinationRanking current_destinations = {
      all_destinations[0], all_destinations[1], all_destinations[2],
      all_destinations[3], all_destinations[4], all_destinations[5],
  };

  InitializeOverflowMenuOrdererWithRanking(NO, current_destinations);

  // Access model to start customization session
  DestinationCustomizationModel* destinationModel =
      overflow_menu_orderer_.destinationCustomizationModel;

  ASSERT_EQ(destinationModel.hasChanged, NO);

  base::HistogramTester tester;

  [overflow_menu_orderer_ commitDestinationsUpdate];

  tester.ExpectTotalCount("IOS.OverflowMenu.Customization.DestinationRemoved",
                          0);
  tester.ExpectTotalCount("IOS.OverflowMenu.Customization.DestinationAdded", 0);
  tester.ExpectTotalCount(
      "IOS.OverflowMenu.Customization.DestinationsCustomized", 0);

  tester.ExpectTotalCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.FirstPosition", 0);
  tester.ExpectTotalCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.SecondPosition", 0);
  tester.ExpectTotalCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.ThirdPosition", 0);
  tester.ExpectTotalCount(
      "IOS.OverflowMenu.Customization.DestinationsReordered.FourthPosition", 0);
}

// Tests that the proper metrics are recorded when action customization
// completes.
TEST_F(OverflowMenuOrdererTest, ActionCustomizationRecordsMetrics) {
  ActionRanking all_actions = SampleActions();
  ActionRanking current_actions = {
      all_actions[0], all_actions[1], all_actions[2],
      all_actions[3], all_actions[4], all_actions[5],
  };

  InitializeOverflowMenuOrderer(NO);
  action_provider_.basePageActions = current_actions;
  [overflow_menu_orderer_ updatePageActions];

  // Hide actions 4 and 5.
  ActionCustomizationModel* actionModel =
      overflow_menu_orderer_.actionCustomizationModel;

  OverflowMenuAction* action4 = actionModel.shownActions[4];
  OverflowMenuAction* action5 = actionModel.shownActions[5];
  action4.shown = NO;
  action5.shown = NO;

  base::HistogramTester tester;

  [overflow_menu_orderer_ commitActionsUpdate];

  tester.ExpectTotalCount("IOS.OverflowMenu.Customization.ActionRemoved", 2);
  tester.ExpectBucketCount("IOS.OverflowMenu.Customization.ActionRemoved", 8,
                           1);
  tester.ExpectBucketCount("IOS.OverflowMenu.Customization.ActionRemoved", 9,
                           1);

  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.FirstPosition", 4, 1);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.SecondPosition", 5, 1);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.ThirdPosition", 6, 1);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.FourthPosition", 7, 1);

  std::vector<base::Bucket> buckets =
      tester.GetAllSamples("IOS.OverflowMenu.Customization.ActionsCustomized");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & 1 << 0);
  EXPECT_FALSE(buckets[0].min & 1 << 1);
  EXPECT_FALSE(buckets[0].min & 1 << 2);

  // Now turn action 4 (the first hidden action) back on.
  actionModel = overflow_menu_orderer_.actionCustomizationModel;
  actionModel.hiddenActions[0].shown = YES;

  [overflow_menu_orderer_ commitActionsUpdate];

  tester.ExpectTotalCount("IOS.OverflowMenu.Customization.ActionAdded", 1);
  tester.ExpectBucketCount("IOS.OverflowMenu.Customization.ActionAdded", 8, 1);

  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.FirstPosition", 4, 2);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.SecondPosition", 5, 2);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.ThirdPosition", 6, 2);
  tester.ExpectBucketCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.FourthPosition", 7, 2);

  buckets =
      tester.GetAllSamples("IOS.OverflowMenu.Customization.ActionsCustomized");
  ASSERT_EQ(2U, buckets.size());
  EXPECT_FALSE(buckets[1].min & 1 << 0);
  EXPECT_TRUE(buckets[1].min & 1 << 1);
  EXPECT_FALSE(buckets[1].min & 1 << 2);
}

// Tests that no metrics are recorded if there were no actual changes.
TEST_F(OverflowMenuOrdererTest, ActionCustomizationNoMetricsIfNoChange) {
  ActionRanking all_actions = SampleActions();
  ActionRanking current_actions = {
      all_actions[0], all_actions[1], all_actions[2],
      all_actions[3], all_actions[4], all_actions[5],
  };

  InitializeOverflowMenuOrderer(NO);
  action_provider_.basePageActions = current_actions;
  [overflow_menu_orderer_ updatePageActions];

  // Access model to start customization session
  ActionCustomizationModel* actionModel =
      overflow_menu_orderer_.actionCustomizationModel;

  ASSERT_EQ(actionModel.hasChanged, NO);

  base::HistogramTester tester;

  [overflow_menu_orderer_ commitActionsUpdate];

  tester.ExpectTotalCount("IOS.OverflowMenu.Customization.ActionRemoved", 0);
  tester.ExpectTotalCount("IOS.OverflowMenu.Customization.ActionAdded", 0);
  tester.ExpectTotalCount("IOS.OverflowMenu.Customization.ActionsCustomized",
                          0);

  tester.ExpectTotalCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.FirstPosition", 0);
  tester.ExpectTotalCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.SecondPosition", 0);
  tester.ExpectTotalCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.ThirdPosition", 0);
  tester.ExpectTotalCount(
      "IOS.OverflowMenu.Customization.ActionsReordered.FourthPosition", 0);
}
