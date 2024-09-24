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
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
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
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));

    profile_ = std::move(builder).Build();
    web_state_.SetBrowserState(profile_.get());
    SupervisedUserURLFilterTabHelper::CreateForWebState(&web_state_);
    SupervisedUserErrorContainer::CreateForWebState(&web_state_);
    security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
        &web_state_);
    scoped_feature_list_.InitAndEnableFeature(
        supervised_user::kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS);
  }

  // Signs the user into `email` as the primary Chrome account and sets the
  // given parental control capabilities on this account.
  void SignIn(const std::string& email, bool is_subject_to_parental_controls) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    AccountInfo account = signin::MakePrimaryAccountAvailable(
        identity_manager, email, signin::ConsentLevel::kSignin);
    supervised_user::UpdateSupervisionStatusForAccount(
        account, identity_manager, is_subject_to_parental_controls);

    // Initialize supervised_user services.
    ChildAccountServiceFactory::GetForProfile(profile_.get())->Init();

    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
    supervised_user_service->Init();

    EXPECT_EQ(supervised_user::IsSubjectToParentalControls(profile_.get()),
              is_subject_to_parental_controls);
  }

  // Calls `ShouldAllowRequest` for a request with the given `url_string`.
  // Returns true if the URL request is blocked.
  bool IsURLBlocked(NSString* url_string) {
    // Set up for `ShouldAllowRequest`.
    const web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_LINK, /*target_frame_is_main=*/true,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
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
        SupervisedUserServiceFactory::GetForProfile(profile_.get());

    std::map<std::string, bool> hosts;
    hosts["example.com"] = true;
    supervised_user_service->GetURLFilter()->SetManualHosts(hosts);
    supervised_user_service->GetURLFilter()->SetDefaultFilteringBehavior(
        supervised_user::FilteringBehavior::kAllow);
  }

  void RestrictAllSitesForSupervisedUser() {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
    supervised_user_service->GetURLFilter()->SetDefaultFilteringBehavior(
        supervised_user::FilteringBehavior::kBlock);
  }

 private:
  web::WebTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
