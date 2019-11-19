// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_CONSUMER_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_CONSUMER_H_

#include <map>
#include <string>
#include <vector>

class GoogleServiceAuthError;
class OAuthMultiloginResult;

typedef std::map<std::string, std::string> UserInfoMap;

// An interface that defines the callbacks for objects that
// GaiaAuthFetcher can return data to.
class GaiaAuthConsumer {
 public:
  struct ClientLoginResult {
    ClientLoginResult();
    ClientLoginResult(const std::string& new_sid,
                      const std::string& new_lsid,
                      const std::string& new_token,
                      const std::string& new_data);
    ClientLoginResult(const ClientLoginResult& other);
    ~ClientLoginResult();

    bool operator==(const ClientLoginResult &b) const;

    std::string sid;
    std::string lsid;
    std::string token;
    // TODO(chron): Remove the data field later. Don't use it if possible.
    std::string data;  // Full contents of ClientLogin return.
  };

  struct ClientOAuthResult {
    ClientOAuthResult(const std::string& new_refresh_token,
                      const std::string& new_access_token,
                      int new_expires_in_secs,
                      bool is_child_account,
                      bool is_under_advanced_protection);
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
  };

  // Possible server responses to a token revocation request.
  // Used in UMA, do not delete or reorder values.
  enum class TokenRevocationStatus {
    // Token revocation succeeded.
    kSuccess = 0,
    // Network connection was canceled, no response was received.
    kConnectionCanceled = 1,
    // Network connection failed, no response was received.
    kConnectionFailed = 2,
    // Network connection timed out, no response was received.
    kConnectionTimeout = 3,
    // The token is unknown or invalid.
    kInvalidToken = 4,
    // The request was malformed.
    kInvalidRequest = 5,
    // Internal server error.
    kServerError = 6,
    // Other error.
    kUnknownError = 7,

    kMaxValue = kUnknownError
  };

  enum class ReAuthProofTokenStatus {
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
    kUnknownError = 7,
  };

  virtual ~GaiaAuthConsumer() {}

  virtual void OnClientLoginSuccess(const ClientLoginResult& result) {}
  virtual void OnClientLoginFailure(const GoogleServiceAuthError& error) {}

  virtual void OnClientOAuthCode(const std::string& auth_code) {}
  virtual void OnClientOAuthSuccess(const ClientOAuthResult& result) {}
  virtual void OnClientOAuthFailure(const GoogleServiceAuthError& error) {}

  virtual void OnOAuth2RevokeTokenCompleted(TokenRevocationStatus status) {}

  virtual void OnGetUserInfoSuccess(const UserInfoMap& data) {}
  virtual void OnGetUserInfoFailure(const GoogleServiceAuthError& error) {}

  virtual void OnUberAuthTokenSuccess(const std::string& token) {}
  virtual void OnUberAuthTokenFailure(const GoogleServiceAuthError& error) {}

  virtual void OnMergeSessionSuccess(const std::string& data) {}
  virtual void OnMergeSessionFailure(const GoogleServiceAuthError& error) {}

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
