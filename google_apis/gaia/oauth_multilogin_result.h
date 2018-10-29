// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_
#define GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_

#include <string>
#include <unordered_map>

#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

class OAuthMultiloginResult {
 public:
  OAuthMultiloginResult();
  // Parses cookies and status from JSON response. Maps status to
  // GoogleServiceAuthError::State values or sets error to
  // UNEXPECTED_SERVER_RESPONSE if JSON string cannot be parsed.
  OAuthMultiloginResult(const std::string& raw_data);

  OAuthMultiloginResult(const GoogleServiceAuthError& error);
  OAuthMultiloginResult(const OAuthMultiloginResult& other);
  OAuthMultiloginResult& operator=(const OAuthMultiloginResult& other);
  ~OAuthMultiloginResult();

  std::vector<net::CanonicalCookie> cookies() const { return cookies_; }
  std::vector<std::string> failed_accounts() const { return failed_accounts_; }
  GoogleServiceAuthError error() const { return error_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(OAuthMultiloginResultTest, TryParseCookiesFromValue);

  // Response body that has a form of JSON contains protection characters
  // against XSSI that have to be removed. See go/xssi.
  static base::StringPiece StripXSSICharacters(const std::string& data);

  // Maps status in JSON response to one of the GoogleServiceAuthError state
  // values.
  void TryParseStatusFromValue(base::DictionaryValue* dictionary_value);

  void TryParseCookiesFromValue(base::DictionaryValue* dictionary_value);

  // If error is INVALID_GAIA_CREDENTIALS response is expected to have a list of
  // failed accounts for which tokens are not valid.
  void TryParseFailedAccountsFromValue(base::DictionaryValue* dictionary_value);

  std::vector<net::CanonicalCookie> cookies_;
  std::vector<std::string> failed_accounts_;
  GoogleServiceAuthError error_;
};

#endif  // GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_