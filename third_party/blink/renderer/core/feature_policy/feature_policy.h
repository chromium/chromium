// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <memory>

namespace blink {

class Document;

// Returns a map between feature name (string) and mojom::FeaturePolicyFeature
// (enum).
typedef HashMap<String, mojom::FeaturePolicyFeature> FeatureNameMap;
CORE_EXPORT const FeatureNameMap& GetDefaultFeatureNameMap();

// Converts a header policy string into a vector of allowlists, one for each
// feature specified. Unrecognized features are filtered out. If |messages|
// is not null, then any message in the input will cause a warning message to be
// appended to it.
// Example of a feature policy string:
//     "vibrate a.com b.com; fullscreen 'none'; payment 'self', payment *".
CORE_EXPORT ParsedFeaturePolicy
ParseFeaturePolicyHeader(const String& policy,
                         scoped_refptr<const SecurityOrigin>,
                         Vector<String>* messages);

// Converts a container policy string into a vector of allowlists, given self
// and src origins provided, one for each feature specified. Unrecognized
// features are filtered out. If |messages| is not null, then any message in the
// input will cause as warning message to be appended to it.
// Example of a feature policy string:
//     "vibrate a.com 'src'; fullscreen 'none'; payment 'self', payment *".
CORE_EXPORT ParsedFeaturePolicy
ParseFeaturePolicyAttribute(const String& policy,
                            scoped_refptr<const SecurityOrigin> self_origin,
                            scoped_refptr<const SecurityOrigin> src_origin,
                            Vector<String>* messages,
                            Document* document = nullptr);

// Converts a feature policy string into a vector of allowlists (see comments
// above), with an explicit FeatureNameMap. This algorithm is called by both
// header policy parsing and container policy parsing. |self_origin|,
// |src_origin|, and |document| are nullable.
CORE_EXPORT ParsedFeaturePolicy
ParseFeaturePolicy(const String& policy,
                   scoped_refptr<const SecurityOrigin> self_origin,
                   scoped_refptr<const SecurityOrigin> src_origin,
                   Vector<String>* messages,
                   const FeatureNameMap& feature_names,
                   Document* document = nullptr);

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

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_H_
