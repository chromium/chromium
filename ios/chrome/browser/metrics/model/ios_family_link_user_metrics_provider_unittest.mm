// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_family_link_user_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {
const char kTestEmail[] = "test@gmail.com";
const char kTestEmail1[] = "test1@gmail.com";
const char kTestEmail2[] = "test2@gmail.com";

const char kProfileName1[] = "profile1";
const char kProfileName2[] = "profile2";
}  // namespace

class IOSFamilyLinkUserMetricsProviderTest : public PlatformTest {
 protected:
  IOSFamilyLinkUserMetricsProviderTest() {
    default_browser_state_ = browser_state_manager_.AddBrowserStateWithBuilder(
        CreateBrowserStateBuilder(/*name=*/std::string()));
  }

  IOSFamilyLinkUserMetricsProvider* metrics_provider() {
    return &metrics_provider_;
  }

  TestChromeBrowserStateManager* browser_state_manager() {
    return &browser_state_manager_;
  }

  void SignIn(ChromeBrowserState* browser_state,
              const std::string& email,
              bool is_subject_to_parental_controls,
              bool is_opted_in_to_parental_supervision) {
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

    if (is_subject_to_parental_controls) {
      supervised_user::EnableParentalControls(*browser_state->GetPrefs());
    }
  }

  // Signs the user into `email` as the primary Chrome account and sets the
  // given parental control capabilities on this account.
  void SignIn(const std::string& email,
              bool is_subject_to_parental_controls,
              bool is_opted_in_to_parental_supervision) {
    SignIn(default_browser_state(), email, is_subject_to_parental_controls,
           is_opted_in_to_parental_supervision);
  }

  // Adds a pre-configured test browser state to the manager.
  void AddTestBrowserState(const std::string& name) {
    browser_state_manager_.AddBrowserStateWithBuilder(
        CreateBrowserStateBuilder(name));
  }

  void RestrictAllSitesForSupervisedUser(ChromeBrowserState* browser_state) {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForBrowserState(browser_state);
    supervised_user_service->GetURLFilter()->SetDefaultFilteringBehavior(
        supervised_user::FilteringBehavior::kBlock);
  }

  void AllowUnsafeSitesForSupervisedUser(ChromeBrowserState* browser_state) {
    browser_state->GetPrefs()->SetBoolean(prefs::kSupervisedUserSafeSites,
                                          false);
  }

  ChromeBrowserState* default_browser_state() {
    return default_browser_state_.get();
  }

 private:
  TestChromeBrowserState::Builder CreateBrowserStateBuilder(
      const std::string& name) {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    if (!name.empty()) {
      builder.SetName(name);
    }
    return builder;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestChromeBrowserStateManager browser_state_manager_;
  raw_ptr<ChromeBrowserState> default_browser_state_;

  IOSFamilyLinkUserMetricsProvider metrics_provider_;
};

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfileWithUnknownCapabilitiesDoesNotOutputHistogram) {
  AccountInfo account = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForBrowserState(default_browser_state()),
      kTestEmail, signin::ConsentLevel::kSignin);
  // Does not set account capabilities, default is unknown.

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectTotalCount(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
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
      supervised_user::FamilyLinkUserLogRecord::Segment::
          kSupervisionEnabledByPolicy,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kTryToBlockMatureSites,
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
      supervised_user::FamilyLinkUserLogRecord::Segment::
          kSupervisionEnabledByUser,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kTryToBlockMatureSites,
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
      supervised_user::FamilyLinkUserLogRecord::Segment::kUnsupervised,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      /*expected_count=*/0);
}

TEST_F(
    IOSFamilyLinkUserMetricsProviderTest,
    ProfilesWithMixedSupervisedUsersLoggedAsMixedProfileWithDefaultWebFilter) {
  // Profile with supervision set by user
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/false);
  // Profile with supervision set by policy
  AddTestBrowserState(kProfileName1);
  SignIn(browser_state_manager()->GetBrowserStateByName(kProfileName1),
         kTestEmail1,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::kMixedProfile,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);
}

TEST_F(
    IOSFamilyLinkUserMetricsProviderTest,
    ProfilesWithMixedSupervisedAndAdultUsersLoggedAsMixedProfileWithDefaultWebFilter) {
  // Adult profile
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/false,
         /*is_opted_in_to_parental_supervision=*/false);

  // Profile with supervision set by user
  AddTestBrowserState(kProfileName1);
  SignIn(browser_state_manager()->GetBrowserStateByName(kProfileName1),
         kTestEmail1,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/false);

  // Profile with supervision set by policy
  AddTestBrowserState(kProfileName2);
  SignIn(browser_state_manager()->GetBrowserStateByName(kProfileName2),
         kTestEmail2,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::kMixedProfile,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);
}

TEST_F(IOSFamilyLinkUserMetricsProviderTest, NotSignedInLoggedAsUnsupervised) {
  // Add no profiles
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::kUnsupervised,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      /*expected_count=*/0);
}

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfileWithSupervisedUserWithOnlyApprovedSites) {
  // Profile with supervision set by user
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/true);
  RestrictAllSitesForSupervisedUser(default_browser_state());

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::
          kSupervisionEnabledByUser,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kCertainSites,
      /*expected_bucket_count=*/1);
}

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfileWithSupervisedUserWithAllowAllSites) {
  // Profile with supervision set by user
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/true);
  AllowUnsafeSitesForSupervisedUser(default_browser_state());

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::
          kSupervisionEnabledByUser,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kAllowAllSites,
      /*expected_bucket_count=*/1);
}

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfilesWithMixedSupervisedUsersLoggedAsMixedFilter) {
  // Profile with supervision set by user
  AddTestBrowserState(kProfileName1);
  SignIn(browser_state_manager()->GetBrowserStateByName(kProfileName1),
         kTestEmail1,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/false);
  AllowUnsafeSitesForSupervisedUser(
      browser_state_manager()->GetBrowserStateByName(kProfileName1));

  // Profile with supervision set by policy
  AddTestBrowserState(kProfileName2);
  SignIn(browser_state_manager()->GetBrowserStateByName(kProfileName2),
         kTestEmail2,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/true);
  RestrictAllSitesForSupervisedUser(
      browser_state_manager()->GetBrowserStateByName(kProfileName2));

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::kMixedProfile,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kMixed,
      /*expected_bucket_count=*/1);
}
