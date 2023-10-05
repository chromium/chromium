// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_url_filter_tab_helper.h"

#import "base/memory/scoped_refptr.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

const char kTestEmail[] = "test@gmail.com";
NSString* kExampleURL = @"http://example.com";

}  // namespace

class SupervisedUserURLFilterTabHelperTest : public PlatformTest {
 protected:
  SupervisedUserURLFilterTabHelperTest() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));

    chrome_browser_state_ = builder.Build();
    web_state_.SetBrowserState(chrome_browser_state_.get());
    SupervisedUserURLFilterTabHelper::CreateForWebState(&web_state_);
    SupervisedUserErrorContainer::CreateForWebState(&web_state_);
    security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
        &web_state_);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS},
        {});
  }

  // Signs the user into `email` as the primary Chrome account and sets the
  // given parental control capabilities on this account.
  void SignIn(const std::string& email, bool is_subject_to_parental_controls) {
    AccountInfo account = signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForBrowserState(chrome_browser_state_.get()),
        email, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_is_subject_to_parental_controls(
        is_subject_to_parental_controls);
    // Update child status preference, which is backed by capability state.
    // This action will not be performed by the fake account capability fetcher.
    account.is_child_account = is_subject_to_parental_controls
                                   ? signin::Tribool::kTrue
                                   : signin::Tribool::kFalse;
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForBrowserState(chrome_browser_state_.get()),
        account);

    // Initialize supervised_user services.
    ChildAccountServiceFactory::GetForBrowserState(chrome_browser_state_.get())
        ->Init();

    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    supervised_user_service->Init();

    EXPECT_EQ(supervised_user_service->IsSubjectToParentalControls(),
              is_subject_to_parental_controls);
  }

  // Calls `ShouldAllowRequest` for a request with the given `url_string`.
  // Returns true if the URL request is blocked.
  bool IsURLBlocked(NSString* url_string) {
    // Set up for `ShouldAllowRequest`.
    const web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main=*/true,
        /*target_frame_is_cross_origin=*/false,
        /*has_user_gesture=*/false);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision request_policy =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          request_policy = decision;
          callback_called = true;
          std::move(quit_closure).Run();
        });
    web_state_.ShouldAllowRequest(
        [NSURLRequest requestWithURL:[NSURL URLWithString:url_string]],
        request_info, std::move(callback));
    run_loop.Run();
    EXPECT_TRUE(callback_called);

    return request_policy.ShouldCancelNavigation();
  }

  void AllowExampleSiteForSupervisedUser() {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());

    std::map<std::string, bool> hosts;
    hosts["example.com"] = true;
    supervised_user_service->GetURLFilter()->SetManualHosts(hosts);
    supervised_user_service->GetURLFilter()->SetDefaultFilteringBehavior(
        supervised_user::SupervisedUserURLFilter::ALLOW);
  }

  void RestrictAllSitesForSupervisedUser() {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    supervised_user_service->GetURLFilter()->SetDefaultFilteringBehavior(
        supervised_user::SupervisedUserURLFilter::BLOCK);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::FakeWebState web_state_;
};

TEST_F(SupervisedUserURLFilterTabHelperTest,
       BlockCertainSitesForSupervisedUser) {
  base::HistogramTester histogram_tester;
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/true);
  RestrictAllSitesForSupervisedUser();
  EXPECT_TRUE(IsURLBlocked(kExampleURL));
  histogram_tester.ExpectTotalCount(
      supervised_user::kSupervisedUserURLFilteringResultHistogramName, 1);

  AllowExampleSiteForSupervisedUser();
  EXPECT_FALSE(IsURLBlocked(kExampleURL));
  histogram_tester.ExpectTotalCount(
      supervised_user::kSupervisedUserURLFilteringResultHistogramName, 2);
}

TEST_F(SupervisedUserURLFilterTabHelperTest,
       AllowsAllSitesForNonSupervisedUser) {
  base::HistogramTester histogram_tester;
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/false);
  RestrictAllSitesForSupervisedUser();
  EXPECT_FALSE(IsURLBlocked(kExampleURL));
  AllowExampleSiteForSupervisedUser();
  EXPECT_FALSE(IsURLBlocked(kExampleURL));

  // This histogram is only relevant for supervised users.
  histogram_tester.ExpectTotalCount(
      supervised_user::kSupervisedUserURLFilteringResultHistogramName, 0);
}

TEST_F(SupervisedUserURLFilterTabHelperTest, AllowsAllSitesWhenLoggedOut) {
  RestrictAllSitesForSupervisedUser();
  EXPECT_FALSE(IsURLBlocked(kExampleURL));
  AllowExampleSiteForSupervisedUser();
  EXPECT_FALSE(IsURLBlocked(kExampleURL));
}
