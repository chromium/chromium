// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/about_this_site_tab_helper.h"

#import <string>

#import "base/command_line.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/core/optimization_guide_test_util.h"
#import "components/page_info/core/proto/about_this_site_metadata.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
const char kTestURL[] = "https://a.test/title1.html";
const char kTestURLOrigin[] = "https://a.test";
const char kDinerURL[] = "https://diner.test";
const char kDinerDescrition[] =
    "A domain used in illustrative examples in documents";

page_info::proto::AboutThisSiteMetadata CreateValidSiteInfo() {
  page_info::proto::AboutThisSiteMetadata metadata;
  page_info::proto::SiteInfo* site_info = metadata.mutable_site_info();
  auto* description = site_info->mutable_description();
  description->set_description(kDinerDescrition);
  description->set_lang("en_US");
  description->set_name("Example");
  description->mutable_source()->set_url("https://example.com");
  description->mutable_source()->set_label("Example source");
  site_info->mutable_more_about()->set_url(kDinerURL);

  return metadata;
}
}  // namespace

// Tests fixture for AboutThisSiteTabHelper class.
class AboutThisSiteTabHelperTest : public PlatformTest {
 public:
  AboutThisSiteTabHelperTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::kPurgeHintsStore);

    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kOptimizationGuideMetadataValidation},
        {});
  }

  // Initializes the OptimizationGuide service with an AboutThisSite hint,
  // `site_info`, for the given `url`. It also initializes the
  // AboutThisSiteTabHelper to be tested.
  void MockOptimizationGuideResponse(
      const page_info::proto::AboutThisSiteMetadata& site_info,
      std::string url) {
    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/optimization_guide.proto.AboutThisSiteMetadata");
    site_info.SerializeToString(any_metadata.mutable_value());
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::kHintsProtoOverride,
        optimization_guide::CreateHintsConfig(
            GURL(url), optimization_guide::proto::ABOUT_THIS_SITE,
            &any_metadata));

    InitService();

    // Wait for the hints override from CLI is picked up.
    RetryForHistogramUntilCountReached(
        &histogram_tester_, "OptimizationGuide.UpdateComponentHints.Result", 1);
  }

  // Initializes the OptimizationGuide service as well as the
  // `AboutThisSiteTabHelper` to be tested and the test browser and web state.
  void InitService() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());
    web_state_.SetBrowserState(profile_.get());

    AboutThisSiteTabHelper::CreateForWebState(&web_state_,
                                              optimization_guide_service_);
  }

  // Initializes the OptimizationGuide service as well as the
  // `AboutThisSiteTabHelper`, the test browser and web state for an off the
  // record session. Should only be called after `InitService()` so the
  // `profile_` has been initialized.
  void InitOTRService() {
    ProfileIOS* otr_profile =
        profile_->CreateOffTheRecordProfileWithTestingFactories(
            {TestProfileIOS::TestingFactory{
                OptimizationGuideServiceFactory::GetInstance(),
                OptimizationGuideServiceFactory::GetDefaultFactory()}});
    optimization_guide_service_otr_ =
        OptimizationGuideServiceFactory::GetForProfile(otr_profile);
    web_state_otr_.SetBrowserState(otr_profile);

    AboutThisSiteTabHelper::CreateForWebState(&web_state_otr_,
                                              optimization_guide_service_otr_);
  }

  void CommitToUrlAndNavigate(const GURL& url, bool is_off_the_record = false) {
    context_.SetUrl(url);
    context_.SetHasCommitted(true);

    if (is_off_the_record) {
      web_state_otr_.OnNavigationStarted(&context_);
      web_state_otr_.OnNavigationFinished(&context_);
      web_state_otr_.SetCurrentURL(url);
      web_state_otr_.SetVisibleURL(url);
      return;
    }

    web_state_.OnNavigationStarted(&context_);
    web_state_.OnNavigationFinished(&context_);
    web_state_.SetCurrentURL(url);
    web_state_.SetVisibleURL(url);
  }

  page_info::AboutThisSiteService::DecisionAndMetadata GetAboutThisSiteMetadata(
      bool is_off_the_record = false) {
    if (is_off_the_record) {
      return AboutThisSiteTabHelper::FromWebState(&web_state_otr_)
          ->GetAboutThisSiteMetadata();
    }
    return AboutThisSiteTabHelper::FromWebState(&web_state_)
        ->GetAboutThisSiteMetadata();
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void WaitForTabHelperToFetchInfo() {
    RetryForHistogramUntilCountReached(
        &histogram_tester_, "OptimizationGuide.ApplyDecision.AboutThisSite", 1);
    RunUntilIdle();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  web::FakeWebState web_state_otr_;
  web::FakeNavigationContext context_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_otr_;
};

// Tests if the TabHelper returns the correct information when the
// OptimizationGuide has valid AboutThisSite info for a especific page.
TEST_F(AboutThisSiteTabHelperTest, TestValidSiteInfo) {
  MockOptimizationGuideResponse(CreateValidSiteInfo(), kTestURL);
  CommitToUrlAndNavigate(GURL(kTestURL));
  WaitForTabHelperToFetchInfo();

  std::optional<page_info::proto::AboutThisSiteMetadata>
      about_this_site_metadata;
  optimization_guide::OptimizationGuideDecision decision;
  std::tie(decision, about_this_site_metadata) = GetAboutThisSiteMetadata();

  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue, decision);
  EXPECT_TRUE(about_this_site_metadata.has_value());
  EXPECT_EQ(kDinerURL,
            about_this_site_metadata.value().site_info().more_about().url());
  EXPECT_EQ(
      kDinerDescrition,
      about_this_site_metadata.value().site_info().description().description());
}

// Tests if the TabHelper returns the correct information when the
// OptimizationGuide has valid AboutThisSite info only for the origin of the
// page.
TEST_F(AboutThisSiteTabHelperTest, TestValidSiteInfoForOrigin) {
  MockOptimizationGuideResponse(CreateValidSiteInfo(), kTestURLOrigin);
  CommitToUrlAndNavigate(GURL(kTestURL));
  WaitForTabHelperToFetchInfo();

  std::optional<page_info::proto::AboutThisSiteMetadata>
      about_this_site_metadata;
  optimization_guide::OptimizationGuideDecision decision;
  std::tie(decision, about_this_site_metadata) = GetAboutThisSiteMetadata();

  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse, decision);
  EXPECT_FALSE(about_this_site_metadata.has_value());
}

// Tests if the TabHelper returns the correct information when the
// OptimizationGuide doesn't have a valid AboutThisSite info for the page.
TEST_F(AboutThisSiteTabHelperTest, TestNoAvailableSiteInfo) {
  InitService();
  CommitToUrlAndNavigate(GURL(kTestURL));
  WaitForTabHelperToFetchInfo();

  std::optional<page_info::proto::AboutThisSiteMetadata>
      about_this_site_metadata;
  optimization_guide::OptimizationGuideDecision decision;
  std::tie(decision, about_this_site_metadata) = GetAboutThisSiteMetadata();

  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse, decision);
  EXPECT_FALSE(about_this_site_metadata.has_value());
}

// Tests that the TabHelper returns information on a normal session but it
// doesn't in an off the record session.
TEST_F(AboutThisSiteTabHelperTest, TestValidSiteInfoInOffTheRecord) {
  MockOptimizationGuideResponse(CreateValidSiteInfo(), kTestURL);
  InitOTRService();

  // Check that in an off the record session no valid information is returned.
  CommitToUrlAndNavigate(GURL(kTestURL), /*is_off_the_record=*/true);
  WaitForTabHelperToFetchInfo();

  std::optional<page_info::proto::AboutThisSiteMetadata>
      about_this_site_metadata;
  optimization_guide::OptimizationGuideDecision decision;
  std::tie(decision, about_this_site_metadata) =
      GetAboutThisSiteMetadata(/*is_off_the_record=*/true);

  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse, decision);
  EXPECT_FALSE(about_this_site_metadata.has_value());

  // Check that in a normal session, with the same originiating browser state,
  // valid information is returned.
  CommitToUrlAndNavigate(GURL(kTestURL), /*is_off_the_record=*/false);
  WaitForTabHelperToFetchInfo();

  std::tie(decision, about_this_site_metadata) =
      GetAboutThisSiteMetadata(/*is_off_the_record=*/false);
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue, decision);
  EXPECT_TRUE(about_this_site_metadata.has_value());
}
