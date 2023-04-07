// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

}  // namespace

class DestinationUsageHistoryTest : public PlatformTest {
 public:
  DestinationUsageHistoryTest() {}

 protected:
  void TearDown() override {
    [destination_usage_history_ stop];

    PlatformTest::TearDown();
  }

  // Creates CreateDestinationUsageHistory with empty pref data.
  DestinationUsageHistory* CreateDestinationUsageHistory(
      NSArray<OverflowMenuDestination*>* default_destinations) {
    CreatePrefs();

    destination_usage_history_ =
        [[DestinationUsageHistory alloc] initWithPrefService:prefs_.get()];

    destination_usage_history_.visibleDestinationsCount =
        kVisibleDestinationsCount;

    [destination_usage_history_ start];

    [destination_usage_history_
        sortedDestinationsFromCarouselDestinations:default_destinations];

    return destination_usage_history_;
  }

  // Creates CreateDestinationUsageHistory with past data and `ranking`.
  DestinationUsageHistory* CreateDestinationUsageHistoryWithData(
      std::vector<overflow_menu::Destination>& ranking,
      base::Value::Dict& history,
      NSArray<OverflowMenuDestination*>* default_destinations) {
    base::Value::List previous_ranking;

    for (overflow_menu::Destination destination : ranking) {
      previous_ranking.Append(
          overflow_menu::StringNameForDestination(destination));
    }

    CreatePrefsWithData(previous_ranking, history);

    destination_usage_history_ =
        [[DestinationUsageHistory alloc] initWithPrefService:prefs_.get()];

    destination_usage_history_.visibleDestinationsCount =
        kVisibleDestinationsCount;

    [destination_usage_history_ start];

    [destination_usage_history_
        sortedDestinationsFromCarouselDestinations:default_destinations];

    return destination_usage_history_;
  }

  // Create pref registry for tests.
  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuDestinationUsageHistory, PrefRegistry::LOSSY_PREF);
    prefs_->registry()->RegisterListPref(prefs::kOverflowMenuNewDestinations,
                                         PrefRegistry::LOSSY_PREF);
  }

  // Helper for CreateDestinationUsageHistoryWithData(), inserts day history
  // data and `ranking` for testing pref service.
  void CreatePrefsWithData(base::Value::List& stored_ranking,
                           base::Value::Dict& stored_history) {
    CreatePrefs();

    // Set the passed in `stored_history`.
    base::Value::Dict history = stored_history.Clone();

    // Set the passed in `stored_ranking`.
    base::Value::List ranking = stored_ranking.Clone();
    history.Set("ranking", std::move(ranking));

    prefs_->SetDict(prefs::kOverflowMenuDestinationUsageHistory,
                    std::move(history));
  }

  // Constructs '<day>.<destination>' dotted-path key for base::Value::Dict
  // searching.
  std::string DottedPath(std::string day,
                         overflow_menu::Destination destination) {
    std::string destination_name =
        overflow_menu::StringNameForDestination(destination);

    return day + "." + destination_name;
  }

  // Helper for DottedPath(...), converts day (int) to string, then calls
  // DottedPath(std::string day, overflow_menu::Destination destination).
  std::string DottedPath(int day, overflow_menu::Destination destination) {
    return DottedPath(base::NumberToString(day), destination);
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

  NSArray<OverflowMenuDestination*>* SampleDestinations() {
    OverflowMenuDestination* bookmarksDestination =
        CreateOverflowMenuDestination(overflow_menu::Destination::Bookmarks);
    OverflowMenuDestination* historyDestination =
        CreateOverflowMenuDestination(overflow_menu::Destination::History);
    OverflowMenuDestination* readingListDestination =
        CreateOverflowMenuDestination(overflow_menu::Destination::ReadingList);
    OverflowMenuDestination* passwordsDestination =
        CreateOverflowMenuDestination(overflow_menu::Destination::Passwords);
    OverflowMenuDestination* downloadsDestination =
        CreateOverflowMenuDestination(overflow_menu::Destination::Downloads);
    OverflowMenuDestination* recentTabsDestination =
        CreateOverflowMenuDestination(overflow_menu::Destination::RecentTabs);
    OverflowMenuDestination* siteInfoDestination =
        CreateOverflowMenuDestination(overflow_menu::Destination::SiteInfo);
    OverflowMenuDestination* settingsDestination =
        CreateOverflowMenuDestination(overflow_menu::Destination::Settings);

    NSArray<OverflowMenuDestination*>* destinations = @[
      bookmarksDestination,
      historyDestination,
      readingListDestination,
      passwordsDestination,
      downloadsDestination,
      recentTabsDestination,
      siteInfoDestination,
      settingsDestination,
    ];

    return destinations;
  }

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  DestinationUsageHistory* destination_usage_history_;
};

// Tests the initializer correctly creates a DestinationUsageHistory* with the
// specified Pref service.
TEST_F(DestinationUsageHistoryTest, InitWithPrefService) {
  CreateDestinationUsageHistory(SampleDestinations());

  PrefService* pref_service = prefs_.get();

  EXPECT_NE(
      pref_service->FindPreference(prefs::kOverflowMenuDestinationUsageHistory),
      nullptr);
  EXPECT_TRUE(
      pref_service->HasPrefPath(prefs::kOverflowMenuDestinationUsageHistory));
}

// Tests the initializer correctly creates a DestinationUsageHistory* with the
// specified Pref service, when the prefs have existing data.
TEST_F(DestinationUsageHistoryTest, InitWithPrefServiceForDirtyPrefs) {
  // Construct existing usage data stored in prefs.
  std::vector<overflow_menu::Destination> ranking = {
      overflow_menu::Destination::Bookmarks,
      overflow_menu::Destination::History,
      overflow_menu::Destination::ReadingList,
      overflow_menu::Destination::Passwords,
      overflow_menu::Destination::Downloads,
      overflow_menu::Destination::RecentTabs,
      overflow_menu::Destination::SiteInfo,
      overflow_menu::Destination::Settings,
  };

  base::Value::Dict history;
  base::Value::Dict day_history;
  history.SetByDottedPath(
      DottedPath(TodaysDay().InDays(), overflow_menu::Destination::Bookmarks),
      std::move(day_history));

  // Create DestinationUsageHistory.
  CreateDestinationUsageHistoryWithData(ranking, history, SampleDestinations());

  PrefService* pref_service = prefs_.get();

  EXPECT_NE(
      pref_service->FindPreference(prefs::kOverflowMenuDestinationUsageHistory),
      nullptr);
  EXPECT_TRUE(
      pref_service->HasPrefPath(prefs::kOverflowMenuDestinationUsageHistory));
}

// Tests that a new destination click is incremented and written to Chrome
// Prefs.
TEST_F(DestinationUsageHistoryTest, HandlesNewDestinationClick) {
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory(SampleDestinations());

  // Click bookmarks destination.
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::Bookmarks];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<int> expected = update->FindIntByDottedPath(
      DottedPath(TodaysDay().InDays(), overflow_menu::Destination::Bookmarks));

  // Verify bookmarks entry exists.
  EXPECT_TRUE(expected.has_value());

  // Verify bookmarks entry has single click (plus the default seeded 20 clicks)
  // for today.
  EXPECT_EQ(expected.value(), 21);
}

// Tests that each destination in the history is populated with a default
// number of clicks.
TEST_F(DestinationUsageHistoryTest,
       InjectsDefaultClickCountForAllDestinations) {
  NSArray<OverflowMenuDestination*>* sample_destinations = SampleDestinations();

  CreateDestinationUsageHistory(sample_destinations);

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  for (OverflowMenuDestination* destination in sample_destinations) {
    const std::string dotted_path = DottedPath(
        TodaysDay().InDays(),
        static_cast<overflow_menu::Destination>(destination.destination));

    absl::optional<int> expected = update->FindIntByDottedPath(dotted_path);

    // Verify destination entry exists.
    EXPECT_TRUE(expected.has_value());

    // Verify destination  entry has single click (plus the default seeded 20
    // clicks) for today.
    EXPECT_EQ(expected.value(), 20);
  }
}

// Tests that an existing destination click is incremented.
TEST_F(DestinationUsageHistoryTest,
       HandlesExistingDestinationClickAndAddToPrefs) {
  // Construct existing usage data, where Bookmarks has been clicked 5 times,
  // stored in prefs.
  std::vector<overflow_menu::Destination> ranking = {
      overflow_menu::Destination::Bookmarks,
      overflow_menu::Destination::History,
      overflow_menu::Destination::ReadingList,
      overflow_menu::Destination::Passwords,
      overflow_menu::Destination::Downloads,
      overflow_menu::Destination::RecentTabs,
      overflow_menu::Destination::SiteInfo,
      overflow_menu::Destination::Settings,
  };
  base::Value::Dict history;
  base::Value::Dict day_history;
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::Bookmarks),
                  5);
  history.Set(base::NumberToString(TodaysDay().InDays()),
              std::move(day_history));

  // Create DestinationUsageHistory.
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistoryWithData(ranking, history,
                                            SampleDestinations());

  // Click bookmarks destination.
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::Bookmarks];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<int> expected = update->FindIntByDottedPath(
      DottedPath(TodaysDay().InDays(), overflow_menu::Destination::Bookmarks));

  // Verify bookmarks entry exists.
  EXPECT_TRUE(expected.has_value());

  // Verify bookmarks entry has single click (plus the existing 5 clicks)
  // for today.
  EXPECT_EQ(expected.value(), 6);
}

TEST_F(DestinationUsageHistoryTest, DoesNotSwapTwoShownDestinations) {
  NSArray<OverflowMenuDestination*>* sample_destinations = SampleDestinations();

  std::vector<overflow_menu::Destination> ranking = {
      overflow_menu::Destination::Bookmarks,
      overflow_menu::Destination::History,
      overflow_menu::Destination::ReadingList,
      overflow_menu::Destination::Passwords,
      overflow_menu::Destination::Downloads,
      overflow_menu::Destination::RecentTabs,
      overflow_menu::Destination::SiteInfo,
      overflow_menu::Destination::Settings,
  };
  base::Value::Dict history;

  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistoryWithData(ranking, history,
                                            sample_destinations);

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<base::Value> current_ranking = update->Extract("ranking");
  EXPECT_TRUE(current_ranking.has_value());

  // Click bookmarks Reading List five
  // times.
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::ReadingList];
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::ReadingList];
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::ReadingList];
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::ReadingList];
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::ReadingList];

  absl::optional<base::Value> new_ranking = update->Extract("ranking");
  EXPECT_TRUE(new_ranking.has_value());

  EXPECT_EQ(current_ranking.value(), new_ranking.value());
}

TEST_F(DestinationUsageHistoryTest, DoesNotSwapTwoUnshownDestinations) {
  NSArray<OverflowMenuDestination*>* sample_destinations = SampleDestinations();

  std::vector<overflow_menu::Destination> ranking = {
      overflow_menu::Destination::Bookmarks,
      overflow_menu::Destination::History,
      overflow_menu::Destination::ReadingList,
      overflow_menu::Destination::Passwords,
      overflow_menu::Destination::Downloads,
      overflow_menu::Destination::RecentTabs,
      overflow_menu::Destination::SiteInfo,
      overflow_menu::Destination::Settings,
  };
  base::Value::Dict history;
  base::Value::Dict day_history;
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::Bookmarks),
                  1);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::History),
                  1);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::ReadingList),
                  1);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::Passwords),
                  1);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::Downloads),
                  1);

  history.Set(base::NumberToString(TodaysDay().InDays()),
              std::move(day_history));

  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistoryWithData(ranking, history,
                                            sample_destinations);

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<base::Value> expected_ranking = update->Extract("ranking");

  EXPECT_TRUE(expected_ranking.has_value());

  // Click Recent Tabs (currently in ranking position 6) once.
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::RecentTabs];

  // Click Site Inforamtion (currently in ranking position 7) once.
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::SiteInfo];

  // Click Settings (currently in last position) once.
  [destination_usage_history
      recordClickForDestination:overflow_menu::Destination::Settings];

  absl::optional<base::Value> new_ranking = update->Extract("ranking");

  EXPECT_TRUE(new_ranking.has_value());

  EXPECT_EQ(expected_ranking.value(), new_ranking.value());
}

TEST_F(DestinationUsageHistoryTest, DeletesExpiredUsageData) {
  NSArray<OverflowMenuDestination*>* sample_destinations = SampleDestinations();

  std::vector<overflow_menu::Destination> ranking = {
      overflow_menu::Destination::Bookmarks,
      overflow_menu::Destination::History,
      overflow_menu::Destination::ReadingList,
      overflow_menu::Destination::Passwords,
      overflow_menu::Destination::Downloads,
      overflow_menu::Destination::RecentTabs,
      overflow_menu::Destination::SiteInfo,
      overflow_menu::Destination::Settings,
  };

  base::Value::Dict history;

  // Usage data just a bit older than 1 year.
  base::TimeDelta recently_expired_day = TodaysDay() - base::Days(366);
  base::Value::Dict recently_expired_day_history;
  recently_expired_day_history.Set(overflow_menu::StringNameForDestination(
                                       overflow_menu::Destination::Bookmarks),
                                   1);
  history.Set(base::NumberToString(recently_expired_day.InDays()),
              std::move(recently_expired_day_history));

  // Usage data almost 3 years old.
  base::TimeDelta expired_day = TodaysDay() - base::Days(1000);
  base::Value::Dict expired_day_history;
  expired_day_history.Set(overflow_menu::StringNameForDestination(
                              overflow_menu::Destination::Bookmarks),
                          1);
  history.Set(base::NumberToString(expired_day.InDays()),
              std::move(expired_day_history));

  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistoryWithData(ranking, history,
                                            sample_destinations);

  [destination_usage_history
      sortedDestinationsFromCarouselDestinations:sample_destinations];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  // Has one entry for ranking, and one entry for today's seeded history.
  EXPECT_EQ(update->size(), (size_t)2);
  EXPECT_NE(update->Find(base::NumberToString(TodaysDay().InDays())), nullptr);
  EXPECT_EQ(update->Find(base::NumberToString(recently_expired_day.InDays())),
            nullptr);
  EXPECT_EQ(update->Find(base::NumberToString(expired_day.InDays())), nullptr);
}

TEST_F(DestinationUsageHistoryTest, InsertsNewDestinationInMiddleOfRanking) {
  NSArray<OverflowMenuDestination*>* all_destinations = SampleDestinations();
  NSArray<OverflowMenuDestination*>* current_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
    all_destinations[6],
  ];

  // Creates `DestinationUsageHistory` with initial ranking
  // `current_destinations`.
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory(current_destinations);

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  // Same as `current_destinations`, but has a new element,
  // `all_destinations[7]`, which should eventually be inserted starting at
  // position 4 in the carousel (this is the expected behavior defined by
  // product).
  NSArray<OverflowMenuDestination*>* updated_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
    all_destinations[6],
    // New destination
    all_destinations[7],
  ];

  [destination_usage_history
      sortedDestinationsFromCarouselDestinations:updated_destinations];

  absl::optional<base::Value> ranking = update->Extract("ranking");

  EXPECT_TRUE(ranking.has_value());

  const base::Value::List& actual = ranking.value().GetList();

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(actual[3].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[7].destination));
}

TEST_F(DestinationUsageHistoryTest, InsertsNewDestinationsInMiddleOfRanking) {
  NSArray<OverflowMenuDestination*>* all_destinations = SampleDestinations();
  NSArray<OverflowMenuDestination*>* current_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
  ];

  // Creates `DestinationUsageHistory` with initial ranking
  // `current_destinations`.
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory(current_destinations);

  // Same as `current_destinations`, but has new elements (`all_destinations[6]`
  // and `all_destinations[7]`) inserted starting at position 4 in the carousel
  // (this is the expected behavior defined by product).
  NSArray<OverflowMenuDestination*>* updated_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
    // New destinations
    all_destinations[6],
    all_destinations[7],
  ];

  [destination_usage_history
      sortedDestinationsFromCarouselDestinations:updated_destinations];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<base::Value> ranking = update->Extract("ranking");

  EXPECT_TRUE(ranking.has_value());

  const base::Value::List& actual = ranking.value().GetList();

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[7].destination));

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(actual[4].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[6].destination));
}

TEST_F(DestinationUsageHistoryTest, InsertsAndRemovesNewDestinationsInRanking) {
  NSArray<OverflowMenuDestination*>* all_destinations = SampleDestinations();
  NSArray<OverflowMenuDestination*>* current_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
  ];

  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory(current_destinations);

  NSArray<OverflowMenuDestination*>* updated_destinations = @[
    // NOTE: all_destinations[0] was removed
    // NOTE: all_destinations[1] was removed
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
    // New destinations
    all_destinations[6],
    all_destinations[7],
  ];

  [destination_usage_history
      sortedDestinationsFromCarouselDestinations:updated_destinations];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<base::Value> ranking = update->Extract("ranking");

  EXPECT_TRUE(ranking.has_value());

  const base::Value::List& actual = ranking.value().GetList();

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(actual[0].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[2].destination));

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[7].destination));

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex + 1].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[6].destination));
}

// Tests that the destinations that have a badge are moved in the middle of the
// ranking to get the user's attention; before the untapped destinations.
TEST_F(DestinationUsageHistoryTest, MoveBadgedDestinationsInRanking) {
  NSArray<OverflowMenuDestination*>* all_destinations = SampleDestinations();
  NSArray<OverflowMenuDestination*>* current_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
  ];

  // Creates `DestinationUsageHistory` with initial ranking
  // `current_destinations`.
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory(current_destinations);

  NSArray<OverflowMenuDestination*>* updated_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
    // New destinations
    all_destinations[6],
  ];

  all_destinations[4].badge = BadgeTypeError;

  [destination_usage_history
      sortedDestinationsFromCarouselDestinations:updated_destinations];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<base::Value> ranking = update->Extract("ranking");

  EXPECT_TRUE(ranking.has_value());

  const base::Value::List& actual = ranking.value().GetList();

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[4].destination));
  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex + 1].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[6].destination));
}

// Tests that the destinations that have an error badge have priority over the
// other badges when they are moved.
TEST_F(DestinationUsageHistoryTest, PriorityToErrorBadgeOverOtherBadges) {
  NSArray<OverflowMenuDestination*>* all_destinations = SampleDestinations();
  NSArray<OverflowMenuDestination*>* current_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
  ];

  all_destinations[5].badge = BadgeTypeError;
  all_destinations[3].badge = BadgeTypePromo;

  // Creates `DestinationUsageHistory` with initial ranking
  // `current_destinations`.
  CreateDestinationUsageHistory(current_destinations);

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<base::Value> ranking = update->Extract("ranking");

  EXPECT_TRUE(ranking.has_value());

  const base::Value::List& actual = ranking.value().GetList();

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[5].destination));
  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex + 1].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[3].destination));
}

// Tests that the destinations that have a badge but are in a better position
// than kNewDestinationsInsertionIndex won't be moved hence not demoted.
TEST_F(DestinationUsageHistoryTest, DontMoveBadgedDestinationWithGoodRanking) {
  NSArray<OverflowMenuDestination*>* all_destinations = SampleDestinations();
  NSArray<OverflowMenuDestination*>* current_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
  ];

  all_destinations[0].badge = BadgeTypePromo;

  // Creates `DestinationUsageHistory` with initial ranking
  // `current_destinations`.
  CreateDestinationUsageHistory(current_destinations);

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<base::Value> ranking = update->Extract("ranking");

  EXPECT_TRUE(ranking.has_value());

  const base::Value::List& actual = ranking.value().GetList();

  // Verify that the destination with a badge and with a better ranking than
  // kNewDestinationsInsertionIndex wasn't moved.
  ASSERT_EQ(
      overflow_menu::DestinationForStringName(actual[0].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[0].destination));
}

// Tests that if a destination is both new and has a badge, it will be inserted
// before the other destinations that are only new without a badge assigned.
TEST_F(DestinationUsageHistoryTest, PriorityToBadgeOverNewDestinationStatus) {
  NSArray<OverflowMenuDestination*>* all_destinations = SampleDestinations();
  NSArray<OverflowMenuDestination*>* current_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
  ];

  // Creates `DestinationUsageHistory` with initial ranking
  // `current_destinations`.
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory(current_destinations);

  NSArray<OverflowMenuDestination*>* updated_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    // New destinations
    all_destinations[5],
    all_destinations[6],
    all_destinations[7],
  ];

  all_destinations[6].badge = BadgeTypeNew;

  [destination_usage_history
      sortedDestinationsFromCarouselDestinations:updated_destinations];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<base::Value> ranking = update->Extract("ranking");

  EXPECT_TRUE(ranking.has_value());

  const base::Value::List& actual = ranking.value().GetList();

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[6].destination));
  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex + 1].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[7].destination));
  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex + 2].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[5].destination));
}

// Tests that if a destination is both new and has a badge, it will be inserted
// before the other destinations wtih a badge of the same priority that are not
// new.
TEST_F(DestinationUsageHistoryTest, PriorityToNewDestinationWithBadge) {
  NSArray<OverflowMenuDestination*>* all_destinations = SampleDestinations();
  NSArray<OverflowMenuDestination*>* current_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
  ];

  // Creates `DestinationUsageHistory` with initial ranking
  // `current_destinations`.
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory(current_destinations);

  NSArray<OverflowMenuDestination*>* updated_destinations = @[
    all_destinations[0],
    all_destinations[1],
    all_destinations[2],
    all_destinations[3],
    all_destinations[4],
    all_destinations[5],
    // New destinations
    all_destinations[6],
    all_destinations[7],
  ];

  all_destinations[4].badge = BadgeTypeError;
  all_destinations[5].badge = BadgeTypePromo;
  all_destinations[7].badge = BadgeTypeError;

  [destination_usage_history
      sortedDestinationsFromCarouselDestinations:updated_destinations];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  absl::optional<base::Value> ranking = update->Extract("ranking");

  EXPECT_TRUE(ranking.has_value());

  const base::Value::List& actual = ranking.value().GetList();

  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[7].destination));
  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex + 1].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[4].destination));
  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex + 2].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[5].destination));
  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex + 3].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[6].destination));
  ASSERT_EQ(
      overflow_menu::DestinationForStringName(
          actual[kNewDestinationsInsertionIndex + 4].GetString()),
      static_cast<overflow_menu::Destination>(all_destinations[3].destination));
}
