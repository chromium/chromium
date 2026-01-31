// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_util.h"

#import "base/check_is_test.h"
#import "base/containers/to_vector.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/signin/public/identity_manager/account_capabilities.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "google_apis/gaia/core_account_id.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/signin_util_internal.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"

namespace {

const char kAccountInfoKeyAccountId[] = "account_id";
const char kAccountInfoKeyGaia[] = "gaia";
const char kAccountInfoKeyEmail[] = "email";
const char kAccountInfoKeyFullName[] = "full_name";
const char kAccountInfoKeyGivenName[] = "given_name";
const char kAccountInfoKeyPictureUrl[] = "picture_url";
const char kHistorySyncEnabled[] = "history_sync_enabled";

// Information about the device restore. The value is loaded by
// `LoadDeviceRestoreData()`.
static std::optional<signin::RestoreData> g_restore_data;
static_assert(
    std::is_trivially_destructible<std::optional<signin::RestoreData>>::value);

// Returns a non-empty string found in `dict` by `key` or nullptr if a string is
// not found or `key` contains an empty string.
const std::string* FindStringIfNonEmpty(const base::DictValue& dict,
                                        std::string_view key) {
  const std::string* value = dict.FindString(key);
  if (!value) {
    return nullptr;
  }
  return value->empty() ? nullptr : value;
}

// Returns an AccountInfo from the values in a dictionary.
AccountInfo DictToAccountInfo(const base::DictValue& dict) {
  const std::string* gaia_id = FindStringIfNonEmpty(dict, kAccountInfoKeyGaia);
  const std::string* email = FindStringIfNonEmpty(dict, kAccountInfoKeyEmail);
  if (!gaia_id || !email) {
    return AccountInfo();
  }

  AccountInfo::Builder builder(GaiaId(*gaia_id), *email);
  if (const std::string* account_id =
          FindStringIfNonEmpty(dict, kAccountInfoKeyAccountId)) {
    builder.SetAccountId(CoreAccountId::FromString(*account_id));
  }
  if (const std::string* full_name =
          FindStringIfNonEmpty(dict, kAccountInfoKeyFullName)) {
    builder.SetFullName(*full_name);
  }
  if (const std::string* given_name =
          FindStringIfNonEmpty(dict, kAccountInfoKeyGivenName)) {
    builder.SetGivenName(*given_name);
  }
  if (const std::string* picture_url =
          FindStringIfNonEmpty(dict, kAccountInfoKeyPictureUrl)) {
    builder.SetAvatarUrl(*picture_url);
  }
  return builder.Build();
}

// Loads data related to the device restore. This method needs to be called
// before IO is disallowed on UI thread. This method is called by
// `IsFirstSessionAfterDeviceRestore()` or `LastDeviceRestoreTimestamp()`.
const signin::RestoreData& LoadDeviceRestoreData(
    base::OnceClosure completion = base::DoNothing()) {
  if (!g_restore_data.has_value()) {
    g_restore_data = LoadDeviceRestoreDataInternal(std::move(completion));
  } else {
    std::move(completion).Run();
  }
  return g_restore_data.value();
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

signin::Tribool IsFirstSessionAfterDeviceRestore(base::OnceClosure completion) {
  const signin::RestoreData& restore_data =
      LoadDeviceRestoreData(std::move(completion));
  return restore_data.is_first_session_after_device_restore;
}

std::optional<base::Time> LastDeviceRestoreTimestamp() {
  const signin::RestoreData& restore_data = LoadDeviceRestoreData();
  return restore_data.last_restore_timestamp;
}

void StorePreRestoreIdentity(PrefService* profile_pref,
                             AccountInfo account,
                             bool history_sync_enabled) {
  std::string avatar_url_to_set;
  if (std::optional<std::string_view> avatar_url = account.GetAvatarUrl()) {
    avatar_url_to_set =
        avatar_url->empty() ? kNoPictureURLFound : std::string(*avatar_url);
  }
  ScopedDictPrefUpdate update(profile_pref, prefs::kIosPreRestoreAccountInfo);
  update->Set(kAccountInfoKeyAccountId, account.GetAccountId().ToString());
  update->Set(kAccountInfoKeyGaia, account.GetGaiaId().ToString());
  update->Set(kAccountInfoKeyEmail, account.GetEmail());
  update->Set(kAccountInfoKeyFullName, account.GetFullName().value_or(""));
  update->Set(kAccountInfoKeyGivenName, account.GetGivenName().value_or(""));
  update->Set(kAccountInfoKeyPictureUrl, avatar_url_to_set);
  update->Set(kHistorySyncEnabled, history_sync_enabled);
}

void ClearPreRestoreIdentity(PrefService* profile_pref) {
  profile_pref->ClearPref(prefs::kIosPreRestoreAccountInfo);
}

std::optional<AccountInfo> GetPreRestoreIdentity(PrefService* profile_pref) {
  const base::DictValue& dict =
      profile_pref->GetDict(prefs::kIosPreRestoreAccountInfo);
  if (dict.empty()) {
    return std::optional<AccountInfo>();
  }
  return DictToAccountInfo(dict);
}

bool GetPreRestoreHistorySyncEnabled(PrefService* profile_pref) {
  const base::DictValue& dict =
      profile_pref->GetDict(prefs::kIosPreRestoreAccountInfo);
  if (dict.empty()) {
    return false;
  }
  std::optional<bool> history_sync_enabled = dict.FindBool(kHistorySyncEnabled);
  return history_sync_enabled.value_or(false);
}

base::span<const std::string_view> GetAccountCapabilityNamesForPrefetch() {
  return AccountCapabilities::GetSupportedAccountCapabilityNames();
}

void RunSystemCapabilitiesPrefetch(NSArray<id<SystemIdentity>>* identities) {
  for (id<SystemIdentity> identity : identities) {
    GetApplicationContext()->GetSystemIdentityManager()->FetchCapabilities(
        identity,
        base::ToVector(GetAccountCapabilityNamesForPrefetch(),
                       [](std::string_view sv) { return std::string(sv); }),
        base::BindOnce(^(std::map<std::string, SystemIdentityCapabilityResult>){
            // Ignore the result.
        }));
  }
}

void ResetDeviceRestoreDataForTesting() {
  CHECK_IS_TEST();
  g_restore_data.reset();
}
