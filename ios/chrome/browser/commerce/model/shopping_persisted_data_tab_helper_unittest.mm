// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"

#import "base/base64.h"
#import "base/command_line.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/core/optimization_guide_test_util.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/unified_consent/pref_names.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
constexpr char kTypeURL[] =
    "type.googleapis.com/optimization_guide.proto.PriceTrackingData";
constexpr char kPriceDropUrl[] = "https://merchant.com/has_price_drop.html";
constexpr char kNoPriceDropUrl[] =
    "https://merchant.com/has_no_price__drop.html";
const char kCurrencyCodeUS[] = "USD";
const char kCurrencyCodeCanada[] = "CAD";
const char kDefaultLocale[] = "en";
const char kCurrentPriceFormatted[] = "$5.00";
const char kPreviousPriceFormatted[] = "$10";
const char kFormattedTwoDecimalPlaces[] = "$9.99";
const char kFormattedNoDecimalPlaces[] = "$20";
const int64_t kLessthanTwoUnitsPreviousPrice = 8'500'000;
const int64_t kLessthanTenPercentPreviousPrice = 9'200'000;
const int64_t kHigherThanPreviousPrice = 20'000'000;
const int64_t kLowerThanCurrentPriceMicros = 1'000'000;
const int64_t kCurrentPriceMicros = 5'000'000;
const int64_t kPreviousPreiceMicros = 10'000'000;
const int64_t kFormatNoDecimalPlacesMicros = 20'000'000;
const int64_t kFormatTwoDecimalPlacesMicros = 9'990'000;
const int64_t kOfferId = 50;

void FillPriceTrackingProto(commerce::PriceTrackingData& price_tracking_data,
                            int64_t offer_id,
                            int64_t old_price_micros,
                            int64_t new_price_micros,
                            std::string currency_code) {
  price_tracking_data.mutable_product_update()->set_offer_id(offer_id);
  price_tracking_data.mutable_product_update()
      ->mutable_old_price()
      ->set_currency_code(currency_code);
  price_tracking_data.mutable_product_update()
      ->mutable_new_price()
      ->set_currency_code(currency_code);
  price_tracking_data.mutable_product_update()
      ->mutable_new_price()
      ->set_amount_micros(new_price_micros);
  price_tracking_data.mutable_product_update()
      ->mutable_old_price()
      ->set_amount_micros(old_price_micros);
}

}  // namespace

class ShoppingPersistedDataTabHelperTest : public PlatformTest {
 public:
  ShoppingPersistedDataTabHelperTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::kPurgeHintsStore);
  }

  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    profile_->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity_);
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForProfile(
            profile_.get()));
    auth_service_->SignIn(fake_identity_,
                          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  void MockOptimizationGuideResponse(
      const commerce::PriceTrackingData& price_tracking_data) {
    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(kTypeURL);
    price_tracking_data.SerializeToString(any_metadata.mutable_value());
    optimization_guide::OptimizationMetadata metadata;
    metadata.set_any_metadata(any_metadata);
    OptimizationGuideService* optimization_guide_service =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());
    optimization_guide_service->AddHintForTesting(
        GURL(kPriceDropUrl), optimization_guide::proto::PRICE_TRACKING,
        metadata);
    web_state_.SetBrowserState(profile_.get());
    ShoppingPersistedDataTabHelper::CreateForWebState(&web_state_);
  }

  void CommitToUrlAndNavigate(const GURL& url) {
    context_.SetUrl(url);
    context_.SetHasCommitted(true);
    web_state_.OnNavigationStarted(&context_);
    web_state_.OnNavigationFinished(&context_);
    web_state_.SetCurrentURL(GURL(kPriceDropUrl));
  }

  const ShoppingPersistedDataTabHelper::PriceDrop* GetPriceDrop() {
    return ShoppingPersistedDataTabHelper::FromWebState(&web_state_)
        ->GetPriceDrop();
  }

  BOOL IsPriceDropEmpty() {
    return !GetPriceDrop()->current_price && !GetPriceDrop()->previous_price;
  }

  BOOL IsQualifyingPriceDrop(int64_t current_price_micros,
                             int64_t previous_price_micros) {
    return ShoppingPersistedDataTabHelper::IsQualifyingPriceDrop(
        current_price_micros, previous_price_micros);
  }

  std::u16string FormatPrice(payments::CurrencyFormatter* currency_formatter,
                             long price_micros) {
    return ShoppingPersistedDataTabHelper::FormatPrice(currency_formatter,
                                                       price_micros);
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  std::unique_ptr<payments::CurrencyFormatter> GetCurrencyFormatter(
      const std::string& currency_code) {
    return std::make_unique<payments::CurrencyFormatter>(currency_code,
                                                         kDefaultLocale);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  web::FakeNavigationContext context_;
  id<SystemIdentity> fake_identity_ = nil;
  raw_ptr<AuthenticationService> auth_service_ = nullptr;
};

TEST_F(ShoppingPersistedDataTabHelperTest, TestRegularPriceDrop) {
  commerce::PriceTrackingData price_tracking_data;
  FillPriceTrackingProto(price_tracking_data, kOfferId, kPreviousPreiceMicros,
                         kCurrentPriceMicros, kCurrencyCodeUS);
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  EXPECT_EQ(kCurrentPriceFormatted,
            base::SysNSStringToUTF8(GetPriceDrop()->current_price));
  EXPECT_EQ(kPreviousPriceFormatted,
            base::SysNSStringToUTF8(GetPriceDrop()->previous_price));
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestRegularPriceIncreaseNull) {
  commerce::PriceTrackingData price_tracking_data;
  FillPriceTrackingProto(price_tracking_data, kOfferId,
                         kLowerThanCurrentPriceMicros, kCurrentPriceMicros,
                         kCurrencyCodeUS);
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  EXPECT_TRUE(IsPriceDropEmpty());
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestEqualPriceNull) {
  commerce::PriceTrackingData price_tracking_data;
  FillPriceTrackingProto(price_tracking_data, kOfferId, kCurrentPriceMicros,
                         kCurrentPriceMicros, kCurrencyCodeUS);
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  EXPECT_TRUE(IsPriceDropEmpty());
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestNoPriceDropUrl) {
  commerce::PriceTrackingData price_tracking_data;
  FillPriceTrackingProto(price_tracking_data, kOfferId, kCurrentPriceMicros,
                         kCurrentPriceMicros, kCurrencyCodeUS);
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kNoPriceDropUrl));
  RunUntilIdle();
  EXPECT_TRUE(IsPriceDropEmpty());
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestInconsistentCurrencyCode) {
  commerce::PriceTrackingData price_tracking_data;
  FillPriceTrackingProto(price_tracking_data, kOfferId, kCurrentPriceMicros,
                         kCurrentPriceMicros, kCurrencyCodeUS);
  price_tracking_data.mutable_product_update()
      ->mutable_new_price()
      ->set_currency_code(kCurrencyCodeCanada);
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  EXPECT_TRUE(IsPriceDropEmpty());
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestPriceDropLessThanTwoUnits) {
  commerce::PriceTrackingData price_tracking_data;
  FillPriceTrackingProto(price_tracking_data, kOfferId, kPreviousPreiceMicros,
                         kLessthanTwoUnitsPreviousPrice, kCurrencyCodeUS);
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  EXPECT_TRUE(IsPriceDropEmpty());
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestPriceDropLessThanTenPercent) {
  commerce::PriceTrackingData price_tracking_data;
  FillPriceTrackingProto(price_tracking_data, kOfferId, kPreviousPreiceMicros,
                         kLessthanTenPercentPreviousPrice, kCurrencyCodeUS);
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  EXPECT_TRUE(IsPriceDropEmpty());
}

TEST_F(ShoppingPersistedDataTabHelperTest,
       TestIsQualifyingPriceDropRegularPrice) {
  EXPECT_TRUE(
      IsQualifyingPriceDrop(kCurrentPriceMicros, kPreviousPreiceMicros));
}

TEST_F(ShoppingPersistedDataTabHelperTest,
       TestIsQualifyingPriceDropLessThanTwoUnits) {
  EXPECT_FALSE(IsQualifyingPriceDrop(kLessthanTwoUnitsPreviousPrice,
                                     kPreviousPreiceMicros));
}

TEST_F(ShoppingPersistedDataTabHelperTest,
       TestIsQualifyingPriceDropLessThanTenPercent) {
  EXPECT_FALSE(IsQualifyingPriceDrop(kLessthanTenPercentPreviousPrice,
                                     kPreviousPreiceMicros));
}

TEST_F(ShoppingPersistedDataTabHelperTest,
       TestIsQualifyingPriceDropPriceIncrease) {
  EXPECT_FALSE(
      IsQualifyingPriceDrop(kHigherThanPreviousPrice, kPreviousPreiceMicros));
}

TEST_F(ShoppingPersistedDataTabHelperTest,
       TestCurrencyFormatterNoDecimalPlaces) {
  std::unique_ptr<payments::CurrencyFormatter> currencyFormatter =
      GetCurrencyFormatter(kCurrencyCodeUS);
  EXPECT_EQ(kFormattedNoDecimalPlaces,
            base::UTF16ToUTF8(FormatPrice(currencyFormatter.get(),
                                          kFormatNoDecimalPlacesMicros)));
}

TEST_F(ShoppingPersistedDataTabHelperTest,
       TestCurrencyFormatterTwoDecimalPlaces) {
  std::unique_ptr<payments::CurrencyFormatter> currencyFormatter =
      GetCurrencyFormatter(kCurrencyCodeUS);
  EXPECT_EQ(kFormattedTwoDecimalPlaces,
            base::UTF16ToUTF8(FormatPrice(currencyFormatter.get(),
                                          kFormatTwoDecimalPlacesMicros)));
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestPriceDropHistogram) {
  commerce::PriceTrackingData price_tracking_data;
  FillPriceTrackingProto(price_tracking_data, kOfferId, kPreviousPreiceMicros,
                         kCurrentPriceMicros, kCurrencyCodeUS);
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  ShoppingPersistedDataTabHelper::FromWebState(&web_state_)
      ->LogMetrics(TAB_SWITCHER);
  histogram_tester_.ExpectUniqueSample(
      "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPrice", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPriceDrop", true,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.IsProductDetailPage", true,
      1);
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestNoPriceDropHistogram) {
  commerce::PriceTrackingData price_tracking_data;
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kNoPriceDropUrl));
  RunUntilIdle();
  ShoppingPersistedDataTabHelper::FromWebState(&web_state_)
      ->LogMetrics(TAB_SWITCHER);
  histogram_tester_.ExpectUniqueSample(
      "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPrice", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPriceDrop", false,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.IsProductDetailPage",
      false, 1);
}

TEST_F(ShoppingPersistedDataTabHelperTest,
       TestProductDetailPageNoPriceHistogram) {
  commerce::PriceTrackingData price_tracking_data;
  price_tracking_data.mutable_buyable_product()->set_offer_id(42);
  MockOptimizationGuideResponse(price_tracking_data);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  ShoppingPersistedDataTabHelper::FromWebState(&web_state_)
      ->LogMetrics(TAB_SWITCHER);
  histogram_tester_.ExpectUniqueSample(
      "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPrice", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPriceDrop", false,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.IsProductDetailPage", true,
      1);
}
