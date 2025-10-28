// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "url/gurl.h"

namespace blink {

const char kToken[] = "%s";

URLSyntaxErrorCode IsValidCustomHandlerURLSyntax(
    const GURL& full_url,
    ProtocolHandlerSecurityLevel security_level) {
  std::string_view user_url =
      full_url.is_valid() ? full_url.spec() : std::string_view("");
  return IsValidCustomHandlerURLSyntax(full_url, user_url, security_level);
}

URLSyntaxErrorCode IsValidCustomHandlerURLSyntax(
    const GURL& full_url,
    std::string_view user_url,
    ProtocolHandlerSecurityLevel security_level) {
  // It is a SyntaxError if the custom handler URL, as created by removing
  // the "%s" token and prepending the base url, does not resolve.
  if (full_url.is_empty() || !full_url.is_valid()) {
    return URLSyntaxErrorCode::kInvalidUrl;
  }

  // The specification requires that it is a SyntaxError if the "%s" token is
  // not present.
  size_t index = user_url.find(kToken);
  if (index == std::string_view::npos) {
    return URLSyntaxErrorCode::kMissingToken;
  }

  if (security_level == ProtocolHandlerSecurityLevel::kIsolatedAppFeatures) {
    // For Isolated Web Apps, the protocol URL must conform to the following
    // restrictions:
    // * There must be exactly one placeholder;
    // * This placeholder must be a part of query params.
    if (user_url.rfind(kToken) != index) {
      return URLSyntaxErrorCode::kInvalidUrl;
    }
    if (GURL(user_url).GetQuery().find(kToken) == std::string::npos) {
      return URLSyntaxErrorCode::kInvalidUrl;
    }
  }

  return URLSyntaxErrorCode::kNoError;
}

bool IsValidCustomHandlerScheme(std::string_view scheme,
                                ProtocolHandlerSecurityLevel security_level,
                                bool* has_custom_scheme_prefix) {
  bool allow_scheme_prefix =
      (security_level == ProtocolHandlerSecurityLevel::kExtensionFeatures);
  if (has_custom_scheme_prefix) {
    *has_custom_scheme_prefix = false;
  }

  static constexpr const char kWebPrefix[] = "web+";
  static constexpr const char kExtPrefix[] = "ext+";
  DCHECK_EQ(std::size(kWebPrefix), std::size(kExtPrefix));
  static constexpr const size_t kPrefixLength = std::size(kWebPrefix) - 1;
  if (base::StartsWith(scheme, kWebPrefix,
                       base::CompareCase::INSENSITIVE_ASCII) ||
      (allow_scheme_prefix &&
       base::StartsWith(scheme, kExtPrefix,
                        base::CompareCase::INSENSITIVE_ASCII))) {
    if (has_custom_scheme_prefix) {
      *has_custom_scheme_prefix = true;
    }
    // HTML5 requires that schemes with the |web+| prefix contain one or more
    // ASCII alphas after that prefix.
    auto scheme_name = scheme.substr(kPrefixLength);
    return scheme_name.length() >= 1 &&
           std::ranges::all_of(scheme_name, &base::IsAsciiAlpha<char>);
  }

  if (security_level == ProtocolHandlerSecurityLevel::kIsolatedAppFeatures) {
    // Isolated Apps are allowed to claim any scheme that consists of non-empty
    // blocks of ASCII alpha characters possibly separated by dashes with a
    // total length of at least 2.
    if (scheme.length() < 2) {
      return false;
    }
    return std::ranges::all_of(
        base::SplitStringPiece(scheme, "-", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_ALL),
        [](std::string_view chunk) {
          return !chunk.empty() &&
                 std::ranges::all_of(chunk, &base::IsAsciiAlpha<char>);
        });
  }

  static constexpr const char* const kProtocolSafelist[] = {
      "bitcoin", "cabal",  "dat",    "did",  "doi",  "dweb", "ethereum",
      "geo",     "hyper",  "im",     "ipfs", "ipns", "irc",  "ircs",
      "magnet",  "mailto", "matrix", "mms",  "news", "nntp", "openpgp4fpr",
      "sip",     "sms",    "smsto",  "ssb",  "ssh",  "tel",  "urn",
      "webcal",  "wtai",   "xmpp"};

  std::string lower_scheme = base::ToLowerASCII(scheme);
  if (base::Contains(kProtocolSafelist, lower_scheme)) {
    return true;
  }
  if (lower_scheme == "ftp" || lower_scheme == "ftps" ||
      lower_scheme == "sftp") {
    return true;
  }
  if (base::FeatureList::IsEnabled(
          features::kSafelistPaytoToRegisterProtocolHandler) &&
      lower_scheme == "payto") {
    return true;
  }
  return false;
}

bool IsAllowedCustomHandlerURL(const GURL& url,
                               ProtocolHandlerSecurityLevel security_level) {
  bool has_valid_scheme =
      url.SchemeIsHTTPOrHTTPS() ||
      security_level == ProtocolHandlerSecurityLevel::kSameOrigin ||
      (security_level == ProtocolHandlerSecurityLevel::kExtensionFeatures &&
       CommonSchemeRegistry::IsExtensionScheme(url.GetScheme())) ||
      (security_level == ProtocolHandlerSecurityLevel::kIsolatedAppFeatures &&
       CommonSchemeRegistry::IsIsolatedAppScheme(url.GetScheme()));
  return has_valid_scheme && network::IsUrlPotentiallyTrustworthy(url);
}

}  // namespace blink
