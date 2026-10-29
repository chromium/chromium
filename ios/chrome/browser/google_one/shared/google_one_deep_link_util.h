// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_ONE_SHARED_GOOGLE_ONE_DEEP_LINK_UTIL_H_
#define IOS_CHROME_BROWSER_GOOGLE_ONE_SHARED_GOOGLE_ONE_DEEP_LINK_UTIL_H_

#import <Foundation/Foundation.h>

#import "google_apis/gaia/gaia_id.h"

class GURL;
@protocol SystemIdentity;

// Returns true if `url` is a handled Google One deep link URL.
// If handled and `out_url` is non-null, populates `out_url` with the normalized
// HTTPS GURL.
bool IsGoogleOneDeepLinkURL(const GURL& url, GURL* out_url);

// Returns the account string specified in the Google One deep link `url`.
// The returned string does not necessarily correspond to an account on the
// device.
NSString* GoogleOneAccountFromURL(const GURL& url);

// Returns the SystemIdentity matching `account_param` on the device, or nil if
// no match is found.
// TODO(crbug.com/576575308): Currently ignores `kRestrictAccountsToPatterns`
// and can return an identity on the device that is filtered by
// `kRestrictAccountsToPatterns`.
id<SystemIdentity> FindIdentityForGoogleOneAccount(NSString* account_param);

// Returns the GaiaId matching `account_param` on the device, or an empty GaiaId
// if no match is found.
// TODO(crbug.com/576575308): Currently ignores `kRestrictAccountsToPatterns`
// and can return the `GaiaId` of an account on the device that is filtered by
// `kRestrictAccountsToPatterns`.
GaiaId FindGaiaIdForGoogleOneAccount(NSString* account_param);

#endif  // IOS_CHROME_BROWSER_GOOGLE_ONE_SHARED_GOOGLE_ONE_DEEP_LINK_UTIL_H_
