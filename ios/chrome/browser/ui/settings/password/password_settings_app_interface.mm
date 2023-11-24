// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/apple/foundation_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/password_store_consumer.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/password_manager/ios/fake_bulk_leak_check_service.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_bulk_leak_check_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
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

scoped_refptr<password_manager::PasswordStoreInterface> GetPasswordStore() {
  // ServiceAccessType governs behaviour in Incognito: only modifications with
  // EXPLICIT_ACCESS, which correspond to user's explicit gesture, succeed.
  // This test does not deal with Incognito, and should not run in Incognito
  // context. Therefore IMPLICIT_ACCESS is used to let the test fail if in
  // Incognito context.
  return IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState(),
      ServiceAccessType::IMPLICIT_ACCESS);
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

  bool FetchStoreResults() {
    results_.clear();
    ResetObtained();
    GetPasswordStore()->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
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
bool SaveToPasswordStore(const PasswordForm& form) {
  GetPasswordStore()->AddLogin(form);
  // When we retrieve the form from the store, `in_store` should be set.
  password_manager::PasswordForm expected_form = form;
  expected_form.in_store = password_manager::PasswordForm::Store::kProfileStore;

  // Check the result and ensure PasswordStore processed this.
  FakeStoreConsumer consumer;
  if (!consumer.FetchStoreResults()) {
    return false;
  }
  for (const auto& result : consumer.GetStoreResults()) {
    if (result == expected_form)
      return true;
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
  return form;
}

bool ClearPasswordStore() {
  GetPasswordStore()->RemoveLoginsCreatedBetween(base::Time(), base::Time(),
                                                 base::DoNothing());
  FakeStoreConsumer consumer;
  if (!consumer.FetchStoreResults()) {
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

+ (void)mockReauthenticationModuleShouldReturnSynchronously:(BOOL)returnSync {
  [self mockModule].shouldReturnSynchronously = returnSync;
}

+ (void)mockReauthenticationModuleReturnMockedResult {
  [[self mockModule] returnMockedReauthenticationResult];
}

+ (void)dismissSnackBar {
  [MDCSnackbarManager.defaultManager
      dismissAndCallCompletionBlocksWithCategory:@"PasswordsSnackbarCategory"];
}

+ (void)saveExamplePasswordWithCount:(NSInteger)count {
  for (int i = 1; i <= count; ++i) {
    GetPasswordStore()->AddLogin(CreateSampleFormWithIndex(i));
  }
}

+ (BOOL)saveExamplePassword:(NSString*)password
                   username:(NSString*)username
                     origin:(NSString*)origin {
  PasswordForm example;
  example.username_value = base::SysNSStringToUTF16(username);
  example.password_value = base::SysNSStringToUTF16(password);
  example.url = GURL(base::SysNSStringToUTF16(origin));
  example.signon_realm = example.url.spec();
  return SaveToPasswordStore(example);
}

+ (BOOL)saveExampleNote:(NSString*)note
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
  return SaveToPasswordStore(example);
}

+ (BOOL)saveCompromisedPassword:(NSString*)password
                       username:(NSString*)username
                         origin:(NSString*)origin {
  PasswordForm example;
  example.username_value = base::SysNSStringToUTF16(username);
  example.password_value = base::SysNSStringToUTF16(password);
  example.url = GURL(base::SysNSStringToUTF16(origin));
  example.signon_realm = example.url.spec();
  example.password_issues.insert({password_manager::InsecureType::kLeaked,
                                  password_manager::InsecurityMetadata()});
  return SaveToPasswordStore(example);
}

+ (BOOL)saveMutedCompromisedPassword:(NSString*)password
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
  return SaveToPasswordStore(example);
}

+ (BOOL)saveExampleBlockedOrigin:(NSString*)origin {
  PasswordForm example;
  example.url = GURL(base::SysNSStringToUTF16(origin));
  example.blocked_by_user = true;
  example.signon_realm = example.url.spec();
  return SaveToPasswordStore(example);
}

+ (BOOL)saveExampleFederatedOrigin:(NSString*)federatedOrigin
                          username:(NSString*)username
                            origin:(NSString*)origin {
  PasswordForm federated;
  federated.username_value = base::SysNSStringToUTF16(username);
  federated.url = GURL(base::SysNSStringToUTF16(origin));
  federated.signon_realm = federated.url.spec();
  federated.federation_origin =
      url::Origin::Create(GURL(base::SysNSStringToUTF16(federatedOrigin)));
  return SaveToPasswordStore(federated);
}

+ (NSInteger)passwordStoreResultsCount {
  FakeStoreConsumer consumer;
  if (!consumer.FetchStoreResults()) {
    return -1;
  }
  return consumer.GetStoreResults().size();
}

+ (BOOL)clearPasswordStore {
  return ClearPasswordStore();
}

+ (BOOL)isCredentialsServiceEnabled {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  return browserState->GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);
}

+ (void)setFakeBulkLeakCheckBufferedState:
    (password_manager::BulkLeakCheckServiceInterface::State)state {
  FakeBulkLeakCheckService* fakeBulkLeakCheckService =
      static_cast<FakeBulkLeakCheckService*>(
          IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(
              chrome_test_util::GetOriginalBrowserState()));
  fakeBulkLeakCheckService->SetBufferedState(state);
}

+ (BOOL)isPasscodeSettingsAvailable {
  return ios::provider::SupportsPasscodeSettings();
}

@end
