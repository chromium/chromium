// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/model/mini_map_tab_helper.h"

#import <memory>

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
// The template for Google search engine.
const char kGoogleSearchTemplate[] = "https://www.google.com/?q={searchTerms}";

// Another random template.
const char kNotGoogleSearchTemplate[] =
    "https://www.example.com/?q={searchTerms}";

// A link to Google SRP.
NSString* const kGoogleSRPPage = @"https://www.google.com/search?q=foo";

// A UL to Maps.
NSString* const kMapsLink = @"https://maps.google.com/maps/foo";

// A valid query parameter to mark URL valid for
// MiniMapTabHelperTestMiniMapControllerFactory.
NSString* const kValidQuery = @"valid=true";
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
  return [url.query isEqualToString:kValidQuery];
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

    TestProfileIOS::Builder test_profile_builder;

    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));

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

  [[nodiscard]] bool TestShouldAllowRequest(
      NSString* web_state_url_string,
      NSString* url_string,
      bool feature_enabled,
      bool dse_is_google,
      bool google_maps_installed,
      ui::PageTransition transition_type) {
    NSURL* web_state_url = [NSURL URLWithString:web_state_url_string];
    NSURL* url = [NSURL URLWithString:url_string];
    web_state_.SetCurrentURL(net::GURLWithNSURL(web_state_url));
    web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
    profile_->GetSyncablePrefs()->SetBoolean(prefs::kIosMiniMapShowNativeMap,
                                             feature_enabled);

    TemplateURLData template_url_data;
    if (dse_is_google) {
      template_url_data.SetURL(kGoogleSearchTemplate);
    } else {
      template_url_data.SetURL(kNotGoogleSearchTemplate);
    }
    template_url_service_->ApplyDefaultSearchChangeForTesting(
        &template_url_data, DefaultSearchManager::FROM_USER);

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
    run_loop.Run();
    EXPECT_TRUE(callback_called);
    return policy_decision.ShouldAllowNavigation();
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  MiniMapTabHelperTestMiniMapControllerFactory* factory_;
  std::unique_ptr<TestProfileIOS> profile_;
  id application_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
  raw_ptr<MiniMapTabHelper> tab_helper_ = nullptr;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  id mini_map_commands_handler_;
  bool google_maps_installed_;
};

// Test all the combinations of conditions for ShouldAllowRequest.
TEST_F(MiniMapTabHelperTest, TestNavigations) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  const int google_srp_index = 1 << 0;
  const int link_to_maps_index = 1 << 1;
  const int feature_enabled_index = 1 << 2;
  const int is_dse_google_index = 1 << 3;
  const int google_maps_not_installed_index = 1 << 4;
  const int transition_type_index = 1 << 5;
  const int handled_url_index = 1 << 6;
  const int signed_in = 1 << 7;
  const int total = 1 << 8;
  bool was_signed_in = false;

  for (int scenario = 0; scenario < total; scenario++) {
    NSString* web_state_url =
        (scenario & google_srp_index) ? kGoogleSRPPage : kMapsLink;
    NSString* url =
        (scenario & link_to_maps_index) ? kMapsLink : kGoogleSRPPage;
    bool feature_enabled = scenario & feature_enabled_index;
    bool dse_is_google = scenario & is_dse_google_index;
    bool google_maps_not_installed = scenario & google_maps_not_installed_index;
    if (scenario & handled_url_index) {
      url = [url stringByAppendingFormat:@"?%@", kValidQuery];
    }

    if (scenario & signed_in && !was_signed_in) {
      signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                          signin::ConsentLevel::kSignin);
      was_signed_in = true;
    } else if (!(scenario & signed_in) && was_signed_in) {
      ClearPrimaryAccount(identity_manager);
      was_signed_in = false;
    }
    ui::PageTransition transition_type =
        (scenario & transition_type_index)
            ? ui::PageTransition::PAGE_TRANSITION_LINK
            : ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK;
    if (scenario == total - 1) {
      // If all conditions are true, a command to display the Mini Map should be
      // sent.
      OCMExpect([mini_map_commands_handler_
          presentMiniMapForURL:[NSURL URLWithString:url]]);
    }
    bool res = TestShouldAllowRequest(web_state_url, url, feature_enabled,
                                      dse_is_google, !google_maps_not_installed,
                                      transition_type);
    EXPECT_OCMOCK_VERIFY(mini_map_commands_handler_);
    // Navigation should be blocked only if all conditions are true.
    EXPECT_EQ(scenario != total - 1, res);
  }
}
