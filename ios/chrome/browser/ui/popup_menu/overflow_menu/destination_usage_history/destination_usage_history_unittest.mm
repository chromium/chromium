// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#include "testing/platform_test.h"

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
  DestinationUsageHistory* CreateDestinationUsageHistory() {
    CreatePrefs();

    destination_usage_history_ =
        [[DestinationUsageHistory alloc] initWithPrefService:prefs_.get()];

    return destination_usage_history_;
  }

  DestinationUsageHistory* CreatePopulatedDestinationUsageHistory() {
    CreatePrefsWithInitialData();
    destination_usage_history_ =
        [[DestinationUsageHistory alloc] initWithPrefService:prefs_.get()];
    return destination_usage_history_;
  }

  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuDestinationUsageHistory, PrefRegistry::LOSSY_PREF);
  }

  void CreatePrefsWithInitialData() {
    CreatePrefs();
    base::Value::Dict history;
    base::Value::Dict day_history;
    day_history.Set(overflow_menu::StringNameForDestination(
                        overflow_menu::Destination::Bookmarks),
                    3);
    history.Set(base::NumberToString(TodaysDay()), std::move(day_history));
    prefs_->SetDict(prefs::kOverflowMenuDestinationUsageHistory,
                    std::move(history));
  }

  std::string DottedPath(std::string day, std::string destination_name) {
    return day + "." + destination_name;
  }

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  DestinationUsageHistory* destination_usage_history_;
};

// Tests the initializer correctly creates a DestinationUsageHistory* with the
// specified Pref service.
TEST_F(DestinationUsageHistoryTest, InitWithPrefService) {
  CreateDestinationUsageHistory();

  PrefService* pref_service = destination_usage_history_.prefService;

  EXPECT_NE(
      pref_service->FindPreference(prefs::kOverflowMenuDestinationUsageHistory),
      nullptr);
  EXPECT_FALSE(
      pref_service->HasPrefPath(prefs::kOverflowMenuDestinationUsageHistory));
}

// Tests the initializer correctly creates a DestinationUsageHistory* with the
// specified Pref service, when the prefs have existing data.
TEST_F(DestinationUsageHistoryTest, InitWithPrefServiceForDirtyPrefs) {
  CreatePopulatedDestinationUsageHistory();

  PrefService* pref_service = destination_usage_history_.prefService;

  EXPECT_NE(
      pref_service->FindPreference(prefs::kOverflowMenuDestinationUsageHistory),
      nullptr);
  EXPECT_TRUE(
      pref_service->HasPrefPath(prefs::kOverflowMenuDestinationUsageHistory));
}

// Tests that a new destination click is incremented and written to Chrome
// Prefs.
TEST_F(DestinationUsageHistoryTest, HandlesNewDestinationClickAndAddToPrefs) {
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory();

  // Click bookmarks destination.
  [destination_usage_history
      trackDestinationClick:overflow_menu::Destination::Bookmarks];

  // Fetch saved destination usage history.
  const base::Value* history =
      destination_usage_history_.prefService->GetDictionary(
          prefs::kOverflowMenuDestinationUsageHistory);
  ASSERT_NE(history, nullptr);
  ASSERT_TRUE(history->is_dict());

  const base::Value::Dict* history_dict = history->GetIfDict();
  ASSERT_NE(history, nullptr);

  std::string today = base::NumberToString(TodaysDay());
  std::string destination = overflow_menu::StringNameForDestination(
      overflow_menu::Destination::Bookmarks);
  std::string dotted_path = DottedPath(today, destination);

  // Query saved usage history for Bookmarks entry for |today|.
  const base::Value* target = history_dict->FindByDottedPath(dotted_path);

  // Verify bookmarks entry exists and has been clicked once.
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(destination_usage_history_.prefService->HasPrefPath(
      prefs::kOverflowMenuDestinationUsageHistory));
  EXPECT_EQ(1, target->GetInt());
}

// Tests that an existing destination click is incremented and written to Chrome
// Prefs.
TEST_F(DestinationUsageHistoryTest,
       HandlesExistingDestinationClickAndAddToPrefs) {
  DestinationUsageHistory* destination_usage_history =
      CreatePopulatedDestinationUsageHistory();

  // Click bookmarks destination.
  [destination_usage_history
      trackDestinationClick:overflow_menu::Destination::Bookmarks];

  // Fetch saved destination usage history.
  const base::Value* history =
      destination_usage_history_.prefService->GetDictionary(
          prefs::kOverflowMenuDestinationUsageHistory);
  ASSERT_NE(history, nullptr);
  ASSERT_TRUE(history->is_dict());

  const base::Value::Dict* history_dict = history->GetIfDict();
  ASSERT_NE(history, nullptr);

  std::string today = base::NumberToString(TodaysDay());
  std::string destination = overflow_menu::StringNameForDestination(
      overflow_menu::Destination::Bookmarks);
  std::string dotted_path = DottedPath(today, destination);

  // Query saved usage history for Bookmarks entry for |today|.
  const base::Value* target = history_dict->FindByDottedPath(dotted_path);

  // Verify bookmarks entry exists and has been clicked once.
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(destination_usage_history_.prefService->HasPrefPath(
      prefs::kOverflowMenuDestinationUsageHistory));
  EXPECT_EQ(4, target->GetInt());
}
