// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/shadow/permission_shadow_element.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"

namespace blink {

PermissionShadowElement::PermissionShadowElement(
    HTMLPermissionElement& permission_element)
    : HTMLDivElement(permission_element.GetDocument()),
      permission_element_(&permission_element) {
  SetHasCustomStyleCallbacks();
}

PermissionShadowElement::~PermissionShadowElement() = default;

void PermissionShadowElement::Trace(Visitor* visitor) const {
  visitor->Trace(permission_element_);
  HTMLElement::Trace(visitor);
}

const ComputedStyle* PermissionShadowElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  const ComputedStyle& parent_style = permission_element_->ComputedStyleRef();
  ComputedStyleBuilder style_builder =
      GetDocument().GetStyleResolver().CreateComputedStyleBuilderInheritingFrom(
          parent_style);

  // TODO(https://crbug.com/1462930): Apply styling restrictions here and
  // override `AdjustStyle`
  style_builder.SetDisplay(parent_style.Display());
  style_builder.SetHeight(parent_style.UsedHeight());
  style_builder.SetLineHeight(parent_style.UsedHeight());
  style_builder.SetWidth(parent_style.UsedWidth());
  style_builder.SetVerticalAlign(parent_style.VerticalAlign());
  return style_builder.TakeStyle();
}

}  // namespace blink
