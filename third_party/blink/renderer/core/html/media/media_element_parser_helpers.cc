// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_element_parser_helpers.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/svg_element_type_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace media_element_parser_helpers {

bool IsMediaElement(const Element* element) {
  if ((IsHTMLImageElement(element) || IsSVGImageElement(element)) &&
      !element->GetDocument().IsImageDocument())
    return true;
  if (IsHTMLVideoElement(element) && !element->GetDocument().IsMediaDocument())
    return true;
  return false;
}

void ReportUnsizedMediaViolation(const LayoutObject* layout_object,
                                 bool send_report) {
  const ComputedStyle& style = layout_object->StyleRef();
  if (!style.LogicalWidth().IsSpecified() &&
      !style.LogicalHeight().IsSpecified()) {
    layout_object->GetDocument().CountPotentialFeaturePolicyViolation(
        mojom::FeaturePolicyFeature::kUnsizedMedia);
    if (send_report) {
      layout_object->GetDocument().ReportFeaturePolicyViolation(
          mojom::FeaturePolicyFeature::kUnsizedMedia,
          mojom::FeaturePolicyDisposition::kEnforce);
    }
  }
}

}  // namespace media_element_parser_helpers

}  // namespace blink
