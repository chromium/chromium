// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/client_hints_util.h"

#include "base/containers/contains.h"
#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
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
    network::MetaCHType type,
    bool is_doc_preloader,
    bool is_sync_parser) {
  // If it's not http-equiv="accept-ch" and it's not a preload-or-sync-parser
  // visible meta tag, then we need to warn the dev that js injected the tag.
  if (type != network::MetaCHType::HttpEquivAcceptCH && !is_doc_preloader &&
      !is_sync_parser && local_dom_window) {
    AuditsIssue::ReportClientHintIssue(
        local_dom_window, ClientHintIssueReason::kMetaTagModifiedHTML);
  }

  // If no hints were set, this is a http-equiv="accept-ch" tag, this tag was
  // added by js, the `local_dom_window` is missing, or the feature is disabled,
  // there's nothing more to do.
  if (!client_hints_preferences.UpdateFromMetaCH(
          header_value, url, context, type, is_doc_preloader, is_sync_parser) ||
      type == network::MetaCHType::HttpEquivAcceptCH ||
      !(is_doc_preloader || is_sync_parser) || !local_dom_window) {
    return;
  }

  // Note: .Ascii() would convert tab to ?, which is undesirable.
  network::ClientHintToDelegatedThirdPartiesHeader parsed_ch =
      network::ParseClientHintToDelegatedThirdPartiesHeader(
          header_value.Latin1(), type);

  // If invalid origins were seen in the allow list we need to warn the dev.
  if (parsed_ch.had_invalid_origins) {
    AuditsIssue::ReportClientHintIssue(
        local_dom_window,
        ClientHintIssueReason::kMetaTagAllowListInvalidOrigin);
  }

  // Build vector of client hint permission policies to update.
  auto* const current_policy =
      local_dom_window->GetSecurityContext().GetPermissionsPolicy();
  ParsedPermissionsPolicy container_policy;
  for (const auto& pair : parsed_ch.map) {
    const auto& policy_name = GetClientHintToPolicyFeatureMap().at(pair.first);

    // We need to retain any preexisting settings, just adding new origins.
    const auto& allow_list =
        current_policy->GetAllowlistForFeature(policy_name);
    std::set<blink::OriginWithPossibleWildcards> origin_set(
        allow_list.AllowedOrigins().begin(), allow_list.AllowedOrigins().end());
    for (const auto& origin : pair.second) {
      if (auto origin_with_possible_wildcards =
              blink::OriginWithPossibleWildcards::FromOrigin(origin);
          origin_with_possible_wildcards.has_value()) {
        origin_set.insert(*origin_with_possible_wildcards);
      }
    }
    auto declaration = ParsedPermissionsPolicyDeclaration(
        policy_name,
        std::vector<blink::OriginWithPossibleWildcards>(origin_set.begin(),
                                                        origin_set.end()),
        allow_list.SelfIfMatches(), allow_list.MatchesAll(),
        allow_list.MatchesOpaqueSrc());
    container_policy.push_back(declaration);
  }
  auto new_policy = current_policy->WithClientHints(container_policy);

  // Update third-party delegation permissions for each client hint.
  local_dom_window->GetSecurityContext().SetPermissionsPolicy(
      std::move(new_policy));
}

void UpdateIFrameContainerPolicyWithDelegationSupportForClientHints(
    ParsedPermissionsPolicy& container_policy,
    LocalDOMWindow* local_dom_window) {
  if (!local_dom_window ||
      !local_dom_window->GetSecurityContext().GetPermissionsPolicy()) {
    return;
  }

  // To avoid the following section from being consistently O(n^2) we need to
  // break the container_policy vector into a map. We keep only the first policy
  // seen for each feature per PermissionsPolicy::InheritedValueForFeature.
  std::map<mojom::blink::PermissionsPolicyFeature,
           ParsedPermissionsPolicyDeclaration>
      feature_to_container_policy;
  for (const auto& candidate_policy : container_policy) {
    if (!base::Contains(feature_to_container_policy,
                        candidate_policy.feature)) {
      feature_to_container_policy[candidate_policy.feature] = candidate_policy;
    }
  }

  // Promote client hint features to container policy so any modified by HTML
  // via an accept-ch meta tag can propagate to the iframe.
  for (const auto& feature_and_hint : GetPolicyFeatureToClientHintMap()) {
    // This is the policy which may have been overridden by the meta tag via
    // UpdateWindowPermissionsPolicyWithDelegationSupportForClientHints we want
    // the iframe loader to use instead of the one it got earlier.
    const auto& maybe_window_allow_list =
        local_dom_window->GetSecurityContext()
            .GetPermissionsPolicy()
            ->GetAllowlistForFeatureIfExists(feature_and_hint.first);
    if (!maybe_window_allow_list.has_value()) {
      continue;
    }

    // If the container policy already has a parsed policy for the client hint
    // then use the first instance found and remove the others since that's
    // what `PermissionsPolicy::InheritedValueForFeature` pays attention to.
    ParsedPermissionsPolicyDeclaration merged_policy(feature_and_hint.first);
    auto it = feature_to_container_policy.find(feature_and_hint.first);
    if (it != feature_to_container_policy.end()) {
      merged_policy = it->second;
      RemoveFeatureIfPresent(feature_and_hint.first, container_policy);
    }

    // Now we apply the changes from the parent policy to ensure any changes
    // since it was set are respected;
    merged_policy.self_if_matches =
        maybe_window_allow_list.value().SelfIfMatches();
    merged_policy.matches_all_origins |=
        maybe_window_allow_list.value().MatchesAll();
    merged_policy.matches_opaque_src |=
        maybe_window_allow_list.value().MatchesOpaqueSrc();
    std::set<blink::OriginWithPossibleWildcards> origin_set;
    if (!merged_policy.matches_all_origins) {
      origin_set.insert(merged_policy.allowed_origins.begin(),
                        merged_policy.allowed_origins.end());
      origin_set.insert(
          maybe_window_allow_list.value().AllowedOrigins().begin(),
          maybe_window_allow_list.value().AllowedOrigins().end());
    }
    merged_policy.allowed_origins =
        std::vector(origin_set.begin(), origin_set.end());
    container_policy.push_back(merged_policy);
  }
}

}  // namespace blink
