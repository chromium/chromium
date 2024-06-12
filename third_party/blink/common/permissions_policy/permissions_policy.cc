// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/fenced_frame_permissions_policies.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace blink {

PermissionsPolicy::Allowlist::Allowlist() = default;

PermissionsPolicy::Allowlist::Allowlist(const Allowlist& rhs) = default;

PermissionsPolicy::Allowlist::~Allowlist() = default;

PermissionsPolicy::Allowlist PermissionsPolicy::Allowlist::FromDeclaration(
    const ParsedPermissionsPolicyDeclaration& parsed_declaration) {
  auto result = PermissionsPolicy::Allowlist();
  if (parsed_declaration.self_if_matches) {
    result.AddSelf(parsed_declaration.self_if_matches);
  }
  if (parsed_declaration.matches_all_origins)
    result.AddAll();
  if (parsed_declaration.matches_opaque_src)
    result.AddOpaqueSrc();
  for (const auto& value : parsed_declaration.allowed_origins)
    result.Add(value);

  return result;
}

void PermissionsPolicy::Allowlist::Add(
    const blink::OriginWithPossibleWildcards& origin) {
  allowed_origins_.push_back(origin);
}

void PermissionsPolicy::Allowlist::AddSelf(std::optional<url::Origin> self) {
  self_if_matches_ = std::move(self);
}

void PermissionsPolicy::Allowlist::AddAll() {
  matches_all_origins_ = true;
}

void PermissionsPolicy::Allowlist::AddOpaqueSrc() {
  matches_opaque_src_ = true;
}

bool PermissionsPolicy::Allowlist::Contains(const url::Origin& origin) const {
  if (origin == self_if_matches_) {
    return true;
  }
  for (const auto& allowed_origin : allowed_origins_) {
    if (allowed_origin.DoesMatchOrigin(origin))
      return true;
  }
  if (origin.opaque())
    return matches_opaque_src_;
  return matches_all_origins_;
}

const std::optional<url::Origin>& PermissionsPolicy::Allowlist::SelfIfMatches()
    const {
  return self_if_matches_;
}

bool PermissionsPolicy::Allowlist::MatchesAll() const {
  return matches_all_origins_;
}

void PermissionsPolicy::Allowlist::RemoveMatchesAll() {
  matches_all_origins_ = false;
}

bool PermissionsPolicy::Allowlist::MatchesOpaqueSrc() const {
  return matches_opaque_src_;
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFromParentPolicy(
    const PermissionsPolicy* parent_policy,
    const ParsedPermissionsPolicy& header_policy,
    const ParsedPermissionsPolicy& container_policy,
    const url::Origin& origin) {
  return CreateFromParentPolicy(parent_policy, header_policy, container_policy,
                                origin,
                                GetPermissionsPolicyFeatureList(origin));
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CopyStateFrom(
    const PermissionsPolicy* source) {
  if (!source)
    return nullptr;

  std::unique_ptr<PermissionsPolicy> new_policy = base::WrapUnique(
      new PermissionsPolicy(source->origin_, {source->allowlists_, {}},
                            source->inherited_policies_,
                            GetPermissionsPolicyFeatureList(source->origin_)));

  return new_policy;
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFromParsedPolicy(
    const ParsedPermissionsPolicy& parsed_policy,
    const std::optional<ParsedPermissionsPolicy>& base_policy,
    const url::Origin& origin) {
  return CreateFromParsedPolicy(parsed_policy, base_policy, origin,
                                GetPermissionsPolicyFeatureList(origin));
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFromParsedPolicy(
    const ParsedPermissionsPolicy& parsed_policy,
    const std::optional<ParsedPermissionsPolicy>&
        parsed_policy_for_isolated_app,
    const url::Origin& origin,
    const PermissionsPolicyFeatureList& features) {
  PermissionsPolicyFeatureState inherited_policies;
  AllowlistsAndReportingEndpoints allow_lists_and_reporting_endpoints =
      parsed_policy_for_isolated_app
          ? CombinePolicies(parsed_policy_for_isolated_app.value(),
                            parsed_policy)
          : CreateAllowlistsAndReportingEndpoints(parsed_policy);
  for (const auto& feature : features) {
    inherited_policies[feature.first] =
        base::Contains(allow_lists_and_reporting_endpoints.allowlists_,
                       feature.first) &&
        allow_lists_and_reporting_endpoints.allowlists_[feature.first].Contains(
            origin);
  }

  std::unique_ptr<PermissionsPolicy> new_policy = base::WrapUnique(
      new PermissionsPolicy(origin, allow_lists_and_reporting_endpoints,
                            inherited_policies, features));

  return new_policy;
}

bool PermissionsPolicy::IsFeatureEnabledByInheritedPolicy(
    mojom::PermissionsPolicyFeature feature) const {
  DCHECK(base::Contains(inherited_policies_, feature));
  return inherited_policies_.at(feature);
}

bool PermissionsPolicy::IsFeatureEnabled(
    mojom::PermissionsPolicyFeature feature) const {
  return IsFeatureEnabledForOrigin(feature, origin_);
}

bool PermissionsPolicy::IsFeatureEnabledForOrigin(
    mojom::PermissionsPolicyFeature feature,
    const url::Origin& origin) const {
  return IsFeatureEnabledForOriginImpl(feature, origin, /*opt_in_features=*/{});
}

bool PermissionsPolicy::IsFeatureEnabledForSubresourceRequest(
    mojom::PermissionsPolicyFeature feature,
    const url::Origin& origin,
    const network::ResourceRequest& request) const {
  // Derive the opt-in features from the request attributes.
  std::set<mojom::PermissionsPolicyFeature> opt_in_features;
  if (request.browsing_topics) {
    DCHECK(base::FeatureList::IsEnabled(blink::features::kBrowsingTopics));

    opt_in_features.insert(mojom::PermissionsPolicyFeature::kBrowsingTopics);
    opt_in_features.insert(
        mojom::PermissionsPolicyFeature::kBrowsingTopicsBackwardCompatible);
  }

  // Note that currently permissions for `sharedStorageWritable` are checked
  // using `IsFeatureEnabledForSubresourceRequestAssumingOptIn()`, since a
  // `network::ResourceRequest` is not available at the call site and
  // `blink::ResourceRequest` should not be used in blink public APIs.
  if (request.shared_storage_writable_eligible) {
    DCHECK(base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI));
    opt_in_features.insert(mojom::PermissionsPolicyFeature::kSharedStorage);
  }

  if (request.ad_auction_headers) {
    DCHECK(
        base::FeatureList::IsEnabled(blink::features::kInterestGroupStorage));

    opt_in_features.insert(mojom::PermissionsPolicyFeature::kRunAdAuction);
  }

  return IsFeatureEnabledForOriginImpl(feature, origin, opt_in_features);
}

// Implements Permissions Policy 9.8: Get feature value for origin.
// Version https://www.w3.org/TR/2023/WD-permissions-policy-1-20231218/
bool PermissionsPolicy::GetFeatureValueForOrigin(
    mojom::PermissionsPolicyFeature feature,
    const url::Origin& origin) const {
  DCHECK(base::Contains(*feature_list_, feature));

  // 9.8.2 If policy’s inherited policy for feature is "Disabled", return
  // "Disabled".
  if (!IsFeatureEnabledByInheritedPolicy(feature)) {
    return false;
  }

  // 9.8.3 If feature is present in policy’s declared policy:
  //   1 If the allowlist for feature in policy’s declared policy matches
  //     origin, then return "Enabled".
  //   2 Otherwise return "Disabled".
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end()) {
    return allowlist->second.Contains(origin);
  }

  // 9.8.4 Return "Enabled".
  return true;
}

const PermissionsPolicy::Allowlist PermissionsPolicy::GetAllowlistForDevTools(
    mojom::PermissionsPolicyFeature feature) const {
  // Return an empty allowlist when disabled through inheritance.
  if (!IsFeatureEnabledByInheritedPolicy(feature))
    return PermissionsPolicy::Allowlist();

  // Return defined policy if exists; otherwise return default policy.
  const auto& maybe_allow_list = GetAllowlistForFeatureIfExists(feature);
  if (maybe_allow_list.has_value())
    return maybe_allow_list.value();

  // Note: |allowlists_| purely comes from HTTP header. If a feature is not
  // declared in HTTP header, all origins are implicitly allowed unless the
  // default is `EnableForNone`.
  PermissionsPolicy::Allowlist default_allowlist;
  const PermissionsPolicyFeatureDefault default_policy =
      feature_list_->at(feature);
  switch (default_policy) {
    case PermissionsPolicyFeatureDefault::EnableForAll:
    case PermissionsPolicyFeatureDefault::EnableForSelf:
      default_allowlist.AddAll();
      break;
    case PermissionsPolicyFeatureDefault::EnableForNone:
      break;
  }

  return default_allowlist;
}

// TODO(crbug.com/937131): Use |PermissionsPolicy::GetAllowlistForDevTools|
// to replace this method. This method uses legacy |default_allowlist|
// calculation method.
const PermissionsPolicy::Allowlist PermissionsPolicy::GetAllowlistForFeature(
    mojom::PermissionsPolicyFeature feature) const {
  DCHECK(base::Contains(*feature_list_, feature));
  // Return an empty allowlist when disabled through inheritance.
  if (!IsFeatureEnabledByInheritedPolicy(feature))
    return PermissionsPolicy::Allowlist();

  // Return defined policy if exists; otherwise return default policy.
  const auto& maybe_allow_list = GetAllowlistForFeatureIfExists(feature);
  if (maybe_allow_list.has_value())
    return maybe_allow_list.value();

  const PermissionsPolicyFeatureDefault default_policy =
      feature_list_->at(feature);
  PermissionsPolicy::Allowlist default_allowlist;

  switch (default_policy) {
    case PermissionsPolicyFeatureDefault::EnableForAll:
      default_allowlist.AddAll();
      break;
    case PermissionsPolicyFeatureDefault::EnableForSelf: {
      std::optional<blink::OriginWithPossibleWildcards>
          origin_with_possible_wildcards =
              blink::OriginWithPossibleWildcards::FromOrigin(origin_);
      if (origin_with_possible_wildcards.has_value()) {
        default_allowlist.Add(*origin_with_possible_wildcards);
      }
    } break;
    case PermissionsPolicyFeatureDefault::EnableForNone:
      break;
  }

  return default_allowlist;
}

std::optional<const PermissionsPolicy::Allowlist>
PermissionsPolicy::GetAllowlistForFeatureIfExists(
    mojom::PermissionsPolicyFeature feature) const {
  // Return an empty allowlist when disabled through inheritance.
  if (!IsFeatureEnabledByInheritedPolicy(feature))
    return std::nullopt;

  // Only return allowlist if actually in `allowlists_`.
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end())
    return allowlist->second;
  return std::nullopt;
}

std::optional<std::string> PermissionsPolicy::GetEndpointForFeature(
    mojom::PermissionsPolicyFeature feature) const {
  auto endpoint = reporting_endpoints_.find(feature);
  if (endpoint != reporting_endpoints_.end()) {
    return endpoint->second;
  }
  return std::nullopt;
}

// static
PermissionsPolicy::AllowlistsAndReportingEndpoints
PermissionsPolicy::CreateAllowlistsAndReportingEndpoints(
    const ParsedPermissionsPolicy& parsed_header) {
  AllowlistsAndReportingEndpoints allow_lists_and_reporting_endpoints;
  for (const ParsedPermissionsPolicyDeclaration& parsed_declaration :
       parsed_header) {
    mojom::PermissionsPolicyFeature feature = parsed_declaration.feature;
    DCHECK(feature != mojom::PermissionsPolicyFeature::kNotFound);
    allow_lists_and_reporting_endpoints.allowlists_.emplace(
        feature, Allowlist::FromDeclaration(parsed_declaration));
    if (parsed_declaration.reporting_endpoint.has_value()) {
      allow_lists_and_reporting_endpoints.reporting_endpoints_.insert(
          {feature, parsed_declaration.reporting_endpoint.value()});
    }
  }
  return allow_lists_and_reporting_endpoints;
}

// static
PermissionsPolicy::AllowlistsAndReportingEndpoints
PermissionsPolicy::CombinePolicies(
    const ParsedPermissionsPolicy& base_policy,
    const ParsedPermissionsPolicy& second_policy) {
  PermissionsPolicy::AllowlistsAndReportingEndpoints
      allow_lists_and_reporting_endpoints =
          CreateAllowlistsAndReportingEndpoints(base_policy);
  for (const ParsedPermissionsPolicyDeclaration& parsed_declaration :
       second_policy) {
    mojom::PermissionsPolicyFeature feature = parsed_declaration.feature;
    DCHECK(feature != mojom::PermissionsPolicyFeature::kNotFound);
    const auto& second_allowlist =
        PermissionsPolicy::Allowlist::FromDeclaration(parsed_declaration);
    auto& base_allowlist =
        allow_lists_and_reporting_endpoints.allowlists_.at(feature);

    // If the header does not specify further restrictions we do not need to
    // modify the policy.
    if (second_allowlist.MatchesAll()) {
      continue;
    }

    const auto& second_allowed_origins = second_allowlist.AllowedOrigins();
    // If the manifest allows all origins access to this feature, use the more
    // restrictive header policy.
    if (base_allowlist.MatchesAll()) {
      // TODO(https://crbug.com/40847608): Refactor to use Allowlist::clone()
      // after clone() is implemented.
      base_allowlist.SetAllowedOrigins(second_allowed_origins);
      base_allowlist.RemoveMatchesAll();
      base_allowlist.AddSelf(second_allowlist.SelfIfMatches());
      continue;
    }

    // Otherwise, we use the intersection of origins in the manifest and the
    // header.
    auto manifest_allowed_origins = base_allowlist.AllowedOrigins();
    std::vector<blink::OriginWithPossibleWildcards> final_allowed_origins;
    // TODO(https://crbug.com/339404063): consider rewriting this to not be
    // O(N^2).
    for (const auto& origin : manifest_allowed_origins) {
      if (base::Contains(second_allowed_origins, origin)) {
        final_allowed_origins.push_back(origin);
      }
    }
    base_allowlist.SetAllowedOrigins(final_allowed_origins);
  }
  return allow_lists_and_reporting_endpoints;
}

std::unique_ptr<PermissionsPolicy> PermissionsPolicy::WithClientHints(
    const ParsedPermissionsPolicy& parsed_header) const {
  std::map<mojom::PermissionsPolicyFeature, Allowlist> allowlists = allowlists_;
  for (const ParsedPermissionsPolicyDeclaration& parsed_declaration :
       parsed_header) {
    mojom::PermissionsPolicyFeature feature = parsed_declaration.feature;
    DCHECK(GetPolicyFeatureToClientHintMap().contains(feature));
    allowlists[feature] = Allowlist::FromDeclaration(parsed_declaration);
  }

  return base::WrapUnique(new PermissionsPolicy(
      origin_, {allowlists, reporting_endpoints_}, inherited_policies_,
      GetPermissionsPolicyFeatureList(origin_)));
}

const mojom::PermissionsPolicyFeature
    PermissionsPolicy::defined_opt_in_features_[] = {
        mojom::PermissionsPolicyFeature::kBrowsingTopics,
        mojom::PermissionsPolicyFeature::kBrowsingTopicsBackwardCompatible,
        mojom::PermissionsPolicyFeature::kSharedStorage,
        mojom::PermissionsPolicyFeature::kRunAdAuction};

PermissionsPolicy::PermissionsPolicy(
    url::Origin origin,
    AllowlistsAndReportingEndpoints allow_lists_and_reporting_endpoints,
    PermissionsPolicyFeatureState inherited_policies,
    const PermissionsPolicyFeatureList& feature_list)
    : origin_(std::move(origin)),
      allowlists_(std::move(allow_lists_and_reporting_endpoints.allowlists_)),
      reporting_endpoints_(
          std::move(allow_lists_and_reporting_endpoints.reporting_endpoints_)),
      inherited_policies_(std::move(inherited_policies)),
      feature_list_(feature_list) {}

PermissionsPolicy::~PermissionsPolicy() = default;

// static
std::unique_ptr<PermissionsPolicy>
PermissionsPolicy::CreateFlexibleForFencedFrame(
    const PermissionsPolicy* parent_policy,
    const ParsedPermissionsPolicy& header_policy,
    const ParsedPermissionsPolicy& container_policy,
    const url::Origin& subframe_origin) {
  return CreateFlexibleForFencedFrame(
      parent_policy, header_policy, container_policy, subframe_origin,
      GetPermissionsPolicyFeatureList(subframe_origin));
}

// static
std::unique_ptr<PermissionsPolicy>
PermissionsPolicy::CreateFlexibleForFencedFrame(
    const PermissionsPolicy* parent_policy,
    const ParsedPermissionsPolicy& header_policy,
    const ParsedPermissionsPolicy& container_policy,
    const url::Origin& subframe_origin,
    const PermissionsPolicyFeatureList& features) {
  PermissionsPolicyFeatureState inherited_policies;
  for (const auto& feature : features) {
    if (base::Contains(kFencedFrameAllowedFeatures, feature.first)) {
      inherited_policies[feature.first] = InheritedValueForFeature(
          subframe_origin, parent_policy, feature, container_policy);
    } else {
      inherited_policies[feature.first] = false;
    }
  }
  return base::WrapUnique(new PermissionsPolicy(
      subframe_origin, CreateAllowlistsAndReportingEndpoints(header_policy),
      inherited_policies, features));
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFixedForFencedFrame(
    const url::Origin& origin,
    const ParsedPermissionsPolicy& header_policy,
    base::span<const blink::mojom::PermissionsPolicyFeature>
        effective_enabled_permissions) {
  return CreateFixedForFencedFrame(origin, header_policy,
                                   GetPermissionsPolicyFeatureList(origin),
                                   effective_enabled_permissions);
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFixedForFencedFrame(
    const url::Origin& origin,
    const ParsedPermissionsPolicy& header_policy,
    const PermissionsPolicyFeatureList& features,
    base::span<const blink::mojom::PermissionsPolicyFeature>
        effective_enabled_permissions) {
  PermissionsPolicyFeatureState inherited_policies;
  for (const auto& feature : features) {
    inherited_policies[feature.first] = false;
  }
  for (const blink::mojom::PermissionsPolicyFeature feature :
       effective_enabled_permissions) {
    inherited_policies[feature] = true;
  }

  return base::WrapUnique(new PermissionsPolicy(
      origin, CreateAllowlistsAndReportingEndpoints(header_policy),
      inherited_policies, features));
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFromParentPolicy(
    const PermissionsPolicy* parent_policy,
    const ParsedPermissionsPolicy& header_policy,
    const ParsedPermissionsPolicy& container_policy,
    const url::Origin& origin,
    const PermissionsPolicyFeatureList& features) {
  PermissionsPolicyFeatureState inherited_policies;
  for (const auto& feature : features) {
    inherited_policies[feature.first] = InheritedValueForFeature(
        origin, parent_policy, feature, container_policy);
  }
  return base::WrapUnique(new PermissionsPolicy(
      origin, CreateAllowlistsAndReportingEndpoints(header_policy),
      inherited_policies, features));
}

// Implements Permissions Policy 9.9: Is feature enabled in document for origin?
// Version https://www.w3.org/TR/2023/WD-permissions-policy-1-20230717/
bool PermissionsPolicy::IsFeatureEnabledForOriginImpl(
    mojom::PermissionsPolicyFeature feature,
    const url::Origin& origin,
    const std::set<mojom::PermissionsPolicyFeature>& opt_in_features) const {
  DCHECK(base::Contains(*feature_list_, feature));

  // 9.9.2: If policy’s inherited policy for feature is Disabled, return
  // "Disabled".
  if (!IsFeatureEnabledByInheritedPolicy(feature)) {
    return false;
  }

  // 9.9.3: If feature is present in policy’s declared policy:
  //    1. If the allowlist for feature in policy’s declared policy matches
  //       origin, then return "Enabled".
  //    2. Otherwise return "Disabled".
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end()) {
    return allowlist->second.Contains(origin);
  }

  // Proposed algorithm change in
  // https://github.com/w3c/webappsec-permissions-policy/pull/499: if
  // optInFeatures contains feature, then return "Enabled".
  if (base::Contains(opt_in_features, feature)) {
    return true;
  }

  const PermissionsPolicyFeatureDefault default_policy =
      feature_list_->at(feature);

  switch (default_policy) {
    case PermissionsPolicyFeatureDefault::EnableForAll:
      // 9.9.4: If feature’s default allowlist is *, return "Enabled".
      return true;
    case PermissionsPolicyFeatureDefault::EnableForSelf:
      // 9.9.5: If feature’s default allowlist is 'self', and origin is same
      // origin with document’s origin, return "Enabled".
      if (origin_.IsSameOriginWith(origin)) {
        return true;
      }
      break;
    case PermissionsPolicyFeatureDefault::EnableForNone:
      break;
  }
  // 9.9.6: Return "Disabled".
  return false;
}

bool PermissionsPolicy::IsFeatureEnabledForSubresourceRequestAssumingOptIn(
    mojom::PermissionsPolicyFeature feature,
    const url::Origin& origin) const {
  CHECK(base::Contains(defined_opt_in_features_, feature));

  // Make an opt-in features set containing exactly `feature`, as we're not
  // given access to the full request to derive any other opt-in features.
  std::set<mojom::PermissionsPolicyFeature> opt_in_features({feature});

  return IsFeatureEnabledForOriginImpl(feature, origin, opt_in_features);
}

// Implements Permissions Policy 9.7: Define an inherited policy for
// feature in container at origin.
// Version https://www.w3.org/TR/2023/WD-permissions-policy-1-20230717/
// static
bool PermissionsPolicy::InheritedValueForFeature(
    const url::Origin& origin,
    const PermissionsPolicy* parent_policy,
    std::pair<mojom::PermissionsPolicyFeature, PermissionsPolicyFeatureDefault>
        feature,
    const ParsedPermissionsPolicy& container_policy) {
  // 9.7 1: If container is null, return "Enabled".
  if (!parent_policy) {
    return true;
  }

  // 9.7 2: If the result of executing Get feature value for origin on feature,
  // container’s node document, and container’s node document’s origin is
  // "Disabled", return "Disabled".
  if (!parent_policy->GetFeatureValueForOrigin(feature.first,
                                               parent_policy->origin_)) {
    return false;
  }

  // 9.7 3: If feature was inherited and (if declared) the allowlist for the
  // feature does not match origin, then return "Disabled".
  if (!parent_policy->GetFeatureValueForOrigin(feature.first, origin)) {
    return false;
  }

  for (const auto& decl : container_policy) {
    if (decl.feature == feature.first) {
      // 9.7 5.1: If the allowlist for feature in container policy matches
      // origin, return "Enabled".
      // 9.7 5.2: Otherwise return "Disabled".
      return Allowlist::FromDeclaration(decl).Contains(origin);
    }
  }
  switch (feature.second) {
    case PermissionsPolicyFeatureDefault::EnableForAll:
      // 9.7 6: If feature’s default allowlist is *, return "Enabled".
      return true;
    case PermissionsPolicyFeatureDefault::EnableForSelf:
      // 9.7 7: If feature’s default allowlist is 'self', and origin is same
      // origin with container’s node document’s origin, return "Enabled". 9.7
      if (origin.IsSameOriginWith(parent_policy->origin_)) {
        return true;
      }
      break;
    case PermissionsPolicyFeatureDefault::EnableForNone:
      break;
  }
  // 9.7 8: Otherwise return "Disabled".
  return false;
}

const PermissionsPolicyFeatureList& PermissionsPolicy::GetFeatureList() const {
  return *feature_list_;
}

}  // namespace blink
