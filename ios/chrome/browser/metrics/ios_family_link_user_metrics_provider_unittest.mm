// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "ios/chrome/browser/metrics/ios_family_link_user_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {
const char kTestEmail[] = "test@gmail.com";
}  // namespace

class IOSFamilyLinkUserMetricsProviderTest : public PlatformTest {
 protected:
  IOSFamilyLinkUserMetricsProviderTest() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    browser_state_manager_ =
        std::make_unique<TestChromeBrowserStateManager>(builder.Build());
    TestingApplicationContext::GetGlobal()->SetChromeBrowserStateManager(
        browser_state_manager_.get());
  }

  IOSFamilyLinkUserMetricsProvider* metrics_provider() {
    return &metrics_provider_;
  }

  ChromeBrowserState* browser_state() {
    return browser_state_manager_->GetLastUsedBrowserState();
  }

  // Signs the user into `email` as the primary Chrome account and sets the
  // given parental control capabilities on this account.
  void SignIn(const std::string& email,
              bool is_subject_to_parental_controls,
              bool is_opted_in_to_parental_supervision) {
    ChromeBrowserState* browser_state =
        browser_state_manager_->GetLastUsedBrowserState();
    AccountInfo account = signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForBrowserState(browser_state), email,
        signin::ConsentLevel::kSignin);

    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_is_subject_to_parental_controls(
        is_subject_to_parental_controls);
    mutator.set_is_opted_in_to_parental_supervision(
        is_opted_in_to_parental_supervision);
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForBrowserState(browser_state), account);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ios::ChromeBrowserStateManager> browser_state_manager_;

  IOSFamilyLinkUserMetricsProvider metrics_provider_;
};

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfileWithUnknownCapabilitiesDoesNotOutputHistogram) {
  AccountInfo account = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForBrowserState(browser_state()), kTestEmail,
      signin::ConsentLevel::kSignin);
  // Does not set account capabilities, default is unknown.

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectTotalCount(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      /*expected_count=*/0);
}

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfileWithRequiredSupervisionLoggedAsSupervisionEnabledByPolicy) {
  // Profile with supervision set by policy
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::LogSegment::kSupervisionEnabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfileWithOptionalSupervisionLoggedSupervisionEnabledByUser) {
  // Profile with supervision set by user
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::LogSegment::kSupervisionEnabledByUser,
      /*expected_bucket_count=*/1);
}

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfileWithAdultUserLoggedAsUnsupervised) {
  // Adult profile
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/false,
         /*is_opted_in_to_parental_supervision=*/false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);
}
