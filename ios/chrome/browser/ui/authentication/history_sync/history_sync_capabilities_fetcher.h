// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_CAPABILITIES_FETCHER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_CAPABILITIES_FETCHER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"

using CapabilityFetchCompletionCallback = base::OnceCallback<void(bool)>;

class AuthenticationService;

namespace signin {
class IdentityManager;
}  // namespace signin

// Fetcher for capabilities that define properties of the History Sync Opt-In
// screen.
@interface HistorySyncCapabilitiesFetcher : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

// Stops processing callbacks and stops the async AccountInfo capability
// fetcher.
- (void)shutdown;

// Starts fetching capabilities to determine minor mode restriction status.
- (void)startFetchingRestrictionCapabilityWithCallback:
    (CapabilityFetchCompletionCallback)callback;

// Fetches available capabilities. If capabilities are not immediately ready,
// use fallback value.
- (void)fetchImmediatelyAvailableRestrictionCapabilityWithCallback:
    (CapabilityFetchCompletionCallback)callback;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_CAPABILITIES_FETCHER_H_
