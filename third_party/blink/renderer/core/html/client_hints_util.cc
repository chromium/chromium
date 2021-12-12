// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/client_hints_util.h"

#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

void UpdateWindowPermissionsPolicyWithDelegationSupportForClientHints(
    ClientHintsPreferences& client_hints_preferences,
    LocalDOMWindow* local_dom_window,
    const String& header_value,
    const KURL& url,
    ClientHintsPreferences::Context* context,
    bool is_http_equiv,
    bool is_preload_or_sync_parser) {
  // If no hints were set, this is an http-equiv tag, this tag was added by
  // javascript, the `local_dom_window` is missing, or the feature is disabled,
  // there's nothing more to do.
  if (!client_hints_preferences.UpdateFromMetaTagAcceptCH(
          header_value, url, context, is_http_equiv,
          is_preload_or_sync_parser) ||
      is_http_equiv || !is_preload_or_sync_parser || !local_dom_window ||
      !RuntimeEnabledFeatures::ClientHintThirdPartyDelegationEnabled()) {
    return;
  }

  // Note: .Ascii() would convert tab to ?, which is undesirable.
  absl::optional<network::ClientHintToDelegatedThirdPartiesMap> parsed_ch =
      network::ParseClientHintToDelegatedThirdPartiesHeader(
          header_value.Latin1());

  // Build vector of client hint permission policies to update.
  auto* const current_policy =
      local_dom_window->GetSecurityContext().GetPermissionsPolicy();
  ParsedPermissionsPolicy container_policy;
  for (const auto& pair : parsed_ch.value()) {
    const auto& policy_name = GetClientHintToPolicyFeatureMap().at(pair.first);

    // We need to retain any preexisting settings, just adding new origins.
    const auto& allow_list =
        current_policy->GetAllowlistForFeature(policy_name);
    std::set<url::Origin> origin_set(allow_list.AllowedOrigins().begin(),
                                     allow_list.AllowedOrigins().end());
    origin_set.insert(pair.second.begin(), pair.second.end());
    std::vector<url::Origin> filtered_origins;
    std::copy_if(origin_set.begin(), origin_set.end(),
                 std::back_inserter(filtered_origins),
                 [](const auto& origin) { return !origin.opaque(); });
    auto declaration = ParsedPermissionsPolicyDeclaration(
        policy_name, filtered_origins, allow_list.MatchesAll(),
        allow_list.MatchesOpaqueSrc());
    container_policy.push_back(declaration);
  }
  auto new_policy = PermissionsPolicy::CopyStateFrom(current_policy);
  new_policy->OverwriteHeaderPolicyForClientHints(container_policy);

  // Update third-party delegation permissions for each client hint.
  local_dom_window->GetSecurityContext().SetPermissionsPolicy(
      std::move(new_policy));
}

}  // namespace blink
