// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/feature_policy.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

namespace blink {
namespace {

// Extracts an Allowlist from a ParsedFeaturePolicyDeclaration.
std::unique_ptr<FeaturePolicy::Allowlist> AllowlistFromDeclaration(
    const ParsedFeaturePolicyDeclaration& parsed_declaration,
    const FeaturePolicy::FeatureList& feature_list) {
  std::unique_ptr<FeaturePolicy::Allowlist> result =
      base::WrapUnique(new FeaturePolicy::Allowlist());
  result->SetFallbackValue(parsed_declaration.fallback_value);
  result->SetOpaqueValue(parsed_declaration.opaque_value);
  for (const auto& value : parsed_declaration.allowed_origins)
    result->Add(value);

  return result;
}

}  // namespace

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration()
    : fallback_value(false), opaque_value(false) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    mojom::FeaturePolicyFeature feature)
    : feature(feature), fallback_value(false), opaque_value(false) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    mojom::FeaturePolicyFeature feature,
    const std::vector<url::Origin>& allowed_origins,
    bool fallback_value,
    bool opaque_value)
    : feature(feature),
      allowed_origins(allowed_origins),
      fallback_value(fallback_value),
      opaque_value(opaque_value) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration& ParsedFeaturePolicyDeclaration::operator=(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration::~ParsedFeaturePolicyDeclaration() = default;

bool operator==(const ParsedFeaturePolicyDeclaration& lhs,
                const ParsedFeaturePolicyDeclaration& rhs) {
  return std::tie(lhs.feature, lhs.fallback_value, lhs.opaque_value,
                  lhs.allowed_origins) ==
         std::tie(rhs.feature, rhs.fallback_value, rhs.opaque_value,
                  rhs.allowed_origins);
}

FeaturePolicy::Allowlist::Allowlist()
    : fallback_value_(false), opaque_value_(false) {}

FeaturePolicy::Allowlist::Allowlist(const Allowlist& rhs) = default;

FeaturePolicy::Allowlist::~Allowlist() = default;

void FeaturePolicy::Allowlist::Add(const url::Origin& origin) {
  allowed_origins_.push_back(origin);
}

bool FeaturePolicy::Allowlist::GetValueForOrigin(
    const url::Origin& origin) const {
  for (const auto& allowed_origin : allowed_origins_) {
    if (origin == allowed_origin)
      return true;
  }
  if (origin.opaque())
    return opaque_value_;
  return fallback_value_;
}

bool FeaturePolicy::Allowlist::GetFallbackValue() const {
  return fallback_value_;
}

void FeaturePolicy::Allowlist::SetFallbackValue(bool fallback_value) {
  fallback_value_ = fallback_value;
}

bool FeaturePolicy::Allowlist::GetOpaqueValue() const {
  return opaque_value_;
}

void FeaturePolicy::Allowlist::SetOpaqueValue(bool opaque_value) {
  opaque_value_ = opaque_value;
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::CreateFromParentPolicy(
    const FeaturePolicy* parent_policy,
    const ParsedFeaturePolicy& container_policy,
    const url::Origin& origin) {
  return CreateFromParentPolicy(parent_policy, container_policy, origin,
                                GetDefaultFeatureList());
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::CreateWithOpenerPolicy(
    const FeatureState& inherited_policies,
    const url::Origin& origin) {
  std::unique_ptr<FeaturePolicy> new_policy =
      base::WrapUnique(new FeaturePolicy(origin, GetDefaultFeatureList()));
  new_policy->inherited_policies_ = inherited_policies;
  new_policy->proposed_inherited_policies_ = inherited_policies;
  return new_policy;
}

bool FeaturePolicy::IsFeatureEnabled(
    mojom::FeaturePolicyFeature feature) const {
  return IsFeatureEnabledForOrigin(feature, origin_);
}

bool FeaturePolicy::IsFeatureEnabledForOrigin(
    mojom::FeaturePolicyFeature feature,
    const url::Origin& origin) const {
  return GetFeatureValueForOrigin(feature, origin);
}

bool FeaturePolicy::GetFeatureValueForOrigin(
    mojom::FeaturePolicyFeature feature,
    const url::Origin& origin) const {
  DCHECK(base::Contains(feature_list_, feature));
  DCHECK(base::Contains(inherited_policies_, feature));

  auto inherited_value = inherited_policies_.at(feature);
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end()) {
    auto specified_value = allowlist->second->GetValueForOrigin(origin);
    return inherited_value && specified_value;
  }

  // If no "allowlist" is specified, return default feature value.
  const FeaturePolicy::FeatureDefault default_policy =
      feature_list_.at(feature);
  if (default_policy == FeaturePolicy::FeatureDefault::DisableForAll ||
      (default_policy == FeaturePolicy::FeatureDefault::EnableForSelf &&
       !origin_.IsSameOriginWith(origin)))
    return false;
  return inherited_value;
}

// Temporary code to support metrics: (https://crbug.com/937131)
// This method implements a proposed algorithm change to feature policy in which
// the default allowlist for a feature if not specified in the header, is always
// '*', but where the header allowlist *must* allow the nested frame origin in
// order to delegate use of the feature to that frame.
bool FeaturePolicy::GetProposedFeatureValueForOrigin(
    mojom::FeaturePolicyFeature feature,
    const url::Origin& origin) const {
  DCHECK(base::Contains(feature_list_, feature));
  DCHECK(base::Contains(proposed_inherited_policies_, feature));

  auto inherited_value = proposed_inherited_policies_.at(feature);
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end()) {
    auto specified_value = allowlist->second->GetValueForOrigin(origin);
    return inherited_value && specified_value;
  }

  // If no allowlist is specified, return default feature value.
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

  const FeaturePolicy::FeatureDefault default_policy =
      feature_list_.at(feature);
  FeaturePolicy::Allowlist default_allowlist;

  if (default_policy == FeaturePolicy::FeatureDefault::EnableForAll) {
    default_allowlist.SetFallbackValue(true);
  } else if (default_policy == FeaturePolicy::FeatureDefault::EnableForSelf) {
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

FeaturePolicy::FeatureState FeaturePolicy::GetFeatureState() const {
  FeatureState feature_state;
  for (const auto& pair : GetDefaultFeatureList())
    feature_state[pair.first] = GetFeatureValueForOrigin(pair.first, origin_);
  return feature_state;
}

FeaturePolicy::FeaturePolicy(url::Origin origin,
                             const FeatureList& feature_list)
    : origin_(std::move(origin)), feature_list_(feature_list) {
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
  // For features which are not keys in a container policy, which is the case
  // here *until* we call AddContainerPolicy at the end of this method,
  // https://wicg.github.io/feature-policy/#define-inherited-policy-in-container
  // returns true if |feature| is enabled in |parent_policy| for |origin|.
  for (const auto& feature : features) {
    if (!parent_policy) {
      // If no parent policy, set inherited policy to true.
      new_policy->inherited_policies_[feature.first] = true;
      // Temporary code to support metrics (https://crbug.com/937131)
      new_policy->proposed_inherited_policies_[feature.first] = true;
      // End of temporary metrics code
    } else {
      new_policy->inherited_policies_[feature.first] =
          parent_policy->GetFeatureValueForOrigin(feature.first, origin);

      // Temporary code to support metrics (https://crbug.com/937131)
      new_policy->proposed_inherited_policies_[feature.first] =
          parent_policy->GetProposedFeatureValueForOrigin(
              feature.first, parent_policy->origin_) &&
          parent_policy->GetProposedFeatureValueForOrigin(feature.first,
                                                          origin);
      // For features which currently use 'self' default allowlist, set the
      // proposed inherited policy to "allow self" if the container policy does
      // not mention this feature at all.
      if (feature.second == FeaturePolicy::FeatureDefault::EnableForSelf) {
        bool found_in_container_policy = std::any_of(
            container_policy.begin(), container_policy.end(),
            [&](const auto& decl) { return decl.feature == feature.first; });
        if (!found_in_container_policy) {
          new_policy->proposed_inherited_policies_[feature.first] =
              new_policy->proposed_inherited_policies_[feature.first] &&
              origin.IsSameOriginWith(parent_policy->origin_);
        }
      }
      // End of temporary metrics code
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
  // For features which are keys in a container policy,
  // https://wicg.github.io/feature-policy/#define-inherited-policy-in-container
  // returns true only if |feature| is enabled in |parent| for either |origin|
  // or |parent|'s origin, and the allowlist for |feature| matches |origin|.
  //
  // Roughly, If a feature is enabled in the parent frame, and the parent
  // chooses to delegate it to the child frame, using the iframe attribute, then
  // the feature should be enabled in the child frame.
  for (const ParsedFeaturePolicyDeclaration& parsed_declaration :
       container_policy) {
    mojom::FeaturePolicyFeature feature = parsed_declaration.feature;

    // Temporary code to support metrics: (https://crbug.com/937131)
    // Compute the proposed new inherited value, where the parent *must* allow
    // the feature in the child frame, but where the default header value if not
    // specified is '*'.
    auto proposed_inherited_policy = proposed_inherited_policies_.find(feature);
    if (proposed_inherited_policy != proposed_inherited_policies_.end()) {
      bool& proposed_inherited_value = proposed_inherited_policy->second;
      proposed_inherited_value =
          proposed_inherited_value &&
          AllowlistFromDeclaration(parsed_declaration, feature_list_)
              ->GetValueForOrigin(origin_);
    }
    // End of metrics code

    // Do not allow setting a container policy for a feature which is not in the
    // feature list.
    auto inherited_policy = inherited_policies_.find(feature);
    if (inherited_policy == inherited_policies_.end())
      continue;
    bool& inherited_value = inherited_policy->second;
    // If enabled by |parent_policy| for either |origin| or |parent_policy|'s
    // origin, then enable in the child iff the declared container policy
    // matches |origin|.
    auto parent_value = parent_policy->GetFeatureValueForOrigin(
        feature, parent_policy->origin_);
    inherited_value = inherited_value || parent_value;
    inherited_value = inherited_value && AllowlistFromDeclaration(
                                             parsed_declaration, feature_list_)
                                             ->GetValueForOrigin(origin_);
  }
}

const FeaturePolicy::FeatureList& FeaturePolicy::GetFeatureList() const {
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

// static
// See third_party/blink/public/common/feature_policy/feature_policy.h for
// status of each feature (in spec, implemented, etc).
const FeaturePolicy::FeatureList& FeaturePolicy::GetDefaultFeatureList() {
  static base::NoDestructor<FeatureList> default_feature_list(
      {{mojom::FeaturePolicyFeature::kAccelerometer,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kAccessibilityEvents,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kAmbientLightSensor,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kAutoplay,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintDPR,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintDeviceMemory,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintDownlink,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintECT,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintLang,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintRTT,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintUA,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kClientHintUAArch,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintUAPlatform,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintUAModel,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintUAMobile,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kClientHintUAFullVersion,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintUAPlatformVersion,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintViewportWidth,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClientHintWidth,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kClipboard,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kCamera,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kDocumentDomain,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kDocumentWrite,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kDownloads,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kEncryptedMedia,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kExecutionWhileOutOfViewport,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kExecutionWhileNotRendered,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kFocusWithoutUserActivation,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kFormSubmission,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       // kFrobulate is a test only feature.
       {mojom::FeaturePolicyFeature::kFrobulate,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kFullscreen,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kGeolocation,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kGyroscope,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kHid,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kIdleDetection,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kMagnetometer,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kMicrophone,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kMidiFeature,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kModals,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kOrientationLock,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kPayment,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kPictureInPicture,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kPointerLock,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kPopups,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kPresentation,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kPublicKeyCredentialsGet,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kScript,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kSerial,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kSyncScript,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kSyncXHR,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kTopNavigation,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kUsb,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kVerticalScroll,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kScreenWakeLock,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kWebXr,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kStorageAccessAPI,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForAll)},
       {mojom::FeaturePolicyFeature::kTrustTokenRedemption,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)},
       {mojom::FeaturePolicyFeature::kConversionMeasurement,
        FeatureDefault(FeaturePolicy::FeatureDefault::EnableForSelf)}});
  return *default_feature_list;
}

}  // namespace blink
