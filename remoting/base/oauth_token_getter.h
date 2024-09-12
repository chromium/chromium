// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_OAUTH_TOKEN_GETTER_H_
#define REMOTING_BASE_OAUTH_TOKEN_GETTER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace remoting {

// OAuthTokenGetter caches OAuth access tokens and refreshes them as needed.
class OAuthTokenGetter {
 public:
  // Status of the refresh token attempt.
  enum Status {
    // Success, credentials in user_email/access_token.
    SUCCESS,
    // Network failure (caller may retry).
    NETWORK_ERROR,
    // Authentication failure (permanent).
    AUTH_ERROR,
  };

  typedef base::OnceCallback<void(Status status,
                                  const std::string& user_email,
                                  const std::string& access_token,
                                  const std::string& scopes)>
      TokenCallback;

  // This structure contains information required to perform authorization
  // with the authorization server.
  struct OAuthAuthorizationCredentials {
    // |login| is used to validate |refresh_token| match.
    // |is_service_account| should be True if the OAuth refresh token is for a
    // service account, False for a user account, to allow the correct client-ID
    // to be used.
    OAuthAuthorizationCredentials(const std::string& login,
                                  const std::string& refresh_token,
                                  bool is_service_account,
                                  std::vector<std::string> scopes = {});

    ~OAuthAuthorizationCredentials();

    // The user's account name (i.e. their email address).
    std::string login;

    // Token delegating authority to us to act as the user.
    std::string refresh_token;

    // Whether these credentials belong to a service account.
    bool is_service_account;

    // The scopes for the token to be fetched. If unset, the scopes from the
    // refresh token will be used.
    std::vector<std::string> scopes;
  };

  // This structure contains information required to perform authentication
  // with the authorization server.
  struct OAuthIntermediateCredentials {
    // |authorization_code| is a one time use code used to authenticate with
    // the authorization server.
    // |is_service_account| should be True if the OAuth refresh token is for a
    // service account, False for a user account, to allow the correct client-ID
    // to be used.
    OAuthIntermediateCredentials(const std::string& authorization_code,
                                 bool is_service_account,
                                 std::vector<std::string> scopes = {});

    ~OAuthIntermediateCredentials();

    // Code used to exchange for an access token from the authorization service.
    std::string authorization_code;

    // Override uri for oauth redirect. This is used for client accounts only
    // and is optionally set to override the default used for authentication.
    std::string oauth_redirect_uri;

    // Whether these credentials belong to a service account.
    bool is_service_account;

    // The scopes for the token to be fetched. If unset, the scopes from the
    // access token will be used.
    std::vector<std::string> scopes;
  };

  OAuthTokenGetter() {}

  OAuthTokenGetter(const OAuthTokenGetter&) = delete;
  OAuthTokenGetter& operator=(const OAuthTokenGetter&) = delete;

  virtual ~OAuthTokenGetter() {}

  // Call |on_access_token| with an access token, or the failure status.
  virtual void CallWithToken(
      OAuthTokenGetter::TokenCallback on_access_token) = 0;

  // Invalidates the cache, so the next CallWithToken() will get a fresh access
  // token.
  virtual void InvalidateCache() = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_OAUTH_TOKEN_GETTER_H_
