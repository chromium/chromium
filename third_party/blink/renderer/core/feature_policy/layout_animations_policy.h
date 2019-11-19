// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_LAYOUT_ANIMATIONS_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_LAYOUT_ANIMATIONS_POLICY_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class CSSProperty;
class SecurityContext;

// Helper methods for for 'layout-animations' (kLayoutAnimations) feature
// policy.
class LayoutAnimationsPolicy {
  DISALLOW_NEW();

 public:
  // Returns a set of the CSS properties which are affected by the feature
  // policy 'layout-animations'.
  static const HashSet<const CSSProperty*>& AffectedCSSProperties();

  // Generates a violation report for the blocked |animation_property| only if
  // the feature 'layout-animations' is disabled in |security_context|. Invoking
  // this method emits a potential violation of the 'layout-animations' policy
  // which is tracked by Blink.UserCounters.FeaturePolicy.PotentialViolation.
  static void ReportViolation(const CSSProperty& animated_property,
                              const SecurityContext& security_context);

 private:
  LayoutAnimationsPolicy();

  DISALLOW_COPY_AND_ASSIGN(LayoutAnimationsPolicy);
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_LAYOUT_ANIMATIONS_POLICY_H_
