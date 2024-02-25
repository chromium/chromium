// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_
#define GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "url/gurl.h"

// Values for the 'status' field of multilogin responses. Used for UMA logging,
// do not remove or reorder values.
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

  kMaxValue = kError,
};

// Parses the status field of the response.
COMPONENT_EXPORT(GOOGLE_APIS)
OAuthMultiloginResponseStatus ParseOAuthMultiloginResponseStatus(
    const std::string& status);

class COMPONENT_EXPORT(GOOGLE_APIS) OAuthMultiloginResult {
 public:
  // Parses cookies and status from JSON response. Maps status to
  // GoogleServiceAuthError::State values or sets error to
  // UNEXPECTED_SERVER_RESPONSE if JSON string cannot be parsed.
  OAuthMultiloginResult(const std::string& raw_data);

  OAuthMultiloginResult(OAuthMultiloginResponseStatus status);
  OAuthMultiloginResult(const OAuthMultiloginResult& other);
  OAuthMultiloginResult& operator=(const OAuthMultiloginResult& other);
  ~OAuthMultiloginResult();

  std::vector<net::CanonicalCookie> cookies() const { return cookies_; }
  std::vector<std::string> failed_gaia_ids() const { return failed_gaia_ids_; }
  OAuthMultiloginResponseStatus status() const { return status_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(OAuthMultiloginResultTest, TryParseCookiesFromValue);
  FRIEND_TEST_ALL_PREFIXES(OAuthMultiloginResultTest,
                           ParseRealResponseFromGaia_2021_10);

  // Response body that has a form of JSON contains protection characters
  // against XSSI that have to be removed. See go/xssi.
  static std::string_view StripXSSICharacters(const std::string& data);

  void TryParseCookiesFromValue(const base::Value::Dict& json_value);

  // If error is INVALID_GAIA_CREDENTIALS response is expected to have a list of
  // failed accounts for which tokens are not valid.
  void TryParseFailedAccountsFromValue(const base::Value::Dict& json_value);

  std::vector<net::CanonicalCookie> cookies_;
  std::vector<std::string> failed_gaia_ids_;
  OAuthMultiloginResponseStatus status_;
};

#endif  // GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_
