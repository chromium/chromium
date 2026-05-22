// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/model/mini_map_tab_helper.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/providers/mini_map/test_mini_map.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// A link to Google SRP.
NSString* const kGoogleSRPPage = @"https://www.google.com/search?q=foo";

// A UL to Maps.
NSString* const kMapsLink = @"https://maps.google.com/maps/foo";

// A valid query parameter to mark URL valid for
// MiniMapTabHelperTestMiniMapControllerFactory.
NSString* const kValidQuery = @"valid=true";

// A webstate that records its last open url.
class TestFakeWebState : public web::FakeWebState {
 public:
  void OpenURL(const web::WebState::OpenURLParams& params) override {
    last_open_url_params_ =
        std::make_unique<web::WebState::OpenURLParams>(params);
  }
  web::WebState::OpenURLParams* last_open_url_params() {
    return last_open_url_params_.get();
  }

 private:
  std::unique_ptr<web::WebState::OpenURLParams> last_open_url_params_;
};

}  // namespace

// A Mini map factory that filters out some handled URLs based on their queries.
@interface MiniMapTabHelperTestMiniMapControllerFactory
    : NSObject <MiniMapControllerFactory>
@end

@implementation MiniMapTabHelperTestMiniMapControllerFactory

- (id<MiniMapController>)createMiniMapController {
  return nil;
}

- (BOOL)canHandleURL:(NSURL*)url {
  return [url.query containsString:kValidQuery];
}

@end

class MiniMapTabHelperTest : public PlatformTest {
 public:
  MiniMapTabHelperTest() : application_(OCMClassMock([UIApplication class])) {}

  void SetUp() override {
    PlatformTest::SetUp();
    feature_list_.InitAndEnableFeature(kIOSMiniMapUniversalLink);

    factory_ = [[MiniMapTabHelperTestMiniMapControllerFactory alloc] init];
    ios::provider::test::SetMiniMapControllerFactory(factory_);

    MiniMapServiceFactory::GetInstance();
    TestProfileIOS::Builder test_profile_builder;

    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());

    profile_ = std::move(test_profile_builder).Build();

    web::WebState::CreateParams params(profile_.get());

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetBrowserState(profile_.get());
    web_state_.WasShown();

    MiniMapTabHelper::CreateForWebState(&web_state_);
    tab_helper_ = MiniMapTabHelper::FromWebState(&web_state_);
    OCMStub([application_ sharedApplication]).andReturn(application_);
    OCMStub([application_ canOpenURL:GetGoogleMapsAppURL()])
        .andDo(^(NSInvocation* invocation) {
          [invocation setReturnValue:&google_maps_installed_];
        });

    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service_->Load();
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout, ^{
          return template_url_service_->loaded();
        }));
    mini_map_commands_handler_ =
        OCMStrictProtocolMock(@protocol(MiniMapCommands));
    tab_helper_->SetMiniMapCommands(mini_map_commands_handler_);
  }

  void TearDown() override {
    [application_ stopMocking];
    ios::provider::test::SetMiniMapControllerFactory(nil);
    PlatformTest::TearDown();
  }

  [[nodiscard]] bool TestShouldAllowRequest(NSString* web_state_url_string,
                                            NSString* url_string,
                                            bool feature_enabled,
                                            bool google_maps_installed,
                                            ui::PageTransition transition_type,
                                            bool simulate_success = true) {
    NSURL* web_state_url = [NSURL URLWithString:web_state_url_string];
    NSURL* url = [NSURL URLWithString:url_string];
    web_state_.SetCurrentURL(net::GURLWithNSURL(web_state_url));
    web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
    profile_->GetSyncablePrefs()->SetBoolean(prefs::kIosMiniMapShowNativeMap,
                                             feature_enabled);

    google_maps_installed_ = google_maps_installed;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:UIApplicationDidBecomeActiveNotification
                      object:nil];

    const web::WebStatePolicyDecider::RequestInfo request_info(
        transition_type, /*target_frame_is_main*/ true,
        /*target_frame_is_cross_origin*/ false,
        /*target_window_is_cross_origin*/ false, /*user_initiated*/ true,
        /*user_tapped_recently*/ true);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    base::RunLoop run_loop;
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        }).Then(run_loop.QuitClosure());
    tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                    request_info, std::move(callback));

    if (!callback_called) {
      // The request was intercepted and the callback deferred.
      if (simulate_success) {
        tab_helper_->OnMiniMapSuccess();
      } else {
        tab_helper_->OnMiniMapFailure();
      }
    }

    run_loop.Run();
    EXPECT_TRUE(callback_called);
    return policy_decision.ShouldAllowNavigation();
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  MiniMapTabHelperTestMiniMapControllerFactory* factory_;
  std::unique_ptr<TestProfileIOS> profile_;
  id application_;
  TestFakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
  raw_ptr<MiniMapTabHelper> tab_helper_ = nullptr;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  id mini_map_commands_handler_;
  bool google_maps_installed_;
};

// Test all the combinations of conditions for ShouldAllowRequest.
TEST_F(MiniMapTabHelperTest, TestNavigations) {
  const int google_srp_index = 1 << 0;
  const int link_to_maps_index = 1 << 1;
  const int feature_enabled_index = 1 << 2;
  const int google_maps_not_installed_index = 1 << 3;
  const int transition_type_index = 1 << 4;
  const int handled_url_index = 1 << 5;
  const int total = 1 << 6;

  for (int scenario = 0; scenario < total; scenario++) {
    NSString* web_state_url =
        (scenario & google_srp_index) ? kGoogleSRPPage : kMapsLink;
    NSString* url =
        (scenario & link_to_maps_index) ? kMapsLink : kGoogleSRPPage;
    bool feature_enabled = scenario & feature_enabled_index;
    bool google_maps_not_installed = scenario & google_maps_not_installed_index;
    if (scenario & handled_url_index) {
      url = [url stringByAppendingFormat:@"?%@", kValidQuery];
    }
    ui::PageTransition transition_type =
        (scenario & transition_type_index)
            ? ui::PageTransition::PAGE_TRANSITION_LINK
            : ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK;
    if (scenario == total - 1) {
      NSString* expected_url_string =
          [url stringByAppendingString:@"&utm_campaign=as-npt-bling"];
      OCMExpect([mini_map_commands_handler_
          presentMiniMapNativePreviewForURL:
              [NSURL URLWithString:expected_url_string]]);
    }
    bool res =
        TestShouldAllowRequest(web_state_url, url, feature_enabled,
                               !google_maps_not_installed, transition_type);
    EXPECT_OCMOCK_VERIFY(mini_map_commands_handler_);
    // Navigation should be blocked if all conditions are true, regardless of
    // whether the mini map feature is enabled in the treatment arm.
    bool expected_allow = (scenario | feature_enabled_index) != total - 1;
    EXPECT_EQ(expected_allow, res);
  }
}

// Test that when the feature is disabled in the treatment arm, the URL is
// modified and opened with utm_campaign=as-npt-bling.
TEST_F(MiniMapTabHelperTest, TestTreatmentLoggingWhenDisabled) {
  NSString* const kGoogleMapsLink =
      @"https://www.google.com/maps/foo?valid=true";

  bool res = TestShouldAllowRequest(kGoogleSRPPage, kGoogleMapsLink,
                                    /*feature_enabled=*/false,
                                    /*google_maps_installed=*/false,
                                    ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigation should be blocked (returns false).
  EXPECT_FALSE(res);

  // Check that a new URL was opened with the utm_campaign parameter.
  web::WebState::OpenURLParams* params = web_state_.last_open_url_params();
  ASSERT_TRUE(params);
  EXPECT_EQ(
      params->url.spec(),
      "https://www.google.com/maps/foo?valid=true&utm_campaign=as-npt-bling");
}

// Test that a URL with google.com/maps/... is intercepted when all conditions
// are met.
TEST_F(MiniMapTabHelperTest, TestGoogleMapsURL) {
  NSString* const kGoogleMapsLink =
      @"https://www.google.com/maps/foo?valid=true";
  NSString* const kExpectedLink =
      @"https://www.google.com/maps/foo?valid=true&utm_campaign=as-npt-bling";

  OCMExpect([mini_map_commands_handler_
      presentMiniMapNativePreviewForURL:[NSURL URLWithString:kExpectedLink]]);
  bool res = TestShouldAllowRequest(kGoogleSRPPage, kGoogleMapsLink,
                                    /*feature_enabled=*/true,
                                    /*google_maps_installed=*/false,
                                    ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigation should be blocked (returns false).
  EXPECT_FALSE(res);
  EXPECT_OCMOCK_VERIFY(mini_map_commands_handler_);
}

// Test that a URL with google.com/maps?... is intercepted when all conditions
// are met.
TEST_F(MiniMapTabHelperTest, TestGoogleMapsURLWithoutTrailingSlash) {
  NSString* const kGoogleMapsLink =
      @"https://www.google.com/"
      @"maps?sca_esv=189c82b39954af99&output=search&q=restaurants&valid=true";
  NSString* const kExpectedLink =
      @"https://www.google.com/"
      @"maps?sca_esv=189c82b39954af99&output=search&q=restaurants&valid=true"
      @"&utm_campaign=as-npt-bling";

  OCMExpect([mini_map_commands_handler_
      presentMiniMapNativePreviewForURL:[NSURL URLWithString:kExpectedLink]]);
  bool res = TestShouldAllowRequest(kGoogleSRPPage, kGoogleMapsLink,
                                    /*feature_enabled=*/true,
                                    /*google_maps_installed=*/false,
                                    ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigation should be blocked (returns false).
  EXPECT_FALSE(res);
  EXPECT_OCMOCK_VERIFY(mini_map_commands_handler_);
}

// Test that URLs not meeting all interception criteria are not intercepted.
TEST_F(MiniMapTabHelperTest, TestURLsNotIntercepted) {
  // A list of URLs that should not be intercepted.
  NSArray<NSString*>* non_intercepted_urls = @[
    // Not handleable by MiniMapController (does not contain kValidQuery).
    @"https://www.google.com/maps",
    // Path does not start with /maps/ or equal to /maps.
    @"https://www.google.com/mapsbutnotreallymaps?valid=true",
    // Path is completely different.
    @"https://www.google.com/search?q=maps&valid=true",
    // Host is not google.com or maps.google.*.
    @"https://www.example.com/maps?valid=true",
  ];

  for (NSString* url in non_intercepted_urls) {
    bool res = TestShouldAllowRequest(kGoogleSRPPage, url,
                                      /*feature_enabled=*/true,
                                      /*google_maps_installed=*/false,
                                      ui::PageTransition::PAGE_TRANSITION_LINK);
    // Navigation should be allowed (returns true).
    EXPECT_TRUE(res) << "URL should not be intercepted: "
                     << base::SysNSStringToUTF8(url);
  }
}

// Test that if the Native Preview fails to show, navigation is allowed.
TEST_F(MiniMapTabHelperTest, TestNativePreviewFailure) {
  NSString* const kGoogleMapsLink =
      @"https://www.google.com/maps/foo?valid=true";
  NSString* const kExpectedLink =
      @"https://www.google.com/maps/foo?valid=true&utm_campaign=as-npt-bling";

  OCMExpect([mini_map_commands_handler_
      presentMiniMapNativePreviewForURL:[NSURL URLWithString:kExpectedLink]]);

  // Pass false to simulate failure
  bool res = TestShouldAllowRequest(kGoogleSRPPage, kGoogleMapsLink,
                                    /*feature_enabled=*/true,
                                    /*google_maps_installed=*/false,
                                    ui::PageTransition::PAGE_TRANSITION_LINK,
                                    /*simulate_success=*/false);

  // Navigation should be canceled (returns false) so it can open the modified
  // URL.
  EXPECT_FALSE(res);

  web::WebState::OpenURLParams* params = web_state_.last_open_url_params();
  ASSERT_TRUE(params);
  EXPECT_EQ(
      params->url.spec(),
      "https://www.google.com/maps/foo?valid=true&utm_campaign=as-npt-bling");

  EXPECT_OCMOCK_VERIFY(mini_map_commands_handler_);
}

// Test that a new request allows the previous deferred request to proceed.
TEST_F(MiniMapTabHelperTest, TestReentrancy) {
  NSString* const kGoogleMapsLink1 =
      @"https://www.google.com/maps/foo?valid=true";
  NSString* const kGoogleMapsLink2 =
      @"https://www.google.com/maps/bar?valid=true";
  NSString* const kExpectedLink1 =
      @"https://www.google.com/maps/foo?valid=true&utm_campaign=as-npt-bling";
  NSString* const kExpectedLink2 =
      @"https://www.google.com/maps/bar?valid=true&utm_campaign=as-npt-bling";

  OCMExpect([mini_map_commands_handler_
      presentMiniMapNativePreviewForURL:[NSURL URLWithString:kExpectedLink1]]);
  OCMExpect([mini_map_commands_handler_
      presentMiniMapNativePreviewForURL:[NSURL URLWithString:kExpectedLink2]]);

  web_state_.SetCurrentURL(
      net::GURLWithNSURL([NSURL URLWithString:kGoogleSRPPage]));
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  profile_->GetSyncablePrefs()->SetBoolean(prefs::kIosMiniMapShowNativeMap,
                                           true);
  google_maps_installed_ = false;

  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main*/ true,
      /*target_frame_is_cross_origin*/ false,
      /*target_window_is_cross_origin*/ false, /*user_initiated*/ true,
      /*user_tapped_recently*/ true);

  __block bool callback1_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision decision1 =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback1 =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        decision1 = decision;
        callback1_called = true;
      });

  tab_helper_->ShouldAllowRequest(
      [NSURLRequest requestWithURL:[NSURL URLWithString:kGoogleMapsLink1]],
      request_info, std::move(callback1));

  EXPECT_FALSE(callback1_called);

  __block bool callback2_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision decision2 =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback2 =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        decision2 = decision;
        callback2_called = true;
      });

  tab_helper_->ShouldAllowRequest(
      [NSURLRequest requestWithURL:[NSURL URLWithString:kGoogleMapsLink2]],
      request_info, std::move(callback2));

  // The first callback should be called with ALLOW because it was overwritten!
  EXPECT_TRUE(callback1_called);
  EXPECT_TRUE(decision1.ShouldAllowNavigation());

  // The second callback should NOT be called yet (deferred).
  EXPECT_FALSE(callback2_called);

  // Now resolve the second one
  tab_helper_->OnMiniMapSuccess();
  EXPECT_TRUE(callback2_called);
  EXPECT_FALSE(decision2.ShouldAllowNavigation());

  EXPECT_OCMOCK_VERIFY(mini_map_commands_handler_);
}

// Test that the counterfactual flag causes the URL to be modified and opened.
TEST_F(MiniMapTabHelperTest, TestCounterfactualLogging) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kIOSMiniMapUniversalLinkCounterfactual);

  NSString* const kGoogleMapsLink =
      @"https://www.google.com/maps/foo?valid=true";

  bool res = TestShouldAllowRequest(kGoogleSRPPage, kGoogleMapsLink,
                                    /*feature_enabled=*/true,
                                    /*google_maps_installed=*/false,
                                    ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigation should be blocked (returns false).
  EXPECT_FALSE(res);

  // Check that a new URL was opened with the utm_campaign parameter.
  web::WebState::OpenURLParams* params = web_state_.last_open_url_params();
  ASSERT_TRUE(params);
  EXPECT_EQ(
      params->url.spec(),
      "https://www.google.com/maps/foo?valid=true&utm_campaign=as-npc-bling");

  // Test that the modified URL is ALLOWED (not intercepted again).
  NSString* const kModifiedGoogleMapsLink =
      @"https://www.google.com/maps/foo?valid=true&utm_campaign=as-npc-bling";

  bool res_modified =
      TestShouldAllowRequest(kGoogleSRPPage, kModifiedGoogleMapsLink,
                             /*feature_enabled=*/true,
                             /*google_maps_installed=*/false,
                             ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigation should be ALLOWED (returns true).
  EXPECT_TRUE(res_modified);
}

// Test that the counterfactual flag causes the URL to be modified and opened
// even when the transition type includes qualifiers (e.g. redirect).
TEST_F(MiniMapTabHelperTest, TestCounterfactualLoggingWithRedirect) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kIOSMiniMapUniversalLinkCounterfactual);

  NSString* const kGoogleMapsLink =
      @"https://www.google.com/maps/foo?valid=true";

  ui::PageTransition transition_type = static_cast<ui::PageTransition>(
      ui::PageTransition::PAGE_TRANSITION_LINK |
      ui::PageTransition::PAGE_TRANSITION_SERVER_REDIRECT);

  bool res =
      TestShouldAllowRequest(kGoogleSRPPage, kGoogleMapsLink,
                             /*feature_enabled=*/true,
                             /*google_maps_installed=*/false, transition_type);

  // Navigation should be blocked (returns false).
  EXPECT_FALSE(res);

  // Check that a new URL was opened with the utm_campaign parameter.
  web::WebState::OpenURLParams* params = web_state_.last_open_url_params();
  ASSERT_TRUE(params);
  EXPECT_EQ(
      params->url.spec(),
      "https://www.google.com/maps/foo?valid=true&utm_campaign=as-npc-bling");
}

// Test that when both the experiment feature and counterfactual are disabled,
// the request is not intercepted at all.
TEST_F(MiniMapTabHelperTest, TestExperimentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kIOSMiniMapUniversalLink,
                             kIOSMiniMapUniversalLinkCounterfactual});

  NSString* const kGoogleMapsLink =
      @"https://www.google.com/maps/foo?valid=true";

  bool res = TestShouldAllowRequest(kGoogleSRPPage, kGoogleMapsLink,
                                    /*feature_enabled=*/true,
                                    /*google_maps_installed=*/false,
                                    ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigation should be allowed (returns true).
  EXPECT_TRUE(res);
}
