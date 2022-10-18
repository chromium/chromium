// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The number of days since the Unix epoch; one day, in this context, runs from
// UTC midnight to UTC midnight.
int TodaysDay() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

}  // namespace

class DestinationUsageHistoryTest : public PlatformTest {
 public:
  DestinationUsageHistoryTest() {}

 protected:
  void TearDown() override {
    [destination_usage_history_ disconnect];
    PlatformTest::TearDown();
  }

  // Creates CreateDestinationUsageHistory with empty pref data.
  DestinationUsageHistory* CreateDestinationUsageHistory() {
    CreatePrefs();

    destination_usage_history_ =
        [[DestinationUsageHistory alloc] initWithPrefService:prefs_.get()];

    return destination_usage_history_;
  }

  // Creates CreateDestinationUsageHistory with past data and `ranking`.
  DestinationUsageHistory* CreateDestinationUsageHistoryWithData(
      std::vector<overflow_menu::Destination>& ranking,
      base::Value::Dict& history) {
    base::Value::List prevRanking = RankingAsListValue(ranking);

    CreatePrefsWithData(prevRanking, history);

    destination_usage_history_ =
        [[DestinationUsageHistory alloc] initWithPrefService:prefs_.get()];

    return destination_usage_history_;
  }

  // Create pref registry for tests.
  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuDestinationUsageHistory, PrefRegistry::LOSSY_PREF);
  }

  // Helper for CreateDestinationUsageHistoryWithData(), inserts day history
  // data and `ranking` for testing pref service.
  void CreatePrefsWithData(base::Value::List& storedRanking,
                           base::Value::Dict& storedHistory) {
    CreatePrefs();

    // Set the passed in `storedHistory`.
    base::Value::Dict history = storedHistory.Clone();
    // Set the passed in `storedRanking`.
    base::Value::List ranking = storedRanking.Clone();
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

  // Converts std::vector<overflow_menu::Destination> ranking to
  // base::Value::List ranking.
  base::Value::List RankingAsListValue(
      std::vector<overflow_menu::Destination>& ranking) {
    base::Value::List rankingList;

    for (overflow_menu::Destination destination : ranking) {
      rankingList.Append(overflow_menu::StringNameForDestination(destination));
    }

    return rankingList;  // base::Value automatically does a std::move() upon
                         // return here.
  }

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  DestinationUsageHistory* destination_usage_history_;
  static constexpr int numAboveFoldDestinations = 5;
};

// Tests the initializer correctly creates a DestinationUsageHistory* with the
// specified Pref service.
TEST_F(DestinationUsageHistoryTest, InitWithPrefService) {
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory();

  PrefService* pref_service = destination_usage_history.prefService;

  EXPECT_NE(
      pref_service->FindPreference(prefs::kOverflowMenuDestinationUsageHistory),
      nullptr);
  EXPECT_FALSE(
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
  base::Value::Dict dayHistory;
  history.SetByDottedPath(
      DottedPath(TodaysDay(), overflow_menu::Destination::Bookmarks),
      std::move(dayHistory));

  // Create DestinationUsageHistory.
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistoryWithData(ranking, history);

  // Grab its pref service.
  PrefService* pref_service = destination_usage_history.prefService;

  EXPECT_NE(
      pref_service->FindPreference(prefs::kOverflowMenuDestinationUsageHistory),
      nullptr);
  EXPECT_TRUE(
      pref_service->HasPrefPath(prefs::kOverflowMenuDestinationUsageHistory));
}

// Tests DestinationUsageHistory::disconnect correctly nullifies the
// prefService.
TEST_F(DestinationUsageHistoryTest, DestroysPrefServiceOnDisconnect) {
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory();

  [destination_usage_history disconnect];

  EXPECT_EQ(destination_usage_history.prefService, nullptr);
}

// Tests that a new destination click is incremented and written to Chrome
// Prefs.
TEST_F(DestinationUsageHistoryTest, HandlesNewDestinationClickAndAddToPrefs) {
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory();

  // Click bookmarks destination.
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::Bookmarks
      numAboveFoldDestinations:numAboveFoldDestinations];

  // Fetch saved destination usage history.
  const base::Value::Dict& history =
      destination_usage_history.prefService->GetDict(
          prefs::kOverflowMenuDestinationUsageHistory);

  // Query saved usage history for Bookmarks entry for today.
  const base::Value* target = history.FindByDottedPath(
      DottedPath(TodaysDay(), overflow_menu::Destination::Bookmarks));

  // Verify bookmarks entry exists and has been clicked once.
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(destination_usage_history.prefService->HasPrefPath(
      prefs::kOverflowMenuDestinationUsageHistory));
  EXPECT_EQ(21, target->GetInt());
}

// Tests that each destination in the history is populated with a default number
// of clicks.
TEST_F(DestinationUsageHistoryTest, InjectsDefaultNumClicksForAllDestinations) {
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory();

  // Click bookmarks destination.
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::Bookmarks
      numAboveFoldDestinations:numAboveFoldDestinations];

  // Fetch saved destination usage history.
  const base::Value::Dict& history =
      destination_usage_history.prefService->GetDict(
          prefs::kOverflowMenuDestinationUsageHistory);

  EXPECT_TRUE(destination_usage_history.prefService->HasPrefPath(
      prefs::kOverflowMenuDestinationUsageHistory));

  std::vector<overflow_menu::Destination> destinations = {
      overflow_menu::Destination::Bookmarks,
      overflow_menu::Destination::History,
      overflow_menu::Destination::ReadingList,
      overflow_menu::Destination::Passwords,
      overflow_menu::Destination::Downloads,
      overflow_menu::Destination::RecentTabs,
      overflow_menu::Destination::SiteInfo,
      overflow_menu::Destination::Settings,
  };

  int today = TodaysDay();
  for (overflow_menu::Destination destination : destinations) {
    const base::Value* target =
        history.FindByDottedPath(DottedPath(today, destination));
    int expected_count =
        destination == overflow_menu::Destination::Bookmarks ? 21 : 20;

    EXPECT_NE(target, nullptr);
    EXPECT_EQ(expected_count, target->GetInt());
  }
}

// Tests that an existing destination click is incremented and written to Chrome
// Prefs.
TEST_F(DestinationUsageHistoryTest,
       HandlesExistingDestinationClickAndAddToPrefs) {
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
  base::Value::Dict prefHistory;
  base::Value::Dict prefDayHistory;
  prefDayHistory.Set(overflow_menu::StringNameForDestination(
                         overflow_menu::Destination::Bookmarks),
                     5);
  prefHistory.Set(base::NumberToString(TodaysDay()), std::move(prefDayHistory));

  // Create DestinationUsageHistory.
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistoryWithData(ranking, prefHistory);

  // Click bookmarks destination.
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::Bookmarks
      numAboveFoldDestinations:numAboveFoldDestinations];

  // Fetch saved destination usage history.
  const base::Value::Dict& history =
      destination_usage_history.prefService->GetDict(
          prefs::kOverflowMenuDestinationUsageHistory);

  // Query saved usage history for Bookmarks entry for `TodaysDay`.
  const base::Value* target = history.FindByDottedPath(
      DottedPath(TodaysDay(), overflow_menu::Destination::Bookmarks));

  // Verify bookmarks entry exists and has been clicked once.
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(destination_usage_history.prefService->HasPrefPath(
      prefs::kOverflowMenuDestinationUsageHistory));
  EXPECT_EQ(6, target->GetInt());
}

TEST_F(DestinationUsageHistoryTest, DoesNotSwapTwoShownDestinations) {
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
      CreateDestinationUsageHistoryWithData(ranking, history);

  // Click bookmarks Reading List (currently in ranking position 3) five times.
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::ReadingList
      numAboveFoldDestinations:numAboveFoldDestinations];
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::ReadingList
      numAboveFoldDestinations:numAboveFoldDestinations];
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::ReadingList
      numAboveFoldDestinations:numAboveFoldDestinations];
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::ReadingList
      numAboveFoldDestinations:numAboveFoldDestinations];
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::ReadingList
      numAboveFoldDestinations:numAboveFoldDestinations];

  // Verify that no ranking swaps occurred.
  std::vector<overflow_menu::Destination> newRanking =
      [destination_usage_history
          updatedRankWithCurrentRanking:ranking
               numAboveFoldDestinations:numAboveFoldDestinations];

  EXPECT_EQ(ranking, newRanking);
}

TEST_F(DestinationUsageHistoryTest, DoesNotSwapTwoUnshownDestinations) {
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
  base::Value::Dict dayHistory;
  dayHistory.Set(overflow_menu::StringNameForDestination(
                     overflow_menu::Destination::Bookmarks),
                 1);
  dayHistory.Set(overflow_menu::StringNameForDestination(
                     overflow_menu::Destination::History),
                 1);
  dayHistory.Set(overflow_menu::StringNameForDestination(
                     overflow_menu::Destination::ReadingList),
                 1);
  dayHistory.Set(overflow_menu::StringNameForDestination(
                     overflow_menu::Destination::Passwords),
                 1);
  dayHistory.Set(overflow_menu::StringNameForDestination(
                     overflow_menu::Destination::Downloads),
                 1);

  history.Set(base::NumberToString(TodaysDay()), std::move(dayHistory));

  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistoryWithData(ranking, history);

  // Click Recent Tabs (currently in ranking position 6) once.
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::RecentTabs
      numAboveFoldDestinations:numAboveFoldDestinations];
  EXPECT_EQ(ranking,
            [destination_usage_history
                updatedRankWithCurrentRanking:ranking
                     numAboveFoldDestinations:numAboveFoldDestinations]);

  // Click Site Inforamtion (currently in ranking position 7) once.
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::SiteInfo
      numAboveFoldDestinations:numAboveFoldDestinations];
  EXPECT_EQ(ranking,
            [destination_usage_history
                updatedRankWithCurrentRanking:ranking
                     numAboveFoldDestinations:numAboveFoldDestinations]);

  // Click Settings (currently in last position) once.
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::Settings
      numAboveFoldDestinations:numAboveFoldDestinations];
  EXPECT_EQ(ranking,
            [destination_usage_history
                updatedRankWithCurrentRanking:ranking
                     numAboveFoldDestinations:numAboveFoldDestinations]);
}

TEST_F(DestinationUsageHistoryTest, DeletesExpiredUsageData) {
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
  int recently_expired_day = TodaysDay() - 366;
  base::Value::Dict recently_expired_day_history;
  recently_expired_day_history.Set(overflow_menu::StringNameForDestination(
                                       overflow_menu::Destination::Bookmarks),
                                   1);
  history.Set(base::NumberToString(recently_expired_day),
              std::move(recently_expired_day_history));

  // Usage data almost 3 years old.
  int expired_day = TodaysDay() - 1000;
  base::Value::Dict expired_day_history;
  expired_day_history.Set(overflow_menu::StringNameForDestination(
                              overflow_menu::Destination::Bookmarks),
                          1);
  history.Set(base::NumberToString(expired_day),
              std::move(expired_day_history));

  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistoryWithData(ranking, history);

  // Click destination to trigger ranking algorithm which removes expired data.
  [destination_usage_history
         trackDestinationClick:overflow_menu::Destination::Settings
      numAboveFoldDestinations:numAboveFoldDestinations];

  // Fetch saved destination usage history.
  const base::Value::Dict& saved_history =
      destination_usage_history.prefService->GetDict(
          prefs::kOverflowMenuDestinationUsageHistory);

  std::set<std::string> seen_keys;
  for (auto&& [day, day_history] : saved_history)
    seen_keys.insert(day);

  std::set<std::string> expected_keys = {"ranking",
                                         base::NumberToString(TodaysDay())};
  ASSERT_EQ(expected_keys, seen_keys);
}
