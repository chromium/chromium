// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_model.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

class ReaderModeModelTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
  }

  // Attaches a ReaderModeTabHelper to `web_state_`.
  void AttachReaderModeTabHelper() {
    ReaderModeTabHelper::CreateForWebState(
        web_state_.get(),
        DistillerServiceFactory::GetForProfile(profile_.get()));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// NTP should return a null configuration.
TEST_F(ReaderModeModelTest, FetchConfigurationForNTP) {
  AttachReaderModeTabHelper();
  ReaderModeModel model;
  __block std::unique_ptr<ContextualPanelItemConfiguration> configuration;

  web_state_->SetContentIsHTML(true);
  web_state_->SetVisibleURL(GURL(kChromeUIAboutNewTabURL));
  model.FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(
          ^(base::OnceClosure quit_closure,
            std::unique_ptr<ContextualPanelItemConfiguration> config) {
            configuration = std::move(config);
            std::move(quit_closure).Run();
          },
          task_environment_.QuitClosure()));
  task_environment_.RunUntilQuit();
  EXPECT_EQ(configuration, nullptr);
}

// Non-HTML content should return a null configuration.
TEST_F(ReaderModeModelTest, FetchConfigurationForNonHTMLContent) {
  AttachReaderModeTabHelper();
  ReaderModeModel model;
  __block std::unique_ptr<ContextualPanelItemConfiguration> configuration;

  web_state_->SetContentIsHTML(false);
  web_state_->SetVisibleURL(GURL("https://test.org/doc.pdf"));
  model.FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(
          ^(base::OnceClosure quit_closure,
            std::unique_ptr<ContextualPanelItemConfiguration> config) {
            configuration = std::move(config);
            std::move(quit_closure).Run();
          },
          task_environment_.QuitClosure()));
  task_environment_.RunUntilQuit();
  EXPECT_EQ(configuration, nullptr);
}

// HTML content should return the expected non-null configuration.
TEST_F(ReaderModeModelTest, FetchConfigurationForHTMLContent) {
  AttachReaderModeTabHelper();
  ReaderModeModel model;
  __block std::unique_ptr<ContextualPanelItemConfiguration> configuration;

  web_state_->SetContentIsHTML(true);
  web_state_->SetVisibleURL(GURL("https://test.org/doc.html"));
  model.FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(
          ^(base::OnceClosure quit_closure,
            std::unique_ptr<ContextualPanelItemConfiguration> config) {
            configuration = std::move(config);
            std::move(quit_closure).Run();
          },
          task_environment_.QuitClosure()));
  task_environment_.RunUntilQuit();
  EXPECT_NE(configuration, nullptr);

  EXPECT_EQ(configuration->item_type, ContextualPanelItemType::ReaderModeItem);
  EXPECT_EQ(configuration->image_type,
            ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol);
  EXPECT_EQ(configuration->relevance,
            ContextualPanelItemConfiguration::high_relevance);
}

// WebState without a ReaderModeTabHelper should return a null configuration.
TEST_F(ReaderModeModelTest, FetchConfigurationWithoutReaderModeTabHelper) {
  ReaderModeModel model;
  __block std::unique_ptr<ContextualPanelItemConfiguration> configuration;

  model.FetchConfigurationForWebState(
      web_state_.get(),
      base::BindOnce(
          ^(base::OnceClosure quit_closure,
            std::unique_ptr<ContextualPanelItemConfiguration> config) {
            configuration = std::move(config);
            std::move(quit_closure).Run();
          },
          task_environment_.QuitClosure()));
  task_environment_.RunUntilQuit();
  EXPECT_EQ(configuration, nullptr);
}
