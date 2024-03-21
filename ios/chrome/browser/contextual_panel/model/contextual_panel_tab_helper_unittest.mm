// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/contextual_panel/model/sample/sample_panel_model.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {

/// Fake ContextualPanelTabHelperObserver for test.
class TestContextualPanelTabHelperObserver
    : public ContextualPanelTabHelperObserver {
 public:
  void ContextualPanelHasNewData(ContextualPanelTabHelper* tab_helper,
                                 std::vector<ContextualPanelItemConfiguration>
                                     item_configurations) override {
    item_configurations_ = item_configurations;
    run_loop_->Quit();
  }

  raw_ptr<base::RunLoop> run_loop_;
  std::vector<ContextualPanelItemConfiguration> item_configurations_;
};

}  // namespace

// Unittests related to the ContextualPanelTabHelper.
class ContextualPanelTabHelperTest : public PlatformTest {
 public:
  ContextualPanelTabHelperTest() {}
  ~ContextualPanelTabHelperTest() override {}

  void SetUp() override {
    observer_.run_loop_ = &run_loop_;
    sample_model_ = std::make_unique<SamplePanelModel>();

    std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models;
    models.emplace(ContextualPanelItemType::SamplePanelItem,
                   sample_model_.get());
    ContextualPanelTabHelper::CreateForWebState(&web_state_, models);

    ContextualPanelTabHelper()->AddObserver(&observer_);
  }

  void TearDown() override {
    ContextualPanelTabHelper()->RemoveObserver(&observer_);
  }

  ContextualPanelTabHelper* ContextualPanelTabHelper() {
    return ContextualPanelTabHelper::FromWebState(&web_state_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  base::RunLoop run_loop_;
  web::FakeWebState web_state_;
  TestContextualPanelTabHelperObserver observer_;
  std::unique_ptr<SamplePanelModel> sample_model_;
};

// Tests that the tab helper queries the models and calls any observer when
// a web navigation finishes.
TEST_F(ContextualPanelTabHelperTest, TestObserverIsAlerted) {
  web::FakeNavigationContext context;
  web_state_.OnNavigationFinished(&context);

  run_loop_.Run();

  EXPECT_EQ(1u, observer_.item_configurations_.size());
}
