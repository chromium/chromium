// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_

#include <string>
#include <utility>
#include <vector>

class GURL;

namespace gaia {

struct ListedAccount {
  // The account's ID, as per Chrome, will be determined in the
  // CookieManagerService.
  std::string id;
  std::string email;
  std::string gaia_id;
  std::string raw_email;
  bool valid = true;
  bool signed_out = false;
  bool verified = true;

  ListedAccount();
  ListedAccount(const ListedAccount& other);
  ~ListedAccount();
};

// Perform basic canonicalization of |email_address|, taking into account that
// gmail does not consider '.' or caps inside a username to matter.
// If |email_address| is not a valid, returns it in lower case without
// additional canonicalization.
std::string CanonicalizeEmail(const std::string& email_address);

// Returns the canonical form of the given domain.
std::string CanonicalizeDomain(const std::string& domain);

// Sanitize emails. Currently, it only ensures all emails have a domain by
// adding gmail.com if no domain is present.
std::string SanitizeEmail(const std::string& email_address);

// Returns true if the two specified email addresses are the same.  Both
// addresses are first sanitized and then canonicalized before comparing.
bool AreEmailsSame(const std::string& email1, const std::string& email2);

// Extract the domain part from the canonical form of the given email.
std::string ExtractDomainName(const std::string& email);

bool IsGaiaSignonRealm(const GURL& url);

// Parses JSON data returned by /ListAccounts call, returning a vector of
// email/valid pairs.  An email addresses is considered valid if a passive
// login would succeed (i.e. the user does not need to reauthenticate).
// If there an error parsing the JSON, then false is returned.
// If either |accounts| or |signed_out_accounts| is null, the corresponding
// accounts returned from /ListAccounts will be ignored.
bool ParseListAccountsData(const std::string& data,
                           std::vector<ListedAccount>* accounts,
                           std::vector<ListedAccount>* signed_out_accounts);

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_
