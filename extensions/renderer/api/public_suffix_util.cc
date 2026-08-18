// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/public_suffix_util.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/url_canon.h"

namespace extensions::api::public_suffix {

namespace {

using net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES;
using net::registry_controlled_domains::GetCanonicalHostRegistryLength;
using net::registry_controlled_domains::GetDomainAndRegistry;
using net::registry_controlled_domains::HostIsRegistryIdentifier;
using net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;

std::string ApplyEncoding(std::string domain, DomainEncoding encoding) {
  switch (encoding) {
    case DomainEncoding::kNone:
    case DomainEncoding::kPunycode:
      return domain;
    case DomainEncoding::kDisplay:
      return base::UTF16ToUTF8(url_formatter::IDNToUnicode(domain));
  }
  NOTREACHED();
}

}  // namespace

std::optional<ParsedHostname> ParseHostname(std::string_view hostname) {
  // Reject percent-encoding, since CanonicalizeHost is too permissive with
  // cases like "%2e" -> ".".
  if (hostname.contains('%')) {
    return std::nullopt;
  }

  url::CanonHostInfo host_info;
  std::string normalized_host = net::CanonicalizeHost(hostname, &host_info);
  if (normalized_host.empty()) {
    return std::nullopt;
  }

  if (host_info.IsIPAddress()) {
    return ParsedHostname{std::move(normalized_host), true};
  }

  // The `IsCanonicalizedHostCompliant()` function rejects any leading dots,
  // whereas the `chrome.publicSuffix` API is spec'd to accept one. So trim the
  // first one, but let the `IsCanonicalizedHostCompliant()` check fail if there
  // were multiple.
  if (normalized_host.starts_with('.')) {
    normalized_host.erase(0, 1);
  }

  if (!net::IsCanonicalizedHostCompliant(normalized_host)) {
    return std::nullopt;
  }

  return ParsedHostname{std::move(normalized_host), false};
}

bool IsKnownSuffix(const ParsedHostname& hostname) {
  if (hostname.is_ip_address) {
    return false;
  }

  return HostIsRegistryIdentifier(hostname.value, INCLUDE_PRIVATE_REGISTRIES);
}

std::optional<std::string> GetKnownSuffix(const ParsedHostname& hostname) {
  if (hostname.is_ip_address) {
    return std::nullopt;
  }
  const size_t registry_length = GetCanonicalHostRegistryLength(
      hostname.value, EXCLUDE_UNKNOWN_REGISTRIES, INCLUDE_PRIVATE_REGISTRIES);
  CHECK_NE(registry_length, std::string::npos);

  if (registry_length > 0) {
    return hostname.value.substr(hostname.value.size() - registry_length);
  }

  if (HostIsRegistryIdentifier(hostname.value, INCLUDE_PRIVATE_REGISTRIES)) {
    return hostname.value;
  }

  return std::nullopt;
}

std::optional<std::string> GetDomain(const ParsedHostname& hostname,
                                     const DomainOptions& options) {
  if (hostname.is_ip_address) {
    if (!options.allow_ip_address.value_or(false)) {
      return std::nullopt;
    }
    return hostname.value;
  }

  std::optional<std::string> known_suffix = GetKnownSuffix(hostname);
  std::string domain =
      GetDomainAndRegistry(hostname.value, INCLUDE_PRIVATE_REGISTRIES);

  if (known_suffix == hostname.value) {
    if (!options.allow_plain_suffix.value_or(false)) {
      return std::nullopt;
    }
    return ApplyEncoding(hostname.value, options.encoding);
  }

  if (known_suffix.has_value()) {
    return ApplyEncoding(std::move(domain), options.encoding);
  }

  if (!options.allow_unknown_suffix.value_or(false)) {
    return std::nullopt;
  }

  // GetDomainAndRegistry() returns nothing for a single-label host, but the
  // API is spec'd to return it (e.g. "localhost" -> "localhost").
  if (domain.empty()) {
    return ApplyEncoding(hostname.value, options.encoding);
  }

  return ApplyEncoding(std::move(domain), options.encoding);
}

}  // namespace extensions::api::public_suffix
