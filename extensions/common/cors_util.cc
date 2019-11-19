// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/cors_util.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

namespace {

uint16_t GetEffectivePort(const std::string& port_string) {
  int port_int = 0;
  bool success = base::StringToInt(port_string, &port_int);
  // The URLPattern should verify that |port| is a number or "*", so conversion
  // should never fail.
  DCHECK(success) << port_string;
  return port_int;
}

void AddURLPatternSetToList(
    const URLPatternSet& pattern_set,
    std::vector<network::mojom::CorsOriginPatternPtr>* list,
    network::mojom::CorsOriginAccessMatchPriority priority) {
  static const char* const kSchemes[] = {
    content::kChromeUIScheme,
#if defined(OS_CHROMEOS)
    content::kExternalFileScheme,
#endif
    extensions::kExtensionScheme,
    url::kFileScheme,
    url::kFtpScheme,
    url::kHttpScheme,
    url::kHttpsScheme,
  };
  for (const URLPattern& pattern : pattern_set) {
    for (const char* const scheme : kSchemes) {
      if (!pattern.MatchesScheme(scheme))
        continue;
      network::mojom::CorsDomainMatchMode domain_match_mode =
          pattern.match_subdomains()
              ? network::mojom::CorsDomainMatchMode::kAllowSubdomains
              : network::mojom::CorsDomainMatchMode::kDisallowSubdomains;
      network::mojom::CorsPortMatchMode port_match_mode =
          (pattern.port() == "*")
              ? network::mojom::CorsPortMatchMode::kAllowAnyPort
              : network::mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort;
      uint16_t port =
          (port_match_mode ==
           network::mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort)
              ? GetEffectivePort(pattern.port())
              : 0u;
      list->push_back(network::mojom::CorsOriginPattern::New(
          scheme, pattern.host(), port, domain_match_mode, port_match_mode,
          priority));
    }
  }
}

}  // namespace

std::vector<network::mojom::CorsOriginPatternPtr>
CreateCorsOriginAccessAllowList(
    const Extension& extension,
    PermissionsData::EffectiveHostPermissionsMode mode) {
  std::vector<network::mojom::CorsOriginPatternPtr> allow_list;

  // Permissions declared by the extension.
  URLPatternSet origin_permissions =
      extension.permissions_data()->GetEffectiveHostPermissions(mode);
  AddURLPatternSetToList(
      origin_permissions, &allow_list,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  // Hosts exempted from the enterprise policy blocklist.
  // This set intersection is necessary to prevent an enterprise policy from
  // granting a host permission the extension didn't ask for.
  URLPatternSet policy_allowed_host_patterns =
      URLPatternSet::CreateIntersection(
          extension.permissions_data()->policy_allowed_hosts(),
          origin_permissions, URLPatternSet::IntersectionBehavior::kDetailed);
  AddURLPatternSetToList(
      policy_allowed_host_patterns, &allow_list,
      network::mojom::CorsOriginAccessMatchPriority::kMediumPriority);

  return allow_list;
}

std::vector<network::mojom::CorsOriginPatternPtr>
CreateCorsOriginAccessBlockList(const Extension& extension) {
  std::vector<network::mojom::CorsOriginPatternPtr> block_list;

  // Hosts blocked by enterprise policy.
  AddURLPatternSetToList(
      extension.permissions_data()->policy_blocked_hosts(), &block_list,
      network::mojom::CorsOriginAccessMatchPriority::kLowPriority);

  GURL webstore_launch_url = extension_urls::GetWebstoreLaunchURL();
  block_list.push_back(network::mojom::CorsOriginPattern::New(
      webstore_launch_url.scheme(), webstore_launch_url.host(), /*port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kHighPriority));

  // TODO(devlin): Should we also block the webstore update URL here? See
  // https://crbug.com/826946 for a related instance.
  return block_list;
}

}  // namespace extensions
