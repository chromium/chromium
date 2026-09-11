// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_UTIL_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_UTIL_H_

#import <Foundation/Foundation.h>

#import <string>
#import <string_view>

#import "base/containers/flat_map.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "url/gurl.h"

namespace password_manager {
struct PasswordForm;
}

class FaviconLoader;
class ProfileIOS;

extern const char kSyncStoreHistogramName[];

// Returns the equivalent of a unique record identifier. Built from the unique
// columns in the logins database.
NSString* RecordIdentifierForPasswordForm(
    const password_manager::PasswordForm& form);

// Fetches the favicon and saves it to the Chrome app group storage.
void FetchFaviconForURLToPath(FaviconLoader* favicon_loader,
                              const GURL& site_url,
                              NSString* filename,
                              bool skip_max_verification,
                              bool fallback_to_google_server);

// Returns the favicon file key.
std::string GetFaviconFileKey(const GURL& url);

// Returns whether `key` is a valid favicon file key.
bool IsValidFaviconFileKey(NSString* key);

// Update favicons in the Chrome app group storage.
void UpdateFaviconsStorageForProfile(base::WeakPtr<ProfileIOS> weak_profile,
                                     bool fallback_to_google_server);

// Returns a map where the keys are favicon file names (they are hashes of the
// associated URL) and values are their modification date (or creation date if
// modification date is missing).
base::flat_map<std::string, base::Time> GetFaviconsListAndFreshness();

// Returns whether a favicon for `favicon_key` should be fetched based on the
// cached freshness in `favicon_map`.
bool ShouldFetchFavicon(
    std::string_view favicon_key,
    const base::flat_map<std::string, base::Time>& favicon_map);

// Returns whether the favicon folder is available.
bool IsFaviconFolderAvailable();

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_UTIL_H_
