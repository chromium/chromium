// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_PARSER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_helper.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Document;
class ExecutionContext;
class FeaturePolicyParserDelegate;

// These values match the "FeaturePolicyAllowlistType" enum in
// tools/metrics/histograms/enums.xml. Entries should not be renumbered and
// numeric values should never be reused.
enum class FeaturePolicyAllowlistType {
  kEmpty = 0,
  kNone = 1,
  kSelf = 2,
  kSrc = 3,
  kStar = 4,
  kOrigins = 5,
  kKeywordsOnly = 6,
  kMixed = 7,
  kMinValue = 0,
  kMaxValue = kMixed
};

// Returns the list of features which are currently available in this context,
// including any features which have been made available by an origin trial.
CORE_EXPORT const Vector<String> GetAvailableFeatures(ExecutionContext*);

// FeaturePolicyParser is a collection of methods which are used to convert
// Feature Policy declarations, in headers and iframe attributes, into
// ParsedFeaturePolicy structs. This class encapsulates all of the logic for
// parsing feature names, origin lists, and threshold values.
// Note that code outside of /renderer/ should not be parsing policy directives
// from strings, but if necessary, should be constructing ParsedFeaturePolicy
// structs directly.
class CORE_EXPORT FeaturePolicyParser {
  STATIC_ONLY(FeaturePolicyParser);

 public:
  // Converts a header policy string into a vector of allowlists, one for each
  // feature specified. Unrecognized features are filtered out. If |messages| is
  // not null, then any message in the input will cause a warning message to be
  // appended to it. The optional ExecutionContext is used to determine if any
  // origin trials affect the parsing.
  // Example of a feature policy string:
  //     "vibrate a.com b.com; fullscreen 'none'; payment 'self', payment *".
  static ParsedFeaturePolicy ParseHeader(
      const String& policy,
      scoped_refptr<const SecurityOrigin>,
      Vector<String>* messages,
      FeaturePolicyParserDelegate* delegate = nullptr);

  // Converts a container policy string into a vector of allowlists, given self
  // and src origins provided, one for each feature specified. Unrecognized
  // features are filtered out. If |messages| is not null, then any message in
  // the input will cause as warning message to be appended to it. Example of a
  // feature policy string:
  //     "vibrate a.com 'src'; fullscreen 'none'; payment 'self', payment *".
  static ParsedFeaturePolicy ParseAttribute(
      const String& policy,
      scoped_refptr<const SecurityOrigin> self_origin,
      scoped_refptr<const SecurityOrigin> src_origin,
      Vector<String>* messages,
      Document* document = nullptr);

  // Converts a feature policy string into a vector of allowlists (see comments
  // above), with an explicit FeatureNameMap. This algorithm is called by both
  // header policy parsing and container policy parsing. |self_origin|,
  // |src_origin|, and |execution_context| are nullable. The optional
  // ExecutionContext is used to determine if any origin trials affect the
  // parsing.
  static ParsedFeaturePolicy Parse(
      const String& policy,
      scoped_refptr<const SecurityOrigin> self_origin,
      scoped_refptr<const SecurityOrigin> src_origin,
      Vector<String>* messages,
      const FeatureNameMap& feature_names,
      FeaturePolicyParserDelegate* delegate = nullptr);

  // Used for LLVM fuzzer test
  static void ParseValueForFuzzer(mojom::PolicyValueType, const String&);

 private:
  static PolicyValue GetFallbackValueForFeature(
      mojom::FeaturePolicyFeature feature);
  static PolicyValue ParseValueForType(mojom::PolicyValueType feature_type,
                                       const String& value_string,
                                       bool* ok);
};
// Returns true iff any declaration in the policy is for the given feature.
CORE_EXPORT bool IsFeatureDeclared(mojom::FeaturePolicyFeature,
                                   const ParsedFeaturePolicy&);

// Removes any declaration in the policy for the given feature. Returns true if
// the policy was modified.
CORE_EXPORT bool RemoveFeatureIfPresent(mojom::FeaturePolicyFeature,
                                        ParsedFeaturePolicy&);

// If no declaration in the policy exists already for the feature, adds a
// declaration which disallows the feature in all origins. Returns true if the
// policy was modified.
CORE_EXPORT bool DisallowFeatureIfNotPresent(mojom::FeaturePolicyFeature,
                                             ParsedFeaturePolicy&);

// If no declaration in the policy exists already for the feature, adds a
// declaration which allows the feature in all origins. Returns true if the
// policy was modified.
CORE_EXPORT bool AllowFeatureEverywhereIfNotPresent(mojom::FeaturePolicyFeature,
                                                    ParsedFeaturePolicy&);

// Replaces any existing declarations in the policy for the given feature with
// a declaration which disallows the feature in all origins.
CORE_EXPORT void DisallowFeature(mojom::FeaturePolicyFeature,
                                 ParsedFeaturePolicy&);

// Replaces any existing declarations in the policy for the given feature with
// a declaration which allows the feature in all origins.
CORE_EXPORT void AllowFeatureEverywhere(mojom::FeaturePolicyFeature,
                                        ParsedFeaturePolicy&);

CORE_EXPORT const String& GetNameForFeature(mojom::FeaturePolicyFeature);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_PARSER_H_
