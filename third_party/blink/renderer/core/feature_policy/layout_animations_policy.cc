// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/layout_animations_policy.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {
namespace {
String GetViolationMessage(const CSSProperty& property) {
  return String::Format(
      "Feature policy violation: CSS property '%s' violates feature policy "
      "'%s' which is disabled in this document",
      property.GetPropertyNameString().Utf8().c_str(),
      GetNameForFeature(mojom::FeaturePolicyFeature::kLayoutAnimations)
          .Utf8()
          .c_str());
}
}  // namespace

LayoutAnimationsPolicy::LayoutAnimationsPolicy() = default;

// static
const HashSet<const CSSProperty*>&
LayoutAnimationsPolicy::AffectedCSSProperties() {
  DEFINE_STATIC_LOCAL(
      HashSet<const CSSProperty*>, properties,
      ({&GetCSSPropertyBottom(), &GetCSSPropertyHeight(), &GetCSSPropertyLeft(),
        &GetCSSPropertyRight(), &GetCSSPropertyTop(), &GetCSSPropertyWidth()}));
  return properties;
}

// static
void LayoutAnimationsPolicy::ReportViolation(
    const CSSProperty& animated_property,
    const SecurityContext& security_context) {
  DCHECK(AffectedCSSProperties().Contains(&animated_property));
  auto state = security_context.GetFeatureEnabledState(
      mojom::FeaturePolicyFeature::kLayoutAnimations);
  security_context.CountPotentialFeaturePolicyViolation(
      mojom::FeaturePolicyFeature::kLayoutAnimations);
  if (state == FeatureEnabledState::kEnabled)
    return;
  security_context.ReportFeaturePolicyViolation(
      mojom::FeaturePolicyFeature::kLayoutAnimations,
      state == FeatureEnabledState::kReportOnly
          ? mojom::FeaturePolicyDisposition::kReport
          : mojom::FeaturePolicyDisposition::kEnforce,
      GetViolationMessage(animated_property));
}

}  // namespace blink
