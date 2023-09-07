// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_CONSUMER_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_CONSUMER_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"

class GoogleServiceAuthError;
class OAuthMultiloginResult;

// An interface that defines the callbacks for objects that
// GaiaAuthFetcher can return data to.
class COMPONENT_EXPORT(GOOGLE_APIS) GaiaAuthConsumer {
 public:
  struct COMPONENT_EXPORT(GOOGLE_APIS) ClientOAuthResult {
    ClientOAuthResult(const std::string& refresh_token,
                      const std::string& access_token,
                      int expires_in_secs,
                      bool is_child_account,
                      bool is_under_advanced_protection,
                      bool is_bound_to_key);
    ClientOAuthResult(const ClientOAuthResult& other);
    ~ClientOAuthResult();

    bool operator==(const ClientOAuthResult &b) const;

    // OAuth2 refresh token.  Used to mint new access tokens when needed.
    std::string refresh_token;

    // OAuth2 access token.  Token to pass to endpoints that require oauth2
    // authentication.
    std::string access_token;

    // The lifespan of |access_token| in seconds.
    int expires_in_secs;

    // Whether the authenticated user is a child account.
    bool is_child_account;

    // Whether the authenticated user is in advanced protection program.
    bool is_under_advanced_protection;

    // Whether the refresh token is bound to key.
    bool is_bound_to_key;
  };

  // Possible server responses to a token revocation request.
  enum class TokenRevocationStatus {
    // Token revocation succeeded.
    kSuccess,
    // Network connection was canceled, no response was received.
    kConnectionCanceled,
    // Network connection failed, no response was received.
    kConnectionFailed,
    // Network connection timed out, no response was received.
    kConnectionTimeout,
    // The token is unknown or invalid.
    kInvalidToken,
    // The request was malformed.
    kInvalidRequest,
    // Internal server error.
    kServerError,
    // Other error.
    kUnknownError,
  };

  enum class ReAuthProofTokenStatus : int {
    // Successful request: used only to control FakeGaia response.
    kSuccess = 0,
    // Request had invalid format.
    kInvalidRequest = 1,
    // Password was incorrect.
    kInvalidGrant = 2,
    // Unauthorized OAuth client.
    kUnauthorizedClient = 3,
    // Scope of OAuth token was insufficient.
    kInsufficientScope = 4,
    // No credential specified.
    kCredentialNotSet = 5,
    // A network error.
    kNetworkError = 6,
    // Other error.
    kUnknownError = 7
  };

  virtual ~GaiaAuthConsumer() {}

  virtual void OnClientOAuthCode(const std::string& auth_code) {}
  virtual void OnClientOAuthSuccess(const ClientOAuthResult& result) {}
  virtual void OnClientOAuthFailure(const GoogleServiceAuthError& error) {}

  virtual void OnOAuth2RevokeTokenCompleted(TokenRevocationStatus status) {}

  virtual void OnListAccountsSuccess(const std::string& data) {}
  virtual void OnListAccountsFailure(const GoogleServiceAuthError& error) {}

  virtual void OnOAuthMultiloginFinished(const OAuthMultiloginResult& result) {}

  virtual void OnLogOutSuccess() {}
  virtual void OnLogOutFailure(const GoogleServiceAuthError& error) {}

  virtual void OnGetCheckConnectionInfoSuccess(const std::string& data) {}
  virtual void OnGetCheckConnectionInfoError(
      const GoogleServiceAuthError& error) {}

  virtual void OnReAuthProofTokenSuccess(
      const std::string& reauth_proof_token) {}
  virtual void OnReAuthProofTokenFailure(const ReAuthProofTokenStatus error) {}
};

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_CONSUMER_H_
