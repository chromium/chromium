// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/feature_list.h"
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
    const std::string_view& user_url) {
  // The specification requires that it is a SyntaxError if the "%s" token is
  // not present.
  int index = user_url.find(kToken);
  if (-1 == index)
    return URLSyntaxErrorCode::kMissingToken;

  // It is also a SyntaxError if the custom handler URL, as created by removing
  // the "%s" token and prepending the base url, does not resolve.
  if (full_url.is_empty() || !full_url.is_valid())
    return URLSyntaxErrorCode::kInvalidUrl;

  return URLSyntaxErrorCode::kNoError;
}

bool IsValidCustomHandlerScheme(std::string_view scheme,
                                ProtocolHandlerSecurityLevel security_level,
                                bool* has_custom_scheme_prefix) {
  bool allow_scheme_prefix =
      (security_level >= ProtocolHandlerSecurityLevel::kExtensionFeatures);
  if (has_custom_scheme_prefix)
    *has_custom_scheme_prefix = false;

  static constexpr const char kWebPrefix[] = "web+";
  static constexpr const char kExtPrefix[] = "ext+";
  DCHECK_EQ(std::size(kWebPrefix), std::size(kExtPrefix));
  static constexpr const size_t kPrefixLength = std::size(kWebPrefix) - 1;
  if (base::StartsWith(scheme, kWebPrefix,
                       base::CompareCase::INSENSITIVE_ASCII) ||
      (allow_scheme_prefix &&
       base::StartsWith(scheme, kExtPrefix,
                        base::CompareCase::INSENSITIVE_ASCII))) {
    if (has_custom_scheme_prefix)
      *has_custom_scheme_prefix = true;
    // HTML5 requires that schemes with the |web+| prefix contain one or more
    // ASCII alphas after that prefix.
    auto scheme_name = scheme.substr(kPrefixLength);
    if (scheme_name.empty())
      return false;
    for (auto& character : scheme_name) {
      if (!base::IsAsciiAlpha(character))
        return false;
    }
    return true;
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
  if (base::FeatureList::IsEnabled(
          features::kSafelistFTPToRegisterProtocolHandler) &&
      (lower_scheme == "ftp" || lower_scheme == "ftps" ||
       lower_scheme == "sftp")) {
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
       CommonSchemeRegistry::IsExtensionScheme(url.scheme()));
  return has_valid_scheme && network::IsUrlPotentiallyTrustworthy(url);
}

}  // namespace blink
