// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_unittest.h"

#import <Foundation/Foundation.h>
#import <set>
#import <vector>

#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"
#import "ios/chrome/browser/promos_manager/promo.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The number of days since the Unix epoch; one day, in this context, runs
// from UTC midnight to UTC midnight.
int TodaysDay() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

}  // namespace

PromosManagerTest::PromosManagerTest() {
  scoped_feature_list_.InitWithFeatures({kFullscreenPromosManager}, {});
}
PromosManagerTest::~PromosManagerTest() {}

NSArray<ImpressionLimit*>* PromosManagerTest::TestImpressionLimits() {
  static NSArray<ImpressionLimit*>* limits;
  static dispatch_once_t onceToken;

  dispatch_once(&onceToken, ^{
    ImpressionLimit* oncePerWeek = [[ImpressionLimit alloc] initWithLimit:1
                                                               forNumDays:7];
    ImpressionLimit* twicePerMonth = [[ImpressionLimit alloc] initWithLimit:2
                                                                 forNumDays:31];
    limits = @[ oncePerWeek, twicePerMonth ];
  });

  return limits;
}

Promo* PromosManagerTest::TestPromo() {
  return [[Promo alloc] initWithIdentifier:promos_manager::Promo::Test];
}

Promo* PromosManagerTest::TestPromoWithImpressionLimits() {
  return [[Promo alloc] initWithIdentifier:promos_manager::Promo::Test
                       andImpressionLimits:TestImpressionLimits()];
}

void PromosManagerTest::CreatePromosManager() {
  CreatePrefs();
  promos_manager_ = std::make_unique<PromosManager>(local_state_.get());
  promos_manager_->Init();
}

// Create pref registry for tests.
void PromosManagerTest::CreatePrefs() {
  local_state_ = std::make_unique<TestingPrefServiceSimple>();

  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerImpressions);
  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerActivePromos);
  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerSingleDisplayActivePromos);
}

// Tests the initializer correctly creates a PromosManager* with the
// specified Pref service.
TEST_F(PromosManagerTest, InitWithPrefService) {
  CreatePromosManager();

  EXPECT_NE(local_state_->FindPreference(prefs::kIosPromosManagerImpressions),
            nullptr);
  EXPECT_NE(local_state_->FindPreference(prefs::kIosPromosManagerActivePromos),
            nullptr);
  EXPECT_NE(local_state_->FindPreference(
                prefs::kIosPromosManagerSingleDisplayActivePromos),
            nullptr);
  EXPECT_FALSE(local_state_->HasPrefPath(prefs::kIosPromosManagerImpressions));
  EXPECT_FALSE(local_state_->HasPrefPath(prefs::kIosPromosManagerActivePromos));
  EXPECT_FALSE(local_state_->HasPrefPath(
      prefs::kIosPromosManagerSingleDisplayActivePromos));
}

// Tests promos_manager::NameForPromo correctly returns the string
// representation of a given promo.
TEST_F(PromosManagerTest, ReturnsNameForTestPromo) {
  EXPECT_EQ(promos_manager::NameForPromo(promos_manager::Promo::Test),
            "promos_manager::Promo::Test");
}

// Tests promos_manager::PromoForName correctly returns the
// promos_manager::Promo given its string name.
TEST_F(PromosManagerTest, ReturnsTestPromoForName) {
  EXPECT_EQ(promos_manager::PromoForName("promos_manager::Promo::Test"),
            promos_manager::Promo::Test);
}

// Tests promos_manager::PromoForName correctly returns absl::nullopt for bad
// input.
TEST_F(PromosManagerTest, ReturnsNulloptForBadName) {
  EXPECT_FALSE(promos_manager::PromoForName("promos_manager::Promo::FOOBAR")
                   .has_value());
}

// Tests PromosManagerTest::TestPromo() correctly creates one mock promo.
TEST_F(PromosManagerTest, CreatesPromo) {
  Promo* promo = TestPromo();

  EXPECT_NE(promo, nil);
  EXPECT_EQ((int)promo.impressionLimits.count, 0);
}

// Tests PromosManagerTest::TestPromoWithImpressionLimits() correctly creates
// one mock promo with mock impression limits.
TEST_F(PromosManagerTest, CreatesPromoWithImpressionLimits) {
  Promo* promoWithImpressionLimits = TestPromoWithImpressionLimits();

  EXPECT_NE(promoWithImpressionLimits, nil);
  EXPECT_EQ((int)promoWithImpressionLimits.impressionLimits.count, 2);
}

// Tests PromosManagerTest::TestImpressionLimits() correctly creates two mock
// impression limits.
TEST_F(PromosManagerTest, CreatesImpressionLimits) {
  NSArray<ImpressionLimit*>* impressionLimits = TestImpressionLimits();

  EXPECT_NE(impressionLimits, nil);
  EXPECT_EQ(impressionLimits[0].numImpressions, 1);
  EXPECT_EQ(impressionLimits[0].numDays, 7);
  EXPECT_EQ(impressionLimits[1].numImpressions, 2);
  EXPECT_EQ(impressionLimits[1].numDays, 31);
}

// Tests PromosManager::ImpressionCounts() correctly returns a counts list from
// an impression counts map.
TEST_F(PromosManagerTest, ReturnsImpressionCounts) {
  std::map<promos_manager::Promo, int> promo_impression_counts = {
      {promos_manager::Promo::Test, 3},
      {promos_manager::Promo::AppStoreRating, 1},
      {promos_manager::Promo::CredentialProviderExtension, 6},
      {promos_manager::Promo::DefaultBrowser, 5},
  };

  std::vector<int> counts = {3, 5, 1, 6};

  EXPECT_EQ(promos_manager_->ImpressionCounts(promo_impression_counts), counts);
}

// Tests PromosManager::ImpressionCounts() correctly returns an empty counts
// list for an empty impression counts map.
TEST_F(PromosManagerTest, ReturnsEmptyImpressionCounts) {
  std::map<promos_manager::Promo, int> promo_impression_counts;
  std::vector<int> counts;

  EXPECT_EQ(promos_manager_->ImpressionCounts(promo_impression_counts), counts);
}

// Tests PromosManager::TotalImpressionCount() correctly adds the counts of
// different promos from an impression counts map.
TEST_F(PromosManagerTest, ReturnsTotalImpressionCount) {
  std::map<promos_manager::Promo, int> promo_impression_counts = {
      {promos_manager::Promo::Test, 3},
      {promos_manager::Promo::AppStoreRating, 1},
      {promos_manager::Promo::CredentialProviderExtension, 6},
      {promos_manager::Promo::DefaultBrowser, 5},

  };

  EXPECT_EQ(promos_manager_->TotalImpressionCount(promo_impression_counts), 15);
}

// Tests PromosManager::TotalImpressionCount() returns zero for an empty
// impression counts map.
TEST_F(PromosManagerTest, ReturnsZeroForTotalImpressionCount) {
  std::map<promos_manager::Promo, int> promo_impression_counts;

  EXPECT_EQ(promos_manager_->TotalImpressionCount(promo_impression_counts), 0);
}

// Tests PromosManager::MaxImpressionCount() correctly returns the max
// impression count from an impression counts map.
TEST_F(PromosManagerTest, ReturnsMaxImpressionCount) {
  std::map<promos_manager::Promo, int> promo_impression_counts = {
      {promos_manager::Promo::Test, 3},
      {promos_manager::Promo::AppStoreRating, 1},
      {promos_manager::Promo::CredentialProviderExtension, 6},
      {promos_manager::Promo::DefaultBrowser, 5},
  };

  EXPECT_EQ(promos_manager_->MaxImpressionCount(promo_impression_counts), 6);
}

// Tests PromosManager::MaxImpressionCount() correctly returns zero for an empty
// impression counts map.
TEST_F(PromosManagerTest, ReturnsZeroForMaxImpressionCount) {
  std::map<promos_manager::Promo, int> promo_impression_counts;

  EXPECT_EQ(promos_manager_->MaxImpressionCount(promo_impression_counts), 0);
}

// Tests PromosManager::AnyImpressionLimitTriggered() correctly detects an
// impression limit is triggered.
TEST_F(PromosManagerTest, DetectsSingleImpressionLimitTriggered) {
  ImpressionLimit* thricePerWeek = [[ImpressionLimit alloc] initWithLimit:3
                                                               forNumDays:7];
  NSArray<ImpressionLimit*>* limits = @[
    thricePerWeek,
  ];

  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(3, 1, limits), true);
  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(4, 5, limits), true);
  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(4, 6, limits), true);
  // This is technically the 8th day, so it's the start of a new week, and
  // doesn't hit the limit.
  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(3, 7, limits), false);
}

// Tests PromosManager::AnyImpressionLimitTriggered() correctly detects an
// impression limit is triggered over multiple impression limits.
TEST_F(PromosManagerTest, DetectsOneOfMultipleImpressionLimitsTriggered) {
  ImpressionLimit* onceEveryTwoDays = [[ImpressionLimit alloc] initWithLimit:1
                                                                  forNumDays:2];
  ImpressionLimit* thricePerWeek = [[ImpressionLimit alloc] initWithLimit:3
                                                               forNumDays:7];
  NSArray<ImpressionLimit*>* limits = @[
    thricePerWeek,
    onceEveryTwoDays,
  ];

  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(1, 1, limits), true);
  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(1, 2, limits), false);
  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(2, 2, limits), false);
  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(2, 4, limits), false);
}

// Tests PromosManager::AnyImpressionLimitTriggered() correctly detects no
// impression limits are triggered.
TEST_F(PromosManagerTest, DetectsNoImpressionLimitTriggered) {
  ImpressionLimit* onceEveryTwoDays = [[ImpressionLimit alloc] initWithLimit:1
                                                                  forNumDays:2];
  ImpressionLimit* thricePerWeek = [[ImpressionLimit alloc] initWithLimit:3
                                                               forNumDays:7];
  NSArray<ImpressionLimit*>* limits = @[ onceEveryTwoDays, thricePerWeek ];

  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(1, 1, nil), false);
  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(0, 3, limits), false);
  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(1, 5, limits), false);
  EXPECT_EQ(promos_manager_->AnyImpressionLimitTriggered(2, 5, limits), false);
}

// Tests PromosManager::CanShowPromo() correctly allows a promo to be shown
// because it hasn't met any impression limits.
TEST_F(PromosManagerTest, DecidesCanShowPromo) {
  CreatePromosManager();

  const std::vector<promos_manager::Impression> zeroImpressions = {};
  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::Test,
                                          zeroImpressions),
            true);
}

// Tests PromosManager::CanShowPromo() correctly denies promos from being shown
// as they've triggered impression limits.
TEST_F(PromosManagerTest, DecidesCannotShowPromo) {
  CreatePromosManager();

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 14),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 180),
  };

  // False because triggers no more than 1 impression per month global
  // impression limit.
  EXPECT_EQ(
      promos_manager_->CanShowPromo(promos_manager::Promo::Test, impressions),
      false);
  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::DefaultBrowser,
                                          impressions),
            false);
  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::AppStoreRating,
                                          impressions),
            false);
  // False because an impression has already been shown this month, even though
  // it's not the CredentialProviderExtension promo.
  EXPECT_EQ(
      promos_manager_->CanShowPromo(
          promos_manager::Promo::CredentialProviderExtension, impressions),
      false);
}

// Tests PromosManager::LeastRecentlyShown() correctly returns a list of active
// promos sorted by least recently shown.
TEST_F(PromosManagerTest, ReturnsLeastRecentlyShown) {
  const std::set<promos_manager::Promo> active_promos = {
      promos_manager::Promo::Test,
      promos_manager::Promo::CredentialProviderExtension,
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::DefaultBrowser,
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 14),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 180),
  };

  std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::CredentialProviderExtension,
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::DefaultBrowser,
      promos_manager::Promo::Test,
  };

  EXPECT_EQ(promos_manager_->LeastRecentlyShown(active_promos, impressions),
            expected);
}

// Tests PromosManager::LeastRecentlyShown() correctly returns a list of
// active / promos sorted by least recently shown (with some impressions /
// belonging to inactive promo campaigns).
TEST_F(PromosManagerTest, ReturnsLeastRecentlyShownWithSomeInactivePromos) {
  const std::set<promos_manager::Promo> active_promos = {
      promos_manager::Promo::Test,
      promos_manager::Promo::AppStoreRating,
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 14),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 180),
  };

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::Test,
  };

  EXPECT_EQ(promos_manager_->LeastRecentlyShown(active_promos, impressions),
            expected);
}

// Tests PromosManager::LeastRecentlyShown() gracefully returns a list of
// active / promos when multiple promos are tied for least recently shown.
TEST_F(PromosManagerTest, ReturnsLeastRecentlyShownBreakingTies) {
  const std::set<promos_manager::Promo> active_promos = {
      promos_manager::Promo::Test,
      promos_manager::Promo::CredentialProviderExtension,
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::DefaultBrowser,
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser, today),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating, today),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today),
  };

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::CredentialProviderExtension,
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::DefaultBrowser,
      promos_manager::Promo::Test,
  };

  EXPECT_EQ(promos_manager_->LeastRecentlyShown(active_promos, impressions),
            expected);
}

// Tests PromosManager::LeastRecentlyShown() gracefully returns a single promo
// in a list when the impression history contains only one active promo
// campaign.
TEST_F(PromosManagerTest, ReturnsLeastRecentlyShownWithOnlyOnePromoActive) {
  const std::set<promos_manager::Promo> active_promos = {
      promos_manager::Promo::Test,
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 14),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 180),
  };

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::Test,
  };

  EXPECT_EQ(promos_manager_->LeastRecentlyShown(active_promos, impressions),
            expected);
}

// Tests PromosManager::LeastRecentlyShown() gracefully returns an empty array
// when there are no active promo campaigns.
TEST_F(PromosManagerTest,
       ReturnsEmptyListWhenLeastRecentlyShownHasNoActivePromoCampaigns) {
  const std::set<promos_manager::Promo> active_promos;

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 14),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 180),
  };

  const std::vector<promos_manager::Promo> expected;

  EXPECT_EQ(promos_manager_->LeastRecentlyShown(active_promos, impressions),
            expected);
}

// Tests PromosManager::LeastRecentlyShown() gracefully returns an empty array
// when no impression history exists (i.e. there's no 'least recently
// shown' promo because empty impression history means no promo has ever been
// shown.)
TEST_F(PromosManagerTest,
       ReturnsEmptyListWhenLeastRecentlyShownHasNoImpressionHistory) {
  const std::set<promos_manager::Promo> active_promos = {
      promos_manager::Promo::CredentialProviderExtension,
  };

  const std::vector<promos_manager::Impression> impressions;
  const std::vector<promos_manager::Promo> expected;

  EXPECT_EQ(promos_manager_->LeastRecentlyShown(active_promos, impressions),
            expected);
}

// Tests PromosManager::LeastRecentlyShown() sorts unshown promos before shown
// promos.
TEST_F(PromosManagerTest,
       SortsUnshownPromosBeforeShownPromosForLeastRecentlyShown) {
  const std::set<promos_manager::Promo> active_promos = {
      promos_manager::Promo::Test,
      promos_manager::Promo::CredentialProviderExtension,
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::DefaultBrowser,
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7),
  };

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::CredentialProviderExtension,
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::DefaultBrowser,
      promos_manager::Promo::Test,
  };

  EXPECT_EQ(promos_manager_->LeastRecentlyShown(active_promos, impressions),
            expected);
}

// Tests PromosManager::ImpressionHistory() correctly ingests impression history
// (base::Value::List) and returns corresponding
// std::vector<promos_manager::Impression>.
TEST_F(PromosManagerTest, ReturnsImpressionHistory) {
  int today = TodaysDay();

  base::Value::Dict first_impression;
  first_impression.Set(
      promos_manager::kImpressionPromoKey,
      promos_manager::NameForPromo(promos_manager::Promo::DefaultBrowser));
  first_impression.Set(promos_manager::kImpressionDayKey, today);

  base::Value::Dict second_impression;
  second_impression.Set(
      promos_manager::kImpressionPromoKey,
      promos_manager::NameForPromo(promos_manager::Promo::AppStoreRating));
  second_impression.Set(promos_manager::kImpressionDayKey, today - 7);

  base::Value::List impressions;
  impressions.Append(first_impression.Clone());
  impressions.Append(second_impression.Clone());

  std::vector<promos_manager::Impression> expected = {
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser, today),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 7),
  };

  std::vector<promos_manager::Impression> result =
      promos_manager_->ImpressionHistory(impressions);

  EXPECT_EQ(expected.size(), result.size());
  EXPECT_EQ(expected[0].promo, result[0].promo);
  EXPECT_EQ(expected[0].day, result[0].day);
  EXPECT_EQ(expected[1].promo, result[1].promo);
  EXPECT_EQ(expected[1].day, result[1].day);
}

// Tests PromosManager::ImpressionHistory() correctly ingests empty impression
// history (base::Value::List) and returns empty
// std::vector<promos_manager::Impression>.
TEST_F(PromosManagerTest, ReturnsBlankImpressionHistoryForBlankPrefs) {
  base::Value::List impressions;

  std::vector<promos_manager::Impression> result =
      promos_manager_->ImpressionHistory(impressions);

  EXPECT_TRUE(result.empty());
}

// Tests PromosManager::ImpressionHistory() correctly ingests impression history
// with malformed data (base::Value::List) and returns corresponding
// std::vector<promos_manager::Impression> without malformed entries.
TEST_F(PromosManagerTest, ReturnsImpressionHistoryBySkippingMalformedEntries) {
  int today = TodaysDay();

  base::Value::Dict first_impression;
  first_impression.Set(
      promos_manager::kImpressionPromoKey,
      promos_manager::NameForPromo(promos_manager::Promo::DefaultBrowser));
  first_impression.Set(promos_manager::kImpressionDayKey, today);

  base::Value::Dict second_impression;
  second_impression.Set("foobar", promos_manager::NameForPromo(
                                      promos_manager::Promo::AppStoreRating));
  second_impression.Set(promos_manager::kImpressionDayKey, today - 7);

  base::Value::List impressions;
  impressions.Append(first_impression.Clone());
  impressions.Append(second_impression.Clone());

  std::vector<promos_manager::Impression> expected = {
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser, today),
  };

  std::vector<promos_manager::Impression> result =
      promos_manager_->ImpressionHistory(impressions);

  EXPECT_EQ(expected.size(), result.size());
  EXPECT_EQ(expected[0].promo, result[0].promo);
  EXPECT_EQ(expected[0].day, result[0].day);
}

// Tests PromosManager::ActivePromos() correctly ingests active promos
// (base::Value::List) and returns corresponding
// std::vector<promos_manager::Promo>.
TEST_F(PromosManagerTest, ReturnsActivePromos) {
  base::Value::List promos;
  promos.Append("promos_manager::Promo::DefaultBrowser");
  promos.Append("promos_manager::Promo::AppStoreRating");
  promos.Append("promos_manager::Promo::CredentialProviderExtension");

  std::set<promos_manager::Promo> expected = {
      promos_manager::Promo::DefaultBrowser,
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::CredentialProviderExtension,
  };

  std::set<promos_manager::Promo> result =
      promos_manager_->ActivePromos(promos);

  EXPECT_EQ(expected, promos_manager_->ActivePromos(promos));
}

// Tests PromosManager::ActivePromos() correctly ingests empty active promos
// (base::Value::List) and returns empty std::set<promos_manager::Promo>.
TEST_F(PromosManagerTest, ReturnsBlankActivePromosForBlankPrefs) {
  base::Value::List promos;

  std::set<promos_manager::Promo> result =
      promos_manager_->ActivePromos(promos);

  EXPECT_TRUE(result.empty());
}

// Tests PromosManager::ActivePromos() correctly ingests active promos with
// malformed data (base::Value::List) and returns corresponding
// std::vector<promos_manager::Promo> with malformed entries pruned.
TEST_F(PromosManagerTest, ReturnsActivePromosAndSkipsMalformedData) {
  base::Value::List promos;
  promos.Append("promos_manager::Promo::DefaultBrowser");
  promos.Append("promos_manager::Promo::AppStoreRating");
  promos.Append("promos_manager::Promo::FOOBAR");

  std::set<promos_manager::Promo> expected = {
      promos_manager::Promo::DefaultBrowser,
      promos_manager::Promo::AppStoreRating,
  };

  std::set<promos_manager::Promo> result =
      promos_manager_->ActivePromos(promos);

  EXPECT_EQ(expected, promos_manager_->ActivePromos(promos));
}

// Tests PromosManager::RegisterPromoForContinuousDisplay() correctly registers
// a promo for continuous display by writing the promo's name to the pref
// `kIosPromosManagerActivePromos`.
TEST_F(PromosManagerTest, RegistersPromoForContinuousDisplay) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerActivePromos).empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::AppStoreRating);
  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)2);

  // Register new promo.
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::DefaultBrowser);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)3);
  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos)[0],
            promos_manager::NameForPromo(
                promos_manager::Promo::CredentialProviderExtension));
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerActivePromos)[1],
      promos_manager::NameForPromo(promos_manager::Promo::AppStoreRating));
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerActivePromos)[2],
      promos_manager::NameForPromo(promos_manager::Promo::DefaultBrowser));
}

// Tests PromosManager::RegisterPromoForContinuousDisplay() correctly registers
// a promo—for the very first time—for continuous display by writing the
// promo's name to the pref `kIosPromosManagerActivePromos`.
TEST_F(PromosManagerTest,
       RegistersPromoForContinuousDisplayForEmptyActivePromos) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerActivePromos).empty());

  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::DefaultBrowser);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)1);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerActivePromos)[0],
      promos_manager::NameForPromo(promos_manager::Promo::DefaultBrowser));
}

// Tests PromosManager::RegisterPromoForContinuousDisplay() correctly registers
// an already-registered promo for continuous display by first erasing, and then
// re-writing, the promo's name to the pref `kIosPromosManagerActivePromos`;
// tests no duplicate entries are created.
TEST_F(PromosManagerTest, RegistersAlreadyRegisteredPromoForContinuousDisplay) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerActivePromos).empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::AppStoreRating);
  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)2);

  // Register existing promo.
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)2);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerActivePromos)[0],
      promos_manager::NameForPromo(promos_manager::Promo::AppStoreRating));
  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos)[1],
            promos_manager::NameForPromo(
                promos_manager::Promo::CredentialProviderExtension));
}

// Tests PromosManager::RegisterPromoForContinuousDisplay() correctly registers
// an already-registered promo for continuous display—for the very first time—by
// first erasing, and then re-writing, the promo's name to the pref
// `kIosPromosManagerActivePromos`; tests no duplicate entries are created.
TEST_F(
    PromosManagerTest,
    RegistersAlreadyRegisteredPromoForContinuousDisplayForEmptyActivePromos) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerActivePromos).empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  // Register existing promo.
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)1);
  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos)[0],
            promos_manager::NameForPromo(
                promos_manager::Promo::CredentialProviderExtension));
}

// Tests PromosManager::RegisterPromoForSingleDisplay() correctly registers
// a promo for single display by writing the promo's name to the pref
// `kIosPromosManagerSingleDisplayActivePromos`.
TEST_F(PromosManagerTest, RegistersPromoForSingleDisplay) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::AppStoreRating);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)2);

  // Register new promo.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DefaultBrowser);

  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)3);
  EXPECT_EQ(local_state_->GetList(
                prefs::kIosPromosManagerSingleDisplayActivePromos)[0],
            promos_manager::NameForPromo(
                promos_manager::Promo::CredentialProviderExtension));
  EXPECT_EQ(
      local_state_->GetList(
          prefs::kIosPromosManagerSingleDisplayActivePromos)[1],
      promos_manager::NameForPromo(promos_manager::Promo::AppStoreRating));
  EXPECT_EQ(
      local_state_->GetList(
          prefs::kIosPromosManagerSingleDisplayActivePromos)[2],
      promos_manager::NameForPromo(promos_manager::Promo::DefaultBrowser));
}

// Tests PromosManager::RegisterPromoForSingleDisplay() correctly registers
// a promo for single display—for the very first time—by writing the promo's
// name to the pref `kIosPromosManagerSingleDisplayActivePromos`.
TEST_F(PromosManagerTest, RegistersPromoForSingleDisplayForEmptyActivePromos) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .empty());

  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DefaultBrowser);

  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)1);
  EXPECT_EQ(
      local_state_->GetList(
          prefs::kIosPromosManagerSingleDisplayActivePromos)[0],
      promos_manager::NameForPromo(promos_manager::Promo::DefaultBrowser));
}

// Tests PromosManager::RegisterPromoForSingleDisplay() correctly registers
// an already-registered promo for single display by first erasing, and then
// re-writing, the promo's name to the pref
// `kIosPromosManagerSingleDisplayActivePromos`; tests no duplicate entries are
// created.
TEST_F(PromosManagerTest, RegistersAlreadyRegisteredPromoForSingleDisplay) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::AppStoreRating);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)2);

  // Register existing promo.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)2);
  EXPECT_EQ(
      local_state_->GetList(
          prefs::kIosPromosManagerSingleDisplayActivePromos)[0],
      promos_manager::NameForPromo(promos_manager::Promo::AppStoreRating));
  EXPECT_EQ(local_state_->GetList(
                prefs::kIosPromosManagerSingleDisplayActivePromos)[1],
            promos_manager::NameForPromo(
                promos_manager::Promo::CredentialProviderExtension));
}

// Tests PromosManager::RegisterPromoForSingleDisplay() correctly registers
// an already-registered promo for single display—for the very first time—by
// first erasing, and then re-writing, the promo's name to the pref
// `kIosPromosManagerSingleDisplayActivePromos`; tests no duplicate entries are
// created.
TEST_F(PromosManagerTest,
       RegistersAlreadyRegisteredPromoForSingleDisplayForEmptyActivePromos) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  // Register existing promo.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)1);
  EXPECT_EQ(local_state_->GetList(
                prefs::kIosPromosManagerSingleDisplayActivePromos)[0],
            promos_manager::NameForPromo(
                promos_manager::Promo::CredentialProviderExtension));
}

// Tests PromosManager::InitializePromoImpressionLimits() correctly registers
// promo-specific impression limits.
TEST_F(PromosManagerTest, RegistersPromoSpecificImpressionLimits) {
  CreatePromosManager();

  ImpressionLimit* onceEveryTwoDays = [[ImpressionLimit alloc] initWithLimit:1
                                                                  forNumDays:2];
  ImpressionLimit* thricePerWeek = [[ImpressionLimit alloc] initWithLimit:3
                                                               forNumDays:7];
  NSArray<ImpressionLimit*>* defaultBrowserLimits = @[
    onceEveryTwoDays,
  ];

  NSArray<ImpressionLimit*>* credentialProviderLimits = @[
    thricePerWeek,
  ];

  base::small_map<std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>
      promoImpressionLimits;
  promoImpressionLimits[promos_manager::Promo::DefaultBrowser] =
      defaultBrowserLimits;
  promoImpressionLimits[promos_manager::Promo::CredentialProviderExtension] =
      credentialProviderLimits;
  promos_manager_->InitializePromoImpressionLimits(
      std::move(promoImpressionLimits));

  EXPECT_EQ(promos_manager_->PromoImpressionLimits(
                promos_manager::Promo::DefaultBrowser),
            defaultBrowserLimits);
  EXPECT_EQ(promos_manager_->PromoImpressionLimits(
                promos_manager::Promo::CredentialProviderExtension),
            credentialProviderLimits);
}

// Tests PromosManager::RecordImpression() correctly records a new impression.
TEST_F(PromosManagerTest, RecordsImpression) {
  CreatePromosManager();
  promos_manager_->RecordImpression(promos_manager::Promo::DefaultBrowser);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerImpressions).size(),
            (size_t)1);

  promos_manager_->RecordImpression(
      promos_manager::Promo::CredentialProviderExtension);

  const auto& impression_history =
      local_state_->GetList(prefs::kIosPromosManagerImpressions);
  const base::Value::Dict& first_impression = impression_history[0].GetDict();
  const base::Value::Dict& second_impression = impression_history[1].GetDict();

  EXPECT_EQ(impression_history.size(), (size_t)2);
  EXPECT_EQ(*first_impression.FindString(promos_manager::kImpressionPromoKey),
            "promos_manager::Promo::DefaultBrowser");
  EXPECT_TRUE(
      first_impression.FindInt(promos_manager::kImpressionDayKey).has_value());
  EXPECT_EQ(first_impression.FindInt(promos_manager::kImpressionDayKey).value(),
            TodaysDay());
  EXPECT_EQ(*second_impression.FindString(promos_manager::kImpressionPromoKey),
            "promos_manager::Promo::CredentialProviderExtension");
  EXPECT_TRUE(
      second_impression.FindInt(promos_manager::kImpressionDayKey).has_value());
  EXPECT_EQ(
      second_impression.FindInt(promos_manager::kImpressionDayKey).value(),
      TodaysDay());
}

// Tests PromosManager::DeregisterPromo() correctly deregisters a currently
// active promo campaign.
TEST_F(PromosManagerTest, DeregistersActivePromo) {
  CreatePromosManager();

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)0);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)0);

  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)1);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)0);

  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)1);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)1);

  promos_manager_->DeregisterPromo(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)0);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)0);
}

// Tests PromosManager::DeregisterPromo() gracefully handles deregistration if
// the promo doesn't exist in a given active promos list.
TEST_F(PromosManagerTest, DeregistersNonExistentPromo) {
  CreatePromosManager();

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)0);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)0);

  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)1);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)0);

  promos_manager_->DeregisterPromo(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)0);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)0);
}
