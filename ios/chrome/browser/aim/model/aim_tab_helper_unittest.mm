// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/aim_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

std::unique_ptr<KeyedService> BuildMockIOSChromeAimEligibilityService(
    ProfileIOS* profile) {
  return MockIOSChromeAimEligibilityService::CreateTestingProfileService(
      profile);
}

}  // namespace

class AimTabHelperTest : public PlatformTest {
 protected:
  AimTabHelperTest() {
    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kAimUrlNavigationFetchEnabled);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeAimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockIOSChromeAimEligibilityService));
    profile_ = std::move(builder).Build();

    web_state_.SetBrowserState(profile_.get());
    AimTabHelper::CreateForWebState(&web_state_);

    aim_eligibility_service_ = static_cast<MockIOSChromeAimEligibilityService*>(
        IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get()));

    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service_->Load();

    // Add default search provider
    TemplateURLData data;
    data.SetShortName(u"Test");
    data.SetKeyword(u"test");
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    TemplateURL* template_url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  raw_ptr<MockIOSChromeAimEligibilityService> aim_eligibility_service_;
  raw_ptr<TemplateURLService> template_url_service_;
};

// Tests that navigation to a non-HTTP/HTTPS URL does not fetch eligibility.
TEST_F(AimTabHelperTest, DidFinishNavigation_NotHttpOrHttps) {
  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("chrome://version"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);

  AimTabHelper::FromWebState(&web_state_)
      ->DidFinishNavigation(&web_state_, &context);
}

// Tests that same-document navigation does not fetch eligibility.
TEST_F(AimTabHelperTest, DidFinishNavigation_SameDocument) {
  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.google.com/search?q=test"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(true);

  AimTabHelper::FromWebState(&web_state_)
      ->DidFinishNavigation(&web_state_, &context);
}

// Tests that a navigation that hasn't committed does not fetch eligibility.
TEST_F(AimTabHelperTest, DidFinishNavigation_NotCommitted) {
  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.google.com/search?q=test"));
  context.SetHasCommitted(false);
  context.SetIsSameDocument(false);

  AimTabHelper::FromWebState(&web_state_)
      ->DidFinishNavigation(&web_state_, &context);
}

// Tests that when feature is disabled, it does not fetch eligibility.
TEST_F(AimTabHelperTest, DidFinishNavigation_FeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      omnibox::kAimUrlNavigationFetchEnabled);

  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.google.com/search?q=test"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);

  AimTabHelper::FromWebState(&web_state_)
      ->DidFinishNavigation(&web_state_, &context);
}

// Tests that when aim eligible service says not eligible, it does not fetch.
TEST_F(AimTabHelperTest, DidFinishNavigation_NotAimEligible) {
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.google.com/search?q=test"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);

  AimTabHelper::FromWebState(&web_state_)
      ->DidFinishNavigation(&web_state_, &context);
}

// Tests that when the URL is not a search URL, it does not fetch.
TEST_F(AimTabHelperTest, DidFinishNavigation_NotSearchUrl) {
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.example.com"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);

  AimTabHelper::FromWebState(&web_state_)
      ->DidFinishNavigation(&web_state_, &context);
}

// Tests that when it lacks AIM URL params, it does not fetch.
TEST_F(AimTabHelperTest, DidFinishNavigation_NoAimUrlParams) {
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*aim_eligibility_service_, HasAimUrlParams(testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*aim_eligibility_service_, FetchEligibility(testing::_)).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.google.com/search?q=test"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);

  AimTabHelper::FromWebState(&web_state_)
      ->DidFinishNavigation(&web_state_, &context);
}

// Tests that eligibility is fetched on a valid search navigation.
TEST_F(AimTabHelperTest, DidFinishNavigation_FetchesEligibility) {
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*aim_eligibility_service_, HasAimUrlParams(testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *aim_eligibility_service_,
      FetchEligibility(AimEligibilityService::RequestSource::kAimUrlNavigation))
      .Times(1);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.google.com/search?q=test"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);

  AimTabHelper::FromWebState(&web_state_)
      ->DidFinishNavigation(&web_state_, &context);
}
