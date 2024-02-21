// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

EditingTestBase::EditingTestBase() = default;

EditingTestBase::~EditingTestBase() = default;

Position EditingTestBase::SetCaretTextToBody(
    const std::string& selection_text) {
  const SelectionInDOMTree selection = SetSelectionTextToBody(selection_text);
  DCHECK(selection.IsCaret())
      << "|selection_text| should contain a caret marker '|'";
  return selection.Anchor();
}

SelectionInDOMTree EditingTestBase::SetSelectionTextToBody(
    const std::string& selection_text) {
  return SetSelectionText(GetDocument().body(), selection_text);
}

SelectionInDOMTree EditingTestBase::SetSelectionText(
    HTMLElement* element,
    const std::string& selection_text) {
  const SelectionInDOMTree selection =
      SelectionSample::SetSelectionText(element, selection_text);
  UpdateAllLifecyclePhasesForTest();
  return selection;
}

std::string EditingTestBase::GetSelectionTextFromBody(
    const SelectionInDOMTree& selection) const {
  return SelectionSample::GetSelectionText(*GetDocument().body(), selection);
}

std::string EditingTestBase::GetSelectionTextFromBody() const {
  return GetSelectionTextFromBody(Selection().GetSelectionInDOMTree());
}

std::string EditingTestBase::GetSelectionTextInFlatTreeFromBody(
    const SelectionInFlatTree& selection) const {
  return SelectionSample::GetSelectionTextInFlatTree(*GetDocument().body(),
                                                     selection);
}

std::string EditingTestBase::GetCaretTextFromBody(
    const Position& position) const {
  DCHECK(position.IsValidFor(GetDocument()))
      << "A valid position must be provided " << position;
  return GetSelectionTextFromBody(
      SelectionInDOMTree::Builder().Collapse(position).Build());
}

ShadowRoot* EditingTestBase::CreateShadowRootForElementWithIDAndSetInnerHTML(
    TreeScope& scope,
    const char* host_element_id,
    const char* shadow_root_content) {
  ShadowRoot& shadow_root =
      scope.getElementById(AtomicString::FromUTF8(host_element_id))
          ->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(String::FromUTF8(shadow_root_content),
                           ASSERT_NO_EXCEPTION);
  scope.GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  return &shadow_root;
}

ShadowRoot* EditingTestBase::SetShadowContent(const char* shadow_content,
                                              const char* host) {
  ShadowRoot* shadow_root = CreateShadowRootForElementWithIDAndSetInnerHTML(
      GetDocument(), host, shadow_content);
  return shadow_root;
}

}  // namespace blink
