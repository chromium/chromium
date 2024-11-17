// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_model.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/commerce_types.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/price_insights/model/price_insights_feature.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

using testing::_;

namespace {

const char kTestUrl[] = "https://www.merchant.com/price_drop_product";
const char kTestSecondUrl[] =
    "https://www.merchant.com/second_price_drop_product";
std::string kTestTitle = "Product";

}  // namespace

// Unittests related to the PriceInsightsModel.
class PriceInsightsModelTest : public PlatformTest {
 public:
  PriceInsightsModelTest() {}
  ~PriceInsightsModelTest() override {}

  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));
    test_profile_ = std::move(builder).Build();
    std::unique_ptr<web::FakeNavigationManager> navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(GURL(kTestUrl), ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetNavigationManager(std::move(navigation_manager));
    web_state_->SetBrowserState(test_profile_.get());
    web_state_->SetNavigationItemCount(1);
    web_state_->SetCurrentURL(GURL(kTestUrl));
    web_state_->SetBrowserState(test_profile_.get());
    price_insights_model_ = std::make_unique<PriceInsightsModel>();
    shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForProfile(test_profile_.get()));
    shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);
    shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(std::nullopt);
    shopping_service_->SetIsSubscribedCallbackValue(false);
    shopping_service_->SetIsShoppingListEligible(true);
    fetch_configuration_callback_count = 0;
  }

  void FetchConfigurationCallback(
      std::unique_ptr<ContextualPanelItemConfiguration> configuration) {
    returned_configuration_ = std::move(configuration);
    fetch_configuration_callback_count++;
  }

  int GetPriceInsightsCallbacksCount() {
    return price_insights_model_->callbacks_.size();
  }

  int GetPriceInsightsCallbacksValueCountForUrl(const GURL& product_url) {
    auto callbacks_it = price_insights_model_->callbacks_.find(product_url);
    return callbacks_it->second.size();
  }

  int GetPriceInsightsExecutionsCount() {
    return price_insights_model_->price_insights_executions_.size();
  }

 protected:
  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PriceInsightsModel> price_insights_model_;
  raw_ptr<commerce::MockShoppingService> shopping_service_;
  std::unique_ptr<ContextualPanelItemConfiguration> returned_configuration_;
  int fetch_configuration_callback_count;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<TestProfileIOS> test_profile_;
};

// Tests that fetching the configuration for the price insights model returns no
// data when there's any product info.
TEST_F(PriceInsightsModelTest, TestFetchConfigurationNoProductInfo) {
  base::RunLoop run_loop;

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(0);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(nullptr, config);
}

// Tests that fetching the configuration for the price insights model returns no
// data when product info has no title and no product cluster title.
TEST_F(PriceInsightsModelTest, TestFetchConfigurationNoTitleNoClusterTitle) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(0);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(nullptr, config);
}

// Tests that fetching the configuration for the price insights model returns no
// data when product info is available without tracking.
TEST_F(PriceInsightsModelTest, TestFetchConfigurationProductInfoNoTracking) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));
  shopping_service_->SetIsShoppingListEligible(false);

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(nullptr, config);
}

// Test that GetProductInfoForUrl return data when the configuration is fetched.
TEST_F(PriceInsightsModelTest, TestFetchProductInfo) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  EXPECT_EQ(1, GetPriceInsightsCallbacksCount());
  EXPECT_EQ(1, GetPriceInsightsExecutionsCount());

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(false, config->price_insights_info.has_value());
  EXPECT_EQ(true, config->product_info.has_value());
  commerce::ProductInfo info2 = config->product_info.value();
  EXPECT_EQ(kTestTitle, info2.title);

  EXPECT_EQ(0, GetPriceInsightsCallbacksCount());
  EXPECT_EQ(0, GetPriceInsightsExecutionsCount());
}

// Test that GetPriceInsightsInfoForUrl return data when the configuration is
// fetched.
TEST_F(PriceInsightsModelTest, TestFetchPriceInsightsInfo) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->product_cluster_id = 123u;
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(0);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  EXPECT_EQ(1, GetPriceInsightsCallbacksCount());
  EXPECT_EQ(1, GetPriceInsightsExecutionsCount());

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(true, config->product_info.has_value());
  EXPECT_EQ(true, config->price_insights_info.has_value());
  commerce::PriceInsightsInfo info2 = config->price_insights_info.value();
  EXPECT_EQ(123u, info2.product_cluster_id);

  EXPECT_EQ(0, GetPriceInsightsCallbacksCount());
  EXPECT_EQ(0, GetPriceInsightsExecutionsCount());
}

// Tests that GetProductInfoForUrl is only called once when multiple requests
// in-flight have the same URL.
TEST_F(PriceInsightsModelTest, TestMultipleRequestForTheSameURL) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> product_info;
  product_info.emplace();
  product_info->title = kTestTitle;
  shopping_service_->SetResponseForGetProductInfoForUrl(
      std::move(product_info));

  std::optional<commerce::PriceInsightsInfo> price_insights_info;
  price_insights_info.emplace();
  price_insights_info->catalog_history_prices.emplace_back("2021-01-01",
                                                           3330000);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_insights_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(0);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  EXPECT_EQ(1, GetPriceInsightsCallbacksCount());
  EXPECT_EQ(2, GetPriceInsightsCallbacksValueCountForUrl(GURL(kTestUrl)));
  EXPECT_EQ(1, GetPriceInsightsExecutionsCount());

  run_loop.Run();

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return fetch_configuration_callback_count == 2;
      }));

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(true, config->product_info.has_value());
  EXPECT_EQ(true, config->price_insights_info.has_value());

  EXPECT_EQ(0, GetPriceInsightsCallbacksCount());
  EXPECT_EQ(0, GetPriceInsightsExecutionsCount());
}

// Tests that GetProductInfoForUrl is called for each request in-flight that has
// a different URL.
TEST_F(PriceInsightsModelTest, TestMultipleRequestForDifferentURL) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> product_info;
  product_info.emplace();
  product_info->title = kTestTitle;
  shopping_service_->SetResponseForGetProductInfoForUrl(
      std::move(product_info));

  std::optional<commerce::PriceInsightsInfo> price_insights_info;
  price_insights_info.emplace();
  price_insights_info->catalog_history_prices.emplace_back("2021-01-01",
                                                           3330000);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_insights_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(2);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(2);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(0);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  web_state_->SetCurrentURL(GURL(kTestSecondUrl));

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  EXPECT_EQ(2, GetPriceInsightsCallbacksCount());
  EXPECT_EQ(2, GetPriceInsightsExecutionsCount());

  run_loop.Run();

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return fetch_configuration_callback_count == 2;
      }));

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(true, config->product_info.has_value());
  EXPECT_EQ(true, config->price_insights_info.has_value());

  EXPECT_EQ(0, GetPriceInsightsCallbacksCount());
  EXPECT_EQ(0, GetPriceInsightsExecutionsCount());
}

// Test that GetProductInfoForUrl return data when the configuration is fetched.
TEST_F(PriceInsightsModelTest, TestFetchIsSubscribed) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));
  shopping_service_->SetIsSubscribedCallbackValue(true);

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(true, config->is_subscribed);
}

// Test that GetProductInfoForUrl return data when the configuration is fetched.
TEST_F(PriceInsightsModelTest, TestFetchProductInfoWithPriceTrackAvailable) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));
  shopping_service_->SetIsSubscribedCallbackValue(false);

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(true, config->can_price_track);
}

// Test that price track is not available when the eligibility is not met.
TEST_F(PriceInsightsModelTest, TestFetchProductInfoWithPriceTrackUnavailable) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));
  shopping_service_->SetIsSubscribedCallbackValue(false);
  shopping_service_->SetIsShoppingListEligible(false);

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->product_cluster_id = 123u;
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(0);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(false, config->can_price_track);
}

// Test that GetProductInfo, GetProductInfoForUrl, and IsSubscribed all return
// data for the config.
TEST_F(PriceInsightsModelTest, TestFetchCompleteConfig) {
  base::RunLoop run_loop;

  shopping_service_->SetIsSubscribedCallbackValue(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->product_cluster_id = 123u;
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(true, config->is_subscribed);
}

// Test that GetProductInfo, GetProductInfoForUrl, all return
// data for the config when the product cannot be tracked.
TEST_F(PriceInsightsModelTest, TestFetchPriceInsightsWhenTrackUnavailable) {
  base::RunLoop run_loop;

  shopping_service_->SetIsSubscribedCallbackValue(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));
  shopping_service_->SetIsSubscribedCallbackValue(false);
  shopping_service_->SetIsShoppingListEligible(false);

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->product_cluster_id = 123u;
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(false, config->is_subscribed);
  EXPECT_EQ(false, config->can_price_track);
  EXPECT_EQ(true, config->product_info.has_value());
  EXPECT_EQ(true, config->price_insights_info.has_value());
}

// Test that when the price bucket is unknown, the entrypoint message is empty
// and the relevance is set to low.
TEST_F(PriceInsightsModelTest, TestPriceBucketUnknownEmptyMessageLowRelevance) {
  base::RunLoop run_loop;

  features_.InitAndEnableFeatureWithParameters(
      commerce::kPriceInsightsIos,
      {{kLowPriceParam, kLowPriceParamPriceIsLow}});

  shopping_service_->SetIsSubscribedCallbackValue(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->product_cluster_id = 123u;
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  price_info->price_bucket = commerce::PriceBucket::kUnknown;
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_PRICE_INSIGHTS_ACCESSIBILITY),
            config->accessibility_label);
  EXPECT_EQ("", config->entrypoint_message);
  EXPECT_EQ(base::SysNSStringToUTF8(kDownTrendSymbol),
            config->entrypoint_image_name);
  EXPECT_EQ(ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol,
            config->image_type);
  EXPECT_EQ(ContextualPanelItemConfiguration::low_relevance, config->relevance);
  EXPECT_EQ(&feature_engagement::kIPHiOSContextualPanelPriceInsightsFeature,
            config->iph_feature);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointUsed,
            config->iph_entrypoint_used_event_name);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed,
            config->iph_entrypoint_explicitly_dismissed);
}

// Test that when the price bucket is low, the entrypoint message is set to a
// specific string, and the relevance is set to high.
TEST_F(PriceInsightsModelTest, TestPriceBucketLowLowPriceMessageHighRelevance) {
  base::RunLoop run_loop;

  features_.InitAndEnableFeatureWithParameters(
      commerce::kPriceInsightsIos,
      {{kLowPriceParam, kLowPriceParamPriceIsLow}});

  shopping_service_->SetIsSubscribedCallbackValue(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->product_cluster_id = 123u;
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  price_info->price_bucket = commerce::PriceBucket::kLowPrice;
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_SHOPPING_INSIGHTS_ICON_EXPANDED_TEXT_LOW_PRICE),
            config->accessibility_label);
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_SHOPPING_INSIGHTS_ICON_EXPANDED_TEXT_LOW_PRICE),
            config->entrypoint_message);
  EXPECT_EQ(base::SysNSStringToUTF8(kDownTrendSymbol),
            config->entrypoint_image_name);
  EXPECT_EQ(ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol,
            config->image_type);
  EXPECT_EQ(ContextualPanelItemConfiguration::high_relevance,
            config->relevance);
  EXPECT_EQ(&feature_engagement::kIPHiOSContextualPanelPriceInsightsFeature,
            config->iph_feature);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointUsed,
            config->iph_entrypoint_used_event_name);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed,
            config->iph_entrypoint_explicitly_dismissed);
}

// Test that when the price bucket is low, the entrypoint message is set to a
// specific string, and the relevance is set to high.
TEST_F(PriceInsightsModelTest, TestPriceBucketLowGoodDealMessageHighRelevance) {
  base::RunLoop run_loop;

  features_.InitAndEnableFeatureWithParameters(
      commerce::kPriceInsightsIos,
      {{kLowPriceParam, kLowPriceParamGoodDealNow}});

  shopping_service_->SetIsSubscribedCallbackValue(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->product_cluster_id = 123u;
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  price_info->price_bucket = commerce::PriceBucket::kLowPrice;
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_INSIGHTS_ICON_EXPANDED_TEXT_GOOD_DEAL),
            config->accessibility_label);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_INSIGHTS_ICON_EXPANDED_TEXT_GOOD_DEAL),
            config->entrypoint_message);
  EXPECT_EQ(base::SysNSStringToUTF8(kDownTrendSymbol),
            config->entrypoint_image_name);
  EXPECT_EQ(ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol,
            config->image_type);
  EXPECT_EQ(ContextualPanelItemConfiguration::high_relevance,
            config->relevance);
  EXPECT_EQ(&feature_engagement::kIPHiOSContextualPanelPriceInsightsFeature,
            config->iph_feature);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointUsed,
            config->iph_entrypoint_used_event_name);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed,
            config->iph_entrypoint_explicitly_dismissed);
}

// Test that when the price bucket is low, the entrypoint message is set to a
// specific string, and the relevance is set to high.
TEST_F(PriceInsightsModelTest,
       TestPriceBucketLowSeePriceHistoryMessageHighRelevance) {
  base::RunLoop run_loop;

  features_.InitAndEnableFeatureWithParameters(
      commerce::kPriceInsightsIos,
      {{kLowPriceParam, kLowPriceParamSeePriceHistory}});

  shopping_service_->SetIsSubscribedCallbackValue(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->product_cluster_id = 123u;
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  price_info->price_bucket = commerce::PriceBucket::kLowPrice;
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_INSIGHTS_ICON_EXPANDED_TEXT_PRICE_HISTORY),
      config->accessibility_label);
  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_INSIGHTS_ICON_EXPANDED_TEXT_PRICE_HISTORY),
      config->entrypoint_message);
  EXPECT_EQ(ContextualPanelItemConfiguration::high_relevance,
            config->relevance);
}

// Test that when the price bucket is high and PriceInisghtsHighPrice is
// disabled, the relevance is set to low and the accessibility text are not
// empty.
TEST_F(PriceInsightsModelTest,
       TestHighPriceDisabledPriceBucketHighEmptyMessageLowRelevance) {
  base::RunLoop run_loop;

  features_.InitAndDisableFeature(commerce::kPriceInsightsHighPriceIos);

  shopping_service_->SetIsSubscribedCallbackValue(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  price_info->price_bucket = commerce::PriceBucket::kHighPrice;
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_PRICE_INSIGHTS_ACCESSIBILITY),
            config->accessibility_label);
  EXPECT_EQ("", config->entrypoint_message);
  EXPECT_EQ(base::SysNSStringToUTF8(kDownTrendSymbol),
            config->entrypoint_image_name);
  EXPECT_EQ(ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol,
            config->image_type);
  EXPECT_EQ(ContextualPanelItemConfiguration::low_relevance, config->relevance);
  EXPECT_EQ(&feature_engagement::kIPHiOSContextualPanelPriceInsightsFeature,
            config->iph_feature);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointUsed,
            config->iph_entrypoint_used_event_name);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed,
            config->iph_entrypoint_explicitly_dismissed);
}

// Test that when the price bucket is high and the page is subscribed, the
// relevance is set to low and the accessibility text are not empty.
TEST_F(PriceInsightsModelTest, TestPriceBucketHighSubscribedLowRelevance) {
  base::RunLoop run_loop;

  features_.InitAndEnableFeature(commerce::kPriceInsightsHighPriceIos);

  shopping_service_->SetIsSubscribedCallbackValue(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  price_info->price_bucket = commerce::PriceBucket::kHighPrice;
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_PRICE_INSIGHTS_ACCESSIBILITY),
            config->accessibility_label);
  EXPECT_EQ("", config->entrypoint_message);
  EXPECT_EQ(base::SysNSStringToUTF8(kUpTrendSymbol),
            config->entrypoint_image_name);
  EXPECT_EQ(ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol,
            config->image_type);
  EXPECT_EQ(ContextualPanelItemConfiguration::low_relevance, config->relevance);
  EXPECT_EQ(&feature_engagement::kIPHiOSContextualPanelPriceInsightsFeature,
            config->iph_feature);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointUsed,
            config->iph_entrypoint_used_event_name);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed,
            config->iph_entrypoint_explicitly_dismissed);
}

// Test that when the price bucket is high and the page can not be tracked, the
// relevance is set to low and the accessibility text is not empty.
TEST_F(PriceInsightsModelTest,
       TestPriceBucketHighTrackNotAvailableLowRelevance) {
  base::RunLoop run_loop;

  features_.InitAndEnableFeature(commerce::kPriceInsightsHighPriceIos);

  shopping_service_->SetIsSubscribedCallbackValue(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  price_info->price_bucket = commerce::PriceBucket::kHighPrice;
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(0);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_PRICE_INSIGHTS_ACCESSIBILITY),
            config->accessibility_label);
  EXPECT_EQ("", config->entrypoint_message);
  EXPECT_EQ(base::SysNSStringToUTF8(kUpTrendSymbol),
            config->entrypoint_image_name);
  EXPECT_EQ(ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol,
            config->image_type);
  EXPECT_EQ(ContextualPanelItemConfiguration::low_relevance, config->relevance);
  EXPECT_EQ(&feature_engagement::kIPHiOSContextualPanelPriceInsightsFeature,
            config->iph_feature);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointUsed,
            config->iph_entrypoint_used_event_name);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed,
            config->iph_entrypoint_explicitly_dismissed);
}

// Test that when the price bucket is high and the page is currently not
// subscribed, the relevance is set to high and both the entrypoint text and
// accessibility text are not empty.
TEST_F(PriceInsightsModelTest, TestPriceBucketHighHighRelevance) {
  base::RunLoop run_loop;

  features_.InitAndEnableFeature(commerce::kPriceInsightsHighPriceIos);

  shopping_service_->SetIsSubscribedCallbackValue(false);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = kTestTitle;
  info->product_cluster_id = 12345L;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  std::optional<commerce::PriceInsightsInfo> price_info;
  price_info.emplace();
  price_info->product_cluster_id = 123u;
  price_info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info->catalog_history_prices.emplace_back("2021-01-02", 4440000);
  price_info->price_bucket = commerce::PriceBucket::kHighPrice;
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, GetPriceInsightsInfoForUrl(_, _)).Times(1);
  EXPECT_CALL(*shopping_service_, IsSubscribed(_, _)).Times(1);

  price_insights_model_->FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_INSIGHTS_ICON_PRICE_HIGH_EXPANDED_TEXT),
      config->accessibility_label);
  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_INSIGHTS_ICON_PRICE_HIGH_EXPANDED_TEXT),
      config->entrypoint_message);
  EXPECT_EQ(base::SysNSStringToUTF8(kUpTrendSymbol),
            config->entrypoint_image_name);
  EXPECT_EQ(ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol,
            config->image_type);
  EXPECT_EQ(ContextualPanelItemConfiguration::high_relevance,
            config->relevance);
  EXPECT_EQ(&feature_engagement::kIPHiOSContextualPanelPriceInsightsFeature,
            config->iph_feature);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointUsed,
            config->iph_entrypoint_used_event_name);
  EXPECT_EQ(feature_engagement::events::
                kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed,
            config->iph_entrypoint_explicitly_dismissed);
}
