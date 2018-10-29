// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/isolated_origin_util.h"

#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace content {

// static
bool IsolatedOriginUtil::DoesOriginMatchIsolatedOrigin(
    const url::Origin& origin,
    const url::Origin& isolated_origin) {
  // Don't match subdomains if the isolated origin is an IP address.
  if (isolated_origin.GetURL().HostIsIPAddress())
    return origin == isolated_origin;

  if (origin.scheme() != isolated_origin.scheme())
    return false;

  if (origin.port() != isolated_origin.port())
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
