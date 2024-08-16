// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model.h"

#import "base/test/task_environment.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_item_configuration.h"
#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// Unittests related to the SamplePanelModel.
class SamplePanelModelTest : public PlatformTest {
 public:
  SamplePanelModelTest() {}
  ~SamplePanelModelTest() override {}

  void SetUp() override {
    sample_panel_model_ = std::make_unique<SamplePanelModel>();
  }

  void FetchConfigurationCallback(
      std::unique_ptr<ContextualPanelItemConfiguration> configuration) {
    returned_configuration_ = std::move(configuration);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<SamplePanelModel> sample_panel_model_;
  std::unique_ptr<ContextualPanelItemConfiguration> returned_configuration_;
};

// Tests that fetching the configuration for the sample panel model returns.
TEST_F(SamplePanelModelTest, TestFetchConfiguration) {
  base::RunLoop run_loop;
  web::FakeWebState web_state;

  sample_panel_model_->FetchConfigurationForWebState(
      &web_state,
      base::BindOnce(&SamplePanelModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(returned_configuration_);
  SamplePanelItemConfiguration* config =
      static_cast<SamplePanelItemConfiguration*>(returned_configuration_.get());
  EXPECT_EQ("sample_config", config->sample_name);
  EXPECT_EQ("Large entry point", config->entrypoint_message);
  EXPECT_EQ("chrome_product", config->entrypoint_image_name);
  EXPECT_EQ("Large entry point", config->accessibility_label);
  EXPECT_EQ("Sample bubble", config->iph_title);
  EXPECT_EQ("Sample rich in-product help for the Contextual Panel, which "
            "should appear multiple times a day.",
            config->iph_text);
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  EXPECT_EQ("chrome_search_engine_choice_icon", config->iph_image_name);
#else
  EXPECT_EQ("chromium_search_engine_choice_icon", config->iph_image_name);
#endif
  EXPECT_EQ("ios_contextual_panel_sample_model_entrypoint_used",
            config->iph_entrypoint_used_event_name);
  EXPECT_EQ("ios_contextual_panel_sample_model_entrypoint_explicitly_dismissed",
            config->iph_entrypoint_explicitly_dismissed);
  EXPECT_EQ(&feature_engagement::kIPHiOSContextualPanelSampleModelFeature,
            config->iph_feature);
  EXPECT_EQ(ContextualPanelItemConfiguration::high_relevance,
            config->relevance);
  EXPECT_EQ(ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol,
            config->image_type);
}

// Tests that fetching the configuration for the sample panel model on the NTP
// returns nothing.
TEST_F(SamplePanelModelTest, TestFetchConfigurationEmptyForNTP) {
  base::RunLoop run_loop;
  web::FakeWebState web_state;
  web_state.SetVisibleURL(GURL("chrome://newtab/"));

  sample_panel_model_->FetchConfigurationForWebState(
      &web_state,
      base::BindOnce(&SamplePanelModelTest::FetchConfigurationCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_FALSE(returned_configuration_);
  SamplePanelItemConfiguration* config =
      static_cast<SamplePanelItemConfiguration*>(returned_configuration_.get());
  EXPECT_EQ(nullptr, config);
}
