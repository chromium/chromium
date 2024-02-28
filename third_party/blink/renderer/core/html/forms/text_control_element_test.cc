// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/text_control_element.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

String PlaceholderString(Element& e) {
  auto* text_control = ToTextControlOrNull(e);
  if (text_control && text_control->IsPlaceholderVisible()) {
    if (HTMLElement* placeholder_element = text_control->PlaceholderElement()) {
      return placeholder_element->textContent();
    }
  }
  return String();
}

class TextControlElementTest : public testing::Test {
 protected:
  void SetUp() override;

  DummyPageHolder& Page() const { return *dummy_page_holder_; }
  Document& GetDocument() const { return *document_; }
  TextControlElement& TextControl() const { return *text_control_; }
  HTMLInputElement& Input() const { return *input_; }

  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  void AssertPlaceholderTextIs(const String& element_id, const String& text) {
    auto* e = GetDocument().getElementById(AtomicString(element_id));
    ASSERT_TRUE(e);
    EXPECT_EQ(PlaceholderString(*e), text);
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;

  Persistent<Document> document_;
  Persistent<TextControlElement> text_control_;
  Persistent<HTMLInputElement> input_;
};

void TextControlElementTest::SetUp() {
  dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600), nullptr);

  document_ = &dummy_page_holder_->GetDocument();
  document_->documentElement()->setInnerHTML(
      "<body><textarea id=textarea></textarea><input id=input /></body>");
  UpdateAllLifecyclePhases();
  text_control_ =
      ToTextControl(document_->getElementById(AtomicString("textarea")));
  text_control_->Focus();
  input_ =
      To<HTMLInputElement>(document_->getElementById(AtomicString("input")));
}

TEST_F(TextControlElementTest, SetSelectionRange) {
  EXPECT_EQ(0u, TextControl().selectionStart());
  EXPECT_EQ(0u, TextControl().selectionEnd());

  TextControl().SetInnerEditorValue("Hello, text form.");
  EXPECT_EQ(0u, TextControl().selectionStart());
  EXPECT_EQ(0u, TextControl().selectionEnd());

  TextControl().SetSelectionRange(1, 3);
  EXPECT_EQ(1u, TextControl().selectionStart());
  EXPECT_EQ(3u, TextControl().selectionEnd());
}

TEST_F(TextControlElementTest, SetSelectionRangeDoesNotCauseLayout) {
  Input().Focus();
  Input().SetValue("Hello, input form.");
  Input().SetSelectionRange(1, 1);

  // Force layout if document().updateStyleAndLayoutIgnorePendingStylesheets()
  // is called.
  GetDocument().body()->AppendChild(GetDocument().createTextNode("foo"));
  unsigned start_layout_count = Page().GetFrameView().LayoutCountForTesting();
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  Input().SetSelectionRange(2, 2);
  EXPECT_EQ(start_layout_count, Page().GetFrameView().LayoutCountForTesting());
}

TEST_F(TextControlElementTest, IndexForPosition) {
  Input().SetValue("Hello");
  HTMLElement* inner_editor = Input().InnerEditorElement();
  EXPECT_EQ(5u, TextControlElement::IndexForPosition(
                    inner_editor,
                    Position(inner_editor, PositionAnchorType::kAfterAnchor)));
}

TEST_F(TextControlElementTest, ReadOnlyAttributeChangeEditability) {
  Input().setAttribute(html_names::kStyleAttr, AtomicString("all:initial"));
  Input().setAttribute(html_names::kReadonlyAttr, g_empty_atom);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(EUserModify::kReadOnly,
            Input().InnerEditorElement()->GetComputedStyle()->UsedUserModify());

  Input().removeAttribute(html_names::kReadonlyAttr);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(EUserModify::kReadWritePlaintextOnly,
            Input().InnerEditorElement()->GetComputedStyle()->UsedUserModify());
}

TEST_F(TextControlElementTest, DisabledAttributeChangeEditability) {
  Input().setAttribute(html_names::kStyleAttr, AtomicString("all:initial"));
  Input().setAttribute(html_names::kDisabledAttr, g_empty_atom);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(EUserModify::kReadOnly,
            Input().InnerEditorElement()->GetComputedStyle()->UsedUserModify());

  Input().removeAttribute(html_names::kDisabledAttr);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(EUserModify::kReadWritePlaintextOnly,
            Input().InnerEditorElement()->GetComputedStyle()->UsedUserModify());
}

TEST_F(TextControlElementTest, PlaceholderElement) {
  EXPECT_EQ(Input().PlaceholderElement(), nullptr);
  EXPECT_EQ(TextControl().PlaceholderElement(), nullptr);

  Input().setAttribute(html_names::kPlaceholderAttr, g_empty_atom);
  TextControl().setAttribute(html_names::kPlaceholderAttr, g_empty_atom);
  UpdateAllLifecyclePhases();

  EXPECT_NE(Input().PlaceholderElement(), nullptr);
  EXPECT_NE(TextControl().PlaceholderElement(), nullptr);

  Input().removeAttribute(html_names::kPlaceholderAttr);
  TextControl().removeAttribute(html_names::kPlaceholderAttr);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(Input().PlaceholderElement(), nullptr);
  EXPECT_EQ(TextControl().PlaceholderElement(), nullptr);
}

TEST_F(TextControlElementTest, PlaceholderElementNewlineBehavior) {
  GetDocument().body()->setInnerHTML(
      "<input id='p0' placeholder='first line &#13;&#10;second line'>"
      "<input id='p1' placeholder='&#13;'>");
  UpdateAllLifecyclePhases();
  AssertPlaceholderTextIs("p0", "first line second line");
  AssertPlaceholderTextIs("p1", "");
}

TEST_F(TextControlElementTest, TextAreaPlaceholderElementNewlineBehavior) {
  GetDocument().body()->setInnerHTML(
      "<textarea id='p0' placeholder='first line &#13;&#10;second line'>"
      "</textarea><textarea id='p1' placeholder='&#10;'></textarea>"
      "<textarea id='p2' placeholder='&#13;'></textarea>");
  UpdateAllLifecyclePhases();
  AssertPlaceholderTextIs("p0", "first line \nsecond line");
  AssertPlaceholderTextIs("p1", "\n");
  AssertPlaceholderTextIs("p1", "\n");
}

}  // namespace blink
