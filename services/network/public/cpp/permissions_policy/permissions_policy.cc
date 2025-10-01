// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy.h"

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/permissions_policy/client_hints_permissions_policy_mapping.h"
#include "services/network/public/cpp/permissions_policy/fenced_frame_permissions_policies.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features_bitset.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features_generated.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "url/gurl.h"

namespace {

const std::array<network::mojom::PermissionsPolicyFeature, 5>
    kDefinedOptInFeatures = {
        network::mojom::PermissionsPolicyFeature::kBrowsingTopics,
        network::mojom::PermissionsPolicyFeature::
            kBrowsingTopicsBackwardCompatible,
        network::mojom::PermissionsPolicyFeature::kSharedStorage,
        network::mojom::PermissionsPolicyFeature::kRunAdAuction,
        network::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup};

}  // namespace

namespace network {

PermissionsPolicy::Allowlist::Allowlist() = default;

PermissionsPolicy::Allowlist::Allowlist(const Allowlist& rhs) = default;

PermissionsPolicy::Allowlist::~Allowlist() = default;

PermissionsPolicy::Allowlist::Allowlist(Allowlist&&) noexcept = default;
PermissionsPolicy::Allowlist& PermissionsPolicy::Allowlist::operator=(
    Allowlist&&) noexcept = default;

PermissionsPolicy::Allowlist PermissionsPolicy::Allowlist::FromDeclaration(
    const network::ParsedPermissionsPolicyDeclaration& parsed_declaration) {
  auto result = PermissionsPolicy::Allowlist();
  if (parsed_declaration.self_if_matches) {
    result.AddSelf(parsed_declaration.self_if_matches);
  }
  if (parsed_declaration.matches_all_origins) {
    result.AddAll();
  }
  if (parsed_declaration.matches_opaque_src) {
    result.AddOpaqueSrc();
  }
  for (const auto& value : parsed_declaration.allowed_origins) {
    result.Add(value);
  }

  return result;
}

void PermissionsPolicy::Allowlist::Add(
    const network::OriginWithPossibleWildcards& origin) {
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
    if (allowed_origin.DoesMatchOrigin(origin)) {
      return true;
    }
  }
  if (origin.opaque()) {
    return matches_opaque_src_;
  }
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

PermissionsPolicy::AllowlistsAndReportingEndpoints::
    AllowlistsAndReportingEndpoints() = default;
PermissionsPolicy::AllowlistsAndReportingEndpoints::
    ~AllowlistsAndReportingEndpoints() = default;
PermissionsPolicy::AllowlistsAndReportingEndpoints::
    AllowlistsAndReportingEndpoints(
        const AllowlistsAndReportingEndpoints& other) = default;
PermissionsPolicy::AllowlistsAndReportingEndpoints::
    AllowlistsAndReportingEndpoints(AllowlistsAndReportingEndpoints&& other) =
        default;
PermissionsPolicy::AllowlistsAndReportingEndpoints::
    AllowlistsAndReportingEndpoints(
        std::map<network::mojom::PermissionsPolicyFeature, Allowlist>
            allowlists,
        std::map<network::mojom::PermissionsPolicyFeature, std::string>
            reporting_endpoints)
    : allowlists_(allowlists), reporting_endpoints_(reporting_endpoints) {}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFromParentPolicy(
    const PermissionsPolicy* parent_policy,
    const network::ParsedPermissionsPolicy& header_policy,
    const network::ParsedPermissionsPolicy& container_policy,
    const url::Origin& origin,
    bool headerless) {
  return CreateFromParentPolicy(
      parent_policy, header_policy, container_policy, origin,
      network::GetPermissionsPolicyFeatureList(origin), headerless);
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CopyStateFrom(
    const PermissionsPolicy* source) {
  if (!source) {
    return nullptr;
  }

  std::unique_ptr<PermissionsPolicy> new_policy =
      base::WrapUnique(new PermissionsPolicy(
          source->origin_, {source->allowlists_, {}},
          source->inherited_policies_,
          network::GetPermissionsPolicyFeatureList(source->origin_),
          source->headerless_));

  return new_policy;
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFromParsedPolicy(
    const network::ParsedPermissionsPolicy& parsed_policy,
    const std::optional<network::ParsedPermissionsPolicy>& base_policy,
    const url::Origin& origin) {
  return CreateFromParsedPolicy(
      parsed_policy, base_policy, origin,
      network::GetPermissionsPolicyFeatureList(origin));
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFromParsedPolicy(
    const network::ParsedPermissionsPolicy& parsed_policy,
    const std::optional<network::ParsedPermissionsPolicy>&
        parsed_policy_for_isolated_app,
    const url::Origin& origin,
    const network::PermissionsPolicyFeatureList& features) {
  network::PermissionsPolicyFeaturesBitset inherited_policies;
  AllowlistsAndReportingEndpoints allow_lists_and_reporting_endpoints =
      parsed_policy_for_isolated_app
          ? CombinePolicies(parsed_policy_for_isolated_app.value(),
                            parsed_policy)
          : CreateAllowlistsAndReportingEndpoints(parsed_policy);
  for (const auto& [feature, unused] : features) {
    if (base::Contains(allow_lists_and_reporting_endpoints.allowlists_,
                       feature) &&
        allow_lists_and_reporting_endpoints.allowlists_[feature].Contains(
            origin)) {
      inherited_policies.Add(feature);
    }
  }

  std::unique_ptr<PermissionsPolicy> new_policy = base::WrapUnique(
      new PermissionsPolicy(origin, allow_lists_and_reporting_endpoints,
                            std::move(inherited_policies), features));

  return new_policy;
}

// static
bool PermissionsPolicy::IsHeaderlessUrl(const GURL& url) {
  return url.IsAboutSrcdoc() || url.IsAboutBlank() ||
         url.SchemeIs(url::kDataScheme) || url.SchemeIsBlob();
}

bool PermissionsPolicy::IsFeatureEnabledByInheritedPolicy(
    network::mojom::PermissionsPolicyFeature feature) const {
  return inherited_policies_.Contains(feature);
}

bool PermissionsPolicy::IsFeatureEnabled(
    network::mojom::PermissionsPolicyFeature feature) const {
  return IsFeatureEnabledForOrigin(feature, origin_);
}

// Implements Permissions Policy 9.9: Is feature enabled in document for origin?
// Version https://www.w3.org/TR/2023/WD-permissions-policy-1-20230717/
bool PermissionsPolicy::IsFeatureEnabledForOrigin(
    network::mojom::PermissionsPolicyFeature feature,
    const url::Origin& origin,
    bool override_default_policy_to_all) const {
  DCHECK(base::Contains(*feature_list_, feature));
  DCHECK(!override_default_policy_to_all ||
         base::Contains(kDefinedOptInFeatures, feature));

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
  // https://github.com/w3c/webappsec-permissions-policy/pull/499.
  if (override_default_policy_to_all) {
    return true;
  }

  const network::PermissionsPolicyFeatureDefault default_policy =
      feature_list_->at(feature);

  switch (default_policy) {
    case network::PermissionsPolicyFeatureDefault::EnableForAll:
      // 9.9.4: If feature’s default allowlist is *, return "Enabled".
      return true;
    case network::PermissionsPolicyFeatureDefault::EnableForSelf:
      // 9.9.5: If feature’s default allowlist is 'self', and origin is same
      // origin with document’s origin, return "Enabled".
      if (origin_.IsSameOriginWith(origin)) {
        return true;
      }
      break;
    case network::PermissionsPolicyFeatureDefault::EnableForNone:
      if (headerless_) {
        // Proposed algorithm change in
        // https://github.com/w3c/webappsec-permissions-policy/pull/515:
        // 9.9.6 Return "Disabled".
        return true;
      }
      break;
  }
  // 9.9.6: Return "Disabled".
  return false;
}

// Implements Permissions Policy 9.8: Get feature value for origin.
// Version https://www.w3.org/TR/2023/WD-permissions-policy-1-20231218/
bool PermissionsPolicy::GetFeatureValueForOrigin(
    network::mojom::PermissionsPolicyFeature feature,
    network::PermissionsPolicyFeatureDefault default_policy,
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

  switch (default_policy) {
    case network::PermissionsPolicyFeatureDefault::EnableForAll:
    case network::PermissionsPolicyFeatureDefault::EnableForSelf:
      // 9.8.4 Return "Enabled".
      return true;
    case network::PermissionsPolicyFeatureDefault::EnableForNone:
      // Proposed algorithm change in
      // https://github.com/w3c/webappsec-permissions-policy/pull/515:
      // 9.8.5 Return "Disabled".
      return false;
  }
}

const PermissionsPolicy::Allowlist PermissionsPolicy::GetAllowlistForDevTools(
    network::mojom::PermissionsPolicyFeature feature) const {
  // Return an empty allowlist when disabled through inheritance.
  if (!IsFeatureEnabledByInheritedPolicy(feature)) {
    return PermissionsPolicy::Allowlist();
  }

  // Return defined policy if exists; otherwise return default policy.
  const auto& maybe_allow_list = GetAllowlistForFeatureIfExists(feature);
  if (maybe_allow_list.has_value()) {
    return maybe_allow_list.value();
  }

  // Note: |allowlists_| purely comes from HTTP header. If a feature is not
  // declared in HTTP header, all origins are implicitly allowed unless the
  // default is `EnableForNone`.
  PermissionsPolicy::Allowlist default_allowlist;
  const network::PermissionsPolicyFeatureDefault default_policy =
      feature_list_->at(feature);
  switch (default_policy) {
    case network::PermissionsPolicyFeatureDefault::EnableForAll:
    case network::PermissionsPolicyFeatureDefault::EnableForSelf:
      default_allowlist.AddAll();
      break;
    case network::PermissionsPolicyFeatureDefault::EnableForNone:
      break;
  }

  return default_allowlist;
}

// TODO(crbug.com/40094174): Use |PermissionsPolicy::GetAllowlistForDevTools|
// to replace this method. This method uses legacy |default_allowlist|
// calculation method.
const PermissionsPolicy::Allowlist PermissionsPolicy::GetAllowlistForFeature(
    network::mojom::PermissionsPolicyFeature feature) const {
  DCHECK(base::Contains(*feature_list_, feature));
  // Return an empty allowlist when disabled through inheritance.
  if (!IsFeatureEnabledByInheritedPolicy(feature)) {
    return PermissionsPolicy::Allowlist();
  }

  // Return defined policy if exists; otherwise return default policy.
  const auto& maybe_allow_list = GetAllowlistForFeatureIfExists(feature);
  if (maybe_allow_list.has_value()) {
    return maybe_allow_list.value();
  }

  const network::PermissionsPolicyFeatureDefault default_policy =
      feature_list_->at(feature);
  PermissionsPolicy::Allowlist default_allowlist;

  switch (default_policy) {
    case network::PermissionsPolicyFeatureDefault::EnableForAll:
      default_allowlist.AddAll();
      break;
    case network::PermissionsPolicyFeatureDefault::EnableForSelf: {
      std::optional<network::OriginWithPossibleWildcards>
          origin_with_possible_wildcards =
              network::OriginWithPossibleWildcards::FromOrigin(origin_);
      if (origin_with_possible_wildcards.has_value()) {
        default_allowlist.Add(*origin_with_possible_wildcards);
      }
    } break;
    case network::PermissionsPolicyFeatureDefault::EnableForNone:
      break;
  }

  return default_allowlist;
}

std::optional<const PermissionsPolicy::Allowlist>
PermissionsPolicy::GetAllowlistForFeatureIfExists(
    network::mojom::PermissionsPolicyFeature feature) const {
  // Return an empty allowlist when disabled through inheritance.
  if (!IsFeatureEnabledByInheritedPolicy(feature)) {
    return std::nullopt;
  }

  // Only return allowlist if actually in `allowlists_`.
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end()) {
    return allowlist->second;
  }
  return std::nullopt;
}

std::string PermissionsPolicy::GetEndpointForFeature(
    network::mojom::PermissionsPolicyFeature feature) const {
  auto endpoint = reporting_endpoints_.find(feature);
  if (endpoint != reporting_endpoints_.end()) {
    return endpoint->second;
  }
  return std::string();
}

// static
PermissionsPolicy::AllowlistsAndReportingEndpoints
PermissionsPolicy::CreateAllowlistsAndReportingEndpoints(
    const network::ParsedPermissionsPolicy& parsed_header) {
  AllowlistsAndReportingEndpoints allow_lists_and_reporting_endpoints;
  for (const network::ParsedPermissionsPolicyDeclaration& parsed_declaration :
       parsed_header) {
    network::mojom::PermissionsPolicyFeature feature =
        parsed_declaration.feature;
    DCHECK(feature != network::mojom::PermissionsPolicyFeature::kNotFound);
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
    const network::ParsedPermissionsPolicy& base_policy,
    const network::ParsedPermissionsPolicy& second_policy) {
  PermissionsPolicy::AllowlistsAndReportingEndpoints
      allow_lists_and_reporting_endpoints =
          CreateAllowlistsAndReportingEndpoints(base_policy);
  for (const network::ParsedPermissionsPolicyDeclaration& parsed_declaration :
       second_policy) {
    network::mojom::PermissionsPolicyFeature feature =
        parsed_declaration.feature;
    DCHECK(feature != network::mojom::PermissionsPolicyFeature::kNotFound);

    const auto& second_allowlist =
        PermissionsPolicy::Allowlist::FromDeclaration(parsed_declaration);
    auto* base_allowlist = base::FindOrNull(
        allow_lists_and_reporting_endpoints.allowlists_, feature);
    // If the feature isn't specified in the base policy, we can continue as
    // it shouldn't be in the combined policy either.
    if (!base_allowlist) {
      continue;
    }

    // If the header does not specify further restrictions we do not need to
    // modify the policy.
    if (second_allowlist.MatchesAll()) {
      continue;
    }

    const auto& second_allowed_origins = second_allowlist.AllowedOrigins();
    // If the manifest allows all origins access to this feature, use the more
    // restrictive header policy.
    if (base_allowlist->MatchesAll()) {
      // TODO(https://crbug.com/40847608): Refactor to use Allowlist::clone()
      // after clone() is implemented.
      base_allowlist->SetAllowedOrigins(second_allowed_origins);
      base_allowlist->RemoveMatchesAll();
      base_allowlist->AddSelf(second_allowlist.SelfIfMatches());
      continue;
    }

    // Otherwise, we use the intersection of origins in the manifest and the
    // header.
    auto manifest_allowed_origins = base_allowlist->AllowedOrigins();
    std::vector<network::OriginWithPossibleWildcards> final_allowed_origins;
    // TODO(https://crbug.com/339404063): consider rewriting this to not be
    // O(N^2).
    for (const auto& origin : manifest_allowed_origins) {
      if (base::Contains(second_allowed_origins, origin)) {
        final_allowed_origins.push_back(origin);
      }
    }
    base_allowlist->SetAllowedOrigins(final_allowed_origins);
    if (base_allowlist->SelfIfMatches() != second_allowlist.SelfIfMatches()) {
      base_allowlist->AddSelf(std::nullopt);
    }
  }
  return allow_lists_and_reporting_endpoints;
}

std::unique_ptr<PermissionsPolicy> PermissionsPolicy::WithClientHints(
    const network::ParsedPermissionsPolicy& parsed_header) const {
  std::map<network::mojom::PermissionsPolicyFeature, Allowlist> allowlists =
      allowlists_;
  for (const network::ParsedPermissionsPolicyDeclaration& parsed_declaration :
       parsed_header) {
    network::mojom::PermissionsPolicyFeature feature =
        parsed_declaration.feature;
    DCHECK(network::GetPolicyFeatureToClientHintMap().contains(feature));
    allowlists[feature] = Allowlist::FromDeclaration(parsed_declaration);
  }

  return base::WrapUnique(new PermissionsPolicy(
      origin_, {allowlists, reporting_endpoints_}, inherited_policies_,
      network::GetPermissionsPolicyFeatureList(origin_)));
}

PermissionsPolicy::PermissionsPolicy(mojo::DefaultConstruct::Tag)
    : feature_list_(GetPermissionsPolicyFeatureListUnloadNone()) {}

PermissionsPolicy::PermissionsPolicy(const PermissionsPolicy&) = default;
PermissionsPolicy& PermissionsPolicy::operator=(const PermissionsPolicy&) =
    default;

PermissionsPolicy::PermissionsPolicy(PermissionsPolicy&&) noexcept = default;
PermissionsPolicy& PermissionsPolicy::operator=(PermissionsPolicy&&) noexcept =
    default;

PermissionsPolicy::PermissionsPolicy(
    url::Origin origin,
    AllowlistsAndReportingEndpoints allow_lists_and_reporting_endpoints,
    network::PermissionsPolicyFeaturesBitset inherited_policies,
    const network::PermissionsPolicyFeatureList& feature_list,
    bool headerless)
    : origin_(std::move(origin)),
      headerless_(headerless),
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
    const network::ParsedPermissionsPolicy& header_policy,
    const network::ParsedPermissionsPolicy& container_policy,
    const url::Origin& subframe_origin) {
  return CreateFlexibleForFencedFrame(
      parent_policy, header_policy, container_policy, subframe_origin,
      network::GetPermissionsPolicyFeatureList(subframe_origin));
}

// static
std::unique_ptr<PermissionsPolicy>
PermissionsPolicy::CreateFlexibleForFencedFrame(
    const PermissionsPolicy* parent_policy,
    const network::ParsedPermissionsPolicy& header_policy,
    const network::ParsedPermissionsPolicy& container_policy,
    const url::Origin& subframe_origin,
    const network::PermissionsPolicyFeatureList& features) {
  network::PermissionsPolicyFeaturesBitset inherited_policies;
  for (const auto& [feature, default_value] : features) {
    if (base::Contains(network::kFencedFrameAllowedFeatures, feature) &&
        InheritedValueForFeature(subframe_origin, parent_policy,
                                 {feature, default_value}, container_policy)) {
      inherited_policies.Add(feature);
    }
  }
  return base::WrapUnique(new PermissionsPolicy(
      subframe_origin, CreateAllowlistsAndReportingEndpoints(header_policy),
      std::move(inherited_policies), features));
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFixedForFencedFrame(
    const url::Origin& origin,
    const network::ParsedPermissionsPolicy& header_policy,
    base::span<const network::mojom::PermissionsPolicyFeature>
        effective_enabled_permissions) {
  return CreateFixedForFencedFrame(
      origin, header_policy, network::GetPermissionsPolicyFeatureList(origin),
      effective_enabled_permissions);
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFixedForFencedFrame(
    const url::Origin& origin,
    const network::ParsedPermissionsPolicy& header_policy,
    const network::PermissionsPolicyFeatureList& features,
    base::span<const network::mojom::PermissionsPolicyFeature>
        effective_enabled_permissions) {
  network::PermissionsPolicyFeaturesBitset inherited_policies;
  for (const network::mojom::PermissionsPolicyFeature feature :
       effective_enabled_permissions) {
    inherited_policies.Add(feature);
  }

  return base::WrapUnique(new PermissionsPolicy(
      origin, CreateAllowlistsAndReportingEndpoints(header_policy),
      std::move(inherited_policies), features));
}

// static
std::unique_ptr<PermissionsPolicy> PermissionsPolicy::CreateFromParentPolicy(
    const PermissionsPolicy* parent_policy,
    const network::ParsedPermissionsPolicy& header_policy,
    const network::ParsedPermissionsPolicy& container_policy,
    const url::Origin& origin,
    const network::PermissionsPolicyFeatureList& features,
    bool headerless) {
  network::PermissionsPolicyFeaturesBitset inherited_policies;
  for (const auto& [feature, default_value] : features) {
    if (InheritedValueForFeature(origin, parent_policy,
                                 {feature, default_value}, container_policy)) {
      inherited_policies.Add(feature);
    }
  }
  return base::WrapUnique(new PermissionsPolicy(
      origin, CreateAllowlistsAndReportingEndpoints(header_policy),
      std::move(inherited_policies), features, headerless));
}

// Implements Permissions Policy 9.7: Define an inherited policy for
// feature in container at origin.
// Version https://www.w3.org/TR/2023/WD-permissions-policy-1-20230717/
// static
bool PermissionsPolicy::InheritedValueForFeature(
    const url::Origin& origin,
    const PermissionsPolicy* parent_policy,
    std::pair<network::mojom::PermissionsPolicyFeature,
              network::PermissionsPolicyFeatureDefault> feature,
    const network::ParsedPermissionsPolicy& container_policy) {
  // 9.7 1: If container is null, return "Enabled".
  if (!parent_policy) {
    return true;
  }

  // 9.7 2: If the result of executing Get feature value for origin on feature,
  // container’s node document, and container’s node document’s origin is
  // "Disabled", return "Disabled".
  if (!parent_policy->GetFeatureValueForOrigin(feature.first, feature.second,
                                               parent_policy->origin_)) {
    return false;
  }

  // 9.7 3: If feature was inherited and (if declared) the allowlist for the
  // feature does not match origin, then return "Disabled".
  if (!parent_policy->GetFeatureValueForOrigin(feature.first, feature.second,
                                               origin)) {
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
    case network::PermissionsPolicyFeatureDefault::EnableForAll:
      // 9.7 6: If feature’s default allowlist is *, return "Enabled".
      return true;
    case network::PermissionsPolicyFeatureDefault::EnableForSelf:
      // 9.7 7: If feature’s default allowlist is 'self', and origin is same
      // origin with container’s node document’s origin, return "Enabled". 9.7
      if (origin.IsSameOriginWith(parent_policy->origin_)) {
        return true;
      }
      break;
    case network::PermissionsPolicyFeatureDefault::EnableForNone:
      break;
  }
  // 9.7 8: Otherwise return "Disabled".
  return false;
}

const network::PermissionsPolicyFeatureList& PermissionsPolicy::GetFeatureList()
    const {
  return *feature_list_;
}

}  // namespace network
