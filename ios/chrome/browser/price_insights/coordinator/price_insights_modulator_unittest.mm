// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/coordinator/price_insights_modulator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/commerce/core/commerce_types.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/price_insights/model/price_insights_feature.h"
#import "ios/chrome/browser/price_insights/model/price_insights_model.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_cell.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

using testing::_;

namespace {

const char kTestUrl[] = "https://www.merchant.com/price_drop_product";
const char kTestBuyingOptionsUrl[] =
    "https://www.merchant.com/price_drop_product/jackpot";
const char kTestTitle[] = "Product";
const char kVariant[] = "Variant";
const char kCurrency[] = "USD";
const char kCountry[] = "US";
const uint64_t kClusterId = 123u;

}  // namespace

// Unittests related to the PriceInsightsModulator.
class PriceInsightsModulatorTest : public PlatformTest {
 public:
  PriceInsightsModulatorTest() {}
  ~PriceInsightsModulatorTest() override {}

  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::BookmarkModelFactory::GetInstance(),
                              ios::BookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));

    TestProfileIOS* test_profile =
        profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(test_profile);
    base_view_controller_ = [[FakeUIViewController alloc] init];
    std::unique_ptr<web::FakeNavigationManager> navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(GURL(kTestUrl), ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state_ptr_ = web_state.get();
    raw_ptr<WebStateList> web_state_list = browser_->GetWebStateList();
    web_state_list->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    web_state_ptr_->SetNavigationManager(std::move(navigation_manager));
    web_state_ptr_->SetBrowserState(test_profile);
    web_state_ptr_->SetNavigationItemCount(1);
    web_state_ptr_->SetCurrentURL(GURL(kTestUrl));
    web_state_ptr_->SetBrowserState(test_profile);
    price_insights_model_ = std::make_unique<PriceInsightsModel>();
    shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForProfile(test_profile));
    shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);
    shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(std::nullopt);
    shopping_service_->SetIsShoppingListEligible(true);
  }

  void FetchConfigurationCallback(
      std::unique_ptr<ContextualPanelItemConfiguration> configuration) {
    returned_configuration_ = std::move(configuration);
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::TaskEnvironment task_environment_;
  TestProfileManagerIOS profile_manager_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  raw_ptr<web::FakeWebState> web_state_ptr_;
  std::unique_ptr<PriceInsightsModel> price_insights_model_;
  raw_ptr<commerce::MockShoppingService> shopping_service_;
  std::unique_ptr<ContextualPanelItemConfiguration> returned_configuration_;
};

// Tests that when the subscription status of a page changes after querying the
// model, the modulator assigns the latest subscription status.
TEST_F(PriceInsightsModulatorTest, TestSubscriptionStatusChange) {
  base::RunLoop run_loop;

  commerce::ProductInfo info;
  info.title = kTestTitle;
  info.product_cluster_id = kClusterId;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  commerce::PriceInsightsInfo price_info;
  price_info.product_cluster_id = kClusterId;
  price_info.catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info.catalog_history_prices.emplace_back("2021-01-02", 4440000);
  shopping_service_->SetIsSubscribedCallbackValue(false);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  // Fetch data from the model.
  price_insights_model_->FetchConfigurationForWebState(
      web_state_ptr_,
      base::BindOnce(&PriceInsightsModulatorTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  EXPECT_EQ(false, config->is_subscribed);

  shopping_service_->SetIsSubscribedCallbackValue(true);

  PriceInsightsModulator* modulator = [[PriceInsightsModulator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
               itemConfiguration:config->weak_ptr_factory.GetWeakPtr()];

  // Start the modulator.
  [modulator start];

  UICollectionView* collection_view = [[UICollectionView alloc]
             initWithFrame:CGRectZero
      collectionViewLayout:[[UICollectionViewFlowLayout alloc] init]];
  PriceInsightsCell* cell = static_cast<PriceInsightsCell*>([collection_view
      dequeueConfiguredReusableCellWithRegistration:modulator.panelBlockData
                                                        .cellRegistration
                                       forIndexPath:[NSIndexPath
                                                        indexPathForRow:0
                                                              inSection:0]
                                               item:@"id"]);
  PriceInsightsItem* item = cell.priceInsightsItem;

  EXPECT_EQ(true, item.isPriceTracked);
}

// Tests that PriceInsightsItem has the correct data from
// PriceInsightsItemConfiguration.
TEST_F(PriceInsightsModulatorTest, TestPriceInsightsItemDataFromConfig) {
  base::RunLoop run_loop;

  commerce::ProductInfo info;
  info.product_cluster_title = kTestTitle;
  info.product_cluster_id = kClusterId;
  info.currency_code = kCurrency;
  info.country_code = kCountry;

  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  commerce::PriceInsightsInfo price_info;
  price_info.product_cluster_id = kClusterId;
  price_info.catalog_history_prices.emplace_back("2021-01-01", 3330000);
  price_info.catalog_history_prices.emplace_back("2021-01-02", 4440000);
  price_info.catalog_attributes = kVariant;
  price_info.currency_code = kCurrency;
  price_info.jackpot_url = GURL(kTestBuyingOptionsUrl);
  price_info.has_multiple_catalogs = true;

  shopping_service_->SetIsSubscribedCallbackValue(true);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
      std::move(price_info));

  // Fetch data from the model.
  price_insights_model_->FetchConfigurationForWebState(
      web_state_ptr_,
      base::BindOnce(&PriceInsightsModulatorTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  shopping_service_->SetIsSubscribedCallbackValue(true);

  PriceInsightsModulator* modulator = [[PriceInsightsModulator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
               itemConfiguration:config->weak_ptr_factory.GetWeakPtr()];

  // Start the modulator.
  [modulator start];

  UICollectionView* collection_view = [[UICollectionView alloc]
             initWithFrame:CGRectZero
      collectionViewLayout:[[UICollectionViewFlowLayout alloc] init]];
  PriceInsightsCell* cell = static_cast<PriceInsightsCell*>([collection_view
      dequeueConfiguredReusableCellWithRegistration:modulator.panelBlockData
                                                        .cellRegistration
                                       forIndexPath:[NSIndexPath
                                                        indexPathForRow:0
                                                              inSection:0]
                                               item:@"id"]);
  PriceInsightsItem* item = cell.priceInsightsItem;

  EXPECT_EQ(kTestTitle, base::SysNSStringToUTF8(item.title));
  EXPECT_EQ(kVariant, base::SysNSStringToUTF8(item.variants));
  EXPECT_EQ(kCurrency, item.currency);
  EXPECT_EQ(kCountry, item.country);
  EXPECT_EQ(2ul, [item.priceHistory count]);
  EXPECT_EQ(GURL(kTestBuyingOptionsUrl), item.buyingOptionsURL);
  EXPECT_EQ(true, item.canPriceTrack);
  EXPECT_EQ(true, item.isPriceTracked);
  EXPECT_EQ(GURL(kTestUrl), item.productURL);
  EXPECT_EQ(kClusterId, item.clusterId);
}

// Tests that PriceInsightsItem displays a valid title when
// product_cluster_title is empty.
TEST_F(PriceInsightsModulatorTest, TestPriceInsightsItemTitle) {
  base::RunLoop run_loop;

  commerce::ProductInfo info;
  info.title = kTestTitle;
  info.product_cluster_title = "";
  info.product_cluster_id = kClusterId;
  info.currency_code = kCurrency;
  info.country_code = kCountry;

  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));
  shopping_service_->SetIsSubscribedCallbackValue(true);

  // Fetch data from the model.
  price_insights_model_->FetchConfigurationForWebState(
      web_state_ptr_,
      base::BindOnce(&PriceInsightsModulatorTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));

  run_loop.Run();

  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          returned_configuration_.get());

  shopping_service_->SetIsSubscribedCallbackValue(true);

  PriceInsightsModulator* modulator = [[PriceInsightsModulator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
               itemConfiguration:config->weak_ptr_factory.GetWeakPtr()];

  // Start the modulator.
  [modulator start];

  UICollectionView* collection_view = [[UICollectionView alloc]
             initWithFrame:CGRectZero
      collectionViewLayout:[[UICollectionViewFlowLayout alloc] init]];
  PriceInsightsCell* cell = static_cast<PriceInsightsCell*>([collection_view
      dequeueConfiguredReusableCellWithRegistration:modulator.panelBlockData
                                                        .cellRegistration
                                       forIndexPath:[NSIndexPath
                                                        indexPathForRow:0
                                                              inSection:0]
                                               item:@"id"]);
  PriceInsightsItem* item = cell.priceInsightsItem;

  EXPECT_EQ(kTestTitle, base::SysNSStringToUTF8(item.title));
}
