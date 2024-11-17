// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_validation_tab_helper.h"

#import "base/command_line.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/core/optimization_guide_test_util.h"
#import "components/optimization_guide/proto/string_value.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
constexpr char kHintsHost[] = "hints.com";
constexpr char kHintsURL[] = "https://hints.com/with_hints.html";
}

class OptimizationGuideValidationTabHelperTest : public PlatformTest {
 public:
  OptimizationGuideValidationTabHelperTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::kPurgeHintsStore);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::kDebugLoggingEnabled);
  }

  void SetUpMetadataFetchValidation(const std::string& metadata) {
    optimization_guide::proto::Any any_metadata;
    optimization_guide::proto::StringValue string_value;
    string_value.set_value(metadata);
    any_metadata.set_type_url(
        "type.googleapis.com/optimization_guide.proto.StringValue");
    string_value.SerializeToString(any_metadata.mutable_value());
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::kHintsProtoOverride,
        optimization_guide::CreateHintsConfig(
            GURL(kHintsURL),
            optimization_guide::proto::METADATA_FETCH_VALIDATION,
            &any_metadata));

    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kOptimizationGuideMetadataValidation},
        {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());

    web_state_.SetBrowserState(profile_.get());

    OptimizationGuideValidationTabHelper::CreateForWebState(&web_state_);

    // Wait for the hints override from CLI is picked up.
    RetryForHistogramUntilCountReached(
        &histogram_tester_, "OptimizationGuide.UpdateComponentHints.Result", 1);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;
  web::FakeWebState web_state_;
};

TEST_F(OptimizationGuideValidationTabHelperTest,
       TestValidMetadataFetchHostKeyed) {
  SetUpMetadataFetchValidation(kHintsHost);

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kHintsURL));
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);
  web_state_.OnNavigationFinished(&context);
  RetryForHistogramUntilCountReached(
      &histogram_tester_, "OptimizationGuide.MetadataFetchValidation.Result",
      1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.MetadataFetchValidation.Result", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.MetadataFetchValidation",
      optimization_guide::OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(OptimizationGuideValidationTabHelperTest,
       TestValidMetadataFetchURLKeyed) {
  SetUpMetadataFetchValidation(kHintsURL);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationGuideMetadataValidation,
      {{"is_host_keyed", "false"}});

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kHintsURL));
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);
  web_state_.OnNavigationFinished(&context);
  RetryForHistogramUntilCountReached(
      &histogram_tester_, "OptimizationGuide.MetadataFetchValidation.Result",
      1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.MetadataFetchValidation.Result", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.MetadataFetchValidation",
      optimization_guide::OptimizationTypeDecision::kAllowedByHint, 1);
}

TEST_F(OptimizationGuideValidationTabHelperTest, TestInvalidMetadataFetch) {
  SetUpMetadataFetchValidation("invalid");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kHintsURL));
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);
  web_state_.OnNavigationFinished(&context);
  RetryForHistogramUntilCountReached(
      &histogram_tester_, "OptimizationGuide.MetadataFetchValidation.Result",
      1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.MetadataFetchValidation.Result", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.MetadataFetchValidation",
      optimization_guide::OptimizationTypeDecision::kAllowedByHint, 1);
}
