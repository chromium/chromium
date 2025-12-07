// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_model.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

class ReaderModeModelTest : public ReaderModeTest {
 public:
  void SetUp() override {
    ReaderModeTest::SetUp();
    web_state_ = CreateWebState();
  }

  void DetachReaderModeTabHelper() {
    ReaderModeTabHelper::RemoveFromWebState(web_state());
  }

  web::FakeWebState* web_state() { return web_state_.get(); }

 private:
  std::unique_ptr<web::FakeWebState> web_state_;
};

// NTP should return a null configuration.
TEST_F(ReaderModeModelTest, FetchConfigurationForNTP) {
  ReaderModeModel model;
  __block std::unique_ptr<ContextualPanelItemConfiguration> configuration;

  web_state()->SetContentIsHTML(true);
  LoadWebpage(web_state(), GURL(kChromeUIAboutNewTabURL));
  model.FetchConfigurationForWebState(
      web_state(),
      base::BindOnce(
          ^(base::OnceClosure quit_closure,
            std::unique_ptr<ContextualPanelItemConfiguration> config) {
            configuration = std::move(config);
            std::move(quit_closure).Run();
          },
          task_environment()->QuitClosure()));
  task_environment()->RunUntilQuit();
  EXPECT_EQ(configuration, nullptr);
}

// Non-HTML content should return a null configuration.
TEST_F(ReaderModeModelTest, FetchConfigurationForNonHTMLContent) {
  ReaderModeModel model;
  __block std::unique_ptr<ContextualPanelItemConfiguration> configuration;

  web_state()->SetContentIsHTML(false);
  LoadWebpage(web_state(), GURL("https://test.org/doc.pdf"));
  model.FetchConfigurationForWebState(
      web_state(),
      base::BindOnce(
          ^(base::OnceClosure quit_closure,
            std::unique_ptr<ContextualPanelItemConfiguration> config) {
            configuration = std::move(config);
            std::move(quit_closure).Run();
          },
          task_environment()->QuitClosure()));
  task_environment()->RunUntilQuit();
  EXPECT_EQ(configuration, nullptr);
}

// HTML content should return the expected non-null configuration.
TEST_F(ReaderModeModelTest, FetchConfigurationForHTMLContent) {
  ReaderModeModel model;
  __block std::unique_ptr<ContextualPanelItemConfiguration> configuration;

  GURL test_url("https://test.org/doc.html");
  SetReaderModeState(web_state(), test_url,
                     ReaderModeHeuristicResult::kReaderModeEligible, "");
  LoadWebpage(web_state(), test_url);
  WaitForPageLoadDelayAndRunUntilIdle();

  model.FetchConfigurationForWebState(
      web_state(),
      base::BindOnce(
          ^(base::OnceClosure quit_closure,
            std::unique_ptr<ContextualPanelItemConfiguration> config) {
            configuration = std::move(config);
            std::move(quit_closure).Run();
          },
          task_environment()->QuitClosure()));
  task_environment()->RunUntilQuit();
  EXPECT_NE(configuration, nullptr);

  EXPECT_EQ(configuration->item_type, ContextualPanelItemType::ReaderModeItem);
  EXPECT_EQ(configuration->image_type,
            ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol);
  EXPECT_EQ(configuration->relevance,
            ContextualPanelItemConfiguration::low_relevance - 1);
  EXPECT_TRUE(configuration->entrypoint_message_large_entrypoint_always_shown);
  EXPECT_TRUE(configuration->entrypoint_custom_action);
}

// WebState without a ReaderModeTabHelper should return a null configuration.
TEST_F(ReaderModeModelTest, FetchConfigurationWithoutReaderModeTabHelper) {
  DetachReaderModeTabHelper();
  ReaderModeModel model;
  __block std::unique_ptr<ContextualPanelItemConfiguration> configuration;

  model.FetchConfigurationForWebState(
      web_state(),
      base::BindOnce(
          ^(base::OnceClosure quit_closure,
            std::unique_ptr<ContextualPanelItemConfiguration> config) {
            configuration = std::move(config);
            std::move(quit_closure).Run();
          },
          task_environment()->QuitClosure()));
  task_environment()->RunUntilQuit();
  EXPECT_EQ(configuration, nullptr);
}
