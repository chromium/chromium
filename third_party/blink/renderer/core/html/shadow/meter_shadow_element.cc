// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/shadow/meter_shadow_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"

namespace blink {

MeterShadowElement::MeterShadowElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
}

HTMLMeterElement* MeterShadowElement::MeterElement() const {
  return To<HTMLMeterElement>(OwnerShadowHost());
}

scoped_refptr<ComputedStyle> MeterShadowElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  scoped_refptr<ComputedStyle> style =
      OriginalStyleForLayoutObject(style_recalc_context);
  const ComputedStyle* meter_style = MeterElement()->GetComputedStyle();
  DCHECK(meter_style);
  // For vertical writing-mode, we need to set the direction to rtl so that
  // the meter value bar is rendered bottom up.
  if (!IsHorizontalWritingMode(style->GetWritingMode())) {
    ComputedStyleBuilder builder(*style);
    builder.SetDirection(TextDirection::kRtl);
    style = builder.TakeStyle();
  }
  return style;
}

}  // namespace blink
