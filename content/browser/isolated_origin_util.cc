// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/browser/isolated_origin_util.h"

#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

const char* kAllSubdomainsWildcard = "[*.]";

namespace content {

IsolatedOriginPattern::IsolatedOriginPattern(base::StringPiece pattern)
    : isolate_all_subdomains_(false), is_valid_(false) {
  Parse(pattern);
}

IsolatedOriginPattern::IsolatedOriginPattern(const url::Origin& origin)
    : IsolatedOriginPattern(origin.GetURL().spec()) {}

IsolatedOriginPattern::~IsolatedOriginPattern() = default;
IsolatedOriginPattern::IsolatedOriginPattern(
    const IsolatedOriginPattern& other) = default;
IsolatedOriginPattern& IsolatedOriginPattern::operator=(
    const IsolatedOriginPattern& other) = default;
IsolatedOriginPattern::IsolatedOriginPattern(IsolatedOriginPattern&& other) =
    default;
IsolatedOriginPattern& IsolatedOriginPattern::operator=(
    IsolatedOriginPattern&& other) = default;

bool IsolatedOriginPattern::Parse(const base::StringPiece& unparsed_pattern) {
  pattern_ = unparsed_pattern.as_string();
  origin_ = url::Origin();
  isolate_all_subdomains_ = false;
  is_valid_ = false;

  size_t host_begin = unparsed_pattern.find(url::kStandardSchemeSeparator);
  if (host_begin == base::StringPiece::npos || host_begin == 0)
    return false;

  // Skip over the scheme separator.
  host_begin += strlen(url::kStandardSchemeSeparator);
  if (host_begin >= unparsed_pattern.size())
    return false;

  base::StringPiece scheme_part = unparsed_pattern.substr(0, host_begin);
  base::StringPiece host_part = unparsed_pattern.substr(host_begin);

  // Empty schemes or hosts are invalid for isolation purposes.
  if (host_part.size() == 0)
    return false;

  if (host_part.starts_with(kAllSubdomainsWildcard)) {
    isolate_all_subdomains_ = true;
    host_part.remove_prefix(strlen(kAllSubdomainsWildcard));
  }

  GURL conformant_url(base::JoinString({scheme_part, host_part}, ""));
  origin_ = url::Origin::Create(conformant_url);

  // Ports are ignored when matching isolated origins (see also
  // https://crbug.com/914511).
  const std::string& scheme = origin_.scheme();
  int default_port = url::DefaultPortForScheme(scheme.data(), scheme.length());
  if (origin_.port() != default_port) {
    LOG(ERROR) << "Ignoring port number in isolated origin: " << origin_;
    origin_ = url::Origin::Create(GURL(
        origin_.scheme() + url::kStandardSchemeSeparator + origin_.host()));
  }

  // Can't isolate subdomains of an IP address, must be a valid isolated origin
  // after processing.
  if ((conformant_url.HostIsIPAddress() && isolate_all_subdomains_) ||
      !IsolatedOriginUtil::IsValidIsolatedOrigin(origin_)) {
    origin_ = url::Origin();
    isolate_all_subdomains_ = false;
    return false;
  }

  DCHECK(!is_valid_ || !origin_.opaque());
  is_valid_ = true;
  return true;
}

// static
bool IsolatedOriginUtil::DoesOriginMatchIsolatedOrigin(
    const url::Origin& origin,
    const url::Origin& isolated_origin) {
  // Don't match subdomains if the isolated origin is an IP address.
  if (isolated_origin.GetURL().HostIsIPAddress())
    return origin == isolated_origin;

  // Compare scheme and hostname, but don't compare ports - see
  // https://crbug.com/914511.
  if (origin.scheme() != isolated_origin.scheme())
    return false;

  // Subdomains of an isolated origin are considered to be in the same isolated
  // origin.
  return origin.DomainIs(isolated_origin.host());
}

// static
bool IsolatedOriginUtil::IsValidIsolatedOrigin(const url::Origin& origin) {
  if (origin.opaque())
    return false;

  // Isolated origins should have HTTP or HTTPS schemes.  Hosts in other
  // schemes may not be compatible with subdomain matching.
  GURL origin_gurl = origin.GetURL();
  if (!origin_gurl.SchemeIsHTTPOrHTTPS())
    return false;

  // IP addresses are allowed.
  if (origin_gurl.HostIsIPAddress())
    return true;

  // Disallow hosts such as http://co.uk/, which don't have a valid
  // registry-controlled domain.  This prevents subdomain matching from
  // grouping unrelated sites on a registry into the same origin.
  const bool has_registry_domain =
      net::registry_controlled_domains::HostHasRegistryControlledDomain(
          origin.host(),
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (!has_registry_domain)
    return false;

  // For now, disallow hosts with a trailing dot.
  // TODO(alexmos): Enabling this would require carefully thinking about
  // whether hosts without a trailing dot should match it.
  if (origin.host().back() == '.')
    return false;

  return true;
}

}  // namespace content
