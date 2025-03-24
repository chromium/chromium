// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_H_

#include <map>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features_bitset.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "url/origin.h"

class GURL;

namespace network {

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
// network::mojom::PermissionsPolicyFeature, declared in
// |permissions_policy.mojom|.
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

class COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM) PermissionsPolicy {
 public:
  // Represents a collection of origins which make up an allowlist in a
  // permissions policy. This collection may be set to match every origin
  // (corresponding to the "*" syntax in the policy string, in which case the
  // Contains() method will always return true.
  class COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM) Allowlist final {
   public:
    Allowlist();
    Allowlist(const Allowlist& rhs);
    ~Allowlist();

    Allowlist(Allowlist&&) noexcept;
    Allowlist& operator=(Allowlist&&) noexcept;
    Allowlist& operator=(const Allowlist& other) = default;

    friend bool operator==(const Allowlist&, const Allowlist&) = default;

    // Extracts an Allowlist from a network::ParsedPermissionsPolicyDeclaration.
    static Allowlist FromDeclaration(
        const network::ParsedPermissionsPolicyDeclaration& parsed_declaration);

    // Adds a single origin with possible wildcards to the allowlist.
    void Add(const network::OriginWithPossibleWildcards& origin);

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

    const std::vector<network::OriginWithPossibleWildcards>& AllowedOrigins()
        const {
      return allowed_origins_;
    }

    // Overwrite allowed_origins_ for Isolated Apps that have a more restrictive
    // Permissions-Policy HTTP header than the permissions policy declared in
    // its Web App Manifest.
    void SetAllowedOrigins(
        const std::vector<network::OriginWithPossibleWildcards>&
            allowed_origins) {
      allowed_origins_ = allowed_origins;
    }

   private:
    std::vector<network::OriginWithPossibleWildcards> allowed_origins_;
    std::optional<url::Origin> self_if_matches_;
    bool matches_all_origins_{false};
    bool matches_opaque_src_{false};
  };

  // Only used by mojo traits.
  explicit PermissionsPolicy(mojo::DefaultConstruct::Tag);

  PermissionsPolicy(const PermissionsPolicy&);
  PermissionsPolicy& operator=(const PermissionsPolicy&);
  ~PermissionsPolicy();

  PermissionsPolicy(PermissionsPolicy&&) noexcept;
  PermissionsPolicy& operator=(PermissionsPolicy&&) noexcept;

  friend bool operator==(const PermissionsPolicy&,
                         const PermissionsPolicy&) = default;

  static std::unique_ptr<PermissionsPolicy> CreateFromParentPolicy(
      const PermissionsPolicy* parent_policy,
      const network::ParsedPermissionsPolicy& header_policy,
      const network::ParsedPermissionsPolicy& container_policy,
      const url::Origin& origin,
      bool headerless = false);

  // This copies everything except `reporting_endpoints_`. Returns `nullptr` if
  // the input is `nullptr`.
  static std::unique_ptr<PermissionsPolicy> CopyStateFrom(
      const PermissionsPolicy*);

  // Creates a flexible PermissionsPolicy for a fenced frame. Inheritance is
  // allowed, but only a specific list of permissions are allowed to be enabled.
  static std::unique_ptr<PermissionsPolicy> CreateFlexibleForFencedFrame(
      const PermissionsPolicy* parent_policy,
      const network::ParsedPermissionsPolicy& header_policy,
      const network::ParsedPermissionsPolicy& container_policy,
      const url::Origin& subframe_origin);

  // Creates a fixed PermissionsPolicy for a fenced frame. Only the permissions
  // specified in the fenced frame config's effective enabled permissions are
  // enabled. Permissions do not inherit from the parent to prevent
  // cross-channel communication.
  static std::unique_ptr<PermissionsPolicy> CreateFixedForFencedFrame(
      const url::Origin& origin,
      const network::ParsedPermissionsPolicy& header_policy,
      base::span<const network::mojom::PermissionsPolicyFeature>
          effective_enabled_permissions);

  // Creates a PermissionsPolicy from a parsed policy. If `base_policy` is
  // supplied, it will be used first and the `parsed_policy` may further
  // restrict the policy.
  static std::unique_ptr<PermissionsPolicy> CreateFromParsedPolicy(
      const network::ParsedPermissionsPolicy& parsed_policy,
      const std::optional<network::ParsedPermissionsPolicy>& base_policy,
      const url::Origin& origin);

  // Returns the inherited policy of the given feature.
  static bool InheritedValueForFeature(
      const url::Origin& origin,
      const PermissionsPolicy* parent_policy,
      std::pair<network::mojom::PermissionsPolicyFeature,
                network::PermissionsPolicyFeatureDefault> feature,
      const network::ParsedPermissionsPolicy& container_policy);

  // Various URLs that cannot supply Permissions-Policy headers are treated
  // specially. See
  // https://github.com/fergald/docs/blob/master/explainers/permissions-policy-deprecate-unload.md
  static bool IsHeaderlessUrl(const GURL& url);

  bool IsFeatureEnabled(network::mojom::PermissionsPolicyFeature feature) const;

  // Returns whether or not the given feature is enabled by this policy for a
  // specific origin. If `override_default_policy_to_all` is true (rare), then
  // replace the default policy with '*' in the query. This should only be
  // used by a handful of features that access x-origin data that said origin
  // has expressly opted into. In this scenario, the caller could have just as
  // easily created an x-origin iframe with allow=feature, and since the
  // recipient origin has opted in, they would have called the feature in said
  // iframe. If true, the feature must be defined in `kDefinedOptInFeatures`.
  bool IsFeatureEnabledForOrigin(
      network::mojom::PermissionsPolicyFeature feature,
      const url::Origin& origin,
      bool override_default_policy_to_all = false) const;

  const Allowlist GetAllowlistForDevTools(
      network::mojom::PermissionsPolicyFeature feature) const;

  // Returns the allowlist of a given feature by this policy.
  // TODO(crbug.com/40094174): Use |PermissionsPolicy::GetAllowlistForDevTools|
  // to replace this method. This method uses legacy |default_allowlist|
  // calculation method.
  const Allowlist GetAllowlistForFeature(
      network::mojom::PermissionsPolicyFeature feature) const;

  // Returns the allowlist of a given feature if it already exists. Doesn't
  // build a default allow list based on the policy if not.
  std::optional<const Allowlist> GetAllowlistForFeatureIfExists(
      network::mojom::PermissionsPolicyFeature feature) const;

  // Returns empty string when there is no reporting endpoint.
  std::string GetEndpointForFeature(
      network::mojom::PermissionsPolicyFeature feature) const;

  // Returns a new permissions policy, based on this policy and a client hint
  // header policy set via the accept-ch meta tag. It will fail if header
  // policies not for client hints are included in `parsed_header`.
  // TODO(https://crbug.com/40208054): Replace w/ generic HTML policy
  // modification.
  std::unique_ptr<PermissionsPolicy> WithClientHints(
      const network::ParsedPermissionsPolicy& parsed_header) const;

  const url::Origin& GetOriginForTest() const { return origin_; }
  const std::map<network::mojom::PermissionsPolicyFeature, Allowlist>&
  allowlists() const {
    return allowlists_;
  }

  // Returns the list of features which can be controlled by Permissions Policy.
  const network::PermissionsPolicyFeatureList& GetFeatureList() const;

  bool IsFeatureEnabledByInheritedPolicy(
      network::mojom::PermissionsPolicyFeature feature) const;

 private:
  friend class PermissionsPolicyTest;
  friend struct mojo::StructTraits<network::mojom::PermissionsPolicyDataView,
                                   network::PermissionsPolicy>;

  struct AllowlistsAndReportingEndpoints {
    std::map<network::mojom::PermissionsPolicyFeature, Allowlist> allowlists_;
    std::map<network::mojom::PermissionsPolicyFeature, std::string>
        reporting_endpoints_;

    AllowlistsAndReportingEndpoints();
    ~AllowlistsAndReportingEndpoints();
    AllowlistsAndReportingEndpoints(
        const AllowlistsAndReportingEndpoints& other);
    AllowlistsAndReportingEndpoints(AllowlistsAndReportingEndpoints&& other);
    AllowlistsAndReportingEndpoints(
        std::map<network::mojom::PermissionsPolicyFeature, Allowlist>
            allowlists,
        std::map<network::mojom::PermissionsPolicyFeature, std::string>
            reporting_endpoints);
  };

  // Creates the allowlists and and reporting endpoints from the parsed
  // Permissions-Policy HTTP header. Unrecognized features will be ignored.
  static AllowlistsAndReportingEndpoints CreateAllowlistsAndReportingEndpoints(
      const network::ParsedPermissionsPolicy& parsed_header);

  // Merges |base_policy| with |second_policy|. For each feature, if
  // |base_policy| allows all domains then the |second_policy| overrides it if
  // it specifies an allowlist. If both policies specify an allowlist then the
  // combined allowlist will be the intersection of the two.
  //
  // This is used e.g. for merging the policy in an isolated app manifest with
  // a policy received in an HTTP header.
  static AllowlistsAndReportingEndpoints CombinePolicies(
      const network::ParsedPermissionsPolicy& base_policy,
      const network::ParsedPermissionsPolicy& second_policy);

  PermissionsPolicy(
      url::Origin origin,
      AllowlistsAndReportingEndpoints allow_lists_and_reporting_endpoints,
      network::PermissionsPolicyFeaturesBitset inherited_policies,
      const network::PermissionsPolicyFeatureList& feature_list,
      bool headerless = false);
  static std::unique_ptr<PermissionsPolicy> CreateFromParentPolicy(
      const PermissionsPolicy* parent_policy,
      const network::ParsedPermissionsPolicy& header_policy,
      const network::ParsedPermissionsPolicy& container_policy,
      const url::Origin& origin,
      const network::PermissionsPolicyFeatureList& features,
      bool headerless = false);

  static std::unique_ptr<PermissionsPolicy> CreateFromParsedPolicy(
      const network::ParsedPermissionsPolicy& parsed_policy,
      const std::optional<network::ParsedPermissionsPolicy>&
          parsed_policy_for_isolated_app,
      const url::Origin& origin,
      const network::PermissionsPolicyFeatureList& features);

  static std::unique_ptr<PermissionsPolicy> CreateFlexibleForFencedFrame(
      const PermissionsPolicy* parent_policy,
      const network::ParsedPermissionsPolicy& header_policy,
      const network::ParsedPermissionsPolicy& container_policy,
      const url::Origin& subframe_origin,
      const network::PermissionsPolicyFeatureList& features);

  static std::unique_ptr<PermissionsPolicy> CreateFixedForFencedFrame(
      const url::Origin& origin,
      const network::ParsedPermissionsPolicy& header_policy,
      const network::PermissionsPolicyFeatureList& features,
      base::span<const network::mojom::PermissionsPolicyFeature>
          effective_enabled_permissions);

  // If the feature is in the declared policy, returns whether the given origin
  // exists in its declared allowlist; otherwise, returns the value from
  // inherited policy.
  bool GetFeatureValueForOrigin(
      network::mojom::PermissionsPolicyFeature feature,
      const url::Origin& origin) const;

  // The origin of the document with which this policy is associated.
  url::Origin origin_;

  // If `true` this is a document that cannot have a Permissions-Policy header,
  // e.g. a srcdoc. Docs like this need special treatment for default-off
  // features.
  bool headerless_;

  // Map of feature names to declared allowlists. Any feature which is missing
  // from this map should use the inherited policy.
  std::map<network::mojom::PermissionsPolicyFeature, Allowlist> allowlists_;

  // Map of feature names to reporting endpoints. Any feature which is missing
  // from this map should report to the default endpoint, if it is set.
  std::map<network::mojom::PermissionsPolicyFeature, std::string>
      reporting_endpoints_;

  // Records whether or not each feature was enabled for this frame by its
  // parent frame.
  network::PermissionsPolicyFeaturesBitset inherited_policies_;

  // The map of features to their default enable state.
  raw_ref<const network::PermissionsPolicyFeatureList> feature_list_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_H_
