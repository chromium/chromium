// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_unittest.h"

#import <Foundation/Foundation.h>

#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"
#import "ios/chrome/browser/promos_manager/promo.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "testing/platform_test.h"

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
}

// Tests the initializer correctly creates a PromosManager* with the
// specified Pref service.
TEST_F(PromosManagerTest, InitWithPrefService) {
  CreatePromosManager();

  EXPECT_NE(local_state_->FindPreference(prefs::kIosPromosManagerImpressions),
            nullptr);
  EXPECT_NE(local_state_->FindPreference(prefs::kIosPromosManagerActivePromos),
            nullptr);
  EXPECT_FALSE(local_state_->HasPrefPath(prefs::kIosPromosManagerImpressions));
  EXPECT_FALSE(local_state_->HasPrefPath(prefs::kIosPromosManagerActivePromos));
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

// Tests the last seen day (int) is correctly returned for given
// promos_manager::Promo(s).
TEST_F(PromosManagerTest, ReturnsLastSeenDayForPromo) {
  std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, 7),
      promos_manager::Impression(promos_manager::Promo::Test, 3),
      promos_manager::Impression(promos_manager::Promo::Test, 1),
  };

  EXPECT_EQ(
      promos_manager_->LastSeenDay(promos_manager::Promo::Test, impressions),
      7);
}

// Tests the sentinel value is returned when a given promos_manager::Promo isn't
// found in the impression history.
TEST_F(PromosManagerTest, ReturnsSentinelForNonExistentPromo) {
  std::vector<promos_manager::Impression> impressions;

  EXPECT_EQ(
      promos_manager_->LastSeenDay(promos_manager::Promo::Test, impressions),
      promos_manager::kLastSeenDayPromoNotFound);
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
  const std::vector<promos_manager::Impression> zeroImpressions = {};
  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::Test,
                                          zeroImpressions),
            true);
}

// Tests PromosManager::CanShowPromo() correctly denies promos from being shown
// as they've triggered impression limits.
TEST_F(PromosManagerTest, DecidesCannotShowPromo) {
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
