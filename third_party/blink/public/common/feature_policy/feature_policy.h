// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_FEATURE_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_FEATURE_POLICY_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/feature_policy/policy_value.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-forward.h"
#include "url/origin.h"

namespace blink {

enum class WebSandboxFlags;

// Feature Policy is a mechanism for controlling the availability of web
// platform features in a frame, including all embedded frames. It can be used
// to remove features, automatically refuse API permission requests, or modify
// the behaviour of features. (The specific changes which are made depend on the
// feature; see the specification for details).
//
// Policies can be defined in the HTTP header stream, with the |Feature-Policy|
// HTTP header, or can be set by the |allow| attributes on the iframe element
// which embeds the document.
//
// See https://wicg.github.io/FeaturePolicy/
//
// Key concepts:
//
// Features
// --------
// Features which can be controlled by policy are defined by instances of enum
// mojom::FeaturePolicyFeature, declared in |feature_policy.mojom|.
//
// Allowlists
// ----------
// Allowlists are collections of origins, although two special terms can be used
// when declaring them:
//   "self" refers to the origin of the frame which is declaring the policy.
//   "*" refers to all origins; any origin will match an allowlist which
//   contains it.
//
// Declarations
// ------------
// A feature policy declaration is a mapping of a feature name to an allowlist.
// A set of declarations is a declared policy.
//
// Inherited Policy
// ----------------
// In addition to the declared policy (which may be empty), every frame has
// an inherited policy, which is determined by the context in which it is
// embedded, or by the defaults for each feature in the case of the top-level
// document.
//
// Container Policy
// ----------------
// A declared policy can be set on a specific frame by the embedding page using
// the iframe "allow" attribute, or through attributes such as "allowfullscreen"
// or "allowpaymentrequest". This is the container policy for the embedded
// frame.
//
// Defaults
// --------
// Each defined feature has a default policy, which determines whether the
// feature is available when no policy has been declared, ans determines how the
// feature is inherited across origin boundaries.
//
// If the default policy  is in effect for a frame, then it controls how the
// feature is inherited by any cross-origin iframes embedded by the frame. (See
// the comments below in FeaturePolicy::FeatureDefault for specifics)
//
// Policy Inheritance
// ------------------
// Policies in effect for a frame are inherited by any child frames it embeds.
// Unless another policy is declared in the child, all same-origin children will
// receive the same set of enables features as the parent frame. Whether or not
// features are inherited by cross-origin iframes without an explicit policy is
// determined by the feature's default policy. (Again, see the comments in
// FeaturePolicy::FeatureDefault for details)

// ListValue (PolicyValue)
// ----------------------
// PolicyValue is a union of types (int / double / set<int> / bool ) that can be
// used to specify the parameter of a policy.

// This struct holds feature policy allowlist data that needs to be replicated
// between a RenderFrame and any of its associated RenderFrameProxies. A list of
// these form a ParsedFeaturePolicy.
// NOTE: These types are used for replication frame state between processes.

struct BLINK_COMMON_EXPORT ParsedFeaturePolicyDeclaration {
  ParsedFeaturePolicyDeclaration();
  ParsedFeaturePolicyDeclaration(mojom::FeaturePolicyFeature feature,
                                 mojom::PolicyValueType type);
  ParsedFeaturePolicyDeclaration(
      mojom::FeaturePolicyFeature feature,
      const std::map<url::Origin, PolicyValue> values,
      const PolicyValue& fallback_value,
      const PolicyValue& opaque_value);
  ParsedFeaturePolicyDeclaration(const ParsedFeaturePolicyDeclaration& rhs);
  ParsedFeaturePolicyDeclaration& operator=(
      const ParsedFeaturePolicyDeclaration& rhs);
  ~ParsedFeaturePolicyDeclaration();

  mojom::FeaturePolicyFeature feature;
  // This flag is set true for a declared policy on an <iframe sandbox>
  // container, for a feature which is supposed to be allowed in the sandboxed
  // document. Usually, the 'src' keyword in a declaration will cause the origin
  // of the iframe to be present in |origins|, but for sandboxed iframes, this
  // flag is set instead.

  // This map records all the allowed origins and their corresponding values.
  std::map<url::Origin, PolicyValue> values;
  // Fallback value is used when feature is enabled for all or disabled for all.
  PolicyValue fallback_value;
  PolicyValue opaque_value;
};

using ParsedFeaturePolicy = std::vector<ParsedFeaturePolicyDeclaration>;

bool BLINK_COMMON_EXPORT operator==(const ParsedFeaturePolicyDeclaration& lhs,
                                    const ParsedFeaturePolicyDeclaration& rhs);

class BLINK_COMMON_EXPORT FeaturePolicy {
 public:
  // TODO(iclelland): Generate, instead of this map, a set of bool flags, one
  // for each feature, as all features are supposed to be represented here.
  using FeatureState = std::map<mojom::FeaturePolicyFeature, PolicyValue>;

  // Represents a collection of origins which make up an allowlist in a feature
  // policy. This collection may be set to match every origin (corresponding to
  // the "*" syntax in the policy string, in which case the Contains() method
  // will always return true.
  class BLINK_COMMON_EXPORT Allowlist final {
   public:
    Allowlist(mojom::PolicyValueType type);
    Allowlist(const Allowlist& rhs);
    ~Allowlist();

    // Adds a single origin to the allowlist.
    void Add(const url::Origin& origin, const PolicyValue& value);

    // Returns the value of the given origin if specified, fallback value
    // otherwise.
    // fallback value should be set to maximum unless it is set to 'none'.
    PolicyValue GetValueForOrigin(const url::Origin& origin) const;

    // Returns the fallback value.
    const PolicyValue& GetFallbackValue() const;

    // Sets the fallback value.
    void SetFallbackValue(const PolicyValue& fallback_value);

    // Returns the opaque value.
    const PolicyValue& GetOpaqueValue() const;

    // Sets the opaque value.
    void SetOpaqueValue(const PolicyValue& opaque_value);

    // Returns set of origins in the allowlist.
    const base::flat_set<url::Origin>& Origins() const;

    // Returns set of <origin, value> pair in the allowlist.
    const base::flat_map<url::Origin, PolicyValue>& Values() const;

   private:
    base::flat_map<url::Origin, PolicyValue> values_;
    PolicyValue fallback_value_;
    PolicyValue opaque_value_;
  };

  // The FeaturePolicy::FeatureDefault enum defines the default enable state for
  // a feature when neither it nor any parent frame have declared an explicit
  // policy. The three possibilities map directly to Feature Policy Allowlist
  // semantics.
  //
  // The default values for each feature are set in GetDefaultFeatureList.
  enum class FeatureDefault {
    // Equivalent to []. If this default policy is in effect for a frame, then
    // the feature will not be enabled for that frame or any of its children.
    DisableForAll,

    // Equivalent to ["self"]. If this default policy is in effect for a frame,
    // then the feature will be enabled for that frame, and any same-origin
    // child frames, but not for any cross-origin child frames.
    EnableForSelf,

    // Equivalent to ["*"]. If in effect for a frame, then the feature is
    // enabled for that frame and all of its children.
    EnableForAll
  };

  using FeatureDefaultValue =
      std::pair<FeaturePolicy::FeatureDefault, mojom::PolicyValueType>;
  using FeatureList =
      std::map<mojom::FeaturePolicyFeature, FeaturePolicy::FeatureDefaultValue>;

  ~FeaturePolicy();

  static std::unique_ptr<FeaturePolicy> CreateFromParentPolicy(
      const FeaturePolicy* parent_policy,
      const ParsedFeaturePolicy& container_policy,
      const url::Origin& origin);

  static std::unique_ptr<FeaturePolicy> CreateWithOpenerPolicy(
      const FeatureState& inherited_policies,
      const url::Origin& origin);

  bool IsFeatureEnabled(mojom::FeaturePolicyFeature feature) const;

  bool IsFeatureEnabled(mojom::FeaturePolicyFeature feature,
                        const PolicyValue& threshold_value) const;

  // Returns whether or not the given feature is enabled by this policy for a
  // specific origin.
  bool IsFeatureEnabledForOrigin(mojom::FeaturePolicyFeature feature,
                                 const url::Origin& origin) const;
  bool IsFeatureEnabledForOrigin(mojom::FeaturePolicyFeature feature,
                                 const url::Origin& origin,
                                 const PolicyValue& threshold_value) const;

  // Returns the value of the given feature on the given origin.
  PolicyValue GetFeatureValueForOrigin(mojom::FeaturePolicyFeature feature,
                                       const url::Origin& origin) const;

  // Returns the allowlist of a given feature by this policy.
  const Allowlist GetAllowlistForFeature(
      mojom::FeaturePolicyFeature feature) const;

  // Sets the declared policy from the parsed Feature-Policy HTTP header.
  // Unrecognized features will be ignored.
  void SetHeaderPolicy(const ParsedFeaturePolicy& parsed_header);

  // Returns the current state of feature policies for |origin_|. This includes
  // the |inherited_policies_| as well as the header policies.
  FeatureState GetFeatureState() const;

  const url::Origin& GetOriginForTest() { return origin_; }

  // Returns the list of features which can be controlled by Feature Policy.
  const FeatureList& GetFeatureList() const;
  static const FeatureList& GetDefaultFeatureList();

  static mojom::FeaturePolicyFeature FeatureForSandboxFlag(
      WebSandboxFlags flag);

 private:
  friend class FeaturePolicyTest;

  FeaturePolicy(url::Origin origin, const FeatureList& feature_list);
  static std::unique_ptr<FeaturePolicy> CreateFromParentPolicy(
      const FeaturePolicy* parent_policy,
      const ParsedFeaturePolicy& container_policy,
      const url::Origin& origin,
      const FeatureList& features);

  // Updates the inherited policy with the declarations from the iframe allow*
  // attributes.
  void AddContainerPolicy(const ParsedFeaturePolicy& container_policy,
                          const FeaturePolicy* parent_policy);

  // The origin of the document with which this policy is associated.
  url::Origin origin_;

  // Map of feature names to declared allowlists. Any feature which is missing
  // from this map should use the inherited policy.
  std::map<mojom::FeaturePolicyFeature, std::unique_ptr<Allowlist>> allowlists_;

  // Records whether or not each feature was enabled for this frame by its
  // parent frame.
  FeatureState inherited_policies_;

  const FeatureList& feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FeaturePolicy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_FEATURE_POLICY_H_
