// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <set>
#import <vector>

#import "base/json/values_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/simple_test_clock.h"
#import "base/values.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"
#import "ios/chrome/browser/promos_manager/promo.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager_impl.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

using PromoContext = PromosManagerImpl::PromoContext;

namespace {

// The number of days since the Unix epoch; one day, in this context, runs
// from UTC midnight to UTC midnight.
int TodaysDay() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

const base::TimeDelta kTimeDelta1Day = base::Days(1);
const base::TimeDelta kTimeDelta1Hour = base::Hours(1);

const PromoContext kPromoContextForActive = PromoContext{
    .was_pending = false,
};

}  // namespace

class PromosManagerImplTest : public PlatformTest {
 public:
  PromosManagerImplTest();
  ~PromosManagerImplTest() override;

  // Creates a mock promo without impression limits.
  Promo* TestPromo();

  // Creates a mock promo with impression limits.
  Promo* TestPromoWithImpressionLimits();

  // Creates mock impression limits.
  NSArray<ImpressionLimit*>* TestImpressionLimits();

 protected:
  // Creates PromosManager with empty pref data.
  void CreatePromosManager();

  // Create pref registry for tests.
  void CreatePrefs();

  // Set promo specific impression limits.
  void SetPromoLimits();

  base::SimpleTestClock test_clock_;

  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<PromosManagerImpl> promos_manager_;
  std::unique_ptr<feature_engagement::test::MockTracker> mock_tracker_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

PromosManagerImplTest::PromosManagerImplTest() {
  test_clock_.SetNow(base::Time::Now());
}

PromosManagerImplTest::~PromosManagerImplTest() {}

NSArray<ImpressionLimit*>* PromosManagerImplTest::TestImpressionLimits() {
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

Promo* PromosManagerImplTest::TestPromo() {
  return [[Promo alloc] initWithIdentifier:promos_manager::Promo::Test];
}

Promo* PromosManagerImplTest::TestPromoWithImpressionLimits() {
  return [[Promo alloc] initWithIdentifier:promos_manager::Promo::Test
                       andImpressionLimits:TestImpressionLimits()];
}

void PromosManagerImplTest::CreatePromosManager() {
  CreatePrefs();
  mock_tracker_ = std::make_unique<feature_engagement::test::MockTracker>();
  promos_manager_ = std::make_unique<PromosManagerImpl>(
      local_state_.get(), &test_clock_, mock_tracker_.get(), nullptr);
  promos_manager_->Init();
}

// Create pref registry for tests.
void PromosManagerImplTest::CreatePrefs() {
  local_state_ = std::make_unique<TestingPrefServiceSimple>();

  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerImpressions);
  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerActivePromos);
  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerSingleDisplayActivePromos);
  local_state_->registry()->RegisterDictionaryPref(
      prefs::kIosPromosManagerSingleDisplayPendingPromos);
}

// Set promo specific limits.
void PromosManagerImplTest::SetPromoLimits() {
  ImpressionLimit* oncePerYear = [[ImpressionLimit alloc] initWithLimit:1
                                                             forNumDays:365];
  ImpressionLimit* twicePerYear = [[ImpressionLimit alloc] initWithLimit:2
                                                              forNumDays:365];
  NSArray<ImpressionLimit*>* defaultBrowserLimits = @[
    twicePerYear,
  ];

  NSArray<ImpressionLimit*>* credentialProviderLimits = @[
    oncePerYear,
  ];

  NSArray<ImpressionLimit*>* appStoreRatingLimits = @[
    oncePerYear,
  ];

  NSArray<ImpressionLimit*>* testLimits = @[
    oncePerYear,
  ];

  PromoConfigsSet promoImpressionLimits;
  promoImpressionLimits.emplace(promos_manager::Promo::DefaultBrowser, nullptr,
                                defaultBrowserLimits);
  promoImpressionLimits.emplace(
      promos_manager::Promo::CredentialProviderExtension, nullptr,
      credentialProviderLimits);
  promoImpressionLimits.emplace(promos_manager::Promo::AppStoreRating, nullptr,
                                appStoreRatingLimits);
  promoImpressionLimits.emplace(promos_manager::Promo::Test, nullptr,
                                testLimits);
  promos_manager_->InitializePromoConfigs(std::move(promoImpressionLimits));
}

// Tests the initializer correctly creates a PromosManagerImpl* with the
// specified Pref service.
TEST_F(PromosManagerImplTest, InitWithPrefService) {
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
TEST_F(PromosManagerImplTest, ReturnsNameForTestPromo) {
  EXPECT_EQ(promos_manager::NameForPromo(promos_manager::Promo::Test),
            "promos_manager::Promo::Test");
}

// Tests promos_manager::PromoForName correctly returns the
// promos_manager::Promo given its string name.
TEST_F(PromosManagerImplTest, ReturnsTestPromoForName) {
  EXPECT_EQ(promos_manager::PromoForName("promos_manager::Promo::Test"),
            promos_manager::Promo::Test);
}

// Tests promos_manager::PromoForName correctly returns absl::nullopt for bad
// input.
TEST_F(PromosManagerImplTest, ReturnsNulloptForBadName) {
  EXPECT_FALSE(promos_manager::PromoForName("promos_manager::Promo::FOOBAR")
                   .has_value());
}

// Tests PromosManagerImplTest::TestPromo() correctly creates one mock promo.
TEST_F(PromosManagerImplTest, CreatesPromo) {
  Promo* promo = TestPromo();

  EXPECT_NE(promo, nil);
  EXPECT_EQ((int)promo.impressionLimits.count, 0);
}

// Tests PromosManagerImplTest::TestPromoWithImpressionLimits() correctly
// creates one mock promo with mock impression limits.
TEST_F(PromosManagerImplTest, CreatesPromoWithImpressionLimits) {
  Promo* promoWithImpressionLimits = TestPromoWithImpressionLimits();

  EXPECT_NE(promoWithImpressionLimits, nil);
  EXPECT_EQ((int)promoWithImpressionLimits.impressionLimits.count, 2);
}

// Tests PromosManagerImplTest::TestImpressionLimits() correctly creates two
// mock impression limits.
TEST_F(PromosManagerImplTest, CreatesImpressionLimits) {
  NSArray<ImpressionLimit*>* impressionLimits = TestImpressionLimits();

  EXPECT_NE(impressionLimits, nil);
  EXPECT_EQ(impressionLimits[0].numImpressions, 1);
  EXPECT_EQ(impressionLimits[0].numDays, 7);
  EXPECT_EQ(impressionLimits[1].numImpressions, 2);
  EXPECT_EQ(impressionLimits[1].numDays, 31);
}

// Tests PromosManager::ImpressionCounts() correctly returns a counts list from
// an impression counts map.
TEST_F(PromosManagerImplTest, ReturnsImpressionCounts) {
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
TEST_F(PromosManagerImplTest, ReturnsEmptyImpressionCounts) {
  std::map<promos_manager::Promo, int> promo_impression_counts;
  std::vector<int> counts;

  EXPECT_EQ(promos_manager_->ImpressionCounts(promo_impression_counts), counts);
}

// Tests PromosManager::TotalImpressionCount() correctly adds the counts of
// different promos from an impression counts map.
TEST_F(PromosManagerImplTest, ReturnsTotalImpressionCount) {
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
TEST_F(PromosManagerImplTest, ReturnsZeroForTotalImpressionCount) {
  std::map<promos_manager::Promo, int> promo_impression_counts;

  EXPECT_EQ(promos_manager_->TotalImpressionCount(promo_impression_counts), 0);
}

// Tests PromosManager::AnyImpressionLimitTriggered() correctly detects an
// impression limit is triggered.
TEST_F(PromosManagerImplTest, DetectsSingleImpressionLimitTriggered) {
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
TEST_F(PromosManagerImplTest, DetectsOneOfMultipleImpressionLimitsTriggered) {
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
TEST_F(PromosManagerImplTest, DetectsNoImpressionLimitTriggered) {
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
TEST_F(PromosManagerImplTest, DecidesCanShowPromo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPromosManagerUsesFET);

  CreatePromosManager();

  const std::vector<promos_manager::Impression> zeroImpressions = {};
  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::Test,
                                          zeroImpressions),
            true);
}

// Tests PromosManager::CanShowPromo() correctly allows/denies promos based on
// promo specific limits.
TEST_F(PromosManagerImplTest, CanShowPromo_TestPromoSpecifLimits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPromosManagerUsesFET);

  CreatePromosManager();
  SetPromoLimits();

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today - 40,
                                 false),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 50, false),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 60, false),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 70,
          false),
  };

  // False because triggers no more than 1 impression per 365 days.
  EXPECT_EQ(
      promos_manager_->CanShowPromo(promos_manager::Promo::Test, impressions),
      false);
  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::AppStoreRating,
                                          impressions),
            false);
  EXPECT_EQ(
      promos_manager_->CanShowPromo(
          promos_manager::Promo::CredentialProviderExtension, impressions),
      false);

  // True because `DefaultBrowser` can be shown more than 1 time per 365 days.
  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::DefaultBrowser,
                                          impressions),
            true);
}

// Tests PromosManager::CanShowPromo() correctly allows/denies promos based on
// global per promo impression limits.
TEST_F(PromosManagerImplTest, CanShowPromo_TestGlobalPerPromoImpressionLimits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPromosManagerUsesFET);

  CreatePromosManager();
  SetPromoLimits();

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 29, false),
  };

  // True because this promo is requested first time and global impression
  // limits(3 promo a week, 1 promo every 2 days) are not triggered.
  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::AppStoreRating,
                                          impressions),
            true);
  // False because global per promo limits are triggered (given promo is
  // displayed 1 in 30 days)
  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::DefaultBrowser,
                                          impressions),
            false);
}

// Tests PromosManager::CanShowPromo() correctly allows/denies promos based on
// global impression limits.
TEST_F(PromosManagerImplTest, CanShowPromo_TestGlobalImpressionLimits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPromosManagerUsesFET);

  CreatePromosManager();
  SetPromoLimits();

  int today = TodaysDay();
  {
    const std::vector<promos_manager::Impression> impressions = {
        promos_manager::Impression(promos_manager::Promo::Test, today - 1,
                                   false),
    };

    // False because only 1 promo per 2 days is allowed.
    EXPECT_EQ(promos_manager_->CanShowPromo(
                  promos_manager::Promo::AppStoreRating, impressions),
              false);
  }

  {
    const std::vector<promos_manager::Impression> impressions = {
        promos_manager::Impression(promos_manager::Promo::Test, today - 2,
                                   false),
    };

    // True because 1 promo per 2 days limit is not triggered.
    EXPECT_EQ(promos_manager_->CanShowPromo(
                  promos_manager::Promo::AppStoreRating, impressions),
              true);
  }

  {
    const std::vector<promos_manager::Impression> impressions = {
        promos_manager::Impression(promos_manager::Promo::Test, today - 2,
                                   false),
        promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                   today - 4, false),
        promos_manager::Impression(
            promos_manager::Promo::CredentialProviderExtension, today - 6,
            false),
    };

    // False because cannot show more than 3 promos in 7 days
    EXPECT_EQ(promos_manager_->CanShowPromo(
                  promos_manager::Promo::AppStoreRating, impressions),
              false);
  }

  {
    const std::vector<promos_manager::Impression> impressions = {
        promos_manager::Impression(promos_manager::Promo::Test, today - 2,
                                   false),
        promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                   today - 4, false),
    };

    // True because 1 promo per 2 days and 3 promo a week limits are not
    // triggered.
    EXPECT_EQ(promos_manager_->CanShowPromo(
                  promos_manager::Promo::AppStoreRating, impressions),
              true);
  }
}

// Tests PromosManager::SortPromos() correctly returns a list of active
// promos sorted by impression history.
TEST_F(PromosManagerImplTest, SortPromos) {
  CreatePromosManager();

  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, kPromoContextForActive},
      {promos_manager::Promo::CredentialProviderExtension,
       kPromoContextForActive},
      {promos_manager::Promo::AppStoreRating, kPromoContextForActive},
      {promos_manager::Promo::DefaultBrowser, kPromoContextForActive},
  };

  int today = TodaysDay();

  promos_manager_->impression_history_ = {
      promos_manager::Impression(promos_manager::Promo::Test, today, false),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7, false),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 14, false),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 180,
          false),
  };

  std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::CredentialProviderExtension,
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::DefaultBrowser,
      promos_manager::Promo::Test,
  };

  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests PromosManager::SortPromos() correctly returns a list of
// promos sorted by least recently shown (with some impressions /
// belonging to inactive promo campaigns).
TEST_F(PromosManagerImplTest, SortPromosWithSomeInactivePromos) {
  CreatePromosManager();

  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, kPromoContextForActive},
      {promos_manager::Promo::AppStoreRating, kPromoContextForActive},
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today, false),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7, false),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 14, false),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 180,
          false),
  };

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::Test,
  };

  promos_manager_->impression_history_ = impressions;
  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests PromosManager::SortPromos() correctly returns a list of
//  promos when multiple promos are tied for least recently shown.
TEST_F(PromosManagerImplTest, ReturnsSortPromosBreakingTies) {
  CreatePromosManager();
  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, kPromoContextForActive},
      {promos_manager::Promo::CredentialProviderExtension,
       kPromoContextForActive},
      {promos_manager::Promo::AppStoreRating, kPromoContextForActive},
      {promos_manager::Promo::DefaultBrowser, kPromoContextForActive},
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today, false),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser, today,
                                 false),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating, today,
                                 false),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today, false),
  };

  promos_manager_->impression_history_ = impressions;
  EXPECT_EQ(promos_manager_->SortPromos(active_promos).size(), (size_t)4);
  EXPECT_EQ(promos_manager_->SortPromos(active_promos)[0],
            promos_manager::Promo::CredentialProviderExtension);
}

// Tests `SortPromos` returns a single promo in a list when the impression
// history contains only one active promo.
TEST_F(PromosManagerImplTest, ReturnsSortPromosWithOnlyOnePromoActive) {
  CreatePromosManager();

  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, kPromoContextForActive},
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today, false),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7, false),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 14, false),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 180,
          false),
  };

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::Test,
  };

  promos_manager_->impression_history_ = impressions;
  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests `SortPromos` returns an empty array when there are no active promo.
TEST_F(PromosManagerImplTest,
       ReturnsEmptyListWhenSortPromosHasNoActivePromoCampaigns) {
  CreatePromosManager();
  const std::map<promos_manager::Promo, PromoContext> active_promos;

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today, false),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7, false),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 14, false),
      promos_manager::Impression(
          promos_manager::Promo::CredentialProviderExtension, today - 180,
          false),
  };

  const std::vector<promos_manager::Promo> expected;

  promos_manager_->impression_history_ = impressions;
  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests `SortPromos` returns an empty array when no impression history and no
// active promos exist.
TEST_F(PromosManagerImplTest,
       ReturnsEmptyListWhenSortPromosHasNoImpressionHistory) {
  CreatePromosManager();
  const std::map<promos_manager::Promo, PromoContext> active_promos;

  const std::vector<promos_manager::Impression> impressions;
  const std::vector<promos_manager::Promo> expected;

  promos_manager_->impression_history_ = impressions;
  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests `SortPromos` sorts unshown promos before shown promos.
TEST_F(PromosManagerImplTest,
       SortsUnshownPromosBeforeShownPromosForSortPromos) {
  CreatePromosManager();

  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, kPromoContextForActive},
      {promos_manager::Promo::AppStoreRating, kPromoContextForActive},
      {promos_manager::Promo::DefaultBrowser, kPromoContextForActive},
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today, false),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7, false),
  };

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::DefaultBrowser,
      promos_manager::Promo::Test,
  };

  promos_manager_->impression_history_ = impressions;
  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests `SortPromos` sorts `Choice` promos before others and
// `PostRestoreSignIn` next.
TEST_F(PromosManagerImplTest, SortsPromosPreferCertainTypes) {
  CreatePromosManager();

  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, PromoContext{false}},
      {promos_manager::Promo::DefaultBrowser, PromoContext{true}},
      {promos_manager::Promo::PostRestoreSignInFullscreen, PromoContext{false}},
      {promos_manager::Promo::PostRestoreSignInAlert, PromoContext{false}},
      {promos_manager::Promo::Choice, PromoContext{false}},
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(
          promos_manager::Promo::PostRestoreSignInFullscreen, today - 1, false),
      promos_manager::Impression(promos_manager::Promo::PostRestoreSignInAlert,
                                 today - 2, false),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7, false),

      promos_manager::Impression(promos_manager::Promo::Test, today - 8, false),
  };

  promos_manager_->impression_history_ = impressions;
  std::vector<promos_manager::Promo> sorted =
      promos_manager_->SortPromos(active_promos);
  EXPECT_EQ(sorted.size(), (size_t)5);
  // Choice comes first
  EXPECT_TRUE(sorted[0] == promos_manager::Promo::Choice);
  // tied for the type.
  EXPECT_TRUE(sorted[1] == promos_manager::Promo::PostRestoreSignInFullscreen ||
              sorted[1] == promos_manager::Promo::PostRestoreSignInAlert);
  // with pending state, before the less recently shown promo (Test).
  EXPECT_EQ(sorted[3], promos_manager::Promo::DefaultBrowser);
  EXPECT_EQ(sorted[4], promos_manager::Promo::Test);
}

// Tests `SortPromos` sorts promos with pending state before others without.
TEST_F(PromosManagerImplTest, SortsPromosPreferPendingToNonPending) {
  CreatePromosManager();

  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, PromoContext{true}},
      {promos_manager::Promo::DefaultBrowser, PromoContext{false}},
      {promos_manager::Promo::AppStoreRating, PromoContext{true}},
  };

  int today = TodaysDay();

  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today - 1, false),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 2, false),
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser,
                                 today - 7, false),
  };

  promos_manager_->impression_history_ = impressions;
  std::vector<promos_manager::Promo> sorted =
      promos_manager_->SortPromos(active_promos);
  EXPECT_EQ(sorted.size(), (size_t)3);
  // The first 2 are tied with the pending state.
  EXPECT_TRUE(sorted[0] == promos_manager::Promo::Test ||
              sorted[0] == promos_manager::Promo::AppStoreRating);
  // The one without pending state is at the end.
  EXPECT_EQ(sorted[2], promos_manager::Promo::DefaultBrowser);
}

// Tests PromosManager::ImpressionHistory() correctly ingests impression history
// (base::Value::List) and returns corresponding
// std::vector<promos_manager::Impression>.
TEST_F(PromosManagerImplTest, ReturnsImpressionHistory) {
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
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser, today,
                                 false),
      promos_manager::Impression(promos_manager::Promo::AppStoreRating,
                                 today - 7, false),
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
TEST_F(PromosManagerImplTest, ReturnsBlankImpressionHistoryForBlankPrefs) {
  base::Value::List impressions;

  std::vector<promos_manager::Impression> result =
      promos_manager_->ImpressionHistory(impressions);

  EXPECT_TRUE(result.empty());
}

// Tests PromosManager::ImpressionHistory() correctly ingests impression history
// with malformed data (base::Value::List) and returns corresponding
// std::vector<promos_manager::Impression> without malformed entries.
TEST_F(PromosManagerImplTest,
       ReturnsImpressionHistoryBySkippingMalformedEntries) {
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
      promos_manager::Impression(promos_manager::Promo::DefaultBrowser, today,
                                 false),
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
TEST_F(PromosManagerImplTest, ReturnsActivePromos) {
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
TEST_F(PromosManagerImplTest, ReturnsBlankActivePromosForBlankPrefs) {
  base::Value::List promos;

  std::set<promos_manager::Promo> result =
      promos_manager_->ActivePromos(promos);

  EXPECT_TRUE(result.empty());
}

// Tests PromosManager::ActivePromos() correctly ingests active promos with
// malformed data (base::Value::List) and returns corresponding
// std::vector<promos_manager::Promo> with malformed entries pruned.
TEST_F(PromosManagerImplTest, ReturnsActivePromosAndSkipsMalformedData) {
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

// Tests `InitializePendingPromos` initializes with pending promos.
TEST_F(PromosManagerImplTest, InitializePendingPromos) {
  CreatePromosManager();

  // write to Pref
  base::Value::Dict promos;
  promos.Set("promos_manager::Promo::DefaultBrowser",
             base::TimeToValue(test_clock_.Now()));
  promos.Set("promos_manager::Promo::AppStoreRating",
             base::TimeToValue(test_clock_.Now()));
  local_state_->SetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos,
                        std::move(promos));

  std::map<promos_manager::Promo, base::Time> expected;
  expected[promos_manager::Promo::DefaultBrowser] = test_clock_.Now();
  expected[promos_manager::Promo::AppStoreRating] = test_clock_.Now();

  promos_manager_->InitializePendingPromos();

  EXPECT_EQ(expected, promos_manager_->single_display_pending_promos_);
}

// Tests `InitializePendingPromos` initializes empty Pref.
TEST_F(PromosManagerImplTest, InitializePendingPromosEmpty) {
  CreatePromosManager();
  promos_manager_->InitializePendingPromos();
  std::map<promos_manager::Promo, base::Time> expected;
  EXPECT_EQ(expected, promos_manager_->single_display_pending_promos_);
}

// Tests `InitializePendingPromos` initializes with malformed data with
// malformed entries pruned.
TEST_F(PromosManagerImplTest, InitializePendingPromosMalformedData) {
  CreatePromosManager();

  // write to Pref
  base::Value::Dict promos;
  promos.Set("promos_manager::Promo::DefaultBrowser",
             base::TimeToValue(test_clock_.Now()));
  promos.Set("promos_manager::Promo::Foo",
             base::TimeToValue(test_clock_.Now()));
  promos.Set("promos_manager::Promo::AppStoreRating", base::Value(1));
  local_state_->SetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos,
                        std::move(promos));

  std::map<promos_manager::Promo, base::Time> expected;
  expected[promos_manager::Promo::DefaultBrowser] = test_clock_.Now();

  promos_manager_->InitializePendingPromos();

  EXPECT_EQ(expected, promos_manager_->single_display_pending_promos_);
}

// Tests PromosManager::RegisterPromoForContinuousDisplay() correctly registers
// a promo for continuous display by writing the promo's name to the pref
// `kIosPromosManagerActivePromos`.
TEST_F(PromosManagerImplTest, RegistersPromoForContinuousDisplay) {
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
// a promo for continuous display by writing the promo's name to the pref
// `kIosPromosManagerActivePromos` and immediately updates active_promos_ to
// reflect the new state.
TEST_F(PromosManagerImplTest,
       RegistersPromoForContinuousDisplayAndImmediatelyUpdateVariables) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerActivePromos).empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::AppStoreRating);
  EXPECT_EQ(promos_manager_->active_promos_.size(), (size_t)2);

  // Register new promo.
  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::DefaultBrowser);

  EXPECT_EQ(promos_manager_->active_promos_.size(), (size_t)3);
}

// Tests PromosManager::RegisterPromoForContinuousDisplay() correctly registers
// a promo—for the very first time—for continuous display by writing the
// promo's name to the pref `kIosPromosManagerActivePromos`.
TEST_F(PromosManagerImplTest,
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
TEST_F(PromosManagerImplTest,
       RegistersAlreadyRegisteredPromoForContinuousDisplay) {
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
    PromosManagerImplTest,
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
TEST_F(PromosManagerImplTest, RegistersPromoForSingleDisplay) {
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

// Tests `RegisterPromoForSingleDisplay` with `becomes_active_after_period`
// registers a promo for single display by writing the promo's name and the
// calculated time to the pref `kIosPromosManagerSingleDisplayPendingPromos`.
TEST_F(PromosManagerImplTest,
       RegistersPromoForSingleDisplayWithBecomesActivePeriod) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .empty());

  // Initial state: 1 active promo, 0 pending promo.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::AppStoreRating);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)1);
  EXPECT_EQ(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .size(),
      (size_t)0);

  // Register new promo with becomes_active_period.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension, kTimeDelta1Day);

  // End state: 1 active promo, 1 pending promo.
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)1);
  EXPECT_EQ(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .size(),
      (size_t)1);

  // Active promo in Pref doesn't change.
  EXPECT_EQ(
      local_state_->GetList(
          prefs::kIosPromosManagerSingleDisplayActivePromos)[0],
      promos_manager::NameForPromo(promos_manager::Promo::AppStoreRating));

  // Pending promo in Pref is updated with the correct time.
  absl::optional<base::Time> actual_becomes_active_time = ValueToTime(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .Find(promos_manager::NameForPromo(
              promos_manager::Promo::CredentialProviderExtension)));
  EXPECT_TRUE(actual_becomes_active_time.has_value());
  EXPECT_EQ(actual_becomes_active_time.value(),
            test_clock_.Now() + kTimeDelta1Day);
}

// Tests PromosManager::RegisterPromoForSingleDisplay() correctly registers
// a promo for single display by writing the promo's name to the pref
// `kIosPromosManagerSingleDisplayActivePromos` and immediately updates
// single_display_active_promos_ to reflect the new state.
TEST_F(PromosManagerImplTest,
       RegistersPromoForSingleDisplayAndImmediatelyUpdateVariables) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::AppStoreRating);
  EXPECT_EQ(promos_manager_->single_display_active_promos_.size(), (size_t)2);

  // Register new promo.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DefaultBrowser);

  EXPECT_EQ(promos_manager_->single_display_active_promos_.size(), (size_t)3);
}

// Tests `RegisterPromoForSingleDisplay` with `becomes_active_after_period`
// registers a promo for single display by writing the promo's name and the
// calculated time to the pref `kIosPromosManagerSingleDisplayActivePromos`
// and updates single_display_pending_promos_.
TEST_F(
    PromosManagerImplTest,
    RegistersPromoForSingleDisplayWithBecomesActivePeriodAndUpdateVariables) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .empty());

  // Register
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension, kTimeDelta1Day);

  // End state: 0 active promo, 1 pending promo
  EXPECT_EQ(promos_manager_->single_display_active_promos_.size(), (size_t)0);
  EXPECT_EQ(promos_manager_->single_display_pending_promos_.size(), (size_t)1);
  base::Time actual_becomes_active_time =
      promos_manager_->single_display_pending_promos_
          [promos_manager::Promo::CredentialProviderExtension];
  EXPECT_EQ(actual_becomes_active_time, (test_clock_.Now() + kTimeDelta1Day));
}

// Tests PromosManager::RegisterPromoForSingleDisplay() correctly registers
// a promo for single display—for the very first time—by writing the promo's
// name to the pref `kIosPromosManagerSingleDisplayActivePromos`.
TEST_F(PromosManagerImplTest,
       RegistersPromoForSingleDisplayForEmptyActivePromos) {
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
TEST_F(PromosManagerImplTest, RegistersAlreadyRegisteredPromoForSingleDisplay) {
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
TEST_F(PromosManagerImplTest,
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

// Tests `RegisterPromoForSingleDisplay` with `becomes_active_after_period`
// correctly registers an already-registered promo for single display by
// overriding the existing entry in the pref
// `kIosPromosManagerSingleDisplayPendingPromos`.
TEST_F(PromosManagerImplTest,
       RegistersAlreadyRegisteredPromoForSingleDisplayWithBecomesActiveTime) {
  CreatePromosManager();
  EXPECT_TRUE(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .empty());

  // Initial pending promos state.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension, kTimeDelta1Day);

  // Register the same promo.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension, kTimeDelta1Day * 2);

  // End state: only the second registered promo is in the pref.
  EXPECT_EQ(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .size(),
      (size_t)1);
  absl::optional<base::Time> actual_becomes_active_time = ValueToTime(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .Find(promos_manager::NameForPromo(
              promos_manager::Promo::CredentialProviderExtension)));
  EXPECT_TRUE(actual_becomes_active_time.has_value());
  EXPECT_EQ(actual_becomes_active_time.value(),
            test_clock_.Now() + kTimeDelta1Day * 2);
}

// Tests PromosManager::InitializePromoImpressionLimits() correctly registers
// promo-specific impression limits.
TEST_F(PromosManagerImplTest, RegistersPromoSpecificImpressionLimits) {
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

  PromoConfigsSet promoImpressionLimits;
  promoImpressionLimits.emplace(promos_manager::Promo::DefaultBrowser, nullptr,
                                defaultBrowserLimits);
  promoImpressionLimits.emplace(
      promos_manager::Promo::CredentialProviderExtension, nullptr,
      credentialProviderLimits);
  promos_manager_->InitializePromoConfigs(std::move(promoImpressionLimits));

  EXPECT_EQ(promos_manager_->PromoImpressionLimits(
                promos_manager::Promo::DefaultBrowser),
            defaultBrowserLimits);
  EXPECT_EQ(promos_manager_->PromoImpressionLimits(
                promos_manager::Promo::CredentialProviderExtension),
            credentialProviderLimits);
}

// Tests PromosManager::RecordImpression() correctly records a new impression.
TEST_F(PromosManagerImplTest, RecordsImpression) {
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

// Tests PromosManager::RecordImpression() correctly records a new impression
// and immediately updates impression_history_ to reflect the new state
TEST_F(PromosManagerImplTest, RecordsImpressionAndImmediatelyUpdateVariables) {
  CreatePromosManager();

  promos_manager_->RecordImpression(promos_manager::Promo::DefaultBrowser);
  EXPECT_EQ(promos_manager_->impression_history_.size(), (size_t)1);

  promos_manager_->RecordImpression(
      promos_manager::Promo::CredentialProviderExtension);
  EXPECT_EQ(promos_manager_->impression_history_.size(), (size_t)2);
}

// Tests PromosManager::DeregisterPromo() deregisters a promo, all the entries
// with the same promo type in the Pref/in-memory variables will be removed,
// regardless of the context of being single/continuous or active/pending.
TEST_F(PromosManagerImplTest, DeregistersActivePromo) {
  CreatePromosManager();

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)0);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)0);
  EXPECT_EQ(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .size(),
      (size_t)0);

  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);
  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)1);

  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)1);

  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension, kTimeDelta1Day);
  EXPECT_EQ(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .size(),
      (size_t)1);

  promos_manager_->DeregisterPromo(
      promos_manager::Promo::CredentialProviderExtension);

  // all entries with the same type are removed.
  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)0);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)0);
  EXPECT_EQ(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .size(),
      (size_t)0);
}

// Tests PromosManager::DeregisterPromo() correctly deregisters a currently
// active promo campaign and immediately updates active_promos_ &
// single_display_active_promos_ to reflect the new state.
TEST_F(PromosManagerImplTest,
       DeregistersActivePromoAndImmediatelyUpdateVariables) {
  CreatePromosManager();

  EXPECT_EQ(local_state_->GetList(prefs::kIosPromosManagerActivePromos).size(),
            (size_t)0);
  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)0);
  EXPECT_EQ(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .size(),
      (size_t)0);

  promos_manager_->RegisterPromoForContinuousDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(promos_manager_->active_promos_.size(), (size_t)1);
  EXPECT_EQ(promos_manager_->single_display_active_promos_.size(), (size_t)0);
  EXPECT_EQ(promos_manager_->single_display_pending_promos_.size(), (size_t)0);

  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(promos_manager_->active_promos_.size(), (size_t)1);
  EXPECT_EQ(promos_manager_->single_display_active_promos_.size(), (size_t)1);
  EXPECT_EQ(promos_manager_->single_display_pending_promos_.size(), (size_t)0);

  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension, kTimeDelta1Day);
  EXPECT_EQ(promos_manager_->active_promos_.size(), (size_t)1);
  EXPECT_EQ(promos_manager_->single_display_pending_promos_.size(), (size_t)1);
  EXPECT_EQ(promos_manager_->single_display_active_promos_.size(), (size_t)1);

  promos_manager_->DeregisterPromo(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(promos_manager_->active_promos_.size(), (size_t)0);
  EXPECT_EQ(promos_manager_->single_display_active_promos_.size(), (size_t)0);
  EXPECT_EQ(promos_manager_->single_display_pending_promos_.size(), (size_t)0);
}

// Tests PromosManager::DeregisterPromo() handles the situation where the promo
// doesn't exist in a given active promos list by removing from all lists.
TEST_F(PromosManagerImplTest, DeregistersNonExistentPromo) {
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

// Tests a given single-display promo is automatically deregistered after its
// impression is recorded.
TEST_F(PromosManagerImplTest,
       DeregistersSingleDisplayPromoAfterRecordedImpression) {
  CreatePromosManager();

  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_EQ(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .size(),
      (size_t)1);

  promos_manager_->RecordImpression(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .empty());
}

// Tests a given single-display pending promo is automatically deregistered
// after its impression is recorded.
TEST_F(PromosManagerImplTest,
       DeregistersSingleDisplayPendingPromoAfterRecordedImpression) {
  CreatePromosManager();

  EXPECT_TRUE(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .empty());

  // Initial active promos state.
  promos_manager_->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension, kTimeDelta1Day);

  EXPECT_EQ(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .size(),
      (size_t)1);

  promos_manager_->RecordImpression(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_TRUE(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .empty());
}

// Tests `NextPromoForDisplay` returns a pending promo that has become active
// and takes precedence over other active promos.
TEST_F(PromosManagerImplTest, NextPromoForDisplayReturnsPendingPromo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPromosManagerUsesFET);

  CreatePromosManager();

  promos_manager_->single_display_active_promos_ = {
      promos_manager::Promo::Test,
  };
  promos_manager_->single_display_pending_promos_ = {
      {promos_manager::Promo::CredentialProviderExtension,
       test_clock_.Now() + kTimeDelta1Day},
      {promos_manager::Promo::AppStoreRating,
       test_clock_.Now() + kTimeDelta1Day * 2},
  };

  // Advance to so that the CredentialProviderExtension becomes active.
  test_clock_.Advance(kTimeDelta1Day + kTimeDelta1Hour);

  absl::optional<promos_manager::Promo> promo =
      promos_manager_->NextPromoForDisplay();
  EXPECT_TRUE(promo.has_value());
  EXPECT_EQ(promo.value(), promos_manager::Promo::CredentialProviderExtension);
}

// Tests `NextPromoForDisplay` returns an active promo whose type has the
// highest priority can take precedence over other pending-becomes-active
// promos.
TEST_F(PromosManagerImplTest,
       NextPromoForDisplayReturnsActivePromoOfPrioritizedType) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPromosManagerUsesFET);

  CreatePromosManager();

  promos_manager_->single_display_active_promos_ = {
      promos_manager::Promo::PostRestoreSignInFullscreen,
  };
  promos_manager_->single_display_pending_promos_ = {
      {promos_manager::Promo::CredentialProviderExtension,
       test_clock_.Now() + kTimeDelta1Day},
  };

  // Advance to so that the CredentialProviderExtension becomes active.
  test_clock_.Advance(kTimeDelta1Day + kTimeDelta1Hour);

  absl::optional<promos_manager::Promo> promo =
      promos_manager_->NextPromoForDisplay();

  EXPECT_TRUE(promo.has_value());
  EXPECT_EQ(promo.value(), promos_manager::Promo::PostRestoreSignInFullscreen);
}

// Tests `NextPromoForDisplay` returns empty when non of the pending promos can
// become active.
TEST_F(PromosManagerImplTest, NextPromoForDisplayReturnsEmpty) {
  CreatePromosManager();

  promos_manager_->single_display_active_promos_ = {};
  promos_manager_->single_display_pending_promos_ = {
      {promos_manager::Promo::Test, test_clock_.Now() + kTimeDelta1Hour * 2},
      {promos_manager::Promo::CredentialProviderExtension,
       test_clock_.Now() + kTimeDelta1Day},
  };

  // Advance to so that the none of the pending promo can become active.
  test_clock_.Advance(kTimeDelta1Hour);

  absl::optional<promos_manager::Promo> promo =
      promos_manager_->NextPromoForDisplay();

  EXPECT_FALSE(promo.has_value());
}

// Tests `NextPromoForDisplay` returns empty when non of the promos can pass the
// onceEveryTwoDays impression limit check.
TEST_F(PromosManagerImplTest,
       NextPromoForDisplayReturnsEmptyAfterImpressionCheck) {
  CreatePromosManager();

  promos_manager_->single_display_active_promos_ = {
      promos_manager::Promo::AppStoreRating};
  promos_manager_->single_display_pending_promos_ = {
      {promos_manager::Promo::Test, test_clock_.Now() + kTimeDelta1Hour},
      {promos_manager::Promo::CredentialProviderExtension,
       test_clock_.Now() + kTimeDelta1Hour},
  };

  int today = TodaysDay();
  const std::vector<promos_manager::Impression> impressions = {
      promos_manager::Impression(promos_manager::Promo::Test, today, false),
  };
  promos_manager_->impression_history_ = impressions;

  // Advance the time so that all pending promos can become active, but will
  // fall into the two-day window since the last impression.
  test_clock_.Advance(kTimeDelta1Day);

  absl::optional<promos_manager::Promo> promo =
      promos_manager_->NextPromoForDisplay();

  EXPECT_FALSE(promo.has_value());
}
