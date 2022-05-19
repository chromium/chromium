// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history.h"

#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/pref_names.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class DestinationUsageHistoryTest : public PlatformTest {
 public:
  DestinationUsageHistoryTest() {}

 protected:
  DestinationUsageHistory* CreateDestinationUsageHistory() {
    // Destination Usage History must be created with a Pref service.
    CreatePrefs();

    destination_usage_history_ =
        [[DestinationUsageHistory alloc] initWithPrefService:prefs_.get()];

    return destination_usage_history_;
  }

  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuDestinationUsageHistory, PrefRegistry::LOSSY_PREF);
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

// Tests that a destination click is written to Chrome Prefs.
TEST_F(DestinationUsageHistoryTest, AddsBookmarksClickToPrefs) {
  DestinationUsageHistory* destination_usage_history =
      CreateDestinationUsageHistory();
  [destination_usage_history trackDestinationClick:@"bookmarks"];

  PrefService* pref_service = destination_usage_history_.prefService;

  const base::Value* dictionary =
      pref_service->GetDictionary(prefs::kOverflowMenuDestinationUsageHistory);
  ASSERT_NE(dictionary, nullptr);

  const base::Value* dictionary_value =
      dictionary->FindKeyOfType("lastClicked", base::Value::Type::STRING);
  ASSERT_NE(dictionary_value, nullptr);

  EXPECT_TRUE(
      pref_service->HasPrefPath(prefs::kOverflowMenuDestinationUsageHistory));
  EXPECT_EQ("bookmarks", dictionary_value->GetString());
}
