// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/mock_clipboard_host.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class HTMLTextAreaElementTest : public RenderingTest {
 public:
  HTMLTextAreaElementTest() = default;

  void SetUp() override {
    RenderingTest::SetUp();
    clipboard_provider_ =
        std::make_unique<PageTestBase::MockClipboardHostProvider>(
            GetFrame().GetBrowserInterfaceBroker());
  }
  void TearDown() override {
    clipboard_provider_.reset();
    RenderingTest::TearDown();
  }

 protected:
  HTMLTextAreaElement& TestElement() {
    Element* element = GetDocument().getElementById(AtomicString("test"));
    DCHECK(element);
    return To<HTMLTextAreaElement>(*element);
  }

  mojom::blink::ClipboardHost* ClipboardHost() {
    return clipboard_provider_->clipboard_host();
  }

  std::unique_ptr<PageTestBase::MockClipboardHostProvider> clipboard_provider_;
};

TEST_F(HTMLTextAreaElementTest, SanitizeUserInputValue) {
  UChar kLeadSurrogate = 0xD800;
  EXPECT_EQ("", HTMLTextAreaElement::SanitizeUserInputValue("", 0));
  EXPECT_EQ("", HTMLTextAreaElement::SanitizeUserInputValue("a", 0));
  EXPECT_EQ("", HTMLTextAreaElement::SanitizeUserInputValue("\n", 0));
  StringBuilder builder;
  builder.Append(kLeadSurrogate);
  String lead_surrogate = builder.ToString();
  EXPECT_EQ("", HTMLTextAreaElement::SanitizeUserInputValue(lead_surrogate, 0));

  EXPECT_EQ("", HTMLTextAreaElement::SanitizeUserInputValue("", 1));
  EXPECT_EQ("", HTMLTextAreaElement::SanitizeUserInputValue(lead_surrogate, 1));
  EXPECT_EQ("a", HTMLTextAreaElement::SanitizeUserInputValue("a", 1));
  EXPECT_EQ("\n", HTMLTextAreaElement::SanitizeUserInputValue("\n", 1));
  EXPECT_EQ("\n", HTMLTextAreaElement::SanitizeUserInputValue("\n", 2));

  EXPECT_EQ("abc", HTMLTextAreaElement::SanitizeUserInputValue(
                       String("abc") + lead_surrogate, 4));
  EXPECT_EQ("a\ncd", HTMLTextAreaElement::SanitizeUserInputValue("a\ncdef", 4));
  EXPECT_EQ("a\rcd", HTMLTextAreaElement::SanitizeUserInputValue("a\rcdef", 4));
  EXPECT_EQ("a\r\ncd",
            HTMLTextAreaElement::SanitizeUserInputValue("a\r\ncdef", 4));
}

TEST_F(HTMLTextAreaElementTest, ValueWithHardLineBreaks) {
  LoadAhem();

  // The textarea can contain four letters in each of lines.
  SetBodyContent(R"HTML(
    <textarea id=test wrap=hard
              style="font:10px Ahem; width:40px; height:200px;"></textarea>
  )HTML");
  HTMLTextAreaElement& textarea = TestElement();
  RunDocumentLifecycle();
  EXPECT_TRUE(textarea.ValueWithHardLineBreaks().empty());

  textarea.SetValue("12345678");
  RunDocumentLifecycle();
  EXPECT_EQ("1234\n5678", textarea.ValueWithHardLineBreaks());

  textarea.SetValue("1234567890\n");
  RunDocumentLifecycle();
  EXPECT_EQ("1234\n5678\n90\n", textarea.ValueWithHardLineBreaks());

  Document& doc = GetDocument();
  auto* inner_editor = textarea.InnerEditorElement();
  inner_editor->setTextContent("");
  // We set the value same as the previous one, but the value consists of four
  // Text nodes.
  inner_editor->appendChild(Text::Create(doc, "12"));
  inner_editor->appendChild(Text::Create(doc, "34"));
  inner_editor->appendChild(Text::Create(doc, "5678"));
  inner_editor->appendChild(Text::Create(doc, "90"));
  inner_editor->appendChild(doc.CreateRawElement(html_names::kBrTag));
  RunDocumentLifecycle();
  if (RuntimeEnabledFeatures::TextareaLineEndingsAsBrEnabled()) {
    EXPECT_EQ("1234\n5678\n90\n", textarea.ValueWithHardLineBreaks());
  } else {
    EXPECT_EQ("1234\n5678\n90", textarea.ValueWithHardLineBreaks());
  }
}

TEST_F(HTMLTextAreaElementTest, ValueWithHardLineBreaksRtl) {
  LoadAhem();

  SetBodyContent(R"HTML(
    <textarea id=test wrap=hard style="font:10px Ahem; width:160px;"></textarea>
  )HTML");
  HTMLTextAreaElement& textarea = TestElement();

#define LTO "\xE2\x80\xAD"
#define RTO "\xE2\x80\xAE"
  textarea.SetValue(
      String::FromUTF8(RTO "Hebrew" LTO " English " RTO "Arabic" LTO));
  // This textarea is rendered as:
  //    -----------------
  //    | EnglishwerbeH |
  //    |cibarA         |
  //     ----------------
  RunDocumentLifecycle();
  EXPECT_EQ(String::FromUTF8(RTO "Hebrew" LTO " English \n" RTO "Arabic" LTO),
            textarea.ValueWithHardLineBreaks());
#undef LTO
#undef RTO
}

TEST_F(HTMLTextAreaElementTest, DefaultToolTip) {
  LoadAhem();

  SetBodyContent(R"HTML(
    <textarea id=test></textarea>
  )HTML");
  HTMLTextAreaElement& textarea = TestElement();

  textarea.SetBooleanAttribute(html_names::kRequiredAttr, true);
  EXPECT_EQ("<<ValidationValueMissing>>", textarea.DefaultToolTip());

  textarea.SetBooleanAttribute(html_names::kNovalidateAttr, true);
  EXPECT_EQ(String(), textarea.DefaultToolTip());

  textarea.removeAttribute(html_names::kNovalidateAttr);
  textarea.SetValue("1234567890\n");
  EXPECT_EQ(String(), textarea.DefaultToolTip());
}

TEST_F(HTMLTextAreaElementTest, PlaceholderBreakAfterUndo) {
  Document& doc = GetDocument();
  SetBodyContent("<textarea id=test>foo\n</textarea>");
  HTMLTextAreaElement& textarea = TestElement();
  textarea.Focus();

  // Setup for clipboard commands.
  GetFrame().GetSettings()->SetJavaScriptCanAccessClipboard(true);
  GetFrame().GetSettings()->SetDOMPasteAllowed(true);
  GetFrame().SetHadUserInteraction(true);

  // Cut all.
  // Unlike the initial empty value, this leaves the placeholder break element.
  doc.execCommand("selectall", false, "", ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(doc.execCommand("cut", false, "", ASSERT_NO_EXCEPTION));

  // Paste text with the trailing \n.  It removes the placeholder break element.
  ClipboardHost()->WriteText("foo\n");
  ASSERT_TRUE(doc.execCommand("paste", false, "", ASSERT_NO_EXCEPTION));

  // The undo command re-add the placeholder break element.
  doc.execCommand("undo", false, "", ASSERT_NO_EXCEPTION);
  // The test passes if no DCHECK failure.
  GetFrame().SetHadUserInteraction(false);
}

// crbug.com/442551790
TEST_F(HTMLTextAreaElementTest, ClearWithInsertText) {
  SetBodyInnerHTML("<textarea id=test>some text\n</textarea>");
  auto& textarea = TestElement();
  textarea.Focus();
  textarea.select();
  const auto* inner_editor =
      To<LayoutBlockFlow>(textarea.GetLayoutBox()->FirstChildBox());
  ASSERT_TRUE(inner_editor);

  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_GT(inner_editor->StitchedSize().height, LayoutUnit());
}

// crbug.com/442600643
TEST_F(HTMLTextAreaElementTest, RemoveLastLineWithInsertText) {
  SetBodyInnerHTML(
      "<textarea id=test style='line-height:20px;'>a\nb</textarea>");
  auto& textarea = TestElement();
  textarea.Focus();
  textarea.SetSelectionRange(2, 3);
  const auto* inner_editor =
      To<LayoutBlockFlow>(textarea.GetLayoutBox()->FirstChildBox());
  ASSERT_TRUE(inner_editor);

  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  // The box should have two lines high.
  EXPECT_EQ(inner_editor->StitchedSize().height, LayoutUnit(20 * 2));
}

}  // namespace blink
