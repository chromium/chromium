// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_element_parser_helpers.h"
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

bool IsUnsizedMediaEnabled(const Document& document) {
  if (auto* frame = document.GetFrame()) {
    return frame->DeprecatedIsFeatureEnabled(
        mojom::FeaturePolicyFeature::kUnsizedMedia);
  }
  // Unsized media is by default enabled every where, so when the frame is not
  // available return default policy (true).
  return true;
}

bool ParseIntrinsicSizeAttribute(const String& value,
                                 const Element* element,
                                 IntSize* intrinsic_size,
                                 bool* is_default_intrinsic_size,
                                 String* message) {
  *is_default_intrinsic_size = false;
  unsigned new_width = 0, new_height = 0;
  Vector<String> size;
  value.Split('x', size);
  if (!value.IsEmpty() &&
      (size.size() != 2 ||
       !ParseHTMLNonNegativeInteger(size.at(0), new_width) ||
       !ParseHTMLNonNegativeInteger(size.at(1), new_height))) {
    *message =
        "Unable to parse intrinsicSize: expected [unsigned] x [unsigned]"
        ", got " +
        value;
    new_width = 0;
    new_height = 0;
  }
  if (new_width == 0 && new_height == 0 && IsMediaElement(element) &&
      !IsUnsizedMediaEnabled(element->GetDocument())) {
    new_width = LayoutReplaced::kDefaultWidth;
    new_height = LayoutReplaced::kDefaultHeight;
    *is_default_intrinsic_size = true;
  }

  IntSize new_size(new_width, new_height);
  if (*intrinsic_size != new_size) {
    *intrinsic_size = new_size;
    return true;
  }
  return false;
}

void ReportUnsizedMediaViolation(const LayoutObject* layout_object) {
  const ComputedStyle& style = layout_object->StyleRef();
  if (!style.LogicalWidth().IsSpecified() &&
      !style.LogicalHeight().IsSpecified() && layout_object->GetFrame()) {
    layout_object->GetFrame()->DeprecatedReportFeaturePolicyViolation(
        mojom::FeaturePolicyFeature::kUnsizedMedia);
  }
}

}  // namespace media_element_parser_helpers

}  // namespace blink
