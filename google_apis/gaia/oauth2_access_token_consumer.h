// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_CONSUMER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_CONSUMER_H_

#include <string>

#include "base/component_export.h"
#include "base/time/time.h"

class GoogleServiceAuthError;

// An interface that defines the callbacks for consumers to which
// OAuth2AccessTokenFetcher can return results.
class COMPONENT_EXPORT(GOOGLE_APIS) OAuth2AccessTokenConsumer {
 public:
  // Structure representing information contained in OAuth2 access token.
  struct COMPONENT_EXPORT(GOOGLE_APIS) TokenResponse {
    TokenResponse();
    TokenResponse(const TokenResponse& response);
    TokenResponse(TokenResponse&& response);
    ~TokenResponse();
    TokenResponse& operator=(const TokenResponse& response);
    TokenResponse& operator=(TokenResponse&& response);

    // OAuth2 access token.
    std::string access_token;

    // OAuth2 refresh token.  May be empty.
    std::string refresh_token;

    // The date until which the |access_token| can be used.
    // This value has a built-in safety margin, so it can be used as-is.
    base::Time expiration_time;

    // Contains extra information regarding the user's currently registered
    // services.
    std::string id_token;

    // Helper class to make building TokenResponse objects clearer.
    class COMPONENT_EXPORT(GOOGLE_APIS) Builder {
     public:
      Builder();
      ~Builder();

      Builder& WithAccessToken(const std::string& token);
      Builder& WithRefreshToken(const std::string& token);
      Builder& WithExpirationTime(const base::Time& time);
      Builder& WithIdToken(const std::string& token);

      TokenResponse build();

     private:
      std::string access_token_;
      std::string refresh_token_;
      base::Time expiration_time_;
      std::string id_token_;
    };

   private:
    friend class Builder;
    friend class OAuth2AccessTokenConsumer;

    TokenResponse(const std::string& access_token,
                  const std::string& refresh_token,
                  const base::Time& expiration_time,
                  const std::string& id_token);
  };

  OAuth2AccessTokenConsumer() = default;

  OAuth2AccessTokenConsumer(const OAuth2AccessTokenConsumer&) = delete;
  OAuth2AccessTokenConsumer& operator=(const OAuth2AccessTokenConsumer&) =
      delete;

  virtual ~OAuth2AccessTokenConsumer();

  // Success callback.
  virtual void OnGetTokenSuccess(const TokenResponse& token_response);

  // Failure callback.
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error);

  // Returns the OAuth token consumer name, should be used for logging only.
  virtual std::string GetConsumerName() const = 0;
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_CONSUMER_H_
