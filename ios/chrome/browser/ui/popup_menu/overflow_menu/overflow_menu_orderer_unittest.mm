// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_orderer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
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

}  // namespace

// Fake provider for test purposes.
@interface FakeOverflowMenuDestinationProvider
    : NSObject <OverflowMenuDestinationProvider>

@property(nonatomic, assign) DestinationRanking baseDestinations;

// By default, the provider will create a standard `OverflowMenuDestination`
// and return that in `-destinationForDestinationType:`. This will override
// that to return a custom destination.
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

@end

class OverflowMenuOrdererTest : public PlatformTest {
 public:
  OverflowMenuOrdererTest() {}

 protected:
  void SetUp() override {
    destination_provider_ = [[FakeOverflowMenuDestinationProvider alloc] init];
  }

  void TearDown() override {
    [overflow_menu_orderer_ disconnect];

    PlatformTest::TearDown();
  }

  void InitializeOverflowMenuOrderer(BOOL isIncognito) {
    CreatePrefs();

    overflow_menu_orderer_ =
        [[OverflowMenuOrderer alloc] initWithIsIncognito:isIncognito];

    overflow_menu_orderer_.localStatePrefs = prefs_.get();
    overflow_menu_orderer_.visibleDestinationsCount = kVisibleDestinationsCount;
    overflow_menu_orderer_.destinationProvider = destination_provider_;
  }

  void InitializeOverflowMenuOrdererWithRanking(BOOL isIncognito,
                                                DestinationRanking ranking) {
    InitializeOverflowMenuOrderer(isIncognito);
    destination_provider_.baseDestinations = ranking;
    [overflow_menu_orderer_ sortedDestinations];
  }

  // Create pref registry for tests.
  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuDestinationUsageHistory, PrefRegistry::LOSSY_PREF);
    prefs_->registry()->RegisterListPref(prefs::kOverflowMenuNewDestinations,
                                         PrefRegistry::LOSSY_PREF);
    prefs_->registry()->RegisterListPref(prefs::kOverflowMenuDestinationsOrder);
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

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  OverflowMenuOrderer* overflow_menu_orderer_;
  FakeOverflowMenuDestinationProvider* destination_provider_;
};

// Tests that the ranking pref gets populated after sorting once.
TEST_F(OverflowMenuOrdererTest, StoresInitialRanking) {
  InitializeOverflowMenuOrderer(NO);
  DestinationRanking sample_destinations = SampleDestinations();
  destination_provider_.baseDestinations = sample_destinations;
  [overflow_menu_orderer_ sortedDestinations];

  const base::Value::List& stored_ranking =
      prefs_->GetList(prefs::kOverflowMenuDestinationsOrder);

  EXPECT_EQ(stored_ranking.size(), sample_destinations.size());
}

// Tests that the old pref format (kOverflowMenuDestinationUsageHistory as a
// dict containing both usage history and ranking) is correctly migrated to the
// new format (kOverflowMenuDestinationUsageHistory containing just usage
// history and kOverflowMenuDestinationsOrder containing ranking).
TEST_F(OverflowMenuOrdererTest, MigratesRanking) {
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

  NSArray<OverflowMenuDestination*>* sorted_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(sorted_ranking[3].destination),
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

  NSArray<OverflowMenuDestination*>* sorted_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex].destination),
            all_destinations[7]);

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 1].destination),
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

  NSArray<OverflowMenuDestination*>* sorted_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(sorted_ranking[0].destination),
      all_destinations[2]);

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex].destination),
            all_destinations[7]);

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 1].destination),
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

  NSArray<OverflowMenuDestination*>* sorted_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex].destination),
            all_destinations[4]);
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 1].destination),
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
  NSArray<OverflowMenuDestination*>* initial_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                initial_ranking[kNewDestinationsInsertionIndex].destination),
            all_destinations[5]);
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(
          initial_ranking[kNewDestinationsInsertionIndex + 1].destination),
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
  NSArray<OverflowMenuDestination*>* initial_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  // Verify that the destination with a badge and with a better ranking than
  // kNewDestinationsInsertionIndex wasn't moved.
  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(initial_ranking[0].destination),
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

  NSArray<OverflowMenuDestination*>* sorted_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex].destination),
            all_destinations[6]);
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 1].destination),
            all_destinations[7]);
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 2].destination),
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

  NSArray<OverflowMenuDestination*>* sorted_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex].destination),
            all_destinations[7]);
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 1].destination),
            all_destinations[4]);
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 2].destination),
            all_destinations[5]);
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 3].destination),
            all_destinations[6]);
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 4].destination),
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

  NSArray<OverflowMenuDestination*>* sorted_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  ASSERT_EQ(
      static_cast<overflow_menu::Destination>(sorted_ranking[3].destination),
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

  NSArray<OverflowMenuDestination*>* sorted_ranking =
      [overflow_menu_orderer_ sortedDestinations];

  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex].destination),
            all_destinations[4]);
  ASSERT_EQ(static_cast<overflow_menu::Destination>(
                sorted_ranking[kNewDestinationsInsertionIndex + 1].destination),
            all_destinations[6]);
}
