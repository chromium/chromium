// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_row_element.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/mathml/layout_mathml_block.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"

namespace blink {

MathMLRowElement::MathMLRowElement(const QualifiedName& tagName,
                                   Document& document)
    : MathMLElement(tagName, document) {
  if (HasTagName(mathml_names::kMathTag)) {
    UseCounter::Count(document, WebFeature::kMathMLMathElement);
  }
}

LayoutObject* MathMLRowElement::CreateLayoutObject(const ComputedStyle& style) {
  if (!style.IsDisplayMathType()) {
    return MathMLElement::CreateLayoutObject(style);
  }
  return MakeGarbageCollected<LayoutMathMLBlock>(this);
}

void MathMLRowElement::ChildrenChanged(const ChildrenChange& change) {
  if (change.by_parser == ChildrenChangeSource::kAPI) {
    for (auto& child : Traversal<MathMLOperatorElement>::ChildrenOf(*this)) {
      // TODO(crbug.com/1124298): make this work for embellished operators.
      child.CheckFormAfterSiblingChange();
    }
  }

  MathMLElement::ChildrenChanged(change);
}

Node::InsertionNotificationRequest MathMLRowElement::InsertedInto(
    ContainerNode& root_parent) {
  if (HasTagName(mathml_names::kMathTag) && root_parent.isConnected()) {
    UseCounter::Count(GetDocument(), WebFeature::kMathMLMathElementInDocument);
  }
  return MathMLElement::InsertedInto(root_parent);
}

}  // namespace blink
