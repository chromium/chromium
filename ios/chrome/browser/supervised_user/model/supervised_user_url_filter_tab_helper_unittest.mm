// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_url_filter_tab_helper.h"
#import "base/memory/scoped_refptr.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/core/browser/family_link_settings_service.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_test_environment.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/family_link_settings_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace supervised_user {
namespace {

const char kTestEmail[] = "test@gmail.com";
NSString* const kExampleURL = @"http://example.com";

class SupervisedUserURLFilterTabHelperTest : public PlatformTest {
 protected:
  SupervisedUserURLFilterTabHelperTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(ChildAccountServiceFactory::GetInstance(),
                              ChildAccountServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
    web_state_.SetBrowserState(profile_.get());
    SupervisedUserURLFilterTabHelper::CreateForWebState(&web_state_);
    SupervisedUserErrorContainer::CreateForWebState(&web_state_);
    security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
        &web_state_);
  }

  // Signs the user into `email` as the primary Chrome account and sets the
  // given parental control capabilities on this account.
  void SignIn(const std::string& email, bool is_subject_to_parental_controls) {
    if (is_subject_to_parental_controls) {
      SupervisedUserTestEnvironment::EnableSupervisedAccount(
          IdentityManagerFactory::GetForProfile(profile_.get()));
    } else {
      SupervisedUserTestEnvironment::EnableRegularAccount(
          IdentityManagerFactory::GetForProfile(profile_.get()));
    }

    ASSERT_EQ(IsSubjectToParentalControls(profile_.get()),
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

  FamilyLinkSettingsService* family_link_settings_service() {
    return FamilyLinkSettingsServiceFactory::GetForProfile(profile_.get());
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
};

TEST_F(SupervisedUserURLFilterTabHelperTest,
       BlockCertainSitesForSupervisedUser) {
  base::HistogramTester histogram_tester;
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/true);

  // Restricts all sites for the supervised user.
  SupervisedUserTestEnvironment::SetWebFilterType(
      WebFilterType::kCertainSites, *family_link_settings_service());
  EXPECT_TRUE(IsURLBlocked(kExampleURL));
  histogram_tester.ExpectTotalCount(
      kSupervisedUserURLFilteringResultHistogramName, 1);

  // Adds singular exception to allow "example.com".
  SupervisedUserTestEnvironment::SetManualFilterForHost(
      "example.com", /*allowlist=*/true, *family_link_settings_service());
  EXPECT_FALSE(IsURLBlocked(kExampleURL));
  histogram_tester.ExpectTotalCount(
      kSupervisedUserURLFilteringResultHistogramName, 2);
}

TEST_F(SupervisedUserURLFilterTabHelperTest,
       NonSupervisedUserHasUnblockedExperienceByDefault) {
  base::HistogramTester histogram_tester;
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/false);
  // That's the default behavior of unsupervised user, and it can't be changed.
  EXPECT_FALSE(IsURLBlocked(kExampleURL));
}

TEST_F(SupervisedUserURLFilterTabHelperTest, AllowsAllSitesWhenOffTheRecord) {
  // In an off the record profile:
  // a) Restrict/allow calls are impossible: there is no prod code path that
  // allows for that b) However, the supervised user infrastructure exists, and
  // it correctly allows for arbitrary navigation.
  EXPECT_FALSE(IsURLBlocked(kExampleURL));
}

}  // namespace
}  // namespace supervised_user
