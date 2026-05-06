// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AGE_MISMATCH_CAPABILITIES_FETCHER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AGE_MISMATCH_CAPABILITIES_FETCHER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "google_apis/gaia/core_account_id.h"

namespace signin {
class IdentityManager;

using CapabilityFetchCompletionCallback =
    base::OnceCallback<void(signin::Tribool)>;
}  // namespace signin

@interface AgeMismatchCapabilitiesFetcher : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIdentityManager:
    (signin::IdentityManager*)identityManager NS_DESIGNATED_INITIALIZER;

// Stops processing callbacks and stops the async AccountInfo capability
// fetcher.
- (void)shutdown;

// Starts fetching capability to determine if the user can sign in to Chrome.
- (void)startFetchingCanSignInToChromeCapabilityWithCallback:
            (signin::CapabilityFetchCompletionCallback)callback
                                                  forAccount:
                                                      (CoreAccountId)accountId;

// Returns the CanSignInToChrome capability value for the given account.
- (signin::Tribool)canSignInToChromeCapabilityForAccount:
    (const CoreAccountId&)accountId;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AGE_MISMATCH_CAPABILITIES_FETCHER_H_
