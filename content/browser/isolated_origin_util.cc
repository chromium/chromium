// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/isolated_origin_util.h"

#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"

const char* kAllSubdomainsWildcard = "[*.]";

namespace content {

IsolatedOriginPattern::IsolatedOriginPattern(std::string_view pattern)
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

bool IsolatedOriginPattern::Parse(const std::string_view& unparsed_pattern) {
  pattern_ = std::string(unparsed_pattern);
  origin_ = url::Origin();
  isolate_all_subdomains_ = false;
  is_valid_ = false;

  size_t host_begin = unparsed_pattern.find(url::kStandardSchemeSeparator);
  if (host_begin == std::string_view::npos || host_begin == 0) {
    return false;
  }

  // Skip over the scheme separator.
  host_begin += strlen(url::kStandardSchemeSeparator);
  if (host_begin >= unparsed_pattern.size())
    return false;

  std::string_view scheme_part = unparsed_pattern.substr(0, host_begin);
  std::string_view host_part = unparsed_pattern.substr(host_begin);

  // Empty schemes or hosts are invalid for isolation purposes.
  if (host_part.size() == 0)
    return false;

  if (base::StartsWith(host_part, kAllSubdomainsWildcard)) {
    isolate_all_subdomains_ = true;
    host_part.remove_prefix(strlen(kAllSubdomainsWildcard));
  }

  GURL conformant_url(base::JoinString({scheme_part, host_part}, ""));
  origin_ = url::Origin::Create(conformant_url);

  // Ports are ignored when matching isolated origins (see also
  // https://crbug.com/914511).
  const std::string& scheme = origin_.scheme();
  int default_port = url::DefaultPortForScheme(scheme);
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
  return IsValidIsolatedOriginImpl(origin,
                                   /* is_legacy_isolated_origin_check=*/true);
}

// static
bool IsolatedOriginUtil::IsValidOriginForOptInIsolation(
    const url::Origin& origin) {
  // Per https://html.spec.whatwg.org/C/#initialise-the-document-object,
  // non-secure contexts cannot be isolated via opt-in origin isolation.
  return IsValidIsolatedOriginImpl(
             origin, /* is_legacy_isolated_origin_check=*/false) &&
         network::IsOriginPotentiallyTrustworthy(origin);
}

// static
bool IsolatedOriginUtil::IsValidOriginForOptOutIsolation(
    const url::Origin& origin) {
  // Per https://html.spec.whatwg.org/C/#initialise-the-document-object,
  // non-secure contexts cannot be isolated via opt-in origin isolation,
  // but we allow non-secure contexts to opt-out for legacy sites.
  return IsValidIsolatedOriginImpl(origin,
                                   /* is_legacy_isolated_origin_check=*/false);
}

// static
bool IsolatedOriginUtil::IsValidIsolatedOriginImpl(
    const url::Origin& origin,
    bool is_legacy_isolated_origin_check) {
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
  //
  // This is not relevant for opt-in origin isolation, which doesn't need to
  // match subdomains. (And it'd be bad to check this in that case, as it
  // prohibits http://localhost/; see https://crbug.com/1142894.)
  if (is_legacy_isolated_origin_check) {
    const bool has_registry_domain =
        net::registry_controlled_domains::HostHasRegistryControlledDomain(
            origin.host(),
            net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (!has_registry_domain)
      return false;
  }

  // Disallow hosts with a trailing dot for legacy isolated origins, but allow
  // them for opt-in origin isolation since the spec says that they represent
  // a distinct origin: https://url.spec.whatwg.org/#concept-domain.
  // TODO(alexmos): Legacy isolated origins should probably support trailing
  // dots as well, but enabling this would require carefully thinking about
  // whether hosts without a trailing dot should match it.
  if (is_legacy_isolated_origin_check && origin.host().back() == '.') {
    return false;
  }

  return true;
}

}  // namespace content
