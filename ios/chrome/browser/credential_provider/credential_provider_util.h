// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_UTIL_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_UTIL_H_

#import <Foundation/Foundation.h>

#include "components/password_manager/core/browser/password_form.h"

class ChromeBrowserState;
class FaviconLoader;

// Returns the equivalent of a unique record identifier. Built from the unique
// columns in the logins database.
NSString* RecordIdentifierForPasswordForm(
    const password_manager::PasswordForm& form);

// Fetches the favicon and saves it to the Chrome app group storage.
void FetchFaviconForURLToPath(FaviconLoader* favicon_loader,
                              const GURL& site_url,
                              NSString* filename,
                              bool skip_max_verification,
                              bool sync_enabled);

// Returns the favicon file key.
NSString* GetFaviconFileKey(const GURL& url);

// Update favicons in the Chrome app group storage.
void UpdateFaviconsStorageForBrowserState(
    base::WeakPtr<ChromeBrowserState> weak_browser_state,
    bool sync_enabled);

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_UTIL_H_
