// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_profile_oauth2_token_service_ios_provider_impl.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"
#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"
#import "ios/web_view/public/cwv_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebViewProfileOAuth2TokenServiceIOSProviderImpl::
    WebViewProfileOAuth2TokenServiceIOSProviderImpl(
        IOSWebViewSigninClient* signin_client)
    : signin_client_(signin_client) {}

WebViewProfileOAuth2TokenServiceIOSProviderImpl::
    ~WebViewProfileOAuth2TokenServiceIOSProviderImpl() = default;

void WebViewProfileOAuth2TokenServiceIOSProviderImpl::GetAccessToken(
    const std::string& gaia_id,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    const AccessTokenCallback& callback) {
  // |sync_controller| may still be nil if this is called too early so
  // |callback| will not be invoked. That's OK because this will be called again
  // after |sync_controller| has been set.
  CWVSyncController* sync_controller = signin_client_->GetSyncController();
  [sync_controller fetchAccessTokenForScopes:scopes callback:callback];
}

std::vector<ProfileOAuth2TokenServiceIOSProvider::AccountInfo>
WebViewProfileOAuth2TokenServiceIOSProviderImpl::GetAllAccounts() const {
  // |sync_controller| may still be nil if this is called too early. That's OK
  // because this will be called again after it has been set.
  CWVSyncController* sync_controller = signin_client_->GetSyncController();
  CWVIdentity* current_identity = sync_controller.currentIdentity;
  if (current_identity) {
    AccountInfo account_info;
    account_info.email = base::SysNSStringToUTF8(current_identity.email);
    account_info.gaia = base::SysNSStringToUTF8(current_identity.gaiaID);
    return {account_info};
  }
  return {};
}

AuthenticationErrorCategory
WebViewProfileOAuth2TokenServiceIOSProviderImpl::GetAuthenticationErrorCategory(
    const std::string& gaia_id,
    NSError* error) const {
  // TODO(crbug.com/780937): Implement fully.
  return kAuthenticationErrorCategoryUnknownErrors;
}
