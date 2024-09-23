// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_
#define GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"

namespace network {
class SharedURLLoaderFactory;
}

// A helper class to get and refresh OAuth2 refresh and access tokens.
// Also exposes utility methods for fetching user email and token information.
//
// Supports one request at a time; for parallel requests, create multiple
// instances.
namespace gaia {

struct COMPONENT_EXPORT(GOOGLE_APIS) OAuthClientInfo {
  std::string client_id;
  std::string client_secret;
  std::string redirect_uri;
};

class COMPONENT_EXPORT(GOOGLE_APIS) GaiaOAuthClient {
 public:
  class COMPONENT_EXPORT(GOOGLE_APIS) Delegate {
   public:
    // Invoked on a successful response to the GetTokensFromAuthCode request.
    virtual void OnGetTokensResponse(const std::string& refresh_token,
                                     const std::string& access_token,
                                     int expires_in_seconds) {}
    // Invoked on a successful response to the RefreshToken request.
    virtual void OnRefreshTokenResponse(const std::string& access_token,
                                        int expires_in_seconds) {}
    // Invoked on a successful response to the GetUserInfo request.
    virtual void OnGetUserEmailResponse(const std::string& user_email) {}
    // Invoked on a successful response to the GetUserId request.
    virtual void OnGetUserIdResponse(const std::string& user_id) {}
    // Invoked on a successful response to the GetUserInfo request.
    virtual void OnGetUserInfoResponse(const base::Value::Dict& user_info) {}
    // Invoked on a successful response to the GetTokenInfo request.
    virtual void OnGetTokenInfoResponse(const base::Value::Dict& token_info) {}
    virtual void OnGetAccountCapabilitiesResponse(
        const base::Value::Dict& account_capabilities) {}
    // Invoked when there is an OAuth error with one of the requests.
    virtual void OnOAuthError() = 0;
    // Invoked when there is a network error or upon receiving an invalid
    // response. This is invoked when the maximum number of retries have been
    // exhausted. If max_retries is -1, this is never invoked.
    virtual void OnNetworkError(int response_code) = 0;

   protected:
    virtual ~Delegate() {}
  };

  GaiaOAuthClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  GaiaOAuthClient(const GaiaOAuthClient&) = delete;
  GaiaOAuthClient& operator=(const GaiaOAuthClient&) = delete;

  ~GaiaOAuthClient();

  // In the below methods, |max_retries| specifies the maximum number of times
  // we should retry on a network error in invalid response. This does not
  // apply in the case of an OAuth error (i.e. there was something wrong with
  // the input arguments). Setting |max_retries| to -1 implies infinite retries.

  // Given an OAuth2 authorization code, fetch the long-lived refresh token
  // and a valid access token. After the access token expires, RefreshToken()
  // can be used to fetch a fresh access token. See |max_retries| docs above.
  void GetTokensFromAuthCode(const OAuthClientInfo& oauth_client_info,
                             const std::string& auth_code,
                             int max_retries,
                             Delegate* delegate);

  // Given a valid refresh token (usually fetched via
  // |GetTokensFromAuthCode()|), fetch a fresh access token that can be used
  // to authenticate an API call. If |scopes| is non-empty, then fetch an
  // access token for those specific scopes (assuming the refresh token has the
  // appropriate permissions). See |max_retries| docs above.
  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    const std::vector<std::string>& scopes,
                    int max_retries,
                    Delegate* delegate);

  // Call the userinfo API, returning the user email address associated
  // with the given access token. The provided access token must have
  // https://www.googleapis.com/auth/userinfo.email as one of its scopes.
  // See |max_retries| docs above.
  void GetUserEmail(const std::string& oauth_access_token,
                    int max_retries,
                    Delegate* delegate);

  // Call the userinfo API, returning the user gaia ID associated
  // with the given access token. The provided access token must have
  // https://www.googleapis.com/auth/userinfo.email as one of its scopes.
  // See |max_retries| docs above.
  void GetUserId(const std::string& oauth_access_token,
                 int max_retries,
                 Delegate* delegate);

  // Call the userinfo API, returning all the user info associated
  // with the given access token. The provided access token must have
  // https://www.googleapis.com/auth/userinfo.profile in its scopes.  If
  // email addresses are also to be retrieved, then
  // https://www.googleapis.com/auth/userinfo.email must also be specified.
  // See |max_retries| docs above.
  void GetUserInfo(const std::string& oauth_access_token,
                   int max_retries,
                   Delegate* delegate);

  // Call the tokeninfo API, returning a dictionary of response values. The
  // provided access token may have any scope, and basic results will be
  // returned: issued_to, audience, scope, expires_in, access_type. In
  // addition, if the https://www.googleapis.com/auth/userinfo.email scope is
  // present, the email and verified_email fields will be returned. If the
  // https://www.googleapis.com/auth/userinfo.profile scope is present, the
  // user_id field will be returned. See |max_retries| docs above.
  void GetTokenInfo(const std::string& oauth_access_token,
                    int max_retries,
                    Delegate* delegate);

  // Call the tokeninfo API for given |token_handle|, returning a dictionary of
  // response values. Basic results will be returned via
  // |OnGetTokenInfoResponse| call: audience, expires_in, user_id. See
  // |max_retries| docs above.
  void GetTokenHandleInfo(const std::string& token_handle,
                          int max_retries,
                          Delegate* delegate);

  // Call the account capabilities API, returning a dictionary of response
  // values. Only fetches values for capabilities listed in
  // |capabilities_names|. The provided access token must have
  // https://www.googleapis.com/auth/account.capabilities in its scopes. See
  // |max_retries| docs above.
  void GetAccountCapabilities(
      const std::string& oauth_access_token,
      const std::vector<std::string>& capabilities_names,
      int max_retries,
      Delegate* delegate);

 private:
  // The guts of the implementation live in this class.
  class Core;
  scoped_refptr<Core> core_;
};

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_
