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

class OverflowMenuOrdererTest : public PlatformTest {
 public:
  OverflowMenuOrdererTest() {}

 protected:
  void TearDown() override {
    [overflow_menu_orderer_ disconnect];

    PlatformTest::TearDown();
  }

  void InitializeOverflowMenuOrderer() {
    CreatePrefs();

    overflow_menu_orderer_ =
        [[OverflowMenuOrderer alloc] initWithIsIncognito:NO];

    overflow_menu_orderer_.localStatePrefs = prefs_.get();

    overflow_menu_orderer_.visibleDestinationsCount = kVisibleDestinationsCount;
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
  OverflowMenuOrderer* overflow_menu_orderer_;
};

// Tests that the ranking pref gets populated after sorting once.
TEST_F(OverflowMenuOrdererTest, StoresInitialRanking) {
  InitializeOverflowMenuOrderer();
  NSArray<OverflowMenuDestination*>* sample_destinations = SampleDestinations();
  [overflow_menu_orderer_
      sortedDestinationsFromCarouselDestinations:sample_destinations];

  const base::Value::List& stored_ranking =
      prefs_->GetList(prefs::kOverflowMenuDestinationsOrder);

  EXPECT_EQ(stored_ranking.size(), sample_destinations.count);
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
