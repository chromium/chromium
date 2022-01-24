// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_content_element.h"

#include "third_party/blink/renderer/core/layout/layout_document_transition_content.h"

namespace blink {

DocumentTransitionContentElement::DocumentTransitionContentElement(
    Document& document)
    : HTMLElement(
          QualifiedName(g_null_atom, "transition_content_element", g_null_atom),
          document),
      resource_id_(viz::SharedElementResourceId::Generate()) {
  // TODO(khushalsagar) : Move this to a UA style sheet.
  SetInlineStyleProperty(CSSPropertyID::kPosition, CSSValueID::kAbsolute);
  SetInlineStyleProperty(CSSPropertyID::kTop, 0,
                         CSSPrimitiveValue::UnitType::kPixels);
  SetInlineStyleProperty(CSSPropertyID::kLeft, 0,
                         CSSPrimitiveValue::UnitType::kPixels);

  // This implements the current default behaviour of stretching the snapshot to
  // match container size.
  // TODO(khushalsagar) : Change this to a pattern which preserves the aspect
  // ratio once browser defaults are finalized.
  SetInlineStyleProperty(CSSPropertyID::kWidth, 100,
                         CSSPrimitiveValue::UnitType::kPercentage);
  SetInlineStyleProperty(CSSPropertyID::kHeight, 100,
                         CSSPrimitiveValue::UnitType::kPercentage);
  SetInlineStyleProperty(CSSPropertyID::kObjectFit, CSSValueID::kFill);
}

DocumentTransitionContentElement::~DocumentTransitionContentElement() = default;

void DocumentTransitionContentElement::SetIntrinsicSize(
    const LayoutSize& intrinsic_size) {
  intrinsic_size_ = intrinsic_size;
  if (auto* layout_object = GetLayoutObject()) {
    static_cast<LayoutDocumentTransitionContent*>(layout_object)
        ->OnIntrinsicSizeUpdated(intrinsic_size_);
  }
}

LayoutObject* DocumentTransitionContentElement::CreateLayoutObject(
    const ComputedStyle&,
    LegacyLayout) {
  return MakeGarbageCollected<LayoutDocumentTransitionContent>(this);
}

void DocumentTransitionContentElement::Trace(Visitor* visitor) const {
  HTMLElement::Trace(visitor);
}

}  // namespace blink
