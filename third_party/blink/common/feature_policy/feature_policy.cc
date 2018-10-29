// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/feature_policy.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"

namespace blink {
namespace {

// Extracts an Allowlist from a ParsedFeaturePolicyDeclaration.
std::unique_ptr<FeaturePolicy::Allowlist> AllowlistFromDeclaration(
    const ParsedFeaturePolicyDeclaration& parsed_declaration) {
  std::unique_ptr<FeaturePolicy::Allowlist> result =
      base::WrapUnique(new FeaturePolicy::Allowlist());
  if (parsed_declaration.matches_all_origins)
    result->AddAll();
  for (const auto& origin : parsed_declaration.origins)
    result->Add(origin);

  return result;
}

}  // namespace

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration()
    : matches_all_origins(false), matches_opaque_src(false) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    mojom::FeaturePolicyFeature feature,
    bool matches_all_origins,
    bool matches_opaque_src,
    std::vector<url::Origin> origins)
    : feature(feature),
      matches_all_origins(matches_all_origins),
      matches_opaque_src(matches_opaque_src),
      origins(origins) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration& ParsedFeaturePolicyDeclaration::operator=(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration::~ParsedFeaturePolicyDeclaration() = default;

bool operator==(const ParsedFeaturePolicyDeclaration& lhs,
                const ParsedFeaturePolicyDeclaration& rhs) {
  // This method returns true only when the arguments are actually identical,
  // including the order of elements in the origins vector.
  // TODO(iclelland): Consider making this return true when comparing equal-
  // but-not-identical allowlists, or eliminate those comparisons by maintaining
  // the allowlists in a normalized form.
  // https://crbug.com/710324
  return std::tie(lhs.feature, lhs.matches_all_origins, lhs.origins) ==
         std::tie(rhs.feature, rhs.matches_all_origins, rhs.origins);
}

FeaturePolicy::Allowlist::Allowlist() : matches_all_origins_(false) {}

FeaturePolicy::Allowlist::Allowlist(const Allowlist& rhs) = default;

FeaturePolicy::Allowlist::~Allowlist() = default;

void FeaturePolicy::Allowlist::Add(const url::Origin& origin) {
  origins_.push_back(origin);
}

void FeaturePolicy::Allowlist::AddAll() {
  matches_all_origins_ = true;
}

bool FeaturePolicy::Allowlist::Contains(const url::Origin& origin) const {
  // This does not handle the case where origin is an opaque origin, which is
  // also supposed to exist in the allowlist. (The identical opaque origins
  // should match in that case)
  // TODO(iclelland): Fix that, possibly by having another flag for
  // 'matches_self', which will explicitly match the policy's origin.
  // https://crbug.com/690520
  if (matches_all_origins_)
    return true;
  for (const auto& targetOrigin : origins_) {
    if (!origin.opaque() && targetOrigin.IsSameOriginWith(origin))
      return true;
  }
  return false;
}

bool FeaturePolicy::Allowlist::MatchesAll() const {
  return matches_all_origins_;
}

const std::vector<url::Origin>& FeaturePolicy::Allowlist::Origins() const {
  return origins_;
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::CreateFromParentPolicy(
    const FeaturePolicy* parent_policy,
    const ParsedFeaturePolicy& container_policy,
    const url::Origin& origin) {
  return CreateFromParentPolicy(parent_policy, container_policy, origin,
                                GetDefaultFeatureList());
}

bool FeaturePolicy::IsFeatureEnabled(
    mojom::FeaturePolicyFeature feature) const {
  return IsFeatureEnabledForOrigin(feature, origin_);
}

bool FeaturePolicy::IsFeatureEnabledForOrigin(
    mojom::FeaturePolicyFeature feature,
    const url::Origin& origin) const {
  DCHECK(base::ContainsKey(feature_list_, feature));
  DCHECK(base::ContainsKey(inherited_policies_, feature));
  if (!inherited_policies_.at(feature))
    return false;
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end())
    return allowlist->second->Contains(origin);

  const FeaturePolicy::FeatureDefault default_policy =
      feature_list_.at(feature);
  if (default_policy == FeaturePolicy::FeatureDefault::EnableForAll)
    return true;
  if (default_policy == FeaturePolicy::FeatureDefault::EnableForSelf)
    return origin_.IsSameOriginWith(origin);
  return false;
}

const FeaturePolicy::Allowlist FeaturePolicy::GetAllowlistForFeature(
    mojom::FeaturePolicyFeature feature) const {
  DCHECK(base::ContainsKey(feature_list_, feature));
  DCHECK(base::ContainsKey(inherited_policies_, feature));
  // Disabled through inheritance.
  if (!inherited_policies_.at(feature))
    return FeaturePolicy::Allowlist();

  // Return defined policy if exists; otherwise return default policy.
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end())
    return FeaturePolicy::Allowlist(*(allowlist->second));

  const FeaturePolicy::FeatureDefault default_policy =
      feature_list_.at(feature);
  FeaturePolicy::Allowlist default_allowlist;

  if (default_policy == FeaturePolicy::FeatureDefault::EnableForAll)
    default_allowlist.AddAll();
  else if (default_policy == FeaturePolicy::FeatureDefault::EnableForSelf)
    default_allowlist.Add(origin_);
  return default_allowlist;
}

void FeaturePolicy::SetHeaderPolicy(const ParsedFeaturePolicy& parsed_header) {
  DCHECK(allowlists_.empty());
  for (const ParsedFeaturePolicyDeclaration& parsed_declaration :
       parsed_header) {
    mojom::FeaturePolicyFeature feature = parsed_declaration.feature;
    DCHECK(feature != mojom::FeaturePolicyFeature::kNotFound);
    allowlists_[feature] = AllowlistFromDeclaration(parsed_declaration);
  }
}

FeaturePolicy::FeaturePolicy(url::Origin origin,
                             const FeatureList& feature_list)
    : origin_(std::move(origin)), feature_list_(feature_list) {
  if (origin_.opaque()) {
    // FeaturePolicy was written expecting opaque Origins to be indistinct, but
    // this has changed. Split out a new opaque origin here, to defend against
    // origin-equality.
    origin_ = origin_.DeriveNewOpaqueOrigin();
  }
}

FeaturePolicy::~FeaturePolicy() = default;

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::CreateFromParentPolicy(
    const FeaturePolicy* parent_policy,
    const ParsedFeaturePolicy& container_policy,
    const url::Origin& origin,
    const FeaturePolicy::FeatureList& features) {
  // If there is a non-empty container policy, then there must also be a parent
  // policy.
  DCHECK(parent_policy || container_policy.empty());

  std::unique_ptr<FeaturePolicy> new_policy =
      base::WrapUnique(new FeaturePolicy(origin, features));
  for (const auto& feature : features) {
    if (!parent_policy ||
        parent_policy->IsFeatureEnabledForOrigin(feature.first, origin)) {
      new_policy->inherited_policies_[feature.first] = true;
    } else {
      new_policy->inherited_policies_[feature.first] = false;
    }
  }
  if (!container_policy.empty())
    new_policy->AddContainerPolicy(container_policy, parent_policy);
  return new_policy;
}

void FeaturePolicy::AddContainerPolicy(
    const ParsedFeaturePolicy& container_policy,
    const FeaturePolicy* parent_policy) {
  DCHECK(parent_policy);
  for (const ParsedFeaturePolicyDeclaration& parsed_declaration :
       container_policy) {
    // If a feature is enabled in the parent frame, and the parent chooses to
    // delegate it to the child frame, using the iframe attribute, then the
    // feature should be enabled in the child frame.
    mojom::FeaturePolicyFeature feature = parsed_declaration.feature;
    // Do not allow setting a container policy for a feature which is not in the
    // feature list.
    auto search = inherited_policies_.find(feature);
    if (search == inherited_policies_.end())
      continue;
    bool& inherited_policy = search->second;
    // If the parent frame does not enable the feature, then the child frame
    // must not.
    inherited_policy = false;
    if (parent_policy->IsFeatureEnabled(feature)) {
      if (parsed_declaration.matches_opaque_src && origin_.opaque()) {
        // If the child frame has an opaque origin, and the declared container
        // policy indicates that the feature should be enabled, enable it for
        // the child frame.
        inherited_policy = true;
      } else if (AllowlistFromDeclaration(parsed_declaration)
                     ->Contains(origin_)) {
        // Otherwise, enbable the feature if the declared container policy
        // includes the origin of the child frame.
        inherited_policy = true;
      }
    }
  }
}

// static
// See third_party/blink/public/common/feature_policy/feature_policy.h for
// status of each feature (in spec, implemented, etc).
const FeaturePolicy::FeatureList& FeaturePolicy::GetDefaultFeatureList() {
  static base::NoDestructor<FeatureList> default_feature_list(
      {{mojom::FeaturePolicyFeature::kAccelerometer,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kAccessibilityEvents,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kAmbientLightSensor,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kAutoplay,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kCamera,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kDocumentWrite,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kEncryptedMedia,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kFullscreen,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kGeolocation,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kGyroscope,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kImageCompression,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kLayoutAnimations,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kLazyLoad,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kLegacyImageFormats,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kMagnetometer,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kMaxDownscalingImage,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kMicrophone,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kMidiFeature,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kPayment,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kPictureInPicture,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kSpeaker,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kSyncScript,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kSyncXHR,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kUnsizedMedia,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kUsb,
        FeaturePolicy::FeatureDefault::EnableForSelf},
       {mojom::FeaturePolicyFeature::kVerticalScroll,
        FeaturePolicy::FeatureDefault::EnableForAll},
       {mojom::FeaturePolicyFeature::kWebVr,
        FeaturePolicy::FeatureDefault::EnableForSelf}});
  return *default_feature_list;
}

}  // namespace blink
