// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"

#import "base/base64.h"
#import "base/command_line.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/unified_consent/pref_names.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
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
constexpr char kPriceDropUrl[] = "https://merchant.com/has_price_drop.html";
constexpr char kNoPriceDropUrl[] =
    "https://merchant.com/has_no_price__drop.html";
const char kCurrencyCodeUS[] = "USD";
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

}  // namespace

class ShoppingPersistedDataTabHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));

    profile_ = std::move(builder).Build();
    profile_->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity_);
    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());
    auth_service_->SignIn(fake_identity_,
                          signin_metrics::AccessPoint::kUnknown);
    shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForProfile(profile_.get()));
    web_state_.SetBrowserState(profile_.get());
    ShoppingPersistedDataTabHelper::CreateForWebState(&web_state_);
  }

  commerce::MockShoppingService* shopping_service() {
    return shopping_service_;
  }

  void SetResponseForGetProductInfoForUrl(int64_t offer_id,
                                          int64_t old_price_micros,
                                          int64_t new_price_micros,
                                          std::string currency_code) {
    commerce::ProductInfo product_info;
    product_info.offer_id = offer_id;
    product_info.amount_micros = new_price_micros;
    product_info.previous_amount_micros = old_price_micros;
    product_info.currency_code = currency_code;
    shopping_service()->SetResponseForGetProductInfoForUrl(product_info);
  }
  void SetEmptyResponseForGetProductInfoForUrl() {
    shopping_service()->SetResponseForGetProductInfoForUrl(std::nullopt);
  }

  void SetProductDetailPageResponseForGetProductInfoForUrl(int64_t offer_id) {
    commerce::ProductInfo product_info;
    product_info.offer_id = offer_id;
    shopping_service()->SetResponseForGetProductInfoForUrl(product_info);
  }

  void CommitToUrlAndNavigate(const GURL& url) {
    context_.SetUrl(url);
    context_.SetHasCommitted(true);
    web_state_.OnNavigationStarted(&context_);
    web_state_.OnNavigationFinished(&context_);
    web_state_.SetCurrentURL(GURL(kPriceDropUrl));
  }

  void GetPriceDrop(
      base::OnceCallback<void(
          std::optional<ShoppingPersistedDataTabHelper::PriceDrop>)> callback) {
    ShoppingPersistedDataTabHelper::FromWebState(&web_state_)
        ->GetPriceDrop(std::move(callback));
  }

  void CheckIsPriceDropEmpty(base::OnceClosure closure) {
    GetPriceDrop(base::BindOnce(
                     [](std::optional<ShoppingPersistedDataTabHelper::PriceDrop>
                            price_drop) {
                       EXPECT_TRUE(!price_drop.has_value() ||
                                   (!price_drop->current_price &&
                                    !price_drop->previous_price));
                     })
                     .Then(std::move(closure)));
  }

  void CheckIsPriceDropEmpty() {
    base::RunLoop wait_for_price_drop_is_empty_result;
    CheckIsPriceDropEmpty(wait_for_price_drop_is_empty_result.QuitClosure());
    wait_for_price_drop_is_empty_result.Run();
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
  raw_ptr<commerce::MockShoppingService> shopping_service_;
};

TEST_F(ShoppingPersistedDataTabHelperTest, TestRegularPriceDrop) {
  SetResponseForGetProductInfoForUrl(kOfferId, kPreviousPreiceMicros,
                                     kCurrentPriceMicros, kCurrencyCodeUS);

  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  base::RunLoop wait_for_price_drop_result;

  GetPriceDrop(
      base::BindOnce([](std::optional<ShoppingPersistedDataTabHelper::PriceDrop>
                            price_drop) {
        EXPECT_EQ(kCurrentPriceFormatted,
                  base::SysNSStringToUTF8(price_drop->current_price));
        EXPECT_EQ(kPreviousPriceFormatted,
                  base::SysNSStringToUTF8(price_drop->previous_price));
      }).Then(wait_for_price_drop_result.QuitClosure()));
  wait_for_price_drop_result.Run();
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestRegularPriceIncreaseNull) {
  SetResponseForGetProductInfoForUrl(kOfferId, kLowerThanCurrentPriceMicros,
                                     kCurrentPriceMicros, kCurrencyCodeUS);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  CheckIsPriceDropEmpty();
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestEqualPriceNull) {
  SetResponseForGetProductInfoForUrl(kOfferId, kCurrentPriceMicros,
                                     kCurrentPriceMicros, kCurrencyCodeUS);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  CheckIsPriceDropEmpty();
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestNoPriceDropUrl) {
  SetResponseForGetProductInfoForUrl(kOfferId, kCurrentPriceMicros,
                                     kCurrentPriceMicros, kCurrencyCodeUS);
  CommitToUrlAndNavigate(GURL(kNoPriceDropUrl));
  RunUntilIdle();
  CheckIsPriceDropEmpty();
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestPriceDropLessThanTwoUnits) {
  SetResponseForGetProductInfoForUrl(kOfferId, kPreviousPreiceMicros,
                                     kLessthanTwoUnitsPreviousPrice,
                                     kCurrencyCodeUS);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  CheckIsPriceDropEmpty();
}

TEST_F(ShoppingPersistedDataTabHelperTest, TestPriceDropLessThanTenPercent) {
  SetResponseForGetProductInfoForUrl(kOfferId, kPreviousPreiceMicros,
                                     kLessthanTenPercentPreviousPrice,
                                     kCurrencyCodeUS);
  CommitToUrlAndNavigate(GURL(kPriceDropUrl));
  RunUntilIdle();
  CheckIsPriceDropEmpty();
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
  SetResponseForGetProductInfoForUrl(kOfferId, kPreviousPreiceMicros,
                                     kCurrentPriceMicros, kCurrencyCodeUS);
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
  SetEmptyResponseForGetProductInfoForUrl();
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
  SetProductDetailPageResponseForGetProductInfoForUrl(42);
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
