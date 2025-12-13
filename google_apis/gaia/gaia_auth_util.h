// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"

class GURL;

namespace gaia {

struct COMPONENT_EXPORT(GOOGLE_APIS) ListedAccount {
  // The account's ID, as per Chrome, will be determined in the
  // CookieManagerService.
  CoreAccountId id;
  std::string email;
  GaiaId gaia_id;
  std::string raw_email;
  bool valid = true;
  bool signed_out = false;
  bool verified = true;

  ListedAccount();
  ListedAccount(const ListedAccount&);
  ListedAccount& operator=(const ListedAccount&);
  ~ListedAccount();

  friend bool operator==(const ListedAccount& lhs,
                         const ListedAccount& rhs) = default;
};

struct COMPONENT_EXPORT(GOOGLE_APIS) MultiloginAccountAuthCredentials {
  GaiaId gaia_id;
  std::string token;
  std::string token_binding_assertion;

  MultiloginAccountAuthCredentials(GaiaId gaia_id,
                                   std::string token,
                                   std::string token_binding_assertion);
};

// Perform basic canonicalization of |email_address|, taking into account that
// gmail does not consider '.' or caps inside a username to matter.
// If |email_address| is not a valid, returns it in lower case without
// additional canonicalization.
COMPONENT_EXPORT(GOOGLE_APIS)
std::string CanonicalizeEmail(std::string_view email_address);

// Returns the canonical form of the given domain.
COMPONENT_EXPORT(GOOGLE_APIS)
std::string CanonicalizeDomain(std::string_view domain);

// Sanitize emails. Currently, it only ensures all emails have a domain by
// adding gmail.com if no domain is present.
COMPONENT_EXPORT(GOOGLE_APIS)
std::string SanitizeEmail(std::string_view email_address);

// Returns true if the two specified email addresses are the same.  Both
// addresses are first sanitized and then canonicalized before comparing.
COMPONENT_EXPORT(GOOGLE_APIS)
bool AreEmailsSame(std::string_view email1, std::string_view email2);

// Extract the domain part from the canonical form of the given email.
COMPONENT_EXPORT(GOOGLE_APIS)
std::string ExtractDomainName(std::string_view email);

// Returns whether the user's email is Google internal. This check is meant
// to be used sparingly since it ship Googler-only code to all users.
COMPONENT_EXPORT(GOOGLE_APIS)
bool IsGoogleInternalAccountEmail(std::string_view email);

// Returns true if |email| correspnds to the email of a robot account.
COMPONENT_EXPORT(GOOGLE_APIS)
bool IsGoogleRobotAccountEmail(std::string_view email);

// Mechanically compares the scheme, host, and port of the |url| against the
// GAIA url in GaiaUrls. This means that this function will *not* work for
// determining whether a frame with an "about:blank" URL or "blob:..." URL has
// a GAIA origin and will in that case return false.
COMPONENT_EXPORT(GOOGLE_APIS) bool HasGaiaSchemeHostPort(const GURL& url);

// Parses binary proto data returned by /ListAccounts call into the given
// ListedAccounts. An email addresses is considered valid if a passive login
// would succeed (i.e. the user does not need to reauthenticate).
// If there was a parse error, this method returns false.
// If |accounts| is null, the corresponding accounts returned from /ListAccounts
// will be ignored.
COMPONENT_EXPORT(GOOGLE_APIS)
bool ParseBinaryListAccountsData(const std::string& data,
                                 std::vector<ListedAccount>* accounts);

// Parses base64url encoded protobuf message returned by the remote consent
// flow, returning whether the consent was approved and the gaia id of the user
// that was shown the consent page.
// Returns false if the method failed to decode the protobuf.
// |approved| and |gaia_id| must not be null.
COMPONENT_EXPORT(GOOGLE_APIS)
bool ParseOAuth2MintTokenConsentResult(std::string_view consent_result,
                                       bool* approved,
                                       GaiaId* gaia_id);

// Creates a base64url encoded value representing a bound OAuth token that can
// be used in an Authorization header with the "BoundOAuthToken" type.
// Returns an empty string if the token creation fails.
COMPONENT_EXPORT(GOOGLE_APIS)
std::string CreateBoundOAuthToken(const GaiaId& gaia_id,
                                  const std::string& refresh_token,
                                  const std::string& binding_key_assertion);

// Creates a base64url encoded value representing a list of bound OAuth tokens
// that can be used in an Authorization header with the "MultiOAuth" type.
// Returns an empty string if the header creation fails.
COMPONENT_EXPORT(GOOGLE_APIS)
std::string CreateMultiOAuthHeader(
    const std::vector<MultiloginAccountAuthCredentials>& accounts);

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_
