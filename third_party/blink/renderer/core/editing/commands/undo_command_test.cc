// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#include <memory>

namespace blink {

class UndoCommandTest : public EditingTestBase {
 protected:
  void SetPageActive() {
    // To dispatch "focus" event, we should do below.
    GetPage().GetFocusController().SetActive(true);
    GetPage().GetFocusController().SetFocused(true);
    Selection().SetFrameIsFocused(true);
  }
};

// http://crbug.com/1378068
TEST_F(UndoCommandTest, RedoWithDOMChanges) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  auto* const sample_html = R"HTML(
    <div contenteditable id="sample1">One|</div>
    <div contenteditable id="sample2">Two</div>
    )HTML";

  SetPageActive();
  Selection().SetSelection(SetSelectionTextToBody(sample_html),
                           SetSelectionOptions());

  auto* const script_text = R"SCRIPT(
    const sample1 = document.getElementById('sample1');
    const sample2 = document.getElementById('sample2');
    sample1.addEventListener('focus', () => sample1.append('1'));
    sample2.addEventListener('focus', () => sample2.append('2'));
    [...document.scripts].forEach(x => x.remove());
    )SCRIPT";
  auto& script_element =
      *GetDocument().CreateRawElement(html_names::kScriptTag);
  script_element.setInnerHTML(script_text);
  GetDocument().body()->AppendChild(&script_element);
  UpdateAllLifecyclePhasesForTest();

  GetDocument().execCommand("insertText", false, "ABC", ASSERT_NO_EXCEPTION);
  GetDocument().execCommand("undo", false, "", ASSERT_NO_EXCEPTION);

  GetElementById("sample2")->Focus();
  GetDocument().execCommand("redo", false, "", ASSERT_NO_EXCEPTION);

  auto* const expectation = R"HTML(
    <div contenteditable id="sample1">OneABC|1</div>
    <div contenteditable id="sample2">Two2</div>
    )HTML";
  EXPECT_EQ(expectation, GetSelectionTextFromBody());
}

// http://crbug.com/1378068
TEST_F(UndoCommandTest, UndoWithDOMChanges) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  auto* const sample_html = R"HTML(
    <div contenteditable id="sample1">One|</div>
    <div contenteditable id="sample2">Two</div>
    )HTML";

  SetPageActive();
  Selection().SetSelection(SetSelectionTextToBody(sample_html),
                           SetSelectionOptions());

  auto* const script_text = R"SCRIPT(
    const sample1 = document.getElementById('sample1');
    const sample2 = document.getElementById('sample2');
    sample1.addEventListener('focus', () => sample1.append('1'));
    sample2.addEventListener('focus', () => sample2.append('2'));
    [...document.scripts].forEach(x => x.remove());
    )SCRIPT";
  auto& script_element =
      *GetDocument().CreateRawElement(html_names::kScriptTag);
  script_element.setInnerHTML(script_text);
  GetDocument().body()->AppendChild(&script_element);
  UpdateAllLifecyclePhasesForTest();

  GetDocument().execCommand("insertText", false, "ABC", ASSERT_NO_EXCEPTION);

  GetElementById("sample2")->Focus();
  GetDocument().execCommand("undo", false, "", ASSERT_NO_EXCEPTION);

  auto* const expectation = R"HTML(
    <div contenteditable id="sample1">One|1</div>
    <div contenteditable id="sample2">Two2</div>
    )HTML";
  EXPECT_EQ(expectation, GetSelectionTextFromBody());
}

}  // namespace blink
