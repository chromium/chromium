// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class TextControlElementTest : public testing::Test {
 protected:
  void SetUp() override;

  DummyPageHolder& Page() const { return *dummy_page_holder_; }
  Document& GetDocument() const { return *document_; }
  TextControlElement& TextControl() const { return *text_control_; }
  HTMLInputElement& Input() const { return *input_; }

  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;

  Persistent<Document> document_;
  Persistent<TextControlElement> text_control_;
  Persistent<HTMLInputElement> input_;
};

void TextControlElementTest::SetUp() {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(IntSize(800, 600), &page_clients);

  document_ = &dummy_page_holder_->GetDocument();
  document_->documentElement()->SetInnerHTMLFromString(
      "<body><textarea id=textarea></textarea><input id=input /></body>");
  UpdateAllLifecyclePhases();
  text_control_ = ToTextControl(document_->getElementById("textarea"));
  text_control_->focus();
  input_ = ToHTMLInputElement(document_->getElementById("input"));
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
  Input().focus();
  Input().setValue("Hello, input form.");
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
  Input().setValue("Hello");
  HTMLElement* inner_editor = Input().InnerEditorElement();
  EXPECT_EQ(5u, TextControlElement::IndexForPosition(
                    inner_editor,
                    Position(inner_editor, PositionAnchorType::kAfterAnchor)));
}

TEST_F(TextControlElementTest, ReadOnlyAttributeChangeEditability) {
  Input().setAttribute(html_names::kStyleAttr, "all:initial");
  Input().setAttribute(html_names::kReadonlyAttr, "");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(EUserModify::kReadOnly,
            Input().InnerEditorElement()->GetComputedStyle()->UserModify());

  Input().removeAttribute(html_names::kReadonlyAttr);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(EUserModify::kReadWritePlaintextOnly,
            Input().InnerEditorElement()->GetComputedStyle()->UserModify());
}

TEST_F(TextControlElementTest, DisabledAttributeChangeEditability) {
  Input().setAttribute(html_names::kStyleAttr, "all:initial");
  Input().setAttribute(html_names::kDisabledAttr, "");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(EUserModify::kReadOnly,
            Input().InnerEditorElement()->GetComputedStyle()->UserModify());

  Input().removeAttribute(html_names::kDisabledAttr);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(EUserModify::kReadWritePlaintextOnly,
            Input().InnerEditorElement()->GetComputedStyle()->UserModify());
}

}  // namespace blink
