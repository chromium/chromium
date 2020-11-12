// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/feature_policy.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/feature_policy/feature_policy_features.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

namespace blink {
namespace {

// Extracts an Allowlist from a ParsedFeaturePolicyDeclaration.
std::unique_ptr<FeaturePolicy::Allowlist> AllowlistFromDeclaration(
    const ParsedFeaturePolicyDeclaration& parsed_declaration,
    const FeaturePolicyFeatureList& feature_list) {
  std::unique_ptr<FeaturePolicy::Allowlist> result =
      base::WrapUnique(new FeaturePolicy::Allowlist());
  if (parsed_declaration.matches_all_origins)
    result->AddAll();
  if (parsed_declaration.matches_opaque_src)
    result->AddOpaqueSrc();
  for (const auto& value : parsed_declaration.allowed_origins)
    result->Add(value);

  return result;
}

}  // namespace

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration() = default;

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    mojom::FeaturePolicyFeature feature)
    : feature(feature) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    mojom::FeaturePolicyFeature feature,
    const std::vector<url::Origin>& allowed_origins,
    bool matches_all_origins,
    bool matches_opaque_src)
    : feature(feature),
      allowed_origins(allowed_origins),
      matches_all_origins(matches_all_origins),
      matches_opaque_src(matches_opaque_src) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration& ParsedFeaturePolicyDeclaration::operator=(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration::~ParsedFeaturePolicyDeclaration() = default;

bool operator==(const ParsedFeaturePolicyDeclaration& lhs,
                const ParsedFeaturePolicyDeclaration& rhs) {
  return std::tie(lhs.feature, lhs.matches_all_origins, lhs.matches_opaque_src,
                  lhs.allowed_origins) ==
         std::tie(rhs.feature, rhs.matches_all_origins, rhs.matches_opaque_src,
                  rhs.allowed_origins);
}

FeaturePolicy::Allowlist::Allowlist() = default;

FeaturePolicy::Allowlist::Allowlist(const Allowlist& rhs) = default;

FeaturePolicy::Allowlist::~Allowlist() = default;

void FeaturePolicy::Allowlist::Add(const url::Origin& origin) {
  allowed_origins_.push_back(origin);
}

void FeaturePolicy::Allowlist::AddAll() {
  matches_all_origins_ = true;
}

void FeaturePolicy::Allowlist::AddOpaqueSrc() {
  matches_opaque_src_ = true;
}

bool FeaturePolicy::Allowlist::Contains(const url::Origin& origin) const {
  for (const auto& allowed_origin : allowed_origins_) {
    if (origin == allowed_origin)
      return true;
  }
  if (origin.opaque())
    return matches_opaque_src_;
  return matches_all_origins_;
}

bool FeaturePolicy::Allowlist::MatchesAll() const {
  return matches_all_origins_;
}

bool FeaturePolicy::Allowlist::MatchesOpaqueSrc() const {
  return matches_opaque_src_;
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::CreateFromParentPolicy(
    const FeaturePolicy* parent_policy,
    const ParsedFeaturePolicy& container_policy,
    const url::Origin& origin) {
  return CreateFromParentPolicy(parent_policy, container_policy, origin,
                                GetFeaturePolicyFeatureList());
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::CreateWithOpenerPolicy(
    const FeaturePolicyFeatureState& inherited_policies,
    const url::Origin& origin) {
  std::unique_ptr<FeaturePolicy> new_policy = base::WrapUnique(
      new FeaturePolicy(origin, GetFeaturePolicyFeatureList()));
  new_policy->inherited_policies_ = inherited_policies;
  return new_policy;
}

bool FeaturePolicy::IsFeatureEnabled(
    mojom::FeaturePolicyFeature feature) const {
  return IsFeatureEnabledForOrigin(feature, origin_);
}

bool FeaturePolicy::IsFeatureEnabledForOrigin(
    mojom::FeaturePolicyFeature feature,
    const url::Origin& origin) const {
  DCHECK(base::Contains(feature_list_, feature));
  DCHECK(base::Contains(inherited_policies_, feature));

  auto inherited_value = inherited_policies_.at(feature);
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end()) {
    return inherited_value && allowlist->second->Contains(origin);
  }

  // If no "allowlist" is specified, return default feature value.
  const FeaturePolicyFeatureDefault default_policy = feature_list_.at(feature);
  if (default_policy == FeaturePolicyFeatureDefault::EnableForSelf &&
      !origin_.IsSameOriginWith(origin))
    return false;

  return inherited_value;
}

bool FeaturePolicy::GetFeatureValueForOrigin(
    mojom::FeaturePolicyFeature feature,
    const url::Origin& origin) const {
  DCHECK(base::Contains(feature_list_, feature));
  DCHECK(base::Contains(inherited_policies_, feature));

  auto inherited_value = inherited_policies_.at(feature);
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end()) {
    return inherited_value && allowlist->second->Contains(origin);
  }

  return inherited_value;
}

const FeaturePolicy::Allowlist FeaturePolicy::GetAllowlistForFeature(
    mojom::FeaturePolicyFeature feature) const {
  DCHECK(base::Contains(feature_list_, feature));
  DCHECK(base::Contains(inherited_policies_, feature));
  // Return an empty allowlist when disabled through inheritance.
  if (!inherited_policies_.at(feature))
    return FeaturePolicy::Allowlist();

  // Return defined policy if exists; otherwise return default policy.
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end())
    return FeaturePolicy::Allowlist(*(allowlist->second));

  const FeaturePolicyFeatureDefault default_policy = feature_list_.at(feature);
  FeaturePolicy::Allowlist default_allowlist;

  if (default_policy == FeaturePolicyFeatureDefault::EnableForAll) {
    default_allowlist.AddAll();
  } else if (default_policy == FeaturePolicyFeatureDefault::EnableForSelf) {
    default_allowlist.Add(origin_);
  }

  return default_allowlist;
}

void FeaturePolicy::SetHeaderPolicy(const ParsedFeaturePolicy& parsed_header) {
  DCHECK(allowlists_.empty());
  for (const ParsedFeaturePolicyDeclaration& parsed_declaration :
       parsed_header) {
    mojom::FeaturePolicyFeature feature = parsed_declaration.feature;
    DCHECK(feature != mojom::FeaturePolicyFeature::kNotFound);
    allowlists_[feature] =
        AllowlistFromDeclaration(parsed_declaration, feature_list_);
  }
}

FeaturePolicyFeatureState FeaturePolicy::GetFeatureState() const {
  FeaturePolicyFeatureState feature_state;
  for (const auto& pair : GetFeaturePolicyFeatureList())
    feature_state[pair.first] = GetFeatureValueForOrigin(pair.first, origin_);
  return feature_state;
}

FeaturePolicy::FeaturePolicy(url::Origin origin,
                             const FeaturePolicyFeatureList& feature_list)
    : origin_(std::move(origin)), feature_list_(feature_list) {}

FeaturePolicy::~FeaturePolicy() = default;

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::CreateFromParentPolicy(
    const FeaturePolicy* parent_policy,
    const ParsedFeaturePolicy& container_policy,
    const url::Origin& origin,
    const FeaturePolicyFeatureList& features) {
  // If there is a non-empty container policy, then there must also be a parent
  // policy.
  DCHECK(parent_policy || container_policy.empty());

  std::unique_ptr<FeaturePolicy> new_policy =
      base::WrapUnique(new FeaturePolicy(origin, features));
  for (const auto& feature : features) {
    new_policy->inherited_policies_[feature.first] =
        new_policy->InheritedValueForFeature(parent_policy, feature,
                                             container_policy);
  }
  return new_policy;
}

// Implements Permissions Policy 9.7: Define an inherited policy for feature in
// browsing context and 9.8: Define an inherited policy for feature in container
// at origin.
bool FeaturePolicy::InheritedValueForFeature(
    const FeaturePolicy* parent_policy,
    std::pair<mojom::FeaturePolicyFeature, FeaturePolicyFeatureDefault> feature,
    const ParsedFeaturePolicy& container_policy) const {
  // 9.7 2: Otherwise [If context is not a nested browsing context,] return
  // "Enabled".
  if (!parent_policy)
    return true;

  // 9.8 2: If policy’s inherited policy for feature is "Disabled", return
  // "Disabled".
  if (!parent_policy->GetFeatureValueForOrigin(feature.first,
                                               parent_policy->origin_))
    return false;

  // 9.8 3: If feature is present in policy’s declared policy, and the allowlist
  // for feature in policy’s declared policy does not match origin, then return
  // "Disabled".
  if (!parent_policy->GetFeatureValueForOrigin(feature.first, origin_))
    return false;

  for (const auto& decl : container_policy) {
    if (decl.feature == feature.first) {
      // 9.8 5.1: If the allowlist for feature in container policy matches
      // origin, return "Enabled".
      // 9.8 5.2: Otherwise return "Disabled".
      return AllowlistFromDeclaration(decl, feature_list_)->Contains(origin_);
    }
  }
  // 9.8 6: If feature’s default allowlist is *, return "Enabled".
  if (feature.second == FeaturePolicyFeatureDefault::EnableForAll)
    return true;

  // 9.8 7: If feature’s default allowlist is 'self', and origin is same origin
  // with container’s node document’s origin, return "Enabled".
  // 9.8 8: Otherwise return "Disabled".
  return origin_.IsSameOriginWith(parent_policy->origin_);
}

const FeaturePolicyFeatureList& FeaturePolicy::GetFeatureList() const {
  return feature_list_;
}

// static
mojom::FeaturePolicyFeature FeaturePolicy::FeatureForSandboxFlag(
    network::mojom::WebSandboxFlags flag) {
  switch (flag) {
    case network::mojom::WebSandboxFlags::kAll:
      NOTREACHED();
      break;
    case network::mojom::WebSandboxFlags::kTopNavigation:
      return mojom::FeaturePolicyFeature::kTopNavigation;
    case network::mojom::WebSandboxFlags::kForms:
      return mojom::FeaturePolicyFeature::kFormSubmission;
    case network::mojom::WebSandboxFlags::kAutomaticFeatures:
    case network::mojom::WebSandboxFlags::kScripts:
      return mojom::FeaturePolicyFeature::kScript;
    case network::mojom::WebSandboxFlags::kPopups:
      return mojom::FeaturePolicyFeature::kPopups;
    case network::mojom::WebSandboxFlags::kPointerLock:
      return mojom::FeaturePolicyFeature::kPointerLock;
    case network::mojom::WebSandboxFlags::kOrientationLock:
      return mojom::FeaturePolicyFeature::kOrientationLock;
    case network::mojom::WebSandboxFlags::kModals:
      return mojom::FeaturePolicyFeature::kModals;
    case network::mojom::WebSandboxFlags::kPresentationController:
      return mojom::FeaturePolicyFeature::kPresentation;
    case network::mojom::WebSandboxFlags::kDownloads:
      return mojom::FeaturePolicyFeature::kDownloads;
    // Other flags fall through to the bitmask test below. They are named
    // specifically here so that authors introducing new flags must consider
    // this method when adding them.
    case network::mojom::WebSandboxFlags::kDocumentDomain:
    case network::mojom::WebSandboxFlags::kNavigation:
    case network::mojom::WebSandboxFlags::kNone:
    case network::mojom::WebSandboxFlags::kOrigin:
    case network::mojom::WebSandboxFlags::kPlugins:
    case network::mojom::WebSandboxFlags::
        kPropagatesToAuxiliaryBrowsingContexts:
    case network::mojom::WebSandboxFlags::kTopNavigationByUserActivation:
    case network::mojom::WebSandboxFlags::kStorageAccessByUserActivation:
      break;
  }
  return mojom::FeaturePolicyFeature::kNotFound;
}

}  // namespace blink
