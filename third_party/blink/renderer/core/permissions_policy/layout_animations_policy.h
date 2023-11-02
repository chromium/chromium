// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PERMISSIONS_POLICY_LAYOUT_ANIMATIONS_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PERMISSIONS_POLICY_LAYOUT_ANIMATIONS_POLICY_H_

#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class CSSProperty;
class ExecutionContext;

// Helper methods for for 'layout-animations' (kLayoutAnimations) permissions
// policy.
class LayoutAnimationsPolicy {
  DISALLOW_NEW();

 public:
  LayoutAnimationsPolicy(const LayoutAnimationsPolicy&) = delete;
  LayoutAnimationsPolicy& operator=(const LayoutAnimationsPolicy&) = delete;

  // Returns a set of the CSS properties which are affected by the permissions
  // policy 'layout-animations'.
  static const HashSet<const CSSProperty*>& AffectedCSSProperties();

  // Generates a violation report for the blocked |animation_property| only if
  // the feature 'layout-animations' is disabled in |document|. Invoking
  // this method emits a potential violation of the 'layout-animations' policy
  // which is tracked by
  // Blink.UserCounters.FeaturePolicy.PotentialViolation.
  static void ReportViolation(const CSSProperty& animated_property,
                              ExecutionContext& context);

 private:
  LayoutAnimationsPolicy();
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PERMISSIONS_POLICY_LAYOUT_ANIMATIONS_POLICY_H_
