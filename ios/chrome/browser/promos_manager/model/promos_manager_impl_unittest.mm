// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/model/promos_manager_impl.h"

#import <Foundation/Foundation.h>

#import <optional>
#import <set>
#import <vector>

#import "base/json/values_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/simple_test_clock.h"
#import "base/values.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/impression_limit.h"
#import "ios/chrome/browser/promos_manager/model/promo.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using PromoContext = PromosManagerImpl::PromoContext;

namespace {

const base::TimeDelta kTimeDelta1Day = base::Days(1);
const base::TimeDelta kTimeDelta1Hour = base::Hours(1);

const PromoContext kPromoContextForActive = PromoContext{
    .was_pending = false,
};

BASE_FEATURE(kTestFeatureOne, "test_one", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureTwo, "test_two", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureThree,
             "test_three",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

  base::SimpleTestClock test_clock_;

  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<PromosManagerImpl> promos_manager_;
  feature_engagement::test::MockTracker mock_tracker_;
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
  promos_manager_ = std::make_unique<PromosManagerImpl>(
      local_state_.get(), &test_clock_, &mock_tracker_);
  promos_manager_->Init();
}

// Create pref registry for tests.
void PromosManagerImplTest::CreatePrefs() {
  local_state_ = std::make_unique<TestingPrefServiceSimple>();

  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerActivePromos);
  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerSingleDisplayActivePromos);
  local_state_->registry()->RegisterDictionaryPref(
      prefs::kIosPromosManagerSingleDisplayPendingPromos);
}

// Tests the initializer correctly creates a PromosManagerImpl* with the
// specified Pref service.
TEST_F(PromosManagerImplTest, InitWithPrefService) {
  CreatePromosManager();

  EXPECT_NE(local_state_->FindPreference(prefs::kIosPromosManagerActivePromos),
            nullptr);
  EXPECT_NE(local_state_->FindPreference(
                prefs::kIosPromosManagerSingleDisplayActivePromos),
            nullptr);
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

// Tests promos_manager::PromoForName correctly returns std::nullopt for bad
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

// Tests PromosManager::CanShowPromo() correctly allows a promo to be shown
// because it hasn't met any impression limits.
TEST_F(PromosManagerImplTest, DecidesCanShowPromo) {
  base::test::ScopedFeatureList feature_list;

  CreatePromosManager();

  PromoConfigsSet promoConfigs;
  promoConfigs.emplace(promos_manager::Promo::Test, &kTestFeatureOne);
  promos_manager_->InitializePromoConfigs(std::move(promoConfigs));

  EXPECT_CALL(mock_tracker_, ShouldTriggerHelpUI(testing::Ref(kTestFeatureOne)))
      .WillRepeatedly(testing::Return(true));

  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::Test), true);
}

// Tests PromosManager::CanShowPromo() correctly allows/denies promos based on
// promo specific limits.
TEST_F(PromosManagerImplTest, CanShowPromo_TestPromoSpecificLimits) {
  base::test::ScopedFeatureList feature_list;

  CreatePromosManager();

  PromoConfigsSet promoConfigs;
  promoConfigs.emplace(promos_manager::Promo::Test, &kTestFeatureOne);
  promoConfigs.emplace(promos_manager::Promo::AppStoreRating, &kTestFeatureTwo);
  promoConfigs.emplace(promos_manager::Promo::CredentialProviderExtension,
                       &kTestFeatureThree);
  promos_manager_->InitializePromoConfigs(std::move(promoConfigs));

  // Mock the FET tracker to disallow Test and AppStoreRating due to impression
  // counting rules, and allow CredentialProviderExtension.
  EXPECT_CALL(mock_tracker_, ShouldTriggerHelpUI(testing::Ref(kTestFeatureOne)))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_tracker_, ShouldTriggerHelpUI(testing::Ref(kTestFeatureTwo)))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(kTestFeatureThree)))
      .WillRepeatedly(testing::Return(true));

  EXPECT_EQ(promos_manager_->CanShowPromo(promos_manager::Promo::Test), false);
  EXPECT_EQ(
      promos_manager_->CanShowPromo(promos_manager::Promo::AppStoreRating),
      false);

  EXPECT_EQ(promos_manager_->CanShowPromo(
                promos_manager::Promo::CredentialProviderExtension),
            true);
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

  std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::Test,
      promos_manager::Promo::DefaultBrowser,
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::CredentialProviderExtension,
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

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::Test,
      promos_manager::Promo::AppStoreRating,
  };

  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests PromosManager::SortPromos() correctly returns a list of
//  promos when multiple promos are tied for least recently shown.
TEST_F(PromosManagerImplTest, ReturnsSortPromosBreakingTies) {
  CreatePromosManager();
  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::CredentialProviderExtension,
       kPromoContextForActive},
      {promos_manager::Promo::AppStoreRating, kPromoContextForActive},
      {promos_manager::Promo::DefaultBrowser, kPromoContextForActive},
  };

  PromoConfigsSet promoImpressionLimits;
  promoImpressionLimits.emplace(
      promos_manager::Promo::CredentialProviderExtension, &kTestFeatureOne);
  promoImpressionLimits.emplace(promos_manager::Promo::AppStoreRating,
                                &kTestFeatureTwo);
  promoImpressionLimits.emplace(promos_manager::Promo::DefaultBrowser,
                                &kTestFeatureThree);
  promos_manager_->InitializePromoConfigs(std::move(promoImpressionLimits));

  EXPECT_CALL(mock_tracker_, IsInitialized())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_tracker_, HasEverTriggered(testing::_, true))
      .WillRepeatedly(testing::Return(false));

  EXPECT_EQ(promos_manager_->SortPromos(active_promos).size(), (size_t)3);
  EXPECT_EQ(promos_manager_->SortPromos(active_promos)[0],
            promos_manager::Promo::DefaultBrowser);
}

// Tests `SortPromos` returns a single promo in a list when the impression
// history contains only one active promo.
TEST_F(PromosManagerImplTest, ReturnsSortPromosWithOnlyOnePromoActive) {
  CreatePromosManager();

  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, kPromoContextForActive},
  };

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::Test,
  };

  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests `SortPromos` returns an empty array when there are no active promo.
TEST_F(PromosManagerImplTest,
       ReturnsEmptyListWhenSortPromosHasNoActivePromoCampaigns) {
  CreatePromosManager();
  const std::map<promos_manager::Promo, PromoContext> active_promos;

  const std::vector<promos_manager::Promo> expected;

  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests `SortPromos` returns an empty array when no impression history and no
// active promos exist.
TEST_F(PromosManagerImplTest,
       ReturnsEmptyListWhenSortPromosHasNoImpressionHistory) {
  CreatePromosManager();
  const std::map<promos_manager::Promo, PromoContext> active_promos;

  const std::vector<promos_manager::Promo> expected;

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

  PromoConfigsSet promoImpressionLimits;
  promoImpressionLimits.emplace(promos_manager::Promo::Test, &kTestFeatureOne);
  promoImpressionLimits.emplace(promos_manager::Promo::AppStoreRating,
                                &kTestFeatureTwo);
  promoImpressionLimits.emplace(promos_manager::Promo::DefaultBrowser,
                                &kTestFeatureThree);
  promos_manager_->InitializePromoConfigs(std::move(promoImpressionLimits));

  EXPECT_CALL(mock_tracker_, IsInitialized())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_tracker_,
              HasEverTriggered(testing::Ref(kTestFeatureTwo), true))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_tracker_,
              HasEverTriggered(testing::Ref(kTestFeatureOne), true))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_tracker_,
              HasEverTriggered(testing::Ref(kTestFeatureThree), true))
      .WillRepeatedly(testing::Return(true));

  const std::vector<promos_manager::Promo> expected = {
      promos_manager::Promo::AppStoreRating,
      promos_manager::Promo::Test,
      promos_manager::Promo::DefaultBrowser,
  };

  EXPECT_EQ(promos_manager_->SortPromos(active_promos), expected);
}

// Tests `SortPromos` sorts `PostRestoreSignIn` promos before others and
// `Choice` next.
TEST_F(PromosManagerImplTest, SortsPromosPreferCertainTypes) {
  CreatePromosManager();

  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, PromoContext{false}},
      {promos_manager::Promo::DefaultBrowser, PromoContext{true}},
      {promos_manager::Promo::PostRestoreSignInFullscreen, PromoContext{false}},
      {promos_manager::Promo::PostRestoreSignInAlert, PromoContext{false}},
  };

  std::vector<promos_manager::Promo> sorted =
      promos_manager_->SortPromos(active_promos);
  EXPECT_EQ(sorted.size(), (size_t)4);
  // tied for the type.
  EXPECT_TRUE(sorted[0] == promos_manager::Promo::PostRestoreSignInFullscreen ||
              sorted[0] == promos_manager::Promo::PostRestoreSignInAlert);
  // with pending state, before the less recently shown promo (Test).
  EXPECT_EQ(sorted[2], promos_manager::Promo::DefaultBrowser);
  EXPECT_EQ(sorted[3], promos_manager::Promo::Test);
}

// Tests `SortPromos` sorts promos with pending state before others without.
TEST_F(PromosManagerImplTest, SortsPromosPreferPendingToNonPending) {
  CreatePromosManager();

  const std::map<promos_manager::Promo, PromoContext> active_promos = {
      {promos_manager::Promo::Test, PromoContext{true}},
      {promos_manager::Promo::DefaultBrowser, PromoContext{false}},
      {promos_manager::Promo::AppStoreRating, PromoContext{true}},
  };

  std::vector<promos_manager::Promo> sorted =
      promos_manager_->SortPromos(active_promos);
  EXPECT_EQ(sorted.size(), (size_t)3);
  // The first 2 are tied with the pending state.
  EXPECT_TRUE(sorted[0] == promos_manager::Promo::Test ||
              sorted[0] == promos_manager::Promo::AppStoreRating);
  // The one without pending state is at the end.
  EXPECT_EQ(sorted[2], promos_manager::Promo::DefaultBrowser);
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
  std::optional<base::Time> actual_becomes_active_time = ValueToTime(
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
  std::optional<base::Time> actual_becomes_active_time = ValueToTime(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .Find(promos_manager::NameForPromo(
              promos_manager::Promo::CredentialProviderExtension)));
  EXPECT_TRUE(actual_becomes_active_time.has_value());
  EXPECT_EQ(actual_becomes_active_time.value(),
            test_clock_.Now() + kTimeDelta1Day * 2);
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

// Tests a given single-display promo is automatically deregistered correctly.
TEST_F(PromosManagerImplTest, DeregistersSingleDisplayPromoAfterDisplay) {
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

  promos_manager_->DeregisterAfterDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_TRUE(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos)
          .empty());
}

// Tests a given single-display pending promo is automatically deregistered
// correctly.
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

  promos_manager_->DeregisterAfterDisplay(
      promos_manager::Promo::CredentialProviderExtension);

  EXPECT_TRUE(
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos)
          .empty());
}

// Tests `NextPromoForDisplay` returns a pending promo that has become active
// and takes precedence over other active promos.
TEST_F(PromosManagerImplTest, NextPromoForDisplayReturnsPendingPromo) {
  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histogram_tester;

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

  PromoConfigsSet promoConfigs;
  promoConfigs.emplace(promos_manager::Promo::Test, &kTestFeatureOne);
  promoConfigs.emplace(promos_manager::Promo::CredentialProviderExtension,
                       &kTestFeatureTwo);
  promoConfigs.emplace(promos_manager::Promo::AppStoreRating,
                       &kTestFeatureThree);
  promos_manager_->InitializePromoConfigs(std::move(promoConfigs));

  // Mock the FET tracker to allow all promos.
  EXPECT_CALL(mock_tracker_, ShouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_tracker_, WouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));

  // Advance to so that the CredentialProviderExtension becomes active.
  test_clock_.Advance(kTimeDelta1Day + kTimeDelta1Hour);

  std::optional<promos_manager::Promo> promo =
      promos_manager_->NextPromoForDisplay();
  ASSERT_TRUE(promo.has_value());
  EXPECT_EQ(promo.value(), promos_manager::Promo::CredentialProviderExtension);
  histogram_tester.ExpectUniqueSample(
      "IOS.PromosManager.EligiblePromosInQueueCount", 2, 1);
}

// Tests `NextPromoForDisplay` returns an active promo whose type has the
// highest priority can take precedence over other pending-becomes-active
// promos.
TEST_F(PromosManagerImplTest,
       NextPromoForDisplayReturnsActivePromoOfPrioritizedType) {
  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histogram_tester;

  CreatePromosManager();

  promos_manager_->single_display_active_promos_ = {
      promos_manager::Promo::PostRestoreSignInFullscreen,
  };
  promos_manager_->single_display_pending_promos_ = {
      {promos_manager::Promo::CredentialProviderExtension,
       test_clock_.Now() + kTimeDelta1Day},
  };

  PromoConfigsSet promoConfigs;
  promoConfigs.emplace(promos_manager::Promo::PostRestoreSignInFullscreen,
                       &kTestFeatureOne);
  promoConfigs.emplace(promos_manager::Promo::CredentialProviderExtension,
                       &kTestFeatureTwo);
  promos_manager_->InitializePromoConfigs(std::move(promoConfigs));

  // Mock the FET tracker to allow all promos.
  EXPECT_CALL(mock_tracker_, ShouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_tracker_, WouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));

  // Advance to so that the CredentialProviderExtension becomes active.
  test_clock_.Advance(kTimeDelta1Day + kTimeDelta1Hour);

  std::optional<promos_manager::Promo> promo =
      promos_manager_->NextPromoForDisplay();

  ASSERT_TRUE(promo.has_value());
  EXPECT_EQ(promo.value(), promos_manager::Promo::PostRestoreSignInFullscreen);
  histogram_tester.ExpectUniqueSample(
      "IOS.PromosManager.EligiblePromosInQueueCount", 2, 1);
}

// Tests `NextPromoForDisplay` returns empty when non of the pending promos can
// become active.
TEST_F(PromosManagerImplTest, NextPromoForDisplayReturnsEmpty) {
  base::HistogramTester histogram_tester;
  CreatePromosManager();

  promos_manager_->single_display_active_promos_ = {};
  promos_manager_->single_display_pending_promos_ = {
      {promos_manager::Promo::Test, test_clock_.Now() + kTimeDelta1Hour * 2},
      {promos_manager::Promo::CredentialProviderExtension,
       test_clock_.Now() + kTimeDelta1Day},
  };

  // Advance to so that the none of the pending promo can become active.
  test_clock_.Advance(kTimeDelta1Hour);

  std::optional<promos_manager::Promo> promo =
      promos_manager_->NextPromoForDisplay();

  EXPECT_FALSE(promo.has_value());
  histogram_tester.ExpectTotalCount(
      "IOS.PromosManager.EligiblePromosInQueueCount", 0);
}
