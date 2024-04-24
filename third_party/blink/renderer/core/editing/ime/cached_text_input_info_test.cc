// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/cached_text_input_info.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

class CachedTextInputInfoTest : public EditingTestBase {
 protected:
  CachedTextInputInfo& GetCachedTextInputInfo() {
    return GetInputMethodController().GetCachedTextInputInfoForTesting();
  }

  InputMethodController& GetInputMethodController() {
    return GetFrame().GetInputMethodController();
  }
};

TEST_F(CachedTextInputInfoTest, Basic) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable id=\"sample\">a|b</div>"),
      SetSelectionOptions());
  const Element& sample = *GetElementById("sample");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(1, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("ab", GetCachedTextInputInfo().GetText());

  To<Text>(sample.firstChild())->appendData("X");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(1, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abX", GetCachedTextInputInfo().GetText());
}

// http://crbug.com/1382425
TEST_F(CachedTextInputInfoTest, InlineElementEditable) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody("<span contenteditable><img>|a</img></span>"),
      SetSelectionOptions());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(1, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ(String(u"\uFFFCa"), GetCachedTextInputInfo().GetText());

  auto& span = *GetDocument().QuerySelector(AtomicString("span"));
  span.replaceChild(Text::Create(GetDocument(), "12345"), span.firstChild());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(5, 5),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("12345a", GetCachedTextInputInfo().GetText());
}

// http://crbug.com/1194349
TEST_F(CachedTextInputInfoTest, PlaceholderBRInTextArea) {
  SetBodyContent("<textarea id=target>abc\n</textarea>");
  auto& target = *To<TextControlElement>(GetElementById("target"));

  // Inner editor is <div>abc<br></div>.
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position::LastPositionInNode(*target.InnerEditorElement()))
          .Build(),
      SetSelectionOptions());

  EXPECT_EQ(PlainTextRange(4, 4),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abc\n", GetCachedTextInputInfo().GetText())
      << "We should not emit a newline for placeholder <br>";
}

// http://crbug.com/1197801
TEST_F(CachedTextInputInfoTest, PlaceholderBROnlyInTextArea) {
  SetBodyContent("<textarea id=target></textarea>");
  auto& target = *To<TextControlElement>(GetElementById("target"));
  target.Focus();
  GetDocument().execCommand("insertparagraph", false, "", ASSERT_NO_EXCEPTION);
  GetDocument().execCommand("delete", false, "", ASSERT_NO_EXCEPTION);

  // Inner editor is <div><br></div>.
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position::LastPositionInNode(*target.InnerEditorElement()))
          .Build(),
      SetSelectionOptions());

  EXPECT_EQ(PlainTextRange(0, 0),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("", GetCachedTextInputInfo().GetText());
}

TEST_F(CachedTextInputInfoTest, RelayoutBoundary) {
  InsertStyleElement(
      "#sample { contain: strict; width: 100px; height: 100px; }");
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable><div id=\"sample\">^a|b</div>"),
      SetSelectionOptions());
  const Element& sample = *GetElementById("sample");
  ASSERT_TRUE(sample.GetLayoutObject()->IsRelayoutBoundary());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("ab", GetCachedTextInputInfo().GetText());

  To<Text>(sample.firstChild())->appendData("X");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abX", GetCachedTextInputInfo().GetText());
}

// http://crbug.com/1292516
TEST_F(CachedTextInputInfoTest, PositionAbsolute) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable>"
          "<p id=sample style='position:absolute'>ab|<b>cd</b></p>"
          "</div>"),
      SetSelectionOptions());

  const auto& sample = *GetElementById("sample");
  auto& text_ab = *To<Text>(sample.firstChild());
  const auto& text_cd = *To<Text>(sample.lastChild()->firstChild());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(2, 2),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abcd", GetCachedTextInputInfo().GetText());

  // Insert "AB" after "ab"
  text_ab.appendData("AB");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(2, 2),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abABcd", GetCachedTextInputInfo().GetText());

  // Move caret after "cd"
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(text_cd, 2)).Build(),
      SetSelectionOptions());

  // Insert "CD" after "cd"
  GetDocument().execCommand("insertText", false, "CD", ASSERT_NO_EXCEPTION);

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(8, 8),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abABcdCD", GetCachedTextInputInfo().GetText());
}

// http://crbug.com/1228373
TEST_F(CachedTextInputInfoTest, ShadowTree) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody("<div id=host><template data-mode=open>"
                             "<a>012</a><b>3^45</b>67|8"
                             "</template></div>"),
      SetSelectionOptions());

  EXPECT_EQ(PlainTextRange(4, 8),
            GetInputMethodController().GetSelectionOffsets());

  // Change shadow tree to "XYZ<a>012</a><b>345</b>678"
  auto& shadow_root = *GetElementById("host")->GetShadowRoot();
  shadow_root.insertBefore(Text::Create(GetDocument(), "XYZ"),
                           shadow_root.firstChild());

  // Ask |CachedTextInputInfo| to compute |PlainTextRange| for selection.
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(*To<Text>(shadow_root.lastChild()), 0))
          .Build(),
      SetSelectionOptions());

  EXPECT_EQ(PlainTextRange(9, 9),
            GetInputMethodController().GetSelectionOffsets());
}

// http://crbug.com/1228635
TEST_F(CachedTextInputInfoTest, VisibilityHiddenToVisible) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable id=sample>"
          "<b id=target style='visibility: hidden'>A</b><b>^Z|</b></div>"),
      SetSelectionOptions());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("Z", GetCachedTextInputInfo().GetText())
      << "Texts within visibility:hidden are excluded";

  Element& target = *GetElementById("target");
  target.style()->setProperty(GetDocument().GetExecutionContext(), "visibility",
                              "visible", "", ASSERT_NO_EXCEPTION);

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(1, 2),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("AZ", GetCachedTextInputInfo().GetText());
}

// http://crbug.com/1228635
TEST_F(CachedTextInputInfoTest, VisibilityVisibleToHidden) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable id=sample>"
          "<b id=target style='visibility: visible'>A</b><b>^Z|</b></div>"),
      SetSelectionOptions());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(1, 2),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("AZ", GetCachedTextInputInfo().GetText());

  Element& target = *GetElementById("target");
  target.style()->setProperty(GetDocument().GetExecutionContext(), "visibility",
                              "hidden", "", ASSERT_NO_EXCEPTION);

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("Z", GetCachedTextInputInfo().GetText())
      << "Texts within visibility:hidden are excluded";
}

}  // namespace blink
