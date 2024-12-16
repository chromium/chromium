// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_
#define GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "url/gurl.h"

// Values for the 'status' field of multilogin responses.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OAuthMultiloginResponseStatus)
enum class OAuthMultiloginResponseStatus {
  // Status could not be parsed.
  kUnknownStatus = 0,

  // The request was processed successfully, and the rest of this object
  // contains the cookies to set across domains. The HTTP status code will be
  // 200.
  kOk = 1,

  // Something happened while processing the request that made it fail. It is
  // suspected to be a transient issue, so the client may retry at a later time
  // with exponential backoff. The HTTP status code will be 503.
  kRetry = 2,

  // The input parameters were not as expected (wrong header format, missing
  // parameters, etc). Retrying without changing input parameters will not work.
  // The HTTP status code will be 400.
  kInvalidInput = 3,

  // At least one provided token could not be used to authenticate the
  // corresponding user. This includes the case where the provided Gaia ID does
  // not match with the corresponding OAuth token. The HTTP status code will be
  // 403.
  kInvalidTokens = 4,

  // An error occurred while processing the request, and retrying is not
  // expected to work. The HTTP status code will be 500.
  kError = 5,

  // One or more of the presented tokens was bound, and the server wants the
  // client to retry the request by signing over a server-provided challenge.
  // The client does not need to use exponential backoff but should retry at
  // most once. The HTTP status code will be 400.
  kRetryWithTokenBindingChallenge = 6,

  kMaxValue = kRetryWithTokenBindingChallenge,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:OAuthMultiloginResponseStatus)

// Parses the status field of the response.
COMPONENT_EXPORT(GOOGLE_APIS)
OAuthMultiloginResponseStatus ParseOAuthMultiloginResponseStatus(
    const std::string& status,
    int http_response_code);

class COMPONENT_EXPORT(GOOGLE_APIS) OAuthMultiloginResult {
 public:
  using CookieDecryptor =
      base::RepeatingCallback<std::string(std::string_view)>;

  struct FailedAccount {
    GaiaId gaia_id;

    // If `token_binding_challenge` is not empty, an account error might be
    // recovered by retrying the request with a token binding assertion signed
    // over the challenge.
    std::string token_binding_challenge;
  };

  // Parses cookies and status from JSON response. Maps status to
  // GoogleServiceAuthError::State values or sets error to
  // UNEXPECTED_SERVER_RESPONSE if JSON string cannot be parsed.
  // `cookie_decryptor` is optional and used only if the JSON response contains
  // "token_binding_directed_response" object.
  OAuthMultiloginResult(
      const std::string& raw_data,
      int http_response_code,
      const CookieDecryptor& cookie_decryptor = base::NullCallback());

  explicit OAuthMultiloginResult(OAuthMultiloginResponseStatus status);
  OAuthMultiloginResult(const OAuthMultiloginResult& other);
  OAuthMultiloginResult& operator=(const OAuthMultiloginResult& other);
  ~OAuthMultiloginResult();

  std::vector<net::CanonicalCookie> cookies() const { return cookies_; }
  std::vector<FailedAccount> failed_accounts() const {
    return failed_accounts_;
  }
  OAuthMultiloginResponseStatus status() const { return status_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(OAuthMultiloginResultTest, TryParseCookiesFromValue);
  FRIEND_TEST_ALL_PREFIXES(OAuthMultiloginResultTest,
                           ParseRealResponseFromGaia_2021_10);

  void TryParseCookiesFromValue(
      const base::Value::Dict& json_value,
      const CookieDecryptor& decryptor = base::NullCallback());

  // If `status_` is `kInvalidTokens` or `kRetryWithTokenBindingChallenge`, the
  // response is expected to have a list of failed accounts for which tokens are
  // either not valid or required to sign over a token binding challenge.
  void TryParseFailedAccountsFromValue(const base::Value::Dict& json_value);

  std::vector<net::CanonicalCookie> cookies_;
  std::vector<FailedAccount> failed_accounts_;
  OAuthMultiloginResponseStatus status_;
};

#endif  // GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_
