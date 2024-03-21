// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/sample/sample_panel_model.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "testing/platform_test.h"

// Unittests related to the SamplePanelModel.
class SamplePanelModelTest : public PlatformTest {
 public:
  SamplePanelModelTest() {}
  ~SamplePanelModelTest() override {}

  void SetUp() override {
    sample_panel_model_ = std::make_unique<SamplePanelModel>();
  }

  void FetchConfigurationCallback(
      std::optional<ContextualPanelItemConfiguration> configuration) {
    returned_configuration_ = std::move(configuration);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<SamplePanelModel> sample_panel_model_;
  std::optional<ContextualPanelItemConfiguration> returned_configuration_;
};

// Tests that fetching the configuration for the sample panel model returns.
TEST_F(SamplePanelModelTest, TestFetchConfiguration) {
  base::RunLoop run_loop;

  sample_panel_model_->FetchConfigurationForWebState(
      nullptr, base::BindOnce(&SamplePanelModelTest::FetchConfigurationCallback,
                              base::Unretained(this))
                   .Then(run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(returned_configuration_);
  EXPECT_EQ(ContextualPanelItemConfiguration::high_relevance,
            returned_configuration_->relevance);
  EXPECT_EQ("book.pages", returned_configuration_->entrypoint_image_name);
  EXPECT_EQ(ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol,
            returned_configuration_->image_type);
}
