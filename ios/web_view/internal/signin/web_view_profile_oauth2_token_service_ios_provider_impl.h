// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_PROVIDER_IMPL_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_PROVIDER_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_provider.h"

class IOSWebViewSigninClient;

// Implementation of ProfileOAuth2TokenServiceIOSProvider.
class WebViewProfileOAuth2TokenServiceIOSProviderImpl
    : public ProfileOAuth2TokenServiceIOSProvider {
 public:
  // |signin_client| used to fetch access tokens.
  explicit WebViewProfileOAuth2TokenServiceIOSProviderImpl(
      IOSWebViewSigninClient* signin_client);
  ~WebViewProfileOAuth2TokenServiceIOSProviderImpl() override;

  // ios::ProfileOAuth2TokenServiceIOSProvider
  void GetAccessToken(const std::string& gaia_id,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      const AccessTokenCallback& callback) override;
  std::vector<AccountInfo> GetAllAccounts() const override;
  AuthenticationErrorCategory GetAuthenticationErrorCategory(
      const std::string& gaia_id,
      NSError* error) const override;

 private:
  // Used to obtain access tokens in |GetAccessToken|.
  IOSWebViewSigninClient* const signin_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebViewProfileOAuth2TokenServiceIOSProviderImpl);
};

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_PROVIDER_IMPL_H_
