// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_UTIL_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_UTIL_H_

#import <Foundation/Foundation.h>

#include "components/password_manager/core/browser/password_form.h"

class FaviconLoader;

// Returns the equivalent of a unique record identifier. Built from the unique
// columns in the logins database.
NSString* RecordIdentifierForPasswordForm(
    const password_manager::PasswordForm& form);

// Fetch the favicon and save it to the Chrome app group storage.
void FetchFaviconForURLToPath(FaviconLoader* favicon_loader,
                              const GURL& site_url,
                              NSString* filename);

// Returns the favicon file key.
NSString* GetFaviconFileKey(const GURL& url);

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_UTIL_H_
