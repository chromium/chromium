// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_family_link_user_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/core/browser/supervised_user_log_record.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_test_environment.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/signin/model/signin_test_util.h"
#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/family_link_settings_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {
const char kTestEmail[] = "test@gmail.com";
const char kTestEmail1[] = "test1@gmail.com";
const char kTestEmail2[] = "test2@gmail.com";
}  // namespace

class IOSFamilyLinkUserMetricsProviderTest : public PlatformTest {
 protected:
  IOSFamilyLinkUserMetricsProviderTest() {
    personal_profile_ =
        profile_manager_.AddProfileWithBuilder(CreateProfileBuilder());
  }

  IOSFamilyLinkUserMetricsProvider* metrics_provider() {
    return &metrics_provider_;
  }

  void SignIn(ProfileIOS* profile,
              const std::string& email,
              bool is_subject_to_parental_controls,
              bool is_opted_in_to_parental_supervision) {
    // Ensure that the Child stack is properly initialized.
    CHECK(ChildAccountServiceFactory::GetForProfile(profile));
    CHECK(
        supervised_user::SupervisedUserServiceFactory::GetForProfile(profile));

    AccountInfo account = signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile), email,
        signin::ConsentLevel::kSignin);

    AccountCapabilitiesTestMutator mutator(&account);
    mutator.set_is_subject_to_parental_controls(
        is_subject_to_parental_controls);
    mutator.set_is_opted_in_to_parental_supervision(
        is_opted_in_to_parental_supervision);
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForProfile(profile), account);
    account = AccountInfo::Builder(account)
                  .SetIsChildAccount(
                      signin::TriboolFromBool(is_subject_to_parental_controls))
                  .Build();
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForProfile(profile), account);
  }

  // Signs the user into `email` as the primary Chrome account on the personal
  // profile and sets the given parental control capabilities on this account.
  void SignIn(const std::string& email,
              bool is_subject_to_parental_controls,
              bool is_opted_in_to_parental_supervision) {
    SignIn(personal_profile(), email, is_subject_to_parental_controls,
           is_opted_in_to_parental_supervision);
  }

  // Adds a pre-configured managed test profile to the manager.
  TestProfileIOS* AddManagedTestProfile(id<SystemIdentity> managed_identity) {
    return AddManagedProfileWithBuilder(profile_manager_, managed_identity,
                                        CreateProfileBuilder());
  }

  void RestrictAllSitesForSupervisedUser(ProfileIOS* profile) {
    supervised_user::SupervisedUserTestEnvironment::SetWebFilterType(
        supervised_user::WebFilterType::kCertainSites,
        *supervised_user::FamilyLinkSettingsServiceFactory::GetForProfile(
            profile));
  }

  void AllowUnsafeSitesForSupervisedUser(ProfileIOS* profile) {
    supervised_user::SupervisedUserTestEnvironment::SetWebFilterType(
        supervised_user::WebFilterType::kAllowAllSites,
        *supervised_user::FamilyLinkSettingsServiceFactory::GetForProfile(
            profile));
  }

  ProfileIOS* personal_profile() { return personal_profile_.get(); }

 private:
  TestProfileIOS::Builder CreateProfileBuilder() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(ChildAccountServiceFactory::GetInstance(),
                              ChildAccountServiceFactory::GetDefaultFactory());
    return builder;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> personal_profile_;

  IOSFamilyLinkUserMetricsProvider metrics_provider_;
};

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfileWithUnknownCapabilitiesDoesNotOutputHistogram) {
  AccountInfo account = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(personal_profile()), kTestEmail,
      signin::ConsentLevel::kSignin);
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
      supervised_user::SupervisedUserLogRecord::Segment::
          kSupervisionEnabledByFamilyLinkPolicy,
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
      supervised_user::SupervisedUserLogRecord::Segment::
          kSupervisionEnabledByFamilyLinkUser,
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
      supervised_user::SupervisedUserLogRecord::Segment::kUnsupervised,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      /*expected_count=*/0);
}

TEST_F(
    IOSFamilyLinkUserMetricsProviderTest,
    ProfilesWithMixedSupervisedUsersLoggedAsMixedProfileWithDefaultWebFilter) {
  // Personal profile with supervision set by user
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/false);
  // Managed profile with supervision set by policy
  ProfileIOS* managed_profile =
      AddManagedTestProfile([FakeSystemIdentity fakeManagedIdentity]);
  SignIn(managed_profile, kTestEmail1,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::SupervisedUserLogRecord::Segment::kMixedProfile,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);
}

TEST_F(
    IOSFamilyLinkUserMetricsProviderTest,
    ProfilesWithMixedSupervisedAndAdultUsersLoggedAsMixedProfileWithDefaultWebFilter) {
  // Adult personal profile
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/false,
         /*is_opted_in_to_parental_supervision=*/false);

  // Managed profile with supervision set by user
  ProfileIOS* managed_profile1 =
      AddManagedTestProfile([FakeSystemIdentity fakeManagedIdentity]);
  SignIn(managed_profile1, kTestEmail1,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/false);

  // Managed profile with supervision set by policy
  ProfileIOS* managed_profile2 = AddManagedTestProfile(
      [FakeSystemIdentity identityWithEmail:@"bar@foo.com"]);
  SignIn(managed_profile2, kTestEmail2,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::SupervisedUserLogRecord::Segment::kMixedProfile,
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
      supervised_user::SupervisedUserLogRecord::Segment::kUnsupervised,
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
  RestrictAllSitesForSupervisedUser(personal_profile());

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::SupervisedUserLogRecord::Segment::
          kSupervisionEnabledByFamilyLinkUser,
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
  AllowUnsafeSitesForSupervisedUser(personal_profile());

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::SupervisedUserLogRecord::Segment::
          kSupervisionEnabledByFamilyLinkUser,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kAllowAllSites,
      /*expected_bucket_count=*/1);
}

TEST_F(IOSFamilyLinkUserMetricsProviderTest,
       ProfilesWithMixedSupervisedUsersLoggedAsMixedFilter) {
  // Personal profile with supervision set by user
  SignIn(kTestEmail,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/false);
  AllowUnsafeSitesForSupervisedUser(personal_profile());

  // Managed profile with supervision set by policy
  ProfileIOS* managed_profile =
      AddManagedTestProfile([FakeSystemIdentity fakeManagedIdentity]);
  SignIn(managed_profile, kTestEmail1,
         /*is_subject_to_parental_controls=*/true,
         /*is_opted_in_to_parental_supervision=*/true);
  RestrictAllSitesForSupervisedUser(managed_profile);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::SupervisedUserLogRecord::Segment::kMixedProfile,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kMixed,
      /*expected_bucket_count=*/1);
}
