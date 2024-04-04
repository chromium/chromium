// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_model.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "testing/platform_test.h"

// Unittests related to the PriceInsightsModel.
class PriceInsightsModelTest : public PlatformTest {
 public:
  PriceInsightsModelTest() {}
  ~PriceInsightsModelTest() override {}

  void SetUp() override {
    price_insights_model_ = std::make_unique<PriceInsightsModel>();
  }

  void FetchConfigurationCallback(
      std::unique_ptr<ContextualPanelItemConfiguration> configuration) {
    returned_configuration_ = std::move(configuration);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<PriceInsightsModel> price_insights_model_;
  std::unique_ptr<ContextualPanelItemConfiguration> returned_configuration_;
};

// Tests that fetching the configuration for the price insights model returns.
TEST_F(PriceInsightsModelTest, TestFetchConfiguration) {
  base::RunLoop run_loop;

  price_insights_model_->FetchConfigurationForWebState(
      nullptr,
      base::BindOnce(&PriceInsightsModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(nullptr, returned_configuration_);
}
