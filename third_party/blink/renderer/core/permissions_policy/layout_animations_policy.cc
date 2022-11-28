// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/permissions_policy/layout_animations_policy.h"

#include "third_party/blink/public/common/permissions_policy/document_policy_features.h"
#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {
namespace {
String GetViolationMessage(const CSSProperty& property) {
  return String::Format(
      "Document policy violation: CSS property '%s' violates document policy "
      "'%s' which is disabled in this document",
      property.GetPropertyNameString().Utf8().c_str(),
      GetDocumentPolicyFeatureInfoMap()
          .at(mojom::blink::DocumentPolicyFeature::kLayoutAnimations)
          .feature_name.c_str());
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
    ExecutionContext& context) {
  DCHECK(AffectedCSSProperties().Contains(&animated_property));
  context.IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kLayoutAnimations,
      ReportOptions::kReportOnFailure, GetViolationMessage(animated_property));
}

}  // namespace blink
