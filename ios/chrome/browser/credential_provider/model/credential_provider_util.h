// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_UTIL_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_UTIL_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
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
NSString* GetFaviconFileKey(const GURL& url);

// Update favicons in the Chrome app group storage.
void UpdateFaviconsStorageForProfile(base::WeakPtr<ProfileIOS> weak_profile,
                                     bool fallback_to_google_server);

// Returns a dictionary where the keys are favicon file names (they are hashes
// of the associated URL) and their modification date (or creation date if
// modification date is missing).
NSDictionary<NSString*, NSDate*>* GetFaviconsListAndFreshness();

// Returns whether a favicon for 'favicon_key' should be fetched.
bool ShouldFetchFavicon(NSString* favicon_key,
                        NSDictionary<NSString*, NSDate*>* favicon_dict);

// Deletes the folder containing the favicons. Returns whether the deletion was
// successful.
bool DeleteFaviconsFolder();

// Returns whether the favicon folder is available.
bool IsFaviconFolderAvailable();

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_UTIL_H_
