// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_OAUTH_TOKEN_GETTER_H_
#define REMOTING_BASE_OAUTH_TOKEN_GETTER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"

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
                                  const std::string& access_token)>
      TokenCallback;

  typedef base::RepeatingCallback<void(const std::string& user_email,
                                       const std::string& refresh_token)>
      CredentialsUpdatedCallback;

  // Called if the current refresh token is exchanged for one with new scopes.
  typedef base::RepeatingCallback<void(const std::string& refresh_token)>
      RefreshTokenUpdatedCallback;

  // This structure contains information required to perform authorization
  // with the authorization server.
  struct OAuthAuthorizationCredentials {
    // |login| is used to valdiate |refresh_token| match.
    // |is_service_account| should be True if the OAuth refresh token is for a
    // service account, False for a user account, to allow the correct client-ID
    // to be used.
    OAuthAuthorizationCredentials(const std::string& login,
                                  const std::string& refresh_token,
                                  bool is_service_account);

    ~OAuthAuthorizationCredentials();

    // The user's account name (i.e. their email address).
    std::string login;

    // Token delegating authority to us to act as the user.
    std::string refresh_token;

    // Whether these credentials belong to a service account.
    bool is_service_account;
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
                                 bool is_service_account);

    ~OAuthIntermediateCredentials();

    // Code used to check out a access token from the authrozation service.
    std::string authorization_code;

    // Override uri for oauth redirect. This is used for client accounts only
    // and is optionally set to override the default used for authentication.
    std::string oauth_redirect_uri;

    // Whether these credentials belong to a service account.
    bool is_service_account;
  };

  OAuthTokenGetter() {}
  virtual ~OAuthTokenGetter() {}

  // Call |on_access_token| with an access token, or the failure status.
  virtual void CallWithToken(
      OAuthTokenGetter::TokenCallback on_access_token) = 0;

  // Invalidates the cache, so the next CallWithToken() will get a fresh access
  // token.
  virtual void InvalidateCache() = 0;

  DISALLOW_COPY_AND_ASSIGN(OAuthTokenGetter);
};

}  // namespace remoting

#endif  // REMOTING_BASE_OAUTH_TOKEN_GETTER_H_
