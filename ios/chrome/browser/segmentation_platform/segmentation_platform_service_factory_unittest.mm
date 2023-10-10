// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/prediction_options.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segment_selection_result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/segmentation_platform/public/service_proxy.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace segmentation_platform {
namespace {

// Observer that waits for service initialization.
class WaitServiceInitializedObserver : public ServiceProxy::Observer {
 public:
  explicit WaitServiceInitializedObserver(base::OnceClosure closure)
      : closure_(std::move(closure)) {}
  void OnServiceStatusChanged(bool initialized, int status_flags) override {
    if (initialized) {
      std::move(closure_).Run();
    }
  }

 private:
  base::OnceClosure closure_;
};

}  // namespace
class SegmentationPlatformServiceFactoryTest : public PlatformTest {
 public:
  SegmentationPlatformServiceFactoryTest() {
    // TODO(b/293500507): Create a base class for testing default models.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{optimization_guide::features::kOptimizationTargetPrediction, {}},
         {features::kSegmentationPlatformFeature, {}},
         {features::kSegmentationPlatformIosModuleRanker,
          {{kDefaultModelEnabledParam, "true"}}}},
        {});
  }
  ~SegmentationPlatformServiceFactoryTest() override = default;

  void OnGetClassificationResult(base::RepeatingClosure closure,
                                 const ClassificationResult& result) {
    ASSERT_EQ(result.status, PredictionStatus::kSucceeded);
    EXPECT_FALSE(result.ordered_labels.empty());
    EXPECT_EQ(5u, result.ordered_labels.size());
    EXPECT_EQ("MostVisitedTiles", result.ordered_labels[0]);
    EXPECT_EQ("Shortcuts", result.ordered_labels[1]);
    EXPECT_EQ("SafetyCheck", result.ordered_labels[2]);
    EXPECT_EQ("TabResumption", result.ordered_labels[3]);
    EXPECT_EQ("ParcelTracking", result.ordered_labels[4]);

    std::move(closure).Run();
  }

  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        SegmentationPlatformServiceFactory::GetInstance(),
        SegmentationPlatformServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();

    service = SegmentationPlatformServiceFactory::GetForBrowserState(
        browser_state_.get());
    WaitForServiceInit();

    ChromeBrowserState* otr_browser_state =
        browser_state_->CreateOffTheRecordBrowserStateWithTestingFactories(
            {std::make_pair(
                SegmentationPlatformServiceFactory::GetInstance(),
                SegmentationPlatformServiceFactory::GetDefaultFactory())});
    ASSERT_FALSE(SegmentationPlatformServiceFactory::GetForBrowserState(
        otr_browser_state));
  }

 protected:
  void WaitForServiceInit() {
    base::RunLoop wait_for_init;
    WaitServiceInitializedObserver wait_observer(wait_for_init.QuitClosure());
    service->GetServiceProxy()->AddObserver(&wait_observer);

    wait_for_init.Run();
    while (!service->IsPlatformInitialized()) {
      base::RunLoop().RunUntilIdle();
    }

    service->GetServiceProxy()->RemoveObserver(&wait_observer);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment web_task_env_;
  SegmentationPlatformService* service;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

TEST_F(SegmentationPlatformServiceFactoryTest, Test) {
  // TODO(crbug.com/1333641): Add test for the API once the initialization is
  // fixed.
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestIosModuleRankerModel) {
  segmentation_platform::PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  int mvt_freshness_impression_count = -1;
  int shortcuts_freshness_impression_count = -1;
  int safety_check_freshness_impression_count = -1;
  int tab_resumption_freshness_impression_count = -1;
  int parcel_tracking_freshness_impression_count = -1;
  input_context->metadata_args.emplace(
      segmentation_platform::kMostVisitedTilesFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          mvt_freshness_impression_count));
  input_context->metadata_args.emplace(
      segmentation_platform::kShortcutsFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          shortcuts_freshness_impression_count));
  input_context->metadata_args.emplace(
      segmentation_platform::kSafetyCheckFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          safety_check_freshness_impression_count));
  input_context->metadata_args.emplace(
      segmentation_platform::kTabResumptionFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          tab_resumption_freshness_impression_count));
  input_context->metadata_args.emplace(
      segmentation_platform::kParcelTrackingFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          parcel_tracking_freshness_impression_count));

  base::RunLoop loop;
  service->GetClassificationResult(
      segmentation_platform::kIosModuleRankerKey, prediction_options,
      input_context,
      base::BindOnce(
          &SegmentationPlatformServiceFactoryTest::OnGetClassificationResult,
          base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

}  // namespace segmentation_platform
