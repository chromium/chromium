// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_util.h"

#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/signin/public/identity_manager/account_capabilities.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "google_apis/gaia/core_account_id.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/signin_util_internal.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"

namespace {

const char kAccountInfoKeyAccountId[] = "account_id";
const char kAccountInfoKeyGaia[] = "gaia";
const char kAccountInfoKeyEmail[] = "email";
const char kAccountInfoKeyFullName[] = "full_name";
const char kAccountInfoKeyGivenName[] = "given_name";
const char kAccountInfoKeyPictureUrl[] = "picture_url";
const char kHistorySyncEnabled[] = "history_sync_enabled";

// Copies a string value from a dictionary if the given key is present.
void CopyStringFromDict(std::string& to,
                        const base::Value::Dict& dict,
                        const char* key) {
  const std::string* found = dict.FindString(key);
  if (found) {
    to = *found;
  }
}

// Returns an AccountInfo from the values in a dictionary.
AccountInfo DictToAccountInfo(const base::Value::Dict& dict) {
  AccountInfo account;
  const std::string* account_id_str = dict.FindString(kAccountInfoKeyAccountId);
  if (account_id_str) {
    account.account_id = CoreAccountId::FromString(*account_id_str);
  }
  CopyStringFromDict(account.gaia, dict, kAccountInfoKeyGaia);
  CopyStringFromDict(account.email, dict, kAccountInfoKeyEmail);
  CopyStringFromDict(account.full_name, dict, kAccountInfoKeyFullName);
  CopyStringFromDict(account.given_name, dict, kAccountInfoKeyGivenName);
  CopyStringFromDict(account.picture_url, dict, kAccountInfoKeyPictureUrl);
  return account;
}

}  // namespace

NSArray* GetScopeArray(const std::set<std::string>& scopes) {
  NSMutableArray* scopes_array = [[NSMutableArray alloc] init];
  for (const auto& scope : scopes) {
    [scopes_array addObject:base::SysUTF8ToNSString(scope)];
  }
  return scopes_array;
}

bool ShouldHandleSigninError(NSError* error) {
  return ios::provider::GetSigninErrorCategory(error) !=
         ios::provider::SigninErrorCategory::kUserCancellationError;
}

CGSize GetSizeForIdentityAvatarSize(IdentityAvatarSize avatar_size) {
  CGFloat size = 0;
  switch (avatar_size) {
    case IdentityAvatarSize::TableViewIcon:
      size = 30.;
      break;
    case IdentityAvatarSize::SmallSize:
      size = 32.;
      break;
    case IdentityAvatarSize::Regular:
      size = 40.;
      break;
    case IdentityAvatarSize::Large:
      size = 48.;
      break;
  }
  DCHECK_NE(size, 0);
  return CGSizeMake(size, size);
}

signin::Tribool IsFirstSessionAfterDeviceRestore() {
  if (experimental_flags::SimulatePostDeviceRestore()) {
    return signin::Tribool::kTrue;
  }
  static signin::Tribool is_first_session_after_device_restore =
      signin::Tribool::kUnknown;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    is_first_session_after_device_restore =
        IsFirstSessionAfterDeviceRestoreInternal();
  });
  return is_first_session_after_device_restore;
}

void StorePreRestoreIdentity(PrefService* profile_pref,
                             AccountInfo account,
                             bool history_sync_enabled) {
  ScopedDictPrefUpdate update(profile_pref, prefs::kIosPreRestoreAccountInfo);
  update->Set(kAccountInfoKeyAccountId, account.account_id.ToString());
  update->Set(kAccountInfoKeyGaia, account.gaia);
  update->Set(kAccountInfoKeyEmail, account.email);
  update->Set(kAccountInfoKeyFullName, account.full_name);
  update->Set(kAccountInfoKeyGivenName, account.given_name);
  update->Set(kAccountInfoKeyPictureUrl, account.picture_url);
  update->Set(kHistorySyncEnabled, history_sync_enabled);
}

void ClearPreRestoreIdentity(PrefService* profile_pref) {
  profile_pref->ClearPref(prefs::kIosPreRestoreAccountInfo);
}

std::optional<AccountInfo> GetPreRestoreIdentity(PrefService* profile_pref) {
  const base::Value::Dict& dict =
      profile_pref->GetDict(prefs::kIosPreRestoreAccountInfo);
  if (dict.empty()) {
    return std::optional<AccountInfo>();
  }
  return DictToAccountInfo(dict);
}

bool GetPreRestoreHistorySyncEnabled(PrefService* profile_pref) {
  const base::Value::Dict& dict =
      profile_pref->GetDict(prefs::kIosPreRestoreAccountInfo);
  if (dict.empty()) {
    return false;
  }
  std::optional<bool> history_sync_enabled = dict.FindBool(kHistorySyncEnabled);
  return history_sync_enabled.value_or(false);
}

const std::vector<std::string>& GetAccountCapabilityNamesForPrefetch() {
  return AccountCapabilities::GetSupportedAccountCapabilityNames();
}

void RunSystemCapabilitiesPrefetch(NSArray<id<SystemIdentity>>* identities) {
  const std::vector<std::string>& supported_capabilities =
      GetAccountCapabilityNamesForPrefetch();
  std::set<std::string> supported_capabilities_set(
      supported_capabilities.begin(), supported_capabilities.end());

  for (id<SystemIdentity> identity : identities) {
    GetApplicationContext()->GetSystemIdentityManager()->FetchCapabilities(
        identity, supported_capabilities_set,
        base::BindOnce(^(std::map<std::string, SystemIdentityCapabilityResult>){
            // Ignore the result.
        }));
  }
}
