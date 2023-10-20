// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_table_cell_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/table_constants.h"
#include "third_party/blink/renderer/core/layout/mathml/layout_table_cell_with_anonymous_mrow.h"

namespace blink {

MathMLTableCellElement::MathMLTableCellElement(Document& doc)
    : MathMLElement(mathml_names::kMtdTag, doc) {}

unsigned MathMLTableCellElement::colSpan() const {
  const AtomicString& col_span_value =
      FastGetAttribute(mathml_names::kColumnspanAttr);
  unsigned value = 0;
  if (!ParseHTMLClampedNonNegativeInteger(col_span_value, kMinColSpan,
                                          kMaxColSpan, value)) {
    return kDefaultColSpan;
  }
  return value;
}

unsigned MathMLTableCellElement::rowSpan() const {
  const AtomicString& row_span_value =
      FastGetAttribute(mathml_names::kRowspanAttr);
  unsigned value = 0;
  if (!ParseHTMLClampedNonNegativeInteger(row_span_value, kMinRowSpan,
                                          kMaxRowSpan, value)) {
    return kDefaultRowSpan;
  }
  return value;
}

void MathMLTableCellElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == mathml_names::kRowspanAttr ||
      params.name == mathml_names::kColumnspanAttr) {
    if (auto* cell = DynamicTo<LayoutTableCell>(GetLayoutObject())) {
      cell->ColSpanOrRowSpanChanged();
    }
  } else {
    MathMLElement::ParseAttribute(params);
  }
}

LayoutObject* MathMLTableCellElement::CreateLayoutObject(
    const ComputedStyle& style) {
  if (style.Display() == EDisplay::kTableCell) {
    return MakeGarbageCollected<LayoutTableCellWithAnonymousMrow>(this);
  }
  return MathMLElement::CreateLayoutObject(style);
}

}  // namespace blink
