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
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
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

}  // namespace

class DestinationUsageHistoryTest : public PlatformTest {
 public:
  DestinationUsageHistoryTest() {}

 protected:
  void TearDown() override {
    [destination_usage_history_ stop];

    PlatformTest::TearDown();
  }

  // Initializes `destination_usage_history_` with empty pref data and returns
  // the initial ranking.
  DestinationRanking InitializeDestinationUsageHistory(
      DestinationRanking default_destinations) {
    CreatePrefs();

    destination_usage_history_ =
        [[DestinationUsageHistory alloc] initWithPrefService:prefs_.get()];

    destination_usage_history_.visibleDestinationsCount =
        kVisibleDestinationsCount;

    [destination_usage_history_ start];

    DestinationRanking initial_ranking = [destination_usage_history_
        sortedDestinationsFromCurrentRanking:{}
                       availableDestinations:default_destinations];

    return initial_ranking;
  }

  // Initializes `destination_usage_history_` with past data and `ranking` and
  // returns the new ranking.
  DestinationRanking InitializeDestinationUsageHistoryWithData(
      DestinationRanking& ranking,
      base::Value::Dict& history,
      DestinationRanking default_destinations) {
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

    DestinationRanking initial_ranking = [destination_usage_history_
        sortedDestinationsFromCurrentRanking:ranking
                       availableDestinations:default_destinations];

    return initial_ranking;
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
  DestinationUsageHistory* destination_usage_history_;
};

// Tests the initializer correctly creates a DestinationUsageHistory* with the
// specified Pref service.
TEST_F(DestinationUsageHistoryTest, InitWithPrefService) {
  InitializeDestinationUsageHistory(SampleDestinations());

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
  InitializeDestinationUsageHistoryWithData(ranking, history,
                                            SampleDestinations());

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
  InitializeDestinationUsageHistory(SampleDestinations());

  // Click bookmarks destination.
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::Bookmarks];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  std::optional<int> expected = update->FindIntByDottedPath(
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
  DestinationRanking sample_destinations = SampleDestinations();

  InitializeDestinationUsageHistory(sample_destinations);

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  for (overflow_menu::Destination destination : sample_destinations) {
    const std::string dotted_path =
        DottedPath(TodaysDay().InDays(), destination);

    std::optional<int> expected = update->FindIntByDottedPath(dotted_path);

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
  InitializeDestinationUsageHistoryWithData(ranking, history,
                                            SampleDestinations());

  // Click bookmarks destination.
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::Bookmarks];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  std::optional<int> expected = update->FindIntByDottedPath(
      DottedPath(TodaysDay().InDays(), overflow_menu::Destination::Bookmarks));

  // Verify bookmarks entry exists.
  EXPECT_TRUE(expected.has_value());

  // Verify bookmarks entry has single click (plus the existing 5 clicks)
  // for today.
  EXPECT_EQ(expected.value(), 6);
}

TEST_F(DestinationUsageHistoryTest, DoesNotSwapTwoShownDestinations) {
  DestinationRanking sample_destinations = SampleDestinations();

  DestinationRanking ranking = {
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

  DestinationRanking initial_ranking =
      InitializeDestinationUsageHistoryWithData(ranking, history,
                                                sample_destinations);
  // Click bookmarks Reading List five
  // times.
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::ReadingList];
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::ReadingList];
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::ReadingList];
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::ReadingList];
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::ReadingList];

  DestinationRanking sorted_ranking = [destination_usage_history_
      sortedDestinationsFromCurrentRanking:initial_ranking
                     availableDestinations:sample_destinations];

  EXPECT_EQ(initial_ranking, sorted_ranking);
}

TEST_F(DestinationUsageHistoryTest, DoesNotSwapTwoUnshownDestinations) {
  DestinationRanking sample_destinations = SampleDestinations();

  DestinationRanking ranking = {
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
                  3);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::History),
                  3);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::ReadingList),
                  3);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::Passwords),
                  3);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::Downloads),
                  3);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::RecentTabs),
                  1);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::SiteInfo),
                  1);
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::Settings),
                  1);

  history.Set(base::NumberToString(TodaysDay().InDays()),
              std::move(day_history));

  DestinationRanking initial_ranking =
      InitializeDestinationUsageHistoryWithData(ranking, history,
                                                sample_destinations);

  // Click Recent Tabs (currently in ranking position 6) once.
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::RecentTabs];

  // Click Site Inforamtion (currently in ranking position 7) once.
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::SiteInfo];

  // Click Settings (currently in last position) once.
  [destination_usage_history_
      recordClickForDestination:overflow_menu::Destination::Settings];

  DestinationRanking sorted_ranking = [destination_usage_history_
      sortedDestinationsFromCurrentRanking:initial_ranking
                     availableDestinations:sample_destinations];

  EXPECT_EQ(initial_ranking, sorted_ranking);
}

TEST_F(DestinationUsageHistoryTest, DeletesExpiredUsageData) {
  DestinationRanking sample_destinations = SampleDestinations();

  DestinationRanking ranking = {
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

  InitializeDestinationUsageHistoryWithData(ranking, history,
                                            sample_destinations);

  [destination_usage_history_
      sortedDestinationsFromCurrentRanking:ranking
                     availableDestinations:sample_destinations];

  ScopedDictPrefUpdate update(prefs_.get(),
                              prefs::kOverflowMenuDestinationUsageHistory);

  // Has one entry for today's seeded history.
  EXPECT_EQ(update->size(), (size_t)1);
  EXPECT_NE(update->Find(base::NumberToString(TodaysDay().InDays())), nullptr);
  EXPECT_EQ(update->Find(base::NumberToString(recently_expired_day.InDays())),
            nullptr);
  EXPECT_EQ(update->Find(base::NumberToString(expired_day.InDays())), nullptr);
}

TEST_F(DestinationUsageHistoryTest, ClearsUsageData) {
  base::Value::Dict history;

  // Usage data for yesterday.
  base::TimeDelta day = TodaysDay() - base::Days(1);
  base::Value::Dict day_history;
  day_history.Set(overflow_menu::StringNameForDestination(
                      overflow_menu::Destination::Bookmarks),
                  1);
  history.Set(base::NumberToString(day.InDays()), std::move(day_history));

  DestinationRanking ranking;
  InitializeDestinationUsageHistoryWithData(ranking, history, {});

  [destination_usage_history_ clearStoredClickData];

  const base::Value::Dict& new_history =
      prefs_->GetDict(prefs::kOverflowMenuDestinationUsageHistory);
  EXPECT_EQ(new_history.size(), 0u);
}
