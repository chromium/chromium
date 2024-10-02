// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_H_

#include <map>
#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "url/origin.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace blink {

class ResourceRequest;

// Permissions Policy is a mechanism for controlling the availability of web
// platform features in a frame, including all embedded frames. It can be used
// to remove features, automatically refuse API permission requests, or modify
// the behaviour of features. (The specific changes which are made depend on the
// feature; see the specification for details).
//
// Policies can be defined in the HTTP header stream, with the
// |Permissions-Policy| HTTP header, or can be set by the |allow| attributes on
// the iframe element which embeds the document.
//
// See https://w3c.github.io/webappsec-permissions-policy/
//
// Key concepts:
//
// Features
// --------
// Features which can be controlled by policy are defined by instances of enum
// mojom::PermissionsPolicyFeature, declared in |permissions_policy.mojom|.
//
// Allowlists
// ----------
// Allowlists are collections of origins, although several special terms can be
// used when declaring them:
//   "none" indicates that no origins should be included in the allowlist.
//   "self" refers to the origin of the frame which is declaring the policy.
//   "src" refers to the origin specified by the attributes of the iframe
//   element which embeds the document. This incorporates the src, srcdoc, and
//   sandbox attributes.
//   "*" refers to all origins; any origin will match an allowlist which
//   contains it.
//
// Declarations
// ------------
// A permissions policy declaration is a mapping of a feature name to an
// allowlist. A set of declarations is a declared policy.
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
// feature is available when no policy has been declared, and determines how the
// feature is inherited across origin boundaries.
//
// If the default policy is in effect for a frame, then it controls how the
// feature is inherited by any cross-origin iframes embedded by the frame. (See
// the comments in |PermissionsPolicyFeatureDefault| in
// permissions_policy_features.h for specifics.)
//
// Policy Inheritance
// ------------------
// Policies in effect for a frame are inherited by any child frames it embeds.
// Unless another policy is declared in the child, all same-origin children will
// receive the same set of enabled features as the parent frame. Whether or not
// features are inherited by cross-origin iframes without an explicit policy is
// determined by the feature's default policy. (Again, see the comments in
// |PermissionsPolicyFeatureDefault| in permissions_policy_features.h for
// details)

class BLINK_COMMON_EXPORT PermissionsPolicy {
 public:
  // Represents a collection of origins which make up an allowlist in a
  // permissions policy. This collection may be set to match every origin
  // (corresponding to the "*" syntax in the policy string, in which case the
  // Contains() method will always return true.
  class BLINK_COMMON_EXPORT Allowlist final {
   public:
    Allowlist();
    Allowlist(const Allowlist& rhs);
    ~Allowlist();

    // Extracts an Allowlist from a ParsedPermissionsPolicyDeclaration.
    static Allowlist FromDeclaration(
        const ParsedPermissionsPolicyDeclaration& parsed_declaration);

    // Adds a single origin with possible wildcards to the allowlist.
    void Add(const blink::OriginWithPossibleWildcards& origin);

    // Add an origin representing self to the allowlist.
    void AddSelf(std::optional<url::Origin> self);

    // Adds all origins to the allowlist.
    void AddAll();

    // Sets the allowlist to match the opaque origin implied by the 'src'
    // keyword.
    void AddOpaqueSrc();

    // Returns true if the given origin has been added to the allowlist.
    bool Contains(const url::Origin& origin) const;

    // Returns the origin for self if included in the allowlist.
    const std::optional<url::Origin>& SelfIfMatches() const;

    // Returns true if the allowlist matches all origins.
    bool MatchesAll() const;

    // Sets matches_all_origins_ to false for Isolated Apps that have a more
    // restrictive Permissions-Policy HTTP header than the permissions policy
    // declared in its Web App Manifest.
    void RemoveMatchesAll();

    // Returns true if the allowlist should match the opaque origin implied by
    // the 'src' keyword.
    bool MatchesOpaqueSrc() const;

    const std::vector<OriginWithPossibleWildcards>& AllowedOrigins() const {
      return allowed_origins_;
    }

    // Overwrite allowed_origins_ for Isolated Apps that have a more restrictive
    // Permissions-Policy HTTP header than the permissions policy declared in
    // its Web App Manifest.
    void SetAllowedOrigins(
        const std::vector<OriginWithPossibleWildcards>& allowed_origins) {
      allowed_origins_ = allowed_origins;
    }

   private:
    std::vector<OriginWithPossibleWildcards> allowed_origins_;
    std::optional<url::Origin> self_if_matches_;
    bool matches_all_origins_{false};
    bool matches_opaque_src_{false};
  };

  PermissionsPolicy(const PermissionsPolicy&) = delete;
  PermissionsPolicy& operator=(const PermissionsPolicy&) = delete;
  ~PermissionsPolicy();

  static std::unique_ptr<PermissionsPolicy> CreateFromParentPolicy(
      const PermissionsPolicy* parent_policy,
      const ParsedPermissionsPolicy& header_policy,
      const ParsedPermissionsPolicy& container_policy,
      const url::Origin& origin);

  static std::unique_ptr<PermissionsPolicy> CopyStateFrom(
      const PermissionsPolicy*);

  // Creates a flexible PermissionsPolicy for a fenced frame. Inheritance is
  // allowed, but only a specific list of permissions are allowed to be enabled.
  static std::unique_ptr<PermissionsPolicy> CreateFlexibleForFencedFrame(
      const PermissionsPolicy* parent_policy,
      const ParsedPermissionsPolicy& header_policy,
      const ParsedPermissionsPolicy& container_policy,
      const url::Origin& subframe_origin);

  // Creates a fixed PermissionsPolicy for a fenced frame. Only the permissions
  // specified in the fenced frame config's effective enabled permissions are
  // enabled. Permissions do not inherit from the parent to prevent
  // cross-channel communication.
  static std::unique_ptr<PermissionsPolicy> CreateFixedForFencedFrame(
      const url::Origin& origin,
      const ParsedPermissionsPolicy& header_policy,
      base::span<const blink::mojom::PermissionsPolicyFeature>
          effective_enabled_permissions);

  // Creates a PermissionsPolicy from a parsed policy. If `base_policy` is
  // supplied, it will be used first and the `parsed_policy` may further
  // restrict the policy.
  static std::unique_ptr<PermissionsPolicy> CreateFromParsedPolicy(
      const ParsedPermissionsPolicy& parsed_policy,
      const std::optional<ParsedPermissionsPolicy>& base_policy,
      const url::Origin& origin);

  bool IsFeatureEnabled(mojom::PermissionsPolicyFeature feature) const;

  // Returns whether or not the given feature is enabled by this policy for a
  // specific origin.
  bool IsFeatureEnabledForOrigin(mojom::PermissionsPolicyFeature feature,
                                 const url::Origin& origin) const;

  // Returns whether or not the given feature is enabled by this policy for a
  // subresource request, given the ongoing request/redirect origin.
  bool IsFeatureEnabledForSubresourceRequest(
      mojom::PermissionsPolicyFeature feature,
      const url::Origin& origin,
      const network::ResourceRequest& request) const;

  const Allowlist GetAllowlistForDevTools(
      mojom::PermissionsPolicyFeature feature) const;

  // Returns the allowlist of a given feature by this policy.
  // TODO(crbug.com/937131): Use |PermissionsPolicy::GetAllowlistForDevTools|
  // to replace this method. This method uses legacy |default_allowlist|
  // calculation method.
  const Allowlist GetAllowlistForFeature(
      mojom::PermissionsPolicyFeature feature) const;

  // Returns the allowlist of a given feature if it already exists. Doesn't
  // build a default allow list based on the policy if not.
  std::optional<const Allowlist> GetAllowlistForFeatureIfExists(
      mojom::PermissionsPolicyFeature feature) const;

  std::optional<std::string> GetEndpointForFeature(
      mojom::PermissionsPolicyFeature feature) const;

  // Returns a new permissions policy, based on this policy and a client hint
  // header policy set via the accept-ch meta tag. It will fail if header
  // policies not for client hints are included in `parsed_header`.
  // TODO(https://crbug.com/40208054): Replace w/ generic HTML policy
  // modification.
  std::unique_ptr<PermissionsPolicy> WithClientHints(
      const ParsedPermissionsPolicy& parsed_header) const;

  const url::Origin& GetOriginForTest() const { return origin_; }
  const std::map<mojom::PermissionsPolicyFeature, Allowlist>& allowlists()
      const {
    return allowlists_;
  }

  // Returns the list of features which can be controlled by Permissions Policy.
  const PermissionsPolicyFeatureList& GetFeatureList() const;

  bool IsFeatureEnabledByInheritedPolicy(
      mojom::PermissionsPolicyFeature feature) const;

 private:
  friend class ResourceRequest;
  friend class PermissionsPolicyTest;

  // List of features that have an explicit opt-in mechanism.
  static const mojom::PermissionsPolicyFeature defined_opt_in_features_[];

  struct AllowlistsAndReportingEndpoints {
    std::map<mojom::PermissionsPolicyFeature, Allowlist> allowlists_;
    std::map<mojom::PermissionsPolicyFeature, std::string> reporting_endpoints_;
  };

  // Creates the allowlists and and reporting endpoints from the parsed
  // Permissions-Policy HTTP header. Unrecognized features will be ignored.
  static AllowlistsAndReportingEndpoints CreateAllowlistsAndReportingEndpoints(
      const ParsedPermissionsPolicy& parsed_header);

  // Merges |base_policy| with |second_policy|. For each feature, if
  // |base_policy| allows all domains then the |second_policy| overrides it if
  // it specifies an allowlist. If both policies specify an allowlist then the
  // combined allowlist will be the intersection of the two.
  //
  // This is used e.g. for merging the policy in an isolated app manifest with
  // a policy received in an HTTP header.
  static AllowlistsAndReportingEndpoints CombinePolicies(
      const ParsedPermissionsPolicy& base_policy,
      const ParsedPermissionsPolicy& second_policy);

  PermissionsPolicy(
      url::Origin origin,
      AllowlistsAndReportingEndpoints allow_lists_and_reporting_endpoints,
      PermissionsPolicyFeatureState inherited_policies,
      const PermissionsPolicyFeatureList& feature_list);
  static std::unique_ptr<PermissionsPolicy> CreateFromParentPolicy(
      const PermissionsPolicy* parent_policy,
      const ParsedPermissionsPolicy& header_policy,
      const ParsedPermissionsPolicy& container_policy,
      const url::Origin& origin,
      const PermissionsPolicyFeatureList& features);

  static std::unique_ptr<PermissionsPolicy> CreateFromParsedPolicy(
      const ParsedPermissionsPolicy& parsed_policy,
      const std::optional<ParsedPermissionsPolicy>&
          parsed_policy_for_isolated_app,
      const url::Origin& origin,
      const PermissionsPolicyFeatureList& features);

  static std::unique_ptr<PermissionsPolicy> CreateFlexibleForFencedFrame(
      const PermissionsPolicy* parent_policy,
      const ParsedPermissionsPolicy& header_policy,
      const ParsedPermissionsPolicy& container_policy,
      const url::Origin& subframe_origin,
      const PermissionsPolicyFeatureList& features);

  static std::unique_ptr<PermissionsPolicy> CreateFixedForFencedFrame(
      const url::Origin& origin,
      const ParsedPermissionsPolicy& header_policy,
      const PermissionsPolicyFeatureList& features,
      base::span<const blink::mojom::PermissionsPolicyFeature>
          effective_enabled_permissions);

  // Returns whether or not the given feature is enabled by this policy for a
  // specific origin given a set of opt-in features. The opt-in features cannot
  // override an explicit policy but can override the default policy.
  bool IsFeatureEnabledForOriginImpl(
      mojom::PermissionsPolicyFeature feature,
      const url::Origin& origin,
      const std::set<mojom::PermissionsPolicyFeature>& opt_in_features) const;

  // Returns whether or not the given feature is enabled by this policy for a
  // specific origin, given that the feature is an opt-in feature, and the
  // subresource request for which we are querying has opted-into this feature.
  bool IsFeatureEnabledForSubresourceRequestAssumingOptIn(
      mojom::PermissionsPolicyFeature feature,
      const url::Origin& origin) const;

  // Returns the inherited policy of the given feature.
  static bool InheritedValueForFeature(
      const url::Origin& origin,
      const PermissionsPolicy* parent_policy,
      std::pair<mojom::PermissionsPolicyFeature,
                PermissionsPolicyFeatureDefault> feature,
      const ParsedPermissionsPolicy& container_policy);

  // If the feature is in the declared policy, returns whether the given origin
  // exists in its declared allowlist; otherwise, returns the value from
  // inherited policy.
  bool GetFeatureValueForOrigin(mojom::PermissionsPolicyFeature feature,
                                const url::Origin& origin) const;

  // The origin of the document with which this policy is associated.
  const url::Origin origin_;

  // Map of feature names to declared allowlists. Any feature which is missing
  // from this map should use the inherited policy.
  const std::map<mojom::PermissionsPolicyFeature, Allowlist> allowlists_;

  // Map of feature names to reporting endpoints. Any feature which is missing
  // from this map should report to the default endpoint, if it is set.
  const std::map<mojom::PermissionsPolicyFeature, std::string>
      reporting_endpoints_;

  // Records whether or not each feature was enabled for this frame by its
  // parent frame.
  const PermissionsPolicyFeatureState inherited_policies_;

  // The map of features to their default enable state.
  const raw_ref<const PermissionsPolicyFeatureList> feature_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_H_
