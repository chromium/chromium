// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/apple/foundation_util.h"
#import "base/location.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/password_store_consumer.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/password_manager/ios/fake_bulk_leak_check_service.h"
#import "components/prefs/pref_service.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_bulk_leak_check_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/app/password_test_util.h"
#import "ios/public/provider/chrome/browser/passcode_settings/passcode_settings_api.h"
#import "url/gurl.h"
#import "url/origin.h"

using chrome_test_util::
    SetUpAndReturnMockReauthenticationModuleForPasswordManager;
using password_manager::FakeBulkLeakCheckService;
using password_manager::PasswordForm;

namespace {

constexpr char kEncrypted[] = "encrypted";

scoped_refptr<password_manager::PasswordStoreInterface>
GetPasswordProfileStore() {
  // Ensure that the fails in incognito mode by using IMPLICIT_ACCESS.
  return IOSChromeProfilePasswordStoreFactory::GetForProfile(
      chrome_test_util::GetOriginalProfile(),
      ServiceAccessType::IMPLICIT_ACCESS);
}

// Gets the account store of the password.
scoped_refptr<password_manager::PasswordStoreInterface>
GetPasswordAccountStore() {
  // Ensure that the fails in incognito mode by using IMPLICIT_ACCESS.
  return IOSChromeAccountPasswordStoreFactory::GetForProfile(
      chrome_test_util::GetOriginalProfile(),
      ServiceAccessType::IMPLICIT_ACCESS);
}

// Helper to get the passkey store.
webauthn::PasskeyModel* GetPasskeyStore() {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  return IOSPasskeyModelFactory::GetForProfile(profile);
}

// This class is used to obtain results from the PasswordStore and hence both
// check the success of store updates and ensure that store has finished
// processing.
class FakeStoreConsumer : public password_manager::PasswordStoreConsumer {
 public:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> obtained)
      override {
    obtained_ = std::move(obtained);
  }

  // Retrieves all logins from the profile password store and updates
  // `results_`. Returns true if the logins retrieved successfully.
  bool FetchProfileStoreResults() {
    results_.clear();
    ResetObtained();
    GetPasswordProfileStore()->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
    bool responded =
        base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), ^bool {
          return !AreObtainedReset();
        });
    if (responded) {
      AppendObtainedToResults();
    }
    return responded;
  }

  // Retrieves all logins from the account password store and updates
  // `results_`. Returns true if the logins retrieved successfully.
  bool FetchAccountStoreResults() {
    results_.clear();
    ResetObtained();
    GetPasswordAccountStore()->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
    bool responded =
        base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), ^bool {
          return !AreObtainedReset();
        });
    if (responded) {
      AppendObtainedToResults();
    }
    return responded;
  }

  const std::vector<password_manager::PasswordForm>& GetStoreResults() {
    return results_;
  }

 private:
  // Puts `obtained_` in a known state not corresponding to any PasswordStore
  // state.
  void ResetObtained() {
    obtained_.clear();
    obtained_.emplace_back(nullptr);
  }

  // Returns true if `obtained_` are in the reset state.
  bool AreObtainedReset() { return obtained_.size() == 1 && !obtained_[0]; }

  void AppendObtainedToResults() {
    for (const auto& source : obtained_) {
      results_.emplace_back(*source);
    }
    ResetObtained();
  }

  // Temporary cache of obtained store results.
  std::vector<std::unique_ptr<password_manager::PasswordForm>> obtained_;

  // Combination of fillable and blocked credentials from the store.
  std::vector<password_manager::PasswordForm> results_;

  base::WeakPtrFactory<FakeStoreConsumer> weak_ptr_factory_{this};
};

// Saves `form` to the password store and waits until the async processing is
// done.
bool SaveToPasswordProfileStore(const PasswordForm& form) {
  GetPasswordProfileStore()->AddLogin(form);
  // When we retrieve the form from the store, `in_store` should be set.
  password_manager::PasswordForm expected_form = form;
  expected_form.in_store = password_manager::PasswordForm::Store::kProfileStore;

  // Check the result and ensure PasswordStore processed this.
  FakeStoreConsumer consumer;
  if (!consumer.FetchProfileStoreResults()) {
    return false;
  }
  for (const auto& result : consumer.GetStoreResults()) {
    if (testing::Value(
            result, password_manager::EqualsIgnorePrimaryKey(expected_form))) {
      return true;
    }
  }
  return false;
}

// Saves `form` to the password account store and waits until the async
// processing is done.
// Returns true if `form` is saved successfully, otherwise returns false.
bool SaveToPasswordAccountStore(const PasswordForm& form) {
  GetPasswordAccountStore()->AddLogin(form);
  // When we retrieve the form from the store, `in_store` should be set.
  password_manager::PasswordForm expected_form = form;
  expected_form.in_store = password_manager::PasswordForm::Store::kAccountStore;

  // Check the result and ensure PasswordStore processed this.
  FakeStoreConsumer consumer;
  if (!consumer.FetchAccountStoreResults()) {
    return false;
  }
  for (const auto& result : consumer.GetStoreResults()) {
    if (testing::Value(
            result, password_manager::EqualsIgnorePrimaryKey(expected_form))) {
      return true;
    }
  }
  return false;
}

// Creates a PasswordForm with `index` being part of the username, password,
// origin and realm.
PasswordForm CreateSampleFormWithIndex(int index) {
  PasswordForm form;
  form.username_value =
      base::ASCIIToUTF16(base::StringPrintf("concrete username %02d", index));
  form.password_value =
      base::ASCIIToUTF16(base::StringPrintf("concrete password %02d", index));
  form.url = GURL(base::StringPrintf("https://www%02d.example.com", index));
  form.signon_realm = form.url.spec();
  form.date_created = base::Time::Now();
  return form;
}

bool ClearProfilePasswordStore() {
  GetPasswordProfileStore()->RemoveLoginsCreatedBetween(
      FROM_HERE, base::Time(), base::Time(), base::DoNothing());
  FakeStoreConsumer consumer;
  if (!consumer.FetchProfileStoreResults()) {
    return false;
  }
  return consumer.GetStoreResults().empty();
}

bool ClearAccountPasswordStore() {
  GetPasswordAccountStore()->RemoveLoginsCreatedBetween(
      FROM_HERE, base::Time(), base::Time(), base::DoNothing());
  FakeStoreConsumer consumer;
  if (!consumer.FetchAccountStoreResults()) {
    return false;
  }
  return consumer.GetStoreResults().empty();
}

bool ClearPasswordStores() {
  GetPasswordProfileStore()->RemoveLoginsCreatedBetween(
      FROM_HERE, base::Time(), base::Time(), base::DoNothing());
  GetPasswordAccountStore()->RemoveLoginsCreatedBetween(
      FROM_HERE, base::Time(), base::Time(), base::DoNothing());
  FakeStoreConsumer consumer;
  if (!consumer.FetchProfileStoreResults()) {
    return false;
  }
  if (!consumer.FetchAccountStoreResults()) {
    return false;
  }
  return consumer.GetStoreResults().empty();
}

}  // namespace

@implementation PasswordSettingsAppInterface

static std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
    _scopedReauthOverride;

// Helper for accessing the scoped override's module.
+ (MockReauthenticationModule*)mockModule {
  DCHECK(_scopedReauthOverride);

  return base::apple::ObjCCastStrict<MockReauthenticationModule>(
      _scopedReauthOverride->module);
}

+ (void)setUpMockReauthenticationModule {
  _scopedReauthOverride =
      SetUpAndReturnMockReauthenticationModuleForPasswordManager();
}

+ (void)removeMockReauthenticationModule {
  _scopedReauthOverride = nullptr;
}

+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult {
  [self mockModule].expectedResult = expectedResult;
}

+ (void)mockReauthenticationModuleCanAttempt:(BOOL)canAttempt {
  DCHECK(_scopedReauthOverride);

  [self mockModule].canAttempt = canAttempt;
}

+ (void)mockReauthenticationModuleShouldSkipReAuth:(BOOL)returnSync {
  [self mockModule].shouldSkipReAuth = returnSync;
}

+ (void)mockReauthenticationModuleReturnMockedResult {
  [[self mockModule] returnMockedReauthenticationResult];
}

+ (void)dismissSnackBar {
  [MDCSnackbarManager.defaultManager
      dismissAndCallCompletionBlocksWithCategory:@"PasswordsSnackbarCategory"];
}

+ (void)saveExamplePasswordToProfileWithCount:(NSInteger)count {
  for (int i = 1; i <= count; ++i) {
    GetPasswordProfileStore()->AddLogin(CreateSampleFormWithIndex(i));
  }
}

+ (BOOL)saveExamplePasswordToProfileStore:(NSString*)password
                                 username:(NSString*)username
                                   origin:(NSString*)origin {
  PasswordForm example;
  example.username_value = base::SysNSStringToUTF16(username);
  example.password_value = base::SysNSStringToUTF16(password);
  example.url = GURL(base::SysNSStringToUTF16(origin));
  example.signon_realm = example.url.spec();
  example.date_created = base::Time::Now();
  return SaveToPasswordProfileStore(example);
}

+ (BOOL)saveExamplePasswordToAccountStore:(NSString*)password
                                 username:(NSString*)username
                                   origin:(NSString*)origin {
  PasswordForm example;
  example.username_value = base::SysNSStringToUTF16(username);
  example.password_value = base::SysNSStringToUTF16(password);
  example.url = GURL(base::SysNSStringToUTF16(origin));
  example.signon_realm = example.url.spec();
  example.date_created = base::Time::Now();
  return SaveToPasswordAccountStore(example);
}

+ (BOOL)saveExampleNoteToProfileStore:(NSString*)note
                             password:(NSString*)password
                             username:(NSString*)username
                               origin:(NSString*)origin {
  PasswordForm example;
  example.username_value = base::SysNSStringToUTF16(username);
  example.password_value = base::SysNSStringToUTF16(password);
  example.url = GURL(base::SysNSStringToUTF16(origin));
  example.signon_realm = example.url.spec();
  example.notes = {password_manager::PasswordNote(
      base::SysNSStringToUTF16(note), base::Time::Now())};
  return SaveToPasswordProfileStore(example);
}

+ (BOOL)saveCompromisedPasswordToProfileStore:(NSString*)password
                                     username:(NSString*)username
                                       origin:(NSString*)origin {
  PasswordForm example;
  example.username_value = base::SysNSStringToUTF16(username);
  example.password_value = base::SysNSStringToUTF16(password);
  example.url = GURL(base::SysNSStringToUTF16(origin));
  example.signon_realm = example.url.spec();
  example.password_issues.insert({password_manager::InsecureType::kLeaked,
                                  password_manager::InsecurityMetadata()});
  return SaveToPasswordProfileStore(example);
}

+ (BOOL)saveMutedCompromisedPasswordToProfileStore:(NSString*)password
                                          username:(NSString*)userName
                                            origin:(NSString*)origin {
  PasswordForm example;
  example.username_value = base::SysNSStringToUTF16(userName);
  example.password_value = base::SysNSStringToUTF16(password);
  example.url = GURL(base::SysNSStringToUTF16(origin));
  example.signon_realm = example.url.spec();
  example.password_issues.insert(
      {password_manager::InsecureType::kLeaked,
       password_manager::InsecurityMetadata(
           base::Time::Now(), password_manager::IsMuted(true),
           password_manager::TriggerBackendNotification(false))});
  return SaveToPasswordProfileStore(example);
}

+ (BOOL)saveExampleBlockedOriginToProfileStore:(NSString*)origin {
  PasswordForm example;
  example.url = GURL(base::SysNSStringToUTF16(origin));
  example.blocked_by_user = true;
  example.signon_realm = example.url.spec();
  return SaveToPasswordProfileStore(example);
}

+ (BOOL)saveExampleFederatedOriginToProfileStore:(NSString*)federatedOrigin
                                        username:(NSString*)username
                                          origin:(NSString*)origin {
  PasswordForm federated;
  federated.username_value = base::SysNSStringToUTF16(username);
  federated.url = GURL(base::SysNSStringToUTF16(origin));
  federated.signon_realm = federated.url.spec();
  federated.federation_origin =
      url::SchemeHostPort(GURL(base::SysNSStringToUTF16(federatedOrigin)));
  return SaveToPasswordProfileStore(federated);
}

+ (void)saveExamplePasskeyToStore:(NSString*)rpId
                           userId:(NSString*)userId
                         username:(NSString*)username
                  userDisplayName:(NSString*)userDisplayName {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_credential_id(base::RandBytesAsString(16));
  passkey.set_rp_id(base::SysNSStringToUTF8(rpId));
  passkey.set_user_id(base::SysNSStringToUTF8(userId));
  passkey.set_user_name(base::SysNSStringToUTF8(username));
  passkey.set_user_display_name(base::SysNSStringToUTF8(userDisplayName));
  passkey.set_encrypted(kEncrypted);
  GetPasskeyStore()->AddNewPasskeyForTesting(passkey);
}

+ (NSInteger)passwordProfileStoreResultsCount {
  FakeStoreConsumer consumer;
  if (!consumer.FetchProfileStoreResults()) {
    return -1;
  }
  return consumer.GetStoreResults().size();
}

+ (NSInteger)passwordAccountStoreResultsCount {
  FakeStoreConsumer consumer;
  if (!consumer.FetchAccountStoreResults()) {
    return -1;
  }
  return consumer.GetStoreResults().size();
}

+ (BOOL)clearProfilePasswordStore {
  return ClearProfilePasswordStore();
}

+ (BOOL)clearAccountPasswordStore {
  return ClearAccountPasswordStore();
}

+ (BOOL)clearPasswordStores {
  return ClearPasswordStores();
}

+ (BOOL)isCredentialsServiceEnabled {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  return profile->GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);
}

+ (void)setFakeBulkLeakCheckBufferedState:
    (password_manager::BulkLeakCheckServiceInterface::State)state {
  FakeBulkLeakCheckService* fakeBulkLeakCheckService =
      static_cast<FakeBulkLeakCheckService*>(
          IOSChromeBulkLeakCheckServiceFactory::GetForProfile(
              chrome_test_util::GetOriginalProfile()));
  fakeBulkLeakCheckService->SetBufferedState(state);
}

+ (BOOL)isPasscodeSettingsAvailable {
  return ios::provider::SupportsPasscodeSettings();
}

@end
