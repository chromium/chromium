// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_

#include <set>

#include "components/signin/core/browser/signin_metrics.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/web_view/internal/signin/web_view_profile_oauth2_token_service_ios_provider_impl.h"
#import "ios/web_view/public/cwv_sync_controller.h"

NS_ASSUME_NONNULL_BEGIN

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync

class AccountTrackerService;
class SigninManager;
class ProfileOAuth2TokenService;

@interface CWVSyncController ()

// All dependencies must out live this class.
- (instancetype)
initWithProfileSyncService:(browser_sync::ProfileSyncService*)profileSyncService
     accountTrackerService:(AccountTrackerService*)accountTrackerService
             signinManager:(SigninManager*)signinManager
              tokenService:(ProfileOAuth2TokenService*)tokenService
    NS_DESIGNATED_INITIALIZER;

// Called by WebViewProfileOAuth2TokenServiceIOSProviderImpl to obtain
// access tokens for |scopes| to be passed back in |callback|.
- (void)fetchAccessTokenForScopes:(const std::set<std::string>&)scopes
                         callback:(const ProfileOAuth2TokenServiceIOSProvider::
                                       AccessTokenCallback&)callback;

// Called by IOSWebViewSigninClient when signing out.
- (void)didSignoutWithSourceMetric:(signin_metrics::ProfileSignout)metric;

// Called by IOSWebViewSigninClient when auth error changes.
- (void)didUpdateAuthError:(const GoogleServiceAuthError&)authError;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_
