// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/forms/layout_text_control_inner_editor.h"

#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"

namespace blink {

LayoutTextControlInnerEditor::LayoutTextControlInnerEditor(Element* element)
    : LayoutBlockFlow(element),
      is_multiline_(IsA<HTMLTextAreaElement>(element->OwnerShadowHost()) &&
                    RuntimeEnabledFeatures::TextareaMultipleIfcsEnabled()) {}

void LayoutTextControlInnerEditor::AddChild(LayoutObject* new_child,
                                            LayoutObject* before_child) {
  NOT_DESTROYED();
  if (!is_multiline_) {
    LayoutBlockFlow::AddChild(new_child, before_child);
    return;
  }

  // If a <textarea> has a value "line1\nline2\n", a
  // TextControlInnerEditorElement for the <textarea> has the following DOM
  // structure:
  //
  // TextControlInnerEditorElement
  //   * Text "line1"
  //   * HTMLBRElement
  //   * Text "line2"
  //   * HTMLBRElement
  //   * HTMLBRElement id="textarea-placeholder-break"
  //
  // This function wraps a pair of a Text and an HTMLBRElement with an anonymous
  // block. So a child of LayoutTextControlInnerEditor must be an anonymous
  // LayoutBlockFlow.
  // Exception: It can have non-anonymous blocks during "TestRendering".
  //            See blink::ReplacementFragment.
  //
  // LayoutTextControlInnerEditor
  //   * LayoutBlockFlow (anonymous)
  //     - LayoutText "line1"
  //     - LayoutBR
  //   * LayoutBlockFlow (anonymous)
  //     - LayoutText "line2"
  //     - LayoutBR
  //   * LayoutBlockFlow (anonymous)
  //     - LayoutBR

  if (!before_child) {
    auto* last_anonymous = DynamicTo<LayoutBlockFlow>(LastChild());
    if (last_anonymous && !last_anonymous->LastChild()->IsBR()) {
      last_anonymous->AddChild(new_child);
      return;
    }
    auto* anonymous = LayoutBlockFlow::CreateAnonymous(&GetDocument(), Style());
    LayoutBlockFlow::AddChild(anonymous);
    anonymous->AddChild(new_child);
    return;
  }

  DCHECK(FirstChild());
  auto* before_parent = To<LayoutBlockFlow>(before_child->Parent());

  if (!before_parent->IsAnonymous()) {
    // `before_child` is the "holder" for TestRendering.
    DCHECK_EQ(before_parent, this);
    if (auto* previous = before_child->PreviousSibling()) {
      DCHECK(previous->IsAnonymousBlockFlow());
      auto* previous_last = previous->SlowLastChild();
      if (!previous_last || !previous_last->IsBR()) {
        previous->AddChild(new_child);
        return;
      }
    }
    auto* anonymous = LayoutBlockFlow::CreateAnonymous(&GetDocument(), Style());
    LayoutBlockFlow::AddChild(anonymous, before_child);
    anonymous->AddChild(new_child);
    return;
  }

  if (!new_child->IsBR()) {
    before_parent->AddChild(new_child, before_child);
    return;
  }
  auto* anonymous = LayoutBlockFlow::CreateAnonymous(&GetDocument(), Style());
  LayoutBlockFlow::AddChild(anonymous, before_parent);
  before_parent->MoveChildrenTo(anonymous, before_parent->FirstChild(),
                                before_child, /* full_remove_insert */ true);
  anonymous->AddChild(new_child);
}

void LayoutTextControlInnerEditor::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const StyleChangeContext& style_change_context) {
  LayoutBlockFlow::StyleDidChange(diff, old_style, style_change_context);

  if (RuntimeEnabledFeatures::TextareaMultipleIfcsEnabled() && old_style &&
      old_style->UsedUserModify() != StyleRef().UsedUserModify() &&
      !FirstChild()) {
    // If this has no children and the UserModify state is changed from
    // non-editable to editable, the box height was zero and this box should be
    // relayout to ensure one-line height.
    SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kStyleChange);
  }
}

}  // namespace blink
