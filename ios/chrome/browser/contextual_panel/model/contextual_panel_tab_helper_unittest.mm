// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"

#import "base/memory/weak_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {

/// Fake ContextualPanelTabHelperObserver for test.
class TestContextualPanelTabHelperObserver
    : public ContextualPanelTabHelperObserver {
 public:
  void ContextualPanelHasNewData(
      ContextualPanelTabHelper* tab_helper,
      std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
          item_configurations) override {
    item_configurations_set_ = true;
    item_configurations_ = item_configurations;
    run_loop_->Quit();
  }

  void ContextualPanelTabHelperDestroyed(
      ContextualPanelTabHelper* tab_helper) override {
    tab_helper->RemoveObserver(this);
  }

  bool item_configurations_set_ = false;
  raw_ptr<base::RunLoop> run_loop_;
  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      item_configurations_;
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

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::RunLoop run_loop_;
  web::FakeWebState web_state_;
  TestContextualPanelTabHelperObserver observer_;
  std::unique_ptr<SamplePanelModel> sample_model_;
};

// Tests that the tab helper observer disconnects before the tab helper is
// destroyed. The observers emptiness DCHECK would trigger if that was not the
// case.
TEST_F(ContextualPanelTabHelperTest, TestObserverIsAlertedOnDestroyed) {
  web_state_.CloseWebState();
}

// Tests that the tab helper calls any observers when a web navigation starts.
TEST_F(ContextualPanelTabHelperTest, TestObserverIsAlertedOnNavigationStarted) {
  EXPECT_FALSE(observer_.item_configurations_set_);

  web::FakeNavigationContext context;
  web_state_.OnNavigationStarted(&context);

  run_loop_.Run();

  EXPECT_TRUE(observer_.item_configurations_set_);
  EXPECT_EQ(0u, observer_.item_configurations_.size());
}

// Tests that the tab helper queries the models and calls any observer when
// a web navigation finishes.
TEST_F(ContextualPanelTabHelperTest,
       TestObserverIsAlertedOnNavigationFinished) {
  base::HistogramTester tester;

  web::FakeNavigationContext context;
  web_state_.OnNavigationFinished(&context);

  run_loop_.Run();

  EXPECT_EQ(1u, observer_.item_configurations_.size());
  tester.ExpectUniqueSample(
      "IOS.ContextualPanel.Model.InfoBlocksWithContentCount", 1, 1);
  tester.ExpectBucketCount(
      "IOS.ContextualPanel.Model.Relevance.SamplePanelItem",
      ModelRelevanceType::High, 1);

  tester.ExpectUniqueTimeSample(
      "IOS.ContextualPanel.SamplePanelItem.ModelResponseTime", base::Seconds(0),
      1);
}
