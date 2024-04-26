// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/cors_util.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
CreateCorsOriginAccessAllowList(const Extension& extension) {
  std::vector<network::mojom::CorsOriginPatternPtr> allow_list;

  // Permissions declared by the extension.
  URLPatternSet origin_permissions =
      extension.permissions_data()->GetEffectiveHostPermissions();
  AddURLPatternSetToList(
      origin_permissions, &allow_list,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  // Hosts exempted from the enterprise policy blocklist. This allows
  // enterprises to add "carve outs" for hosts extensions may be allowed to run
  // on. For instance, an enterprise may block "https://*.restricted.example/*",
  // but allow "https://not-sensitive.restricted.example". In order for this to
  // work, the enterprise allowlist has higher priority than the enterprise
  // blocklist.
  // The set intersection is necessary to prevent an enterprise policy from
  // granting a host permission the extension didn't ask for.
  URLPatternSet policy_allowed_host_patterns =
      URLPatternSet::CreateIntersection(
          extension.permissions_data()->policy_allowed_hosts(),
          origin_permissions, URLPatternSet::IntersectionBehavior::kDetailed);

  // TODO(crbug.com/40803363): For now, there is (theoretically) no
  // overlap between user-blocked sites and user-allowed sites. This means that,
  // unlike enterprise policy above, we don't need to add in user-allowed sites
  // here (they should already be granted to the extension, and won't be blocked
  // by user-blocked sites). We should either guarantee this is the case (with
  // DCHECKs) or change this to allow "carve outs" in user host permissions.
  // The latter would likely require adding more knobs to the network layer
  // since we'd need a more complex hierarchy.
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

  // Add hosts blocked by the user. Unintuitively, these are granted *higher*
  // precedence than enterprise blocked sites. This isn't because they are
  // conceptually more important, but rather because we need them to take
  // priority over enterprise allowed sites. Consider the following scenario:
  // - An enterprise blocks https://*.restricted.example.
  // - The enterprise allows https://non-sensitive.restricted.example
  // - The user blocks https://non-sensitive.restricted.example
  // Here, the extension should *not* be allowed to run on
  // https://non-sensitive.restricted.example; the enterprise said it *may*, but
  // the user then denies it access.
  // Note also that enterprise extensions are exempt from user host
  // restrictions, so there's no risk of users blocking enterprise extensions
  // from running on sites.
  // We add user host restrictions with the same priority level as enterprise
  // host allowances; when a block rule and an allow rule have the same
  // priority, the blocking rule wins. We don't add these with "High" priority
  // in order to keep that reserved for browser-defined restrictions.
  // TODO(crbug.com/40803363): This is a pretty tenuous setup. We may
  // just need to plumb more information to the network service.
  AddURLPatternSetToList(
      extension.permissions_data()->GetUserBlockedHosts(), &block_list,
      network::mojom::CorsOriginAccessMatchPriority::kMediumPriority);

  GURL webstore_launch_url = extension_urls::GetWebstoreLaunchURL();
  block_list.push_back(network::mojom::CorsOriginPattern::New(
      webstore_launch_url.scheme(), webstore_launch_url.host(), /*port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kHighPriority));

  GURL new_webstore_launch_url = extension_urls::GetNewWebstoreLaunchURL();
  block_list.push_back(network::mojom::CorsOriginPattern::New(
      new_webstore_launch_url.scheme(), new_webstore_launch_url.host(),
      /*port=*/0, network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kHighPriority));

  // TODO(devlin): Should we also block the webstore update URL here? See
  // https://crbug.com/826946 for a related instance.
  return block_list;
}

}  // namespace extensions
