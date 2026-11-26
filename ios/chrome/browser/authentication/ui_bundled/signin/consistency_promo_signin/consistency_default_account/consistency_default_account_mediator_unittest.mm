// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_mediator.h"

#import <UIKit/UIKit.h>

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/core/browser/account_preview_data_service.h"
#import "components/signin/core/browser/test_account_preview_data_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/base/data_type.h"
#import "components/sync/protocol/sync_enums.pb.h"
#import "components/sync/test/test_sync_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

// Fake consumer recording updates from `ConsistencyDefaultAccountMediator`.
@interface FakeConsistencyDefaultAccountConsumer
    : NSObject <ConsistencyDefaultAccountConsumer>

@property(nonatomic, copy) NSString* labelText;
@property(nonatomic, copy) NSString* skipButtonText;
@property(nonatomic, copy) NSString* email;

@end

@implementation FakeConsistencyDefaultAccountConsumer

- (void)showDefaultAccountWithFullName:(NSString*)fullName
                             givenName:(NSString*)givenName
                                 email:(NSString*)email
                                avatar:(UIImage*)avatar
                               managed:(BOOL)managed {
  _email = [email copy];
}

- (void)hideDefaultAccount {
  _email = nil;
}

@end

namespace {

// Creates an `AccountPreviewPreference` for `gaia_id` with desktop passwords
// enabled.
signin::AccountPreviewDataService::AccountPreviewPreference
CreatePasswordsDesktopPreference(const GaiaId& gaia_id) {
  signin::AccountPreviewDataService::AccountPreviewPreference preference;
  preference.gaia_id = gaia_id;
  preference.preferred_data_types.push_back({.data_type = syncer::PASSWORDS});
  preference.other_device_info.form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;
  preference.other_device_info.enabled_data_types = {syncer::PASSWORDS};
  return preference;
}

class ConsistencyDefaultAccountMediatorTest : public PlatformTest {
 public:
  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    feature_list_.InitAndEnableFeature(
        switches::kEnableAccountPreviewPreferredAccount);
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  FakeSystemIdentityManager* system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  // Creates `mediator_` with `prefilled_email` and attaches `consumer_`.
  void CreateMediator(NSString* prefilled_email) {
    consumer_ = [[FakeConsistencyDefaultAccountConsumer alloc] init];
    mediator_ = [[ConsistencyDefaultAccountMediator alloc]
          initWithIdentityManager:IdentityManagerFactory::GetForProfile(
                                      profile_)
            accountManagerService:ChromeAccountManagerServiceFactory::
                                      GetForProfile(profile_)
        accountPreviewDataService:&account_preview_data_service_
                      syncService:&sync_service_
                     contextStyle:SigninContextStyle::kDefault
                      accessPoint:signin_metrics::AccessPoint::kWebSignin
                   prefilledEmail:prefilled_email];
    mediator_.consumer = consumer_;
  }

 protected:
  // Foundational environment fixtures must precede profile/service state so
  // teardown occurs in reverse declaration order.
  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  signin::TestAccountPreviewDataService account_preview_data_service_;
  syncer::TestSyncService sync_service_;
  FakeConsistencyDefaultAccountConsumer* consumer_ = nil;
  ConsistencyDefaultAccountMediator* mediator_ = nil;
};

// Tests that when multiple identities are on the device, `prefilledEmail`
// selects the matching non-default identity.
TEST_F(ConsistencyDefaultAccountMediatorTest,
       SelectsPrefilledIdentityWithMultipleAccounts) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* identity2 = [FakeSystemIdentity fakeIdentity2];
  system_identity_manager()->AddIdentity(identity1);
  system_identity_manager()->AddIdentity(identity2);

  CreateMediator(identity2.userEmail);

  EXPECT_NSEQ(identity2, mediator_.selectedIdentity);
  EXPECT_NSEQ(identity2.userEmail, consumer_.email);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_WEB_SIGNIN),
              consumer_.labelText);
}

// Tests that when `prefilledEmail` matches the preferred account in
// `AccountPreviewDataService`, both that identity and its customized promo
// label are used.
// TODO(crbug.com/579498250): Check which string to use if an account is both
// preferred and prefilled, and whether this is the expected behavior.
TEST_F(ConsistencyDefaultAccountMediatorTest,
       PrefilledIdentityMatchingPreferredAccountKeepsPreferredLabel) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* identity2 = [FakeSystemIdentity fakeIdentity2];
  system_identity_manager()->AddIdentity(identity1);
  system_identity_manager()->AddIdentity(identity2);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePasswordsDesktopPreference(identity2.gaiaId));

  CreateMediator(identity2.userEmail);

  EXPECT_NSEQ(identity2, mediator_.selectedIdentity);
  std::u16string computer_string =
      l10n_util::GetStringUTF16(IDS_SIGNIN_DEVICE_TYPE_COMPUTER);
  NSString* expected_label = l10n_util::GetNSStringF(
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_DEVICE_TYPE_PASSWORDS,
      computer_string);
  EXPECT_NSEQ(expected_label, consumer_.labelText);
}

// Tests that when `prefilledEmail` selects a different identity than the
// preferred account in `AccountPreviewDataService`, `prefilledEmail` takes
// precedence and the preferred account's promo label is not shown.
TEST_F(ConsistencyDefaultAccountMediatorTest,
       PrefilledIdentityDifferentFromPreferredAccountUsesDefaultLabel) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* identity2 = [FakeSystemIdentity fakeIdentity2];
  system_identity_manager()->AddIdentity(identity1);
  system_identity_manager()->AddIdentity(identity2);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePasswordsDesktopPreference(identity1.gaiaId));

  CreateMediator(identity2.userEmail);

  EXPECT_NSEQ(identity2, mediator_.selectedIdentity);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_WEB_SIGNIN),
              consumer_.labelText);
}

// Tests that when `prefilledEmail` does not match any identity on the device,
// the mediator falls back to the preferred account.
TEST_F(ConsistencyDefaultAccountMediatorTest,
       UnknownPrefilledEmailFallsBackToPreferredAccount) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* identity2 = [FakeSystemIdentity fakeIdentity2];
  system_identity_manager()->AddIdentity(identity1);
  system_identity_manager()->AddIdentity(identity2);
  account_preview_data_service_.SetPreferredAccountForSignin(
      CreatePasswordsDesktopPreference(identity2.gaiaId));

  CreateMediator(@"unknown@example.com");

  EXPECT_NSEQ(identity2, mediator_.selectedIdentity);
  std::u16string computer_string =
      l10n_util::GetStringUTF16(IDS_SIGNIN_DEVICE_TYPE_COMPUTER);
  NSString* expected_label = l10n_util::GetNSStringF(
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_DEVICE_TYPE_PASSWORDS,
      computer_string);
  EXPECT_NSEQ(expected_label, consumer_.labelText);
}

// Tests that when a third identity is added while `prefilledEmail` selected
// `identity2`, `identity2` remains selected.
TEST_F(ConsistencyDefaultAccountMediatorTest,
       AddingIdentityKeepsPrefilledIdentitySelected) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* identity2 = [FakeSystemIdentity fakeIdentity2];
  system_identity_manager()->AddIdentity(identity1);
  system_identity_manager()->AddIdentity(identity2);

  CreateMediator(identity2.userEmail);
  EXPECT_NSEQ(identity2, mediator_.selectedIdentity);

  FakeSystemIdentity* identity3 = [FakeSystemIdentity fakeIdentity3];
  system_identity_manager()->AddIdentity(identity3);

  EXPECT_NSEQ(identity2, mediator_.selectedIdentity);
  EXPECT_NSEQ(identity2.userEmail, consumer_.email);
}

// Tests that forgetting the selected identity on the device falls back to the
// default identity.
TEST_F(ConsistencyDefaultAccountMediatorTest,
       ForgettingSelectedIdentityFallsBackToDefaultIdentity) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* identity2 = [FakeSystemIdentity fakeIdentity2];
  system_identity_manager()->AddIdentity(identity1);
  system_identity_manager()->AddIdentity(identity2);

  CreateMediator(identity2.userEmail);
  EXPECT_NSEQ(identity2, mediator_.selectedIdentity);

  system_identity_manager()->ForgetIdentityFromOtherApplication(identity2);
  system_identity_manager()->WaitForServiceCallbacksToComplete();

  EXPECT_NSEQ(identity1, mediator_.selectedIdentity);
  EXPECT_NSEQ(identity1.userEmail, consumer_.email);
}

}  // namespace
