// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_H_

#import <UIKit/UIKit.h>

#include <map>
#include <optional>
#include <set>
#include <string>

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

class PrefService;

namespace base {
class Time;
}  // namespace base
namespace signin {
enum class Tribool;
}  // namespace signin

// Returns an NSArray of `scopes` as NSStrings.
NSArray* GetScopeArray(const std::set<std::string>& scopes);

// Returns whether the given signin `error` should be handled.
//
// Note that cancel errors and errors handled internally by the signin component
// should not be handled.
bool ShouldHandleSigninError(NSError* error);

// Returns CGSize based on `IdentityAvatarSize`.
CGSize GetSizeForIdentityAvatarSize(IdentityAvatarSize avatar_size);

// Returns whether Chrome has been started after a device restore. This method
// needs to be called once before IO is disallowed on UI thread (or
// `LastDeviceRestoreTimestamp()`).
// `completion` is called once all sentinel files are created.
signin::Tribool IsFirstSessionAfterDeviceRestore(
    base::OnceClosure completion = base::DoNothing());

// Returns the last device restore timestamp. This method needs to be called
// once before IO is disallowed on UI thread (or
// `LastDeviceRestoreTimestamp()`).
// No value is returned:
//   - if the current session after the device restore,
//   - or if, no device restore was done,
//   - or if, it was not possible to know if a device restore was done.
std::optional<base::Time> LastDeviceRestoreTimestamp();

// Stores a user's account info and whether history sync was enabled or not,
// when we detect that it was forgotten during a device restore.
void StorePreRestoreIdentity(PrefService* profile_pref,
                             AccountInfo account,
                             bool history_sync_enabled);

// Clears the identity that was signed-in before the restore.
void ClearPreRestoreIdentity(PrefService* profile_pref);

// Returns the identity that was signed-in before the restore, but is now
// not signed-in.
std::optional<AccountInfo> GetPreRestoreIdentity(PrefService* profile_pref);

// Returns whether history sync was enabled before the restore.
bool GetPreRestoreHistorySyncEnabled(PrefService* profile_pref);

// Returns the list of account capability service names supported in Chrome.
// This is exposed to allow for prefetching capabilities on app startup.
base::span<const std::string_view> GetAccountCapabilityNamesForPrefetch();

// Pre-fetches system capabilities for the given identities so that they
// can be cached for later usage.
void RunSystemCapabilitiesPrefetch(NSArray<id<SystemIdentity>>* identities);

// Resets the data related to device restore. This is for test only.
void ResetDeviceRestoreDataForTesting();

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_H_
