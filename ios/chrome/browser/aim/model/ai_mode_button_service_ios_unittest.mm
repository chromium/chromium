// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios.h"

#import <memory>

#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios_factory.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

std::unique_ptr<KeyedService> CreateMockAimEligibilityService(
    ProfileIOS* profile) {
  return MockIOSChromeAimEligibilityService::CreateTestingProfileService(
      profile);
}

class AIModeButtonServiceIOSTest : public PlatformTest {
 protected:
  AIModeButtonServiceIOSTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeAimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&CreateMockAimEligibilityService));
    profile_ = std::move(builder).Build();

    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service_->Load();

    // Default factory sets Google as DSE.
    google_turl_ = template_url_service_->GetDefaultSearchProvider();

    // Add Bing search engine (used by 3P debug config).
    TemplateURLData bing_data;
    bing_data.SetShortName(u"Bing");
    bing_data.SetKeyword(u"bing");
    bing_data.SetURL("https://www.bing.com/search?q={searchTerms}");
    bing_turl_ =
        template_url_service_->Add(std::make_unique<TemplateURL>(bing_data));

    // Add a 3P engine without AIM config.
    TemplateURLData nongoogle_data;
    nongoogle_data.SetShortName(u"NonGoogle");
    nongoogle_data.SetKeyword(u"nongoogle");
    nongoogle_data.SetURL("https://www.nongoogle.com/search?q={searchTerms}");
    nongoogle_turl_ = template_url_service_->Add(
        std::make_unique<TemplateURL>(nongoogle_data));

    aim_eligibility_service_ = static_cast<MockIOSChromeAimEligibilityService*>(
        IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get()));

    service_ = AIModeButtonServiceIOSFactory::GetForProfile(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  raw_ptr<MockIOSChromeAimEligibilityService> aim_eligibility_service_ =
      nullptr;
  raw_ptr<AIModeButtonServiceIOS> service_ = nullptr;

  raw_ptr<const TemplateURL> google_turl_ = nullptr;
  raw_ptr<TemplateURL> bing_turl_ = nullptr;
  raw_ptr<TemplateURL> nongoogle_turl_ = nullptr;
};

// Tests Google DSE button availability and properties.
TEST_F(AIModeButtonServiceIOSTest, GoogleDseProperties) {
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  EXPECT_TRUE(service_->IsButtonAvailable());
  EXPECT_NSEQ(service_->GetTitle(),
              l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM));
  EXPECT_NE(service_->GetIcon(), nil);
  EXPECT_TRUE(service_->GetUrl().is_valid());

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(false));
  EXPECT_FALSE(service_->IsButtonAvailable());
}

// Tests 3P DSE when the 3P entrypoint feature is disabled.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyDseDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kAim3pEntrypoint);

  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);

  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(testing::Return(true));
  EXPECT_FALSE(service_->IsButtonAvailable());
}

// Tests 3P DSE button availability and properties when enabled.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyDseEnabledWithDebugConfig) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});

  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);

  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(testing::Return(true));
  EXPECT_TRUE(service_->IsButtonAvailable());
  EXPECT_NSEQ(service_->GetTitle(), @"AI Mode for Bing (ĄÜÔ)");
  EXPECT_NE(service_->GetIcon(), nil);
  EXPECT_TRUE(service_->GetUrl().is_valid());

  EXPECT_CALL(*aim_eligibility_service_, IsAimAllowedByFeatureAndPolicy())
      .WillRepeatedly(testing::Return(false));
  EXPECT_FALSE(service_->IsButtonAvailable());
}

// Tests 3P DSE without AIM config.
TEST_F(AIModeButtonServiceIOSTest, ThirdPartyDseWithoutAimConfig) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});

  template_url_service_->SetUserSelectedDefaultSearchProvider(nongoogle_turl_);

  EXPECT_FALSE(service_->IsButtonAvailable());
  EXPECT_NSEQ(service_->GetTitle(),
              l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM));
}

// Tests state change notifications when DSE changes.
TEST_F(AIModeButtonServiceIOSTest, StateChangedNotificationOnDseChange) {
  base::MockRepeatingClosure state_changed_callback;
  auto subscription =
      service_->RegisterStateChangedCallback(state_changed_callback.Get());

  EXPECT_CALL(state_changed_callback, Run()).Times(testing::AtLeast(1));
  template_url_service_->SetUserSelectedDefaultSearchProvider(bing_turl_);
}

}  // namespace
