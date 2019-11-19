// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_DOCUMENT_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_DOCUMENT_POLICY_H_

#include <limits>
#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/feature_policy/policy_value.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom.h"

namespace blink {

// Document Policy is a mechanism for controlling the behaviour of web platform
// features in a document, and for requesting such changes in embedded frames.
// (The specific changes which are made depend on the feature; see the
// specification for details).
//
// Policies can be defined in the HTTP header stream, with the |Document-Policy|
// HTTP header, or can be set by the |policy| attributes on the iframe element
// which embeds the document.
//
// See
// https://github.com/w3c/webappsec-feature-policy/blob/master/document-policy-explainer.md
//
// Key concepts:
//
// Features
// --------
// Features which can be controlled by policy are defined by instances of enum
// mojom::FeaturePolicyFeature, declared in |feature_policy_feature.mojom|.
// TODO(iclelland): Make a clear distinction between feature policy features
// and document policy features.
//
// Declarations
// ------------
// A document policy declaration is a mapping of a feature name to a policy
// value. A set of such declarations is a declared policy. The declared policy
// is attached to a document.
//
// Required Policy
// ----------------
// In addition to the declared policy (which may be empty), every frame has
// an required policy, which is set by the embedding document (or inherited
// from its parent). Any document loaded into a frame with a required policy
// must have a declared policy which is compatible with it. Frames may add new
// requirements to their own subframes, but cannot relax any existing ones.
//
// Advertised Policy
// -----------------
// If a frame has a non-empty required policy, the requirements will be
// advertised on the outgoing HTTP request for any document to be loaded in that
// frame, in the Sec-Required-Document-Policy HTTP header.
//
// Defaults
// --------
// Each defined feature has a default policy, which determines the threshold
// value to use when no policy has been declared.

struct BLINK_COMMON_EXPORT ParsedDocumentPolicyDeclaration {
  mojom::FeaturePolicyFeature feature;
  PolicyValue value;
};

using ParsedDocumentPolicy = std::vector<ParsedDocumentPolicyDeclaration>;

class BLINK_COMMON_EXPORT DocumentPolicy {
 public:
  using FeatureState = std::map<mojom::FeaturePolicyFeature, PolicyValue>;

  ~DocumentPolicy();

  static std::unique_ptr<DocumentPolicy> CreateWithRequiredPolicy(
      const FeatureState& required_policy);

  // Returns true if the feature is unrestricted (has its default value for the
  // platform)
  bool IsFeatureEnabled(mojom::FeaturePolicyFeature feature) const;

  // Returns true if the feature is unrestricted, or is not restricted as much
  // as the given threshold value.
  bool IsFeatureEnabled(mojom::FeaturePolicyFeature feature,
                        const PolicyValue& threshold_value) const;

  // Returns true if the feature is being migrated to document policy
  // TODO(iclelland): remove this method when those features are fully
  // migrated to document policy.
  bool IsFeatureSupported(mojom::FeaturePolicyFeature feature) const;

  // Returns the value of the given feature on the given origin.
  PolicyValue GetFeatureValue(mojom::FeaturePolicyFeature feature) const;

  // Sets the declared policy from the parsed Document-Policy HTTP header.
  // Unrecognized features will be ignored.
  void SetHeaderPolicy(const ParsedDocumentPolicy& parsed_header);

  // Returns the current threshold values assigned to all document policies.
  // the declared header policy as well as any unadvertised required policies
  // (such as sandbox policies).
  FeatureState GetFeatureState() const;

  // Returns the required policy to advertise for an outgoing HTTP request.
  ParsedDocumentPolicy RequiredPolicy() const;

  // Returns true if this document policy is compatible with the given required
  // policy.
  bool IsPolicyCompatible(const ParsedDocumentPolicy& required_policy);

  // Returns the list of features which can be controlled by Document Policy,
  // and their default values.
  static const FeatureState& GetFeatureDefaults();

 private:
  friend class DocumentPolicyTest;

  DocumentPolicy(const FeatureState& feature_list);
  static std::unique_ptr<DocumentPolicy> CreateWithRequiredPolicy(
      const FeatureState& required_policy,
      const FeatureState& defaults);

  void UpdateFeatureState(const FeatureState& feature_state);

  static FeatureState ParsedDocumentPolicyToFeatureState(
      const ParsedDocumentPolicy& policies);

  // Threshold values for each defined feature.
  // TODO(iclelland): Generate these members; pack booleans in bitfields if
  // possible.
  bool font_display_ = true;
  double unoptimized_lossless_images_ = std::numeric_limits<double>::infinity();

  DISALLOW_COPY_AND_ASSIGN(DocumentPolicy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_DOCUMENT_POLICY_H_
