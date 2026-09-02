// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/public/lens_overlay_availability_utils.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/lens/lens_overlay_permission_utils.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/public/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/public/features.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

const char kExampleUrl[] = "https://example.com";
const char kGoogleSearchUrl[] = "https://www.google.com/search?q=test";
const char kGoogleHomePageUrl[] = "https://www.google.com";
const char kLensMWebResultUrl[] =
    "https://www.google.com/search?q=test&vsrid=12345";

class LensOverlayAvailabilityUtilsTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    profile_->GetPrefs()->SetInteger(
        lens::prefs::kLensOverlaySettings,
        static_cast<int>(
            lens::prefs::LensOverlaySettingsPolicyValue::kEnabled));

    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());

    web_state_ = CreateWebStateWithURL(GURL(kExampleUrl));
  }

  void TearDown() override {
    web_state_ = nullptr;
    template_url_service_ = nullptr;
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  std::unique_ptr<web::FakeWebState> CreateWebStateWithURL(const GURL& url) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetCurrentURL(url);
    LensOverlayTabHelper::CreateForWebState(web_state.get());
    NewTabPageTabHelper::CreateForWebState(web_state.get());
    return web_state;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that LocationBar entrypoint is available on a regular webpage.
TEST_F(LensOverlayAvailabilityUtilsTest,
       IsLensOverlayEntrypointAvailable_LocationBar_AvailableOnRegularPage) {
  EXPECT_TRUE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kLocationBar, profile_->GetPrefs(),
      template_url_service_, web_state_.get()));
}

// Tests that LocationBar entrypoint is unavailable in incognito.
TEST_F(LensOverlayAvailabilityUtilsTest,
       IsLensOverlayEntrypointAvailable_LocationBar_UnavailableInIncognito) {
  ProfileIOS* incognito_profile = profile_->GetOffTheRecordProfile();
  auto incognito_web_state = std::make_unique<web::FakeWebState>();
  incognito_web_state->SetBrowserState(incognito_profile);
  incognito_web_state->SetCurrentURL(GURL(kExampleUrl));

  EXPECT_FALSE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kLocationBar, incognito_profile->GetPrefs(),
      template_url_service_, incognito_web_state.get()));
}

// Tests that LocationBar entrypoint is unavailable on the NTP.
TEST_F(LensOverlayAvailabilityUtilsTest,
       IsLensOverlayEntrypointAvailable_LocationBar_UnavailableOnNTP) {
  auto ntp_web_state = CreateWebStateWithURL(GURL(kChromeUINewTabURL));
  EXPECT_FALSE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kLocationBar, profile_->GetPrefs(),
      template_url_service_, ntp_web_state.get()));
}

// Tests that LocationBar entrypoint is unavailable on Google Search pages.
TEST_F(LensOverlayAvailabilityUtilsTest,
       IsLensOverlayEntrypointAvailable_LocationBar_UnavailableOnGoogleSearch) {
  web_state_->SetCurrentURL(GURL(kGoogleSearchUrl));
  EXPECT_FALSE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kLocationBar, profile_->GetPrefs(),
      template_url_service_, web_state_.get()));
}

// Tests that LocationBar entrypoint is unavailable on Google Homepage.
TEST_F(
    LensOverlayAvailabilityUtilsTest,
    IsLensOverlayEntrypointAvailable_LocationBar_UnavailableOnGoogleHomePage) {
  web_state_->SetCurrentURL(GURL(kGoogleHomePageUrl));
  EXPECT_FALSE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kLocationBar, profile_->GetPrefs(),
      template_url_service_, web_state_.get()));
}

// Tests that LocationBar entrypoint is unavailable on Lens mWeb result pages.
TEST_F(
    LensOverlayAvailabilityUtilsTest,
    IsLensOverlayEntrypointAvailable_LocationBar_UnavailableOnLensMWebResult) {
  web_state_->SetCurrentURL(GURL(kLensMWebResultUrl));
  EXPECT_FALSE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kLocationBar, profile_->GetPrefs(),
      template_url_service_, web_state_.get()));
}

// Tests that LocationBar entrypoint is unavailable when policy is disabled.
TEST_F(
    LensOverlayAvailabilityUtilsTest,
    IsLensOverlayEntrypointAvailable_LocationBar_UnavailableWhenPolicyDisabled) {
  profile_->GetPrefs()->SetInteger(
      lens::prefs::kLensOverlaySettings,
      static_cast<int>(lens::prefs::LensOverlaySettingsPolicyValue::kDisabled));
  EXPECT_FALSE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kLocationBar, profile_->GetPrefs(),
      template_url_service_, web_state_.get()));
}

// Tests that entrypoints are unavailable when web_state is null.
TEST_F(LensOverlayAvailabilityUtilsTest,
       IsLensOverlayEntrypointAvailable_UnavailableWhenWebStateIsNull) {
  EXPECT_FALSE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kLocationBar, profile_->GetPrefs(),
      template_url_service_, nullptr));
  EXPECT_FALSE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kOverflowMenu, profile_->GetPrefs(),
      template_url_service_, nullptr));
}

// Tests that OverflowMenu entrypoint is available on a regular page.
TEST_F(LensOverlayAvailabilityUtilsTest,
       IsLensOverlayEntrypointAvailable_OverflowMenu_AvailableOnRegularPage) {
  EXPECT_TRUE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kOverflowMenu, profile_->GetPrefs(),
      template_url_service_, web_state_.get()));
}

// Tests that OverflowMenu entrypoint is unavailable on the NTP.
TEST_F(LensOverlayAvailabilityUtilsTest,
       IsLensOverlayEntrypointAvailable_OverflowMenu_UnavailableOnNTP) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kOverflowMenuNTPRefactor, kChromeNextIa}, {});
  auto ntp_web_state = CreateWebStateWithURL(GURL(kChromeUINewTabURL));
  EXPECT_FALSE(IsLensOverlayEntrypointAvailable(
      LensOverlayEntrypoint::kOverflowMenu, profile_->GetPrefs(),
      template_url_service_, ntp_web_state.get()));
}

}  // namespace
