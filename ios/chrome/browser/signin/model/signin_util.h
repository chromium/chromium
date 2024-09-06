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
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

class PrefService;

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
// needs to be called for the first time before IO is disallowed on UI thread.
// The value is cached. The result is cached for later calls.
signin::Tribool IsFirstSessionAfterDeviceRestore();

// Stores a user's account info and if history sync was enabled or not, when we
// detect that it was forgotten during a device restore.
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
const std::vector<std::string>& GetAccountCapabilityNamesForPrefetch();

// Pre-fetches system capabilities for the given identities so that they
// can be cached for later usage.
void RunSystemCapabilitiesPrefetch(NSArray<id<SystemIdentity>>* identities);

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_H_
