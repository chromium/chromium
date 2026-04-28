// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"

#include <ostream>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/mock_clipboard_host.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/skia/include/core/SkFontTypes.h"

using testing::Each;
using testing::ElementsAre;
using testing::Field;
using testing::Matcher;
using testing::NotNull;
using testing::SizeIs;

namespace blink {

void PrintTo(const WebFormControlElement::GlyphInfo& info, std::ostream* os) {
  *os << "{glyph: " << info.glyph << ", offset: " << info.offset.ToString()
      << ", tot_adv: " << info.total_advance << "}";
}

void PrintTo(const WebFormControlElement::TypefaceRunInfo& info,
             std::ostream* os) {
  *os << "\n{glyphs: [\n";
  SkString name;
  info.typeface->getFamilyName(&name);
  for (size_t i = 0; i < info.glyphs.size(); ++i) {
    *os << "  " << testing::PrintToString(info.glyphs[i]);
    if (i + 1 < info.glyphs.size()) {
      *os << ",\n";
    }
  }
  *os << "],\n"
      << "  typeface: " << name.c_str()
      << ",\n  is_horizontal: " << base::ToString(info.is_horizontal) << "\n}";
}

void PrintTo(const WebFormControlElement::TextRunInfo& info, std::ostream* os) {
  *os << "<location: " << info.location.ToString() << "\n";
  for (size_t i = 0; i < info.typeface_runs.size(); ++i) {
    *os << "TypefaceRunInfo #" << i << ":"
        << testing::PrintToString(info.typeface_runs[i]);
    if (i + 1 < info.typeface_runs.size()) {
      *os << "\n";
    }
  }
  *os << ">";
}

namespace {

class TextRunIsMatcher {
 public:
  using is_gtest_matcher = void;

  TextRunIsMatcher(std::vector<std::string> strs, gfx::RectF location)
      : strs_(strs), location_(location) {}

  bool MatchAndExplain(const WebFormControlElement::TextRunInfo& arg,
                       std::ostream* os) const {
    if (arg.location != location_) {
      if (os) {
        *os << "expected location " << location_.ToString() << " but got "
            << arg.location.ToString();
      }
      return false;
    }

    if (arg.typeface_runs.size() != strs_.size()) {
      if (os) {
        *os << "expected " << strs_.size() << " typeface_runs but got "
            << arg.typeface_runs.size();
      }
      return false;
    }

    size_t typeface_runs_index = 0;
    for (const std::string& expected_str : strs_) {
      const WebFormControlElement::TypefaceRunInfo& run =
          arg.typeface_runs[typeface_runs_index++];

      if (!run.typeface) {
        if (os) {
          *os << "expected a non-null typeface for run #"
              << typeface_runs_index;
        }
        return false;
      }

      std::vector<SkGlyphID> expected_glyphs(expected_str.size());
      size_t glyph_count = run.typeface->textToGlyphs(
          expected_str.data(), expected_str.size(), SkTextEncoding::kUTF8,
          SkSpan<SkGlyphID>(expected_glyphs));
      expected_glyphs.resize(glyph_count);

      if (run.glyphs.size() != expected_glyphs.size()) {
        if (os) {
          *os << "expected " << expected_glyphs.size() << " glyphs but got "
              << run.glyphs.size();
        }
        return false;
      }

      for (size_t i = 0; i < expected_glyphs.size(); ++i) {
        if (run.glyphs[i].glyph != expected_glyphs[i]) {
          if (os) {
            *os << "glyph at index " << i << " expected " << expected_glyphs[i]
                << " but got " << run.glyphs[i].glyph;
          }
          return false;
        }
      }
    }

    return true;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "matches TextRunInfo ";
    DescribeDataTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "does not match TextRunInfo ";
    DescribeDataTo(os);
  }

 private:
  void DescribeDataTo(std::ostream* os) const {
    *os << "at " << location_.ToString() << " with typeface_runs matching {";
    for (size_t i = 0; i < strs_.size(); ++i) {
      *os << "\"" << strs_[i] << "\"";
      if (i + 1 < strs_.size()) {
        *os << ", ";
      }
    }
    *os << "}";
  }

  std::vector<std::string> strs_;
  gfx::RectF location_;
};

testing::Matcher<const WebFormControlElement::TextRunInfo&> TextRunIs(
    std::vector<std::string> strs,
    gfx::RectF location) {
  return TextRunIsMatcher(strs, location);
}

}  // namespace

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
  EXPECT_EQ("1234\n5678\n90\n", textarea.ValueWithHardLineBreaks());
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
      String::FromUtf8(RTO "Hebrew" LTO " English " RTO "Arabic" LTO));
  // This textarea is rendered as:
  //    -----------------
  //    | EnglishwerbeH |
  //    |cibarA         |
  //     ----------------
  RunDocumentLifecycle();
  EXPECT_EQ(String::FromUtf8(RTO "Hebrew" LTO " English \n" RTO "Arabic" LTO),
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

TEST_F(HTMLTextAreaElementTest, DefaultToolTipWithFormNoValidate) {
  LoadAhem();

  SetBodyContent(R"HTML(
    <form novalidate>
      <textarea id=test required></textarea>
    </form>
  )HTML");
  HTMLTextAreaElement& textarea = TestElement();

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
      To<LayoutBlockFlow>(textarea.GetLayoutBox()->SlowFirstChild());
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
      To<LayoutBlockFlow>(textarea.GetLayoutBox()->SlowFirstChild());
  ASSERT_TRUE(inner_editor);

  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  // The box should have two lines high.
  EXPECT_EQ(inner_editor->StitchedSize().height, LayoutUnit(20 * 2));
}

TEST_F(HTMLTextAreaElementTest, GetTextInfoFonts) {
  LoadAhem();
  LoadNoto();

  SetBodyContent(
      "<textarea id=test style='font-family: Ahem, NotoArabic; width:500px'>"
      "XX\n"
      "pp ع\n"
      "</textarea>");
  HTMLTextAreaElement& textarea = TestElement();

  {
    WebFormControlElement::TextInfo text_info = textarea.GetTextInfo();
    ASSERT_EQ(3u, text_info.text_runs.size());
    ASSERT_EQ(1u, text_info.text_runs[0].typeface_runs.size());
    ASSERT_EQ(1u, text_info.text_runs[1].typeface_runs.size());
    ASSERT_EQ(1u, text_info.text_runs[2].typeface_runs.size());
    WebFormControlElement::TypefaceRunInfo& run1 =
        text_info.text_runs[0].typeface_runs[0];
    WebFormControlElement::TypefaceRunInfo& run2 =
        text_info.text_runs[1].typeface_runs[0];
    WebFormControlElement::TypefaceRunInfo& run3 =
        text_info.text_runs[2].typeface_runs[0];

    ASSERT_TRUE(run1.typeface);
    ASSERT_TRUE(run2.typeface);
    ASSERT_TRUE(run3.typeface);
    EXPECT_EQ(run1.typeface, run2.typeface);
    EXPECT_NE(run1.typeface, run3.typeface);
    ASSERT_EQ(2u, run1.glyphs.size());
    ASSERT_EQ(3u, run2.glyphs.size());
    ASSERT_EQ(1u, run3.glyphs.size());
    const int16_t first_glyph = run1.glyphs[0].glyph;
    EXPECT_EQ(first_glyph, run1.glyphs[1].glyph);
    EXPECT_NE(first_glyph, run2.glyphs[0].glyph);
    EXPECT_NE(first_glyph, run3.glyphs[0].glyph);
  }

  {
    textarea.SetValue(u"Hello, العالم!");
    RunDocumentLifecycle();

    WebFormControlElement::TextInfo text_info = textarea.GetTextInfo();
    ASSERT_EQ(3u, text_info.text_runs.size());
    ASSERT_THAT(text_info.text_runs,
                Each(Field(&WebFormControlElement::TextRunInfo::typeface_runs,
                           SizeIs(1))));
    ASSERT_THAT(
        text_info.text_runs,
        Each(Field(
            &WebFormControlElement::TextRunInfo::typeface_runs,
            ElementsAre(Field(&WebFormControlElement::TypefaceRunInfo::typeface,
                              NotNull())))));
    EXPECT_EQ(text_info.text_runs[0].typeface_runs[0].typeface,
              text_info.text_runs[2].typeface_runs[0].typeface);
    EXPECT_NE(text_info.text_runs[0].typeface_runs[0].typeface,
              text_info.text_runs[1].typeface_runs[0].typeface);
    EXPECT_EQ(text_info.text_runs[0].location.y(),
              text_info.text_runs[1].location.y());
    EXPECT_EQ(text_info.text_runs[1].location.y(),
              text_info.text_runs[2].location.y());
  }
}

TEST_F(HTMLTextAreaElementTest, GetTextInfoEmpty) {
  SetBodyContent("<textarea id=test></textarea>");
  HTMLTextAreaElement& textarea = TestElement();

  EXPECT_TRUE(textarea.GetTextInfo().text_runs.empty());
}

TEST_F(HTMLTextAreaElementTest, GetTextInfoSoftWrap) {
  LoadAhem();

  // The textarea can contain four letters in each of lines.
  SetBodyContent(R"HTML(
    <textarea id=test
              style="font:10px Ahem; width:40px; height:200px;"></textarea>
  )HTML");
  HTMLTextAreaElement& textarea = TestElement();
  RunDocumentLifecycle();
  EXPECT_TRUE(textarea.GetTextInfo().text_runs.empty());

  textarea.SetValue("12\n\n345678");
  RunDocumentLifecycle();
  EXPECT_THAT(textarea.GetTextInfo().text_runs,
              ElementsAre(TextRunIs({"12"}, gfx::RectF(0, 0, 20, 10)),
                          TextRunIs({"3456"}, gfx::RectF(0, 20, 40, 10)),
                          TextRunIs({"78"}, gfx::RectF(0, 30, 20, 10))));

  textarea.SetValue("1234567890\n");
  RunDocumentLifecycle();
  EXPECT_THAT(textarea.GetTextInfo().text_runs,
              ElementsAre(TextRunIs({"1234"}, gfx::RectF(0, 0, 40, 10)),
                          TextRunIs({"5678"}, gfx::RectF(0, 10, 40, 10)),
                          TextRunIs({"90"}, gfx::RectF(0, 20, 20, 10))));

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
  EXPECT_THAT(textarea.GetTextInfo().text_runs,
              ElementsAre(TextRunIs({"12"}, gfx::RectF(0, 0, 20, 10)),
                          TextRunIs({"34"}, gfx::RectF(20, 0, 20, 10)),
                          TextRunIs({"5678"}, gfx::RectF(0, 10, 40, 10)),
                          TextRunIs({"90"}, gfx::RectF(0, 20, 20, 10))));
}

TEST_F(HTMLTextAreaElementTest, GetTextInfoLayoutAlign) {
  LoadAhem();

  // The textarea can contain four letters in each of lines.
  SetBodyContent(R"HTML(
    <textarea id=test
              style="font:10px Ahem; width:40px; height:200px; text-align:right;"></textarea> )HTML");
  HTMLTextAreaElement& textarea = TestElement();
  RunDocumentLifecycle();
  EXPECT_TRUE(textarea.GetTextInfo().text_runs.empty());

  textarea.SetValue("12\n\n345678");
  RunDocumentLifecycle();
  EXPECT_THAT(textarea.GetTextInfo().text_runs,
              ElementsAre(TextRunIs({"12"}, gfx::RectF(20, 0, 20, 10)),
                          TextRunIs({"3456"}, gfx::RectF(0, 20, 40, 10)),
                          TextRunIs({"78"}, gfx::RectF(20, 30, 20, 10))));

  textarea.SetValue("1234567890\n");
  RunDocumentLifecycle();
  EXPECT_THAT(textarea.GetTextInfo().text_runs,
              ElementsAre(TextRunIs({"1234"}, gfx::RectF(0, 0, 40, 10)),
                          TextRunIs({"5678"}, gfx::RectF(0, 10, 40, 10)),
                          TextRunIs({"90"}, gfx::RectF(20, 20, 20, 10))));

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
  EXPECT_THAT(textarea.GetTextInfo().text_runs,
              ElementsAre(TextRunIs({"12"}, gfx::RectF(0, 0, 20, 10)),
                          TextRunIs({"34"}, gfx::RectF(20, 0, 20, 10)),
                          TextRunIs({"5678"}, gfx::RectF(0, 10, 40, 10)),
                          TextRunIs({"90"}, gfx::RectF(20, 20, 20, 10))));
}

TEST_F(HTMLTextAreaElementTest, HeuristicCustomPasswordDetectionCSS) {
  SetBodyContent("<textarea id=test>abc</textarea>");
  auto& textarea = TestElement();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(textarea.HasBeenHeuristicCustomPasswordCSS());

  // Applying -webkit-text-security should trigger detection.
  textarea.setAttribute(html_names::kStyleAttr,
                        AtomicString("-webkit-text-security: disc;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(textarea.HasBeenHeuristicCustomPasswordCSS());
}

TEST_F(HTMLTextAreaElementTest, HeuristicCustomPasswordDetectionJS) {
  SetBodyContent("<textarea id=test></textarea>");
  auto& textarea = TestElement();

  // Programmatic value change.
  textarea.SetValue("****a");
  EXPECT_TRUE(textarea.HasBeenHeuristicCustomPasswordJS());
}

TEST_F(HTMLTextAreaElementTest, HeuristicCustomPasswordDetectionJSBetweenTag) {
  SetBodyContent("<textarea id=test>****a</textarea>");
  auto& textarea = TestElement();

  EXPECT_TRUE(textarea.HasBeenHeuristicCustomPasswordJS());
}

// crbug.com/506163510
TEST_F(HTMLTextAreaElementTest, AutofillPreviewScrollTopLeak) {
  LoadAhem();
  SetBodyContent(R"HTML(
    <textarea id=test style="width: 100px; height: 30px; letter-spacing: 2000px; overflow: auto;"></textarea>
  )HTML");
  HTMLTextAreaElement& textarea = TestElement();
  RunDocumentLifecycle();

  // Set suggested value (simulate autofill preview)
  textarea.SetSuggestedValue("XXXXXXXXXX");
  RunDocumentLifecycle();

  textarea.setScrollTop(1e9);
  double preview_scroll_top = static_cast<Element&>(textarea).scrollTop();
  textarea.setScrollTop(0);

  // The preview value should NOT leak via scrollTop.
  EXPECT_EQ(preview_scroll_top, 0);

  // Verify it does not scale with length
  textarea.SetSuggestedValue("XXXXXXXXXXXXXXXXXXXX");
  RunDocumentLifecycle();

  textarea.setScrollTop(1e9);
  double preview_scroll_top_2 = static_cast<Element&>(textarea).scrollTop();
  textarea.setScrollTop(0);

  EXPECT_EQ(preview_scroll_top_2, 0);
}

}  // namespace blink
