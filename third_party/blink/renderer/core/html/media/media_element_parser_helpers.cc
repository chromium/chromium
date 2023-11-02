// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_element_parser_helpers.h"

#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

namespace media_element_parser_helpers {

void CheckUnsizedMediaViolation(const LayoutObject* layout_object,
                                bool send_report) {
  const ComputedStyle& style = layout_object->StyleRef();
  bool is_unsized = !style.LogicalWidth().IsSpecified() &&
                    !style.LogicalHeight().IsSpecified();
  if (is_unsized) {
    layout_object->GetDocument().GetExecutionContext()->IsFeatureEnabled(
        mojom::blink::DocumentPolicyFeature::kUnsizedMedia,
        send_report ? ReportOptions::kReportOnFailure
                    : ReportOptions::kDoNotReport);
  }
}

}  // namespace media_element_parser_helpers

}  // namespace blink
