// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/feature_policy.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

namespace blink {
namespace {

// Extracts an Allowlist from a ParsedFeaturePolicyDeclaration.
std::unique_ptr<FeaturePolicy::Allowlist> AllowlistFromDeclaration(
    const ParsedFeaturePolicyDeclaration& parsed_declaration,
    const FeaturePolicy::FeatureList& feature_list) {
  mojom::PolicyValueType type =
      feature_list.at(parsed_declaration.feature).second;
  std::unique_ptr<FeaturePolicy::Allowlist> result =
      base::WrapUnique(new FeaturePolicy::Allowlist(type));
  result->SetFallbackValue(parsed_declaration.fallback_value);
  result->SetOpaqueValue(parsed_declaration.opaque_value);
  for (const auto& value : parsed_declaration.values)
    result->Add(value.first, value.second);

  return result;
}

}  // namespace

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration()
    : fallback_value(false), opaque_value(false) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    mojom::FeaturePolicyFeature feature,
    mojom::PolicyValueType type)
    : feature(feature),
      fallback_value(PolicyValue::CreateMinPolicyValue(type)),
      opaque_value(PolicyValue::CreateMinPolicyValue(type)) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    mojom::FeaturePolicyFeature feature,
    const std::map<url::Origin, PolicyValue> values,
    const PolicyValue& fallback_value,
    const PolicyValue& opaque_value)
    : feature(feature),
      values(std::move(values)),
      fallback_value(std::move(fallback_value)),
      opaque_value(std::move(opaque_value)) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration& ParsedFeaturePolicyDeclaration::operator=(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration::~ParsedFeaturePolicyDeclaration() = default;

bool operator==(const ParsedFeaturePolicyDeclaration& lhs,
                const ParsedFeaturePolicyDeclaration& rhs) {
  if (lhs.feature != rhs.feature)
    return false;
  if (!(lhs.fallback_value == rhs.fallback_value))
    return false;
  if (!(lhs.opaque_value == rhs.opaque_value))
    return false;
  return lhs.values == rhs.values;
}

FeaturePolicy::Allowlist::Allowlist(mojom::PolicyValueType type)
    : fallback_value_(PolicyValue::CreateMinPolicyValue(type)),
      opaque_value_(PolicyValue::CreateMinPolicyValue(type)) {}

FeaturePolicy::Allowlist::Allowlist(const Allowlist& rhs) = default;

FeaturePolicy::Allowlist::~Allowlist() = default;

void FeaturePolicy::Allowlist::Add(const url::Origin& origin,
                                   const PolicyValue& value) {
  values_[origin] = value;
}

PolicyValue FeaturePolicy::Allowlist::GetValueForOrigin(
    const url::Origin& origin) const {
  // |fallback_value_| will either be min (initialized in the parser) value or
  // set to the corresponding value for * origins.
  auto value = values_.find(origin);
  if (value != values_.end())
    return value->second;
  if (origin.opaque())
    return opaque_value_;
  return fallback_value_;
}

const PolicyValue& FeaturePolicy::Allowlist::GetFallbackValue() const {
  return fallback_value_;
}

void FeaturePolicy::Allowlist::SetFallbackValue(
    const PolicyValue& fallback_value) {
  fallback_value_ = fallback_value;
}

const PolicyValue& FeaturePolicy::Allowlist::GetOpaqueValue() const {
  return opaque_value_;
}

void FeaturePolicy::Allowlist::SetOpaqueValue(const PolicyValue& opaque_value) {
  opaque_value_ = opaque_value;
}

const base::flat_map<url::Origin, PolicyValue>&
FeaturePolicy::Allowlist::Values() const {
  return values_;
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
  return new_policy;
}

bool FeaturePolicy::IsFeatureEnabled(
    mojom::FeaturePolicyFeature feature) const {
  mojom::PolicyValueType feature_type = feature_list_.at(feature).second;
  return IsFeatureEnabledForOrigin(
      feature, origin_, PolicyValue::CreateMaxPolicyValue(feature_type));
}

bool FeaturePolicy::IsFeatureEnabled(mojom::FeaturePolicyFeature feature,
                                     const PolicyValue& threshold_value) const {
  return IsFeatureEnabledForOrigin(feature, origin_, threshold_value);
}

bool FeaturePolicy::IsFeatureEnabledForOrigin(
    mojom::FeaturePolicyFeature feature,
    const url::Origin& origin) const {
  mojom::PolicyValueType feature_type = feature_list_.at(feature).second;
  return GetFeatureValueForOrigin(feature, origin) >=
         PolicyValue::CreateMaxPolicyValue(feature_type);
}

bool FeaturePolicy::IsFeatureEnabledForOrigin(
    mojom::FeaturePolicyFeature feature,
    const url::Origin& origin,
    const PolicyValue& threshold_value) const {
  return GetFeatureValueForOrigin(feature, origin) >= threshold_value;
}

PolicyValue FeaturePolicy::GetFeatureValueForOrigin(
    mojom::FeaturePolicyFeature feature,
    const url::Origin& origin) const {
  DCHECK(base::Contains(feature_list_, feature));
  DCHECK(base::Contains(inherited_policies_, feature));

  auto inherited_value = inherited_policies_.at(feature);
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end()) {
    auto specified_value = allowlist->second->GetValueForOrigin(origin);
    return PolicyValue::Combine(inherited_value, specified_value);
  }

  // If no "allowlist" is specified, return default feature value.
  // Note that combining value "v" with min value "min_v" is "min_v" and
  // comining "v" with max value "max_v" is "v".
  const FeaturePolicy::FeatureDefaultValue default_policy =
      feature_list_.at(feature);
  if (default_policy.first == FeaturePolicy::FeatureDefault::DisableForAll ||
      (default_policy.first == FeaturePolicy::FeatureDefault::EnableForSelf &&
       !origin_.IsSameOriginWith(origin)))
    return PolicyValue::CreateMinPolicyValue(default_policy.second);
  return inherited_value;
}

const FeaturePolicy::Allowlist FeaturePolicy::GetAllowlistForFeature(
    mojom::FeaturePolicyFeature feature) const {
  DCHECK(base::Contains(feature_list_, feature));
  DCHECK(base::Contains(inherited_policies_, feature));
  mojom::PolicyValueType type = feature_list_.at(feature).second;
  // Return an empty allowlist when disabled through inheritance.
  if (inherited_policies_.at(feature) <=
      PolicyValue::CreateMinPolicyValue(type))
    return FeaturePolicy::Allowlist(type);

  // Return defined policy if exists; otherwise return default policy.
  auto allowlist = allowlists_.find(feature);
  if (allowlist != allowlists_.end())
    return FeaturePolicy::Allowlist(*(allowlist->second));

  const FeaturePolicy::FeatureDefaultValue default_policy =
      feature_list_.at(feature);
  FeaturePolicy::Allowlist default_allowlist(type);

  if (default_policy.first == FeaturePolicy::FeatureDefault::EnableForAll) {
    default_allowlist.SetFallbackValue(
        PolicyValue::CreateMaxPolicyValue(default_policy.second));
  } else if (default_policy.first ==
             FeaturePolicy::FeatureDefault::EnableForSelf) {
    default_allowlist.Add(
        origin_, PolicyValue::CreateMaxPolicyValue(default_policy.second));
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
      // If no parent policy, set inherited policy to max value.
      new_policy->inherited_policies_[feature.first] =
          PolicyValue::CreateMaxPolicyValue(feature.second.second);
    } else {
      new_policy->inherited_policies_[feature.first] =
          parent_policy->GetFeatureValueForOrigin(feature.first, origin);
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
    // Do not allow setting a container policy for a feature which is not in the
    // feature list.
    auto inherited_policy = inherited_policies_.find(feature);
    if (inherited_policy == inherited_policies_.end())
      continue;
    PolicyValue& inherited_value = inherited_policy->second;
    // If enabled by |parent_policy| for either |origin| or |parent_policy|'s
    // origin, then enable in the child iff the declared container policy
    // matches |origin|.
    auto parent_value = parent_policy->GetFeatureValueForOrigin(
        feature, parent_policy->origin_);
    inherited_value =
        inherited_value > parent_value ? inherited_value : parent_value;

    inherited_value.Combine(
        AllowlistFromDeclaration(parsed_declaration, feature_list_)
            ->GetValueForOrigin(origin_));
  }
}

const FeaturePolicy::FeatureList& FeaturePolicy::GetFeatureList() const {
  return feature_list_;
}

// static
mojom::FeaturePolicyFeature FeaturePolicy::FeatureForSandboxFlag(
    WebSandboxFlags flag) {
  switch (flag) {
    case WebSandboxFlags::kAll:
      NOTREACHED();
      break;
    case WebSandboxFlags::kTopNavigation:
      return mojom::FeaturePolicyFeature::kTopNavigation;
    case WebSandboxFlags::kForms:
      return mojom::FeaturePolicyFeature::kFormSubmission;
    case WebSandboxFlags::kAutomaticFeatures:
    case WebSandboxFlags::kScripts:
      return mojom::FeaturePolicyFeature::kScript;
    case WebSandboxFlags::kPopups:
      return mojom::FeaturePolicyFeature::kPopups;
    case WebSandboxFlags::kPointerLock:
      return mojom::FeaturePolicyFeature::kPointerLock;
    case WebSandboxFlags::kOrientationLock:
      return mojom::FeaturePolicyFeature::kOrientationLock;
    case WebSandboxFlags::kModals:
      return mojom::FeaturePolicyFeature::kModals;
    case WebSandboxFlags::kPresentationController:
      return mojom::FeaturePolicyFeature::kPresentation;
    case WebSandboxFlags::kDownloads:
      return mojom::FeaturePolicyFeature::kDownloadsWithoutUserActivation;
    // Other flags fall through to the bitmask test below. They are named
    // specifically here so that authors introducing new flags must consider
    // this method when adding them.
    case WebSandboxFlags::kDocumentDomain:
    case WebSandboxFlags::kNavigation:
    case WebSandboxFlags::kNone:
    case WebSandboxFlags::kOrigin:
    case WebSandboxFlags::kPlugins:
    case WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts:
    case WebSandboxFlags::kTopNavigationByUserActivation:
    case WebSandboxFlags::kStorageAccessByUserActivation:
      break;
  }
  return mojom::FeaturePolicyFeature::kNotFound;
}

// static
// See third_party/blink/public/common/feature_policy/feature_policy.h for
// status of each feature (in spec, implemented, etc).
// The second field of FeatureDefaultValue is the type of PolicyValue that is
// asspcoated with the feature.
// TODO(loonybear): replace boolean type value to the actual value type.
const FeaturePolicy::FeatureList& FeaturePolicy::GetDefaultFeatureList() {
  static base::NoDestructor<FeatureList> default_feature_list(
      {{mojom::FeaturePolicyFeature::kAccelerometer,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kAccessibilityEvents,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kAmbientLightSensor,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kAutoplay,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintDPR,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintDeviceMemory,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintDownlink,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintECT,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintLang,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintRTT,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintUA,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintUAArch,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintUAPlatform,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintUAModel,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintViewportWidth,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kClientHintWidth,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kCamera,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kDocumentAccess,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kDocumentDomain,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kDocumentWrite,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kDownloadsWithoutUserActivation,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kEncryptedMedia,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kExecutionWhileOutOfViewport,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kExecutionWhileNotRendered,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kFocusWithoutUserActivation,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kFontDisplay,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kFormSubmission,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       // kFrobulate is a test only feature.
       {mojom::FeaturePolicyFeature::kFrobulate,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kFullscreen,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kGeolocation,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kGyroscope,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kHid,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kIdleDetection,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kLayoutAnimations,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kLazyLoad,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kLoadingFrameDefaultEager,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kMagnetometer,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kOversizedImages,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kDecDouble)},
       {mojom::FeaturePolicyFeature::kMicrophone,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kMidiFeature,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kModals,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kOrientationLock,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kPayment,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kPictureInPicture,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kPointerLock,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kPopups,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kPresentation,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kScript,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kSerial,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kSyncScript,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kSyncXHR,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kTopNavigation,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kDecDouble)},
       {mojom::FeaturePolicyFeature::kUnoptimizedLosslessImagesStrict,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kDecDouble)},
       {mojom::FeaturePolicyFeature::kUnoptimizedLossyImages,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kDecDouble)},
       {mojom::FeaturePolicyFeature::kUnsizedMedia,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kUsb,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kVerticalScroll,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForAll,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kWakeLock,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kWebVr,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)},
       {mojom::FeaturePolicyFeature::kWebXr,
        FeatureDefaultValue(FeaturePolicy::FeatureDefault::EnableForSelf,
                            mojom::PolicyValueType::kBool)}});
  return *default_feature_list;
}

}  // namespace blink
