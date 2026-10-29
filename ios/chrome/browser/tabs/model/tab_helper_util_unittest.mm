// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_helper_util.h"

#import "base/test/scoped_feature_list.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/enterprise/net/core/features.h"
#import "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/features.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_challenge_tab_helper.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_error_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/itunes_urls/model/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/lens/model/lens_tab_helper.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {

// Builds the `EnterpriseProxyService` that `EnterpriseProxyErrorService`
// requires.
std::unique_ptr<KeyedService> BuildMockEnterpriseProxyService(ProfileIOS*) {
  return std::make_unique<
      testing::NiceMock<enterprise_net::MockEnterpriseProxyService>>();
}

}  // namespace

// Test fixture for AttachTabHelpers.
class TabHelperUtilTest : public PlatformTest,
                          public testing::WithParamInterface<TabHelperFilter> {
 public:
  // Returns the string version of the TabHelperFilter test parameter.
  static std::string TabHelperFilterToString(
      testing::TestParamInfo<TabHelperFilter> info) {
    switch (info.param) {
      case TabHelperFilter::kEmpty:
        return "Empty";
      case TabHelperFilter::kPrerender:
        return "Prerender";
      case TabHelperFilter::kReaderMode:
        return "ReaderMode";
      case TabHelperFilter::kLensOverlay:
        return "LensOverlay";
      case TabHelperFilter::kAssistantAim:
        return "AssistantAim";
      case TabHelperFilter::kGeminiWebModal:
        return "GeminiWebModal";
    }
  }

 protected:
  TabHelperUtilTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();

    web_state_.SetBrowserState(profile_.get());
    web_state_.SetWebFramesManager(
        web::ContentWorld::kPageContentWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state_.SetWebFramesManager(
        web::ContentWorld::kIsolatedWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
  }

  TabHelperFilter tab_helper_filter() { return GetParam(); }

  // Replaces `profile_` with a profile that has the enterprise proxy services,
  // and points `web_state_` to it. Dynamic route fetching must be enabled
  // first, as the services are only built while it is.
  void UseProfileWithEnterpriseProxyServices() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(EnterpriseProxyServiceFactoryIOS::GetInstance(),
                              base::BindOnce(&BuildMockEnterpriseProxyService));
    builder.AddTestingFactory(
        EnterpriseProxyErrorServiceFactoryIOS::GetInstance(),
        EnterpriseProxyErrorServiceFactoryIOS::GetDefaultFactory());
    std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
    web_state_.SetBrowserState(profile.get());
    profile_ = std::move(profile);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

// Validate that OverlayRequestQueue is created for all web states.
// This test case demonstrates the default tab helper creation state,
// which is to create and attach itself to all web states.
TEST_P(TabHelperUtilTest, OverlayRequestQueue) {
  AttachTabHelpers(&web_state_, tab_helper_filter());

  EXPECT_TRUE(OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea));
}

// Validate that LensTabHelper is created when there is no filter.
// This test case demonstrates the creation of a tab helper when one
// or more filters result in the suppression of the tab helper.
TEST_P(TabHelperUtilTest, LensTabHelper) {
  AttachTabHelpers(&web_state_, tab_helper_filter());

  switch (tab_helper_filter()) {
    case TabHelperFilter::kEmpty:
      ASSERT_TRUE(LensTabHelper::FromWebState(&web_state_));
      break;
    case TabHelperFilter::kPrerender:
    case TabHelperFilter::kReaderMode:
    case TabHelperFilter::kLensOverlay:
    case TabHelperFilter::kAssistantAim:
    case TabHelperFilter::kGeminiWebModal:
      ASSERT_FALSE(LensTabHelper::FromWebState(&web_state_));
      break;
  }
}

// Validate that CobrowseTabHelper is created when there is no filter, but
// suppressed for Assistant AIM filter.
TEST_P(TabHelperUtilTest, CobrowseTabHelper) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAimCobrowse);

  AttachTabHelpers(&web_state_, tab_helper_filter());

  switch (tab_helper_filter()) {
    case TabHelperFilter::kEmpty:
      ASSERT_TRUE(CobrowseTabHelper::FromWebState(&web_state_));
      break;
    case TabHelperFilter::kPrerender:
    case TabHelperFilter::kReaderMode:
    case TabHelperFilter::kLensOverlay:
    case TabHelperFilter::kAssistantAim:
    case TabHelperFilter::kGeminiWebModal:
      ASSERT_FALSE(CobrowseTabHelper::FromWebState(&web_state_));
      break;
  }
}

// Validate that DeviceTrustChallengeTabHelper is created for WebStates that
// serve a standard navigation and suppressed for the embedded WebStates used
// by other features.
TEST_P(TabHelperUtilTest, DeviceTrustChallengeTabHelper) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::features::kEnableIOSDeviceTrustConnector);

  AttachTabHelpers(&web_state_, tab_helper_filter());

  switch (tab_helper_filter()) {
    case TabHelperFilter::kEmpty:
    case TabHelperFilter::kPrerender:
      ASSERT_TRUE(DeviceTrustChallengeTabHelper::FromWebState(&web_state_));
      break;
    case TabHelperFilter::kReaderMode:
    case TabHelperFilter::kLensOverlay:
    case TabHelperFilter::kAssistantAim:
    case TabHelperFilter::kGeminiWebModal:
      ASSERT_FALSE(DeviceTrustChallengeTabHelper::FromWebState(&web_state_));
      break;
  }
}

// Validate that InfobarBadgeTabHelper is created for tab helper setup with
// standard navigation filter, which does not include special web states for
// Reader Mode or Lens features.
// This is a regression test for crbug.com/465908300.
TEST_P(TabHelperUtilTest, InfobarBadgeTabHelper) {
  AttachTabHelpers(&web_state_, tab_helper_filter());

  InfobarBadgeTabHelper* tab_helper = static_cast<InfobarBadgeTabHelper*>(
      web_state_.GetUserData(InfobarBadgeTabHelper::UserDataKey()));

  switch (tab_helper_filter()) {
    case TabHelperFilter::kEmpty:
    case TabHelperFilter::kPrerender: {
      ASSERT_TRUE(tab_helper);
    } break;
    case TabHelperFilter::kReaderMode:
    case TabHelperFilter::kLensOverlay:
    case TabHelperFilter::kAssistantAim:
    case TabHelperFilter::kGeminiWebModal: {
      ASSERT_FALSE(tab_helper);
    } break;
  }
}

// Tests that `ITunesUrlsHandlerTabHelper` is not created for Prerender, Lens
// Overlay, or Gemini Web Modal WebStates.
TEST_P(TabHelperUtilTest, ITunesUrlsHandlerTabHelper) {
  AttachTabHelpers(&web_state_, tab_helper_filter());

  switch (tab_helper_filter()) {
    case TabHelperFilter::kEmpty:
    case TabHelperFilter::kReaderMode:
    case TabHelperFilter::kAssistantAim:
      EXPECT_TRUE(ITunesUrlsHandlerTabHelper::FromWebState(&web_state_));
      break;
    case TabHelperFilter::kPrerender:
    case TabHelperFilter::kLensOverlay:
    case TabHelperFilter::kGeminiWebModal:
      EXPECT_FALSE(ITunesUrlsHandlerTabHelper::FromWebState(&web_state_));
      break;
  }
}

// Validate that SaasUsageTabHelper is created for WebStates that serve a
// standard navigation and suppressed for embedded WebStates.
TEST_P(TabHelperUtilTest, SaasUsageTabHelper) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_reporting::kSaasUsageReporting);

  AttachTabHelpers(&web_state_, tab_helper_filter());

  switch (tab_helper_filter()) {
    case TabHelperFilter::kEmpty:
    case TabHelperFilter::kPrerender:
      ASSERT_TRUE(
          enterprise_reporting::SaasUsageTabHelper::FromWebState(&web_state_));
      break;
    case TabHelperFilter::kReaderMode:
    case TabHelperFilter::kLensOverlay:
    case TabHelperFilter::kAssistantAim:
    case TabHelperFilter::kGeminiWebModal:
      ASSERT_FALSE(
          enterprise_reporting::SaasUsageTabHelper::FromWebState(&web_state_));
      break;
  }
}

// Tests that `EnterpriseProxyTabHelper` is not created without its service,
// which testing profiles don't have unless a testing factory is set, even with
// dynamic route fetching enabled.
TEST_P(TabHelperUtilTest, EnterpriseProxyTabHelperWithoutService) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_net::kEnableDynamicRouteFetching);

  AttachTabHelpers(&web_state_, tab_helper_filter());

  EXPECT_FALSE(EnterpriseProxyTabHelper::FromWebState(&web_state_));
}

// Tests that `EnterpriseProxyTabHelper` is created when its service exists,
// except for prerendering, which is cancelled on auth challenges.
TEST_P(TabHelperUtilTest, EnterpriseProxyTabHelperWithService) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_net::kEnableDynamicRouteFetching);
  UseProfileWithEnterpriseProxyServices();

  AttachTabHelpers(&web_state_, tab_helper_filter());

  switch (tab_helper_filter()) {
    case TabHelperFilter::kEmpty:
    case TabHelperFilter::kLensOverlay:
    case TabHelperFilter::kReaderMode:
    case TabHelperFilter::kAssistantAim:
    case TabHelperFilter::kGeminiWebModal:
      EXPECT_TRUE(EnterpriseProxyTabHelper::FromWebState(&web_state_));
      break;
    case TabHelperFilter::kPrerender:
      EXPECT_FALSE(EnterpriseProxyTabHelper::FromWebState(&web_state_));
      break;
  }
}

// Tests that `EnterpriseProxyTabHelper` is not created in Incognito, even when
// the original profile has its service.
TEST_P(TabHelperUtilTest, EnterpriseProxyTabHelperOffTheRecord) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_net::kEnableDynamicRouteFetching);
  UseProfileWithEnterpriseProxyServices();
  web_state_.SetBrowserState(
      profile_->CreateOffTheRecordProfileWithTestingFactories());

  AttachTabHelpers(&web_state_, tab_helper_filter());

  EXPECT_FALSE(EnterpriseProxyTabHelper::FromWebState(&web_state_));
}

INSTANTIATE_TEST_SUITE_P(,
                         TabHelperUtilTest,
                         testing::Values(TabHelperFilter::kEmpty,
                                         TabHelperFilter::kPrerender,
                                         TabHelperFilter::kLensOverlay,
                                         TabHelperFilter::kReaderMode,
                                         TabHelperFilter::kAssistantAim,
                                         TabHelperFilter::kGeminiWebModal),
                         TabHelperUtilTest::TabHelperFilterToString);
