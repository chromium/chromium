// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_util.h"

#include <stddef.h>

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "google_apis/gaia/bound_oauth_token.pb.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/list_accounts_response.pb.h"
#include "google_apis/gaia/oauth2_mint_token_consent_result.pb.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace gaia {

namespace {

const char kGmailDomain[] = "gmail.com";
const char kGoogleDomain[] = "google.com";
const char kGooglemailDomain[] = "googlemail.com";

std::string CanonicalizeEmailImpl(std::string_view email_address,
                                  bool change_googlemail_to_gmail) {
  std::string lower_case_email = base::ToLowerASCII(email_address);
  std::vector<std::string> parts = base::SplitString(
      lower_case_email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2U)
    return lower_case_email;

  if (change_googlemail_to_gmail && parts[1] == kGooglemailDomain)
    parts[1] = kGmailDomain;

  if (parts[1] == kGmailDomain)  // only strip '.' for gmail accounts.
    base::RemoveChars(parts[0], ".", &parts[0]);

  std::string new_email = base::JoinString(parts, "@");
  VLOG(1) << "Canonicalized " << email_address << " to " << new_email;
  return new_email;
}

}  // namespace

ListedAccount::ListedAccount() = default;

ListedAccount::ListedAccount(const ListedAccount&) = default;
ListedAccount& ListedAccount::operator=(const ListedAccount&) = default;

ListedAccount::~ListedAccount() = default;

MultiloginAccountAuthCredentials::MultiloginAccountAuthCredentials(
    GaiaId gaia_id,
    std::string token,
    std::string token_binding_assertion)
    : gaia_id(std::move(gaia_id)),
      token(std::move(token)),
      token_binding_assertion(std::move(token_binding_assertion)) {}

std::string CanonicalizeEmail(std::string_view email_address) {
  // CanonicalizeEmail() is called to process email strings that are eventually
  // shown to the user, and may also be used in persisting email strings.  To
  // avoid breaking this existing behavior, this function will not try to
  // change googlemail to gmail.
  return CanonicalizeEmailImpl(email_address, false);
}

std::string CanonicalizeDomain(std::string_view domain) {
  // Canonicalization of domain names means lower-casing them. Make sure to
  // update this function in sync with Canonicalize if this ever changes.
  return base::ToLowerASCII(domain);
}

std::string SanitizeEmail(std::string_view email_address) {
  std::string sanitized(email_address);

  // Apply a default domain if necessary.
  if (!base::Contains(sanitized, '@')) {
    sanitized += '@';
    sanitized += kGmailDomain;
  }

  return sanitized;
}

bool AreEmailsSame(std::string_view email1, std::string_view email2) {
  return CanonicalizeEmailImpl(gaia::SanitizeEmail(email1), true) ==
      CanonicalizeEmailImpl(gaia::SanitizeEmail(email2), true);
}

std::string ExtractDomainName(std::string_view email_address) {
  // First canonicalize which will also verify we have proper domain part.
  if (email_address == "") {
    DUMP_WILL_BE_NOTREACHED();
    return std::string();
  }
  std::string email = CanonicalizeEmail(email_address);
  size_t separator_pos = email.find('@');
  if (separator_pos != std::string::npos &&
      separator_pos < email.length() - 1) {
    return email.substr(separator_pos + 1);
  }
  DUMP_WILL_BE_NOTREACHED() << "Not a proper email address: " << email;
  return std::string();
}

bool IsGoogleInternalAccountEmail(std::string_view email) {
  return ExtractDomainName(SanitizeEmail(email)) == kGoogleDomain;
}

bool IsGoogleRobotAccountEmail(std::string_view email) {
  std::string domain_name = gaia::ExtractDomainName(SanitizeEmail(email));
  return base::EndsWith(domain_name, "gserviceaccount.com") ||
         base::EndsWith(domain_name, "googleusercontent.com");
}

bool HasGaiaSchemeHostPort(const GURL& url) {
  const url::Origin& gaia_origin = GaiaUrls::GetInstance()->gaia_origin();
  CHECK(!gaia_origin.opaque());
  CHECK(gaia_origin.GetURL().SchemeIsHTTPOrHTTPS());

  const url::SchemeHostPort& gaia_scheme_host_port =
      gaia_origin.GetTupleOrPrecursorTupleIfOpaque();

  return url::SchemeHostPort(url) == gaia_scheme_host_port;
}

bool ParseBinaryListAccountsData(const std::string& data,
                                 std::vector<ListedAccount>* accounts) {
  // Clear and rebuild our accounts list if one is given.
  if (accounts) {
    accounts->clear();
  }

  // The input is expected to be base64-encoded.
  std::string decoded_data;
  if (!base::Base64Decode(data, &decoded_data,
                          base::Base64DecodePolicy::kForgiving)) {
    VLOG(1) << "Failed to decode ListAccounts data as a Base64 String";
    return false;
  }

  // Parse our binary proto response.
  ListAccountsResponse parsed_result;
  if (!parsed_result.ParseFromString(decoded_data)) {
    VLOG(1) << "malformed ListAccountsResponse";
    return false;
  }

  // Build a vector of accounts from the cookie. Order is important: the first
  // account in the list is the primary account.
  for (const auto& account : parsed_result.account()) {
    if (account.display_email().empty() || account.obfuscated_id().empty()) {
      continue;
    }

    ListedAccount listed_account;
    listed_account.email = CanonicalizeEmail(account.display_email());
    listed_account.gaia_id = GaiaId(account.obfuscated_id());
    // Assume the account is valid if unspecified for backcompat.
    listed_account.valid =
        !account.has_valid_session() || account.valid_session();
    listed_account.signed_out =
        account.has_signed_out() && account.signed_out();
    listed_account.verified =
        !account.has_is_verified() || account.is_verified();
    listed_account.raw_email = account.display_email();
    if (accounts) {
      accounts->push_back(std::move(listed_account));
    }
  }

  return true;
}

bool ParseOAuth2MintTokenConsentResult(std::string_view consent_result,
                                       bool* approved,
                                       GaiaId* gaia_id) {
  DCHECK(approved);
  DCHECK(gaia_id);

  std::string decoded_result;
  if (!base::Base64UrlDecode(consent_result,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded_result)) {
    VLOG(1) << "Base64UrlDecode() failed to decode the consent result";
    return false;
  }

  OAuth2MintTokenConsentResult parsed_result;
  if (!parsed_result.ParseFromString(decoded_result)) {
    VLOG(1) << "Failed to parse the consent result protobuf message";
    return false;
  }

  *approved = parsed_result.approved();
  *gaia_id = GaiaId(parsed_result.obfuscated_id());
  return true;
}

std::string CreateBoundOAuthToken(const GaiaId& gaia_id,
                                  const std::string& refresh_token,
                                  const std::string& binding_key_assertion) {
  BoundOAuthToken bound_oauth_token;
  bound_oauth_token.set_gaia_id(gaia_id.ToString());
  bound_oauth_token.set_token(refresh_token);
  bound_oauth_token.set_token_binding_assertion(binding_key_assertion);

  std::string serialized = bound_oauth_token.SerializeAsString();
  if (serialized.empty()) {
    VLOG(1) << "Failed to serialize bound OAuth token to protobuf message";
    return std::string();
  }

  std::string base64_encoded;
  base::Base64UrlEncode(serialized, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64_encoded);
  return base64_encoded;
}

std::string CreateMultiOAuthHeader(
    const std::vector<MultiloginAccountAuthCredentials>& accounts) {
  gaia::MultiOAuthHeader header;
  for (const MultiloginAccountAuthCredentials& account : accounts) {
    gaia::MultiOAuthHeader::AccountRequest request;
    request.set_gaia_id(account.gaia_id.ToString());
    request.set_token(account.token);
    if (!account.token_binding_assertion.empty()) {
      request.set_token_binding_assertion(account.token_binding_assertion);
    }
    header.mutable_account_requests()->Add(std::move(request));
  }

  std::string base64_encoded_header;
  base::Base64UrlEncode(header.SerializeAsString(),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64_encoded_header);
  return base64_encoded_header;
}

}  // namespace gaia
