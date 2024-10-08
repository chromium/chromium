// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_text.h"

#include <numeric>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node_data.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"

namespace blink {

using testing::ElementsAre;

namespace {

class LayoutTextTest : public RenderingTest {
 public:
  void SetBasicBody(const char* message) {
    SetBodyInnerHTML(String::Format(
        "<div id='target' style='font-size: 10px;'>%s</div>", message));
  }

  void SetAhemBody(const char* message, const unsigned width) {
    SetBodyInnerHTML(String::Format(
        "<div id='target' style='font: 10px Ahem; width: %uem'>%s</div>", width,
        message));
  }

  LayoutText* GetLayoutTextById(const char* id) {
    return To<LayoutText>(GetLayoutObjectByElementId(id)->SlowFirstChild());
  }

  LayoutText* GetBasicText() { return GetLayoutTextById("target"); }

  void SetSelectionAndUpdateLayoutSelection(const std::string& selection_text) {
    const SelectionInDOMTree selection =
        SelectionSample::SetSelectionText(GetDocument().body(), selection_text);
    UpdateAllLifecyclePhasesForTest();
    Selection().SetSelection(selection, SetSelectionOptions());
    Selection().CommitAppearanceIfNeeded();
  }

  const LayoutText* FindFirstLayoutText() {
    for (const Node& node :
         NodeTraversal::DescendantsOf(*GetDocument().body())) {
      if (node.GetLayoutObject() && node.GetLayoutObject()->IsText())
        return To<LayoutText>(node.GetLayoutObject());
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  PhysicalRect GetSelectionRectFor(const std::string& selection_text) {
    std::stringstream stream;
    stream << "<div style='font: 10px/10px Ahem;'>" << selection_text
           << "</div>";
    SetSelectionAndUpdateLayoutSelection(stream.str());
    const Node* target = GetElementById("target");
    const LayoutObject* layout_object =
        target ? target->GetLayoutObject() : FindFirstLayoutText();
    return layout_object->LocalSelectionVisualRect();
  }

  std::string GetSnapCode(const LayoutText& layout_text,
                          const std::string& caret_text) {
    return GetSnapCode(layout_text,
                       static_cast<unsigned>(caret_text.find('|')));
  }

  std::string GetSnapCode(const char* id, const std::string& caret_text) {
    return GetSnapCode(*GetLayoutTextById(id), caret_text);
  }

  std::string GetSnapCode(const std::string& caret_text) {
    return GetSnapCode(*GetBasicText(), caret_text);
  }

  std::string GetSnapCode(const LayoutText& layout_text, unsigned offset) {
    std::string result(3, '_');
    // Note:: |IsBeforeNonCollapsedCharacter()| and |ContainsCaretOffset()|
    // accept out-of-bound offset but |IsAfterNonCollapsedCharacter()| doesn't.
    result[0] = layout_text.IsBeforeNonCollapsedCharacter(offset) ? 'B' : '-';
    result[1] = layout_text.ContainsCaretOffset(offset) ? 'C' : '-';
    if (offset <= layout_text.TransformedTextLength()) {
      result[2] = layout_text.IsAfterNonCollapsedCharacter(offset) ? 'A' : '-';
    }
    return result;
  }
  static constexpr unsigned kIncludeSnappedWidth = 1;

  std::string GetItemsAsString(const LayoutText& layout_text,
                               int num_glyphs = 0,
                               unsigned flags = 0) {
    if (layout_text.NeedsCollectInlines()) {
      return "LayoutText has NeedsCollectInlines";
    }
    if (!layout_text.HasValidInlineItems()) {
      return "No valid inline items in LayoutText";
    }
    const LayoutBlockFlow& block_flow = *layout_text.FragmentItemsContainer();
    if (block_flow.NeedsCollectInlines()) {
      return "LayoutBlockFlow has NeedsCollectInlines";
    }
    const InlineNodeData& data = *block_flow.GetInlineNodeData();
    std::ostringstream stream;
    for (const InlineItem& item : data.items) {
      if (item.Type() != InlineItem::kText) {
        continue;
      }
      if (item.GetLayoutObject() == layout_text) {
        stream << "*";
      }
      stream << "{'"
             << data.text_content.Substring(item.StartOffset(), item.Length())
                    .Utf8()
             << "'";
      if (const auto* shape_result = item.TextShapeResult()) {
        stream << ", ShapeResult=" << shape_result->StartIndex() << "+"
               << shape_result->NumCharacters();
#if BUILDFLAG(IS_WIN)
        if (shape_result->NumCharacters() != shape_result->NumGlyphs()) {
          stream << " #glyphs=" << shape_result->NumGlyphs();
        }
#else
        // Note: |num_glyphs| depends on installed font, we check only for
        // Windows because most of failures are reported on Windows.
        if (num_glyphs) {
          stream << " #glyphs=" << num_glyphs;
        }
#endif
        if (flags & kIncludeSnappedWidth) {
          stream << " width=" << shape_result->SnappedWidth();
        }
      }
      stream << "}" << std::endl;
    }
    return stream.str();
  }

  unsigned CountNumberOfGlyphs(const LayoutText& layout_text) {
    auto* const items = layout_text.GetInlineItems();
    return std::accumulate(items->begin(), items->end(), 0u,
                           [](unsigned sum, const InlineItem& item) {
                             return sum + item.TextShapeResult()->NumGlyphs();
                           });
  }
};

}  // namespace

#if BUILDFLAG(IS_WIN)
TEST_F(LayoutTextTest, PrewarmFamily) {
  test::ScopedTestFontPrewarmer prewarmer;
  SetBodyInnerHTML(R"HTML(
    <style>
    #container { font-family: testfont; }
    </style>
    <div id="container">text</div>
  )HTML");
  EXPECT_THAT(prewarmer.PrewarmedFamilyNames(), ElementsAre("testfont"));
  LayoutObject* container = GetLayoutObjectByElementId("container");
  EXPECT_TRUE(container->StyleRef()
                  .GetFont()
                  .GetFontDescription()
                  .Family()
                  .IsPrewarmed());
}

// Test `@font-face` fonts are NOT prewarmed.
TEST_F(LayoutTextTest, PrewarmFontFace) {
  test::ScopedTestFontPrewarmer prewarmer;
  SetBodyInnerHTML(R"HTML(
    <style>
    @font-face {
      font-family: testfont;
      src: local(Arial);
    }
    #container { font-family: testfont; }
    </style>
    <div id="container">text</div>
  )HTML");
  EXPECT_THAT(prewarmer.PrewarmedFamilyNames(), ElementsAre());
  LayoutObject* container = GetLayoutObjectByElementId("container");
  EXPECT_FALSE(container->StyleRef()
                   .GetFont()
                   .GetFontDescription()
                   .Family()
                   .IsPrewarmed());
}

TEST_F(LayoutTextTest, PrewarmGenericFamily) {
  test::ScopedTestFontPrewarmer prewarmer;
  SetBodyInnerHTML(R"HTML(
    <style>
    #container { font-family: serif; }
    </style>
    <div id="container">text</div>
  )HTML");
  // No prewarms because |GenericFontFamilySettings| is empty.
  EXPECT_THAT(prewarmer.PrewarmedFamilyNames(), ElementsAre());
  LayoutObject* container = GetLayoutObjectByElementId("container");
  EXPECT_TRUE(container->StyleRef()
                  .GetFont()
                  .GetFontDescription()
                  .Family()
                  .IsPrewarmed());
}
#endif

struct OffsetMappingTestData {
  const char* text;
  unsigned dom_start;
  unsigned dom_end;
  bool success;
  unsigned text_start;
  unsigned text_end;
} offset_mapping_test_data[] = {
    {"<div id=target> a  b  </div>", 0, 1, true, 0, 0},
    {"<div id=target> a  b  </div>", 1, 2, true, 0, 1},
    {"<div id=target> a  b  </div>", 2, 3, true, 1, 2},
    {"<div id=target> a  b  </div>", 2, 4, true, 1, 2},
    {"<div id=target> a  b  </div>", 2, 5, true, 1, 3},
    {"<div id=target> a  b  </div>", 3, 4, true, 2, 2},
    {"<div id=target> a  b  </div>", 3, 5, true, 2, 3},
    {"<div id=target> a  b  </div>", 5, 6, true, 3, 3},
    {"<div id=target> a  b  </div>", 5, 7, true, 3, 3},
    {"<div id=target> a  b  </div>", 6, 7, true, 3, 3},
    {"<div>a <span id=target> </span>b</div>", 0, 1, false, 0, 1}};

std::ostream& operator<<(std::ostream& out, OffsetMappingTestData data) {
  return out << "\"" << data.text << "\" " << data.dom_start << ","
             << data.dom_end << " => " << (data.success ? "true " : "false ")
             << data.text_start << "," << data.text_end;
}

class MapDOMOffsetToTextContentOffset
    : public LayoutTextTest,
      public testing::WithParamInterface<OffsetMappingTestData> {};

INSTANTIATE_TEST_SUITE_P(LayoutTextTest,
                         MapDOMOffsetToTextContentOffset,
                         testing::ValuesIn(offset_mapping_test_data));

TEST_P(MapDOMOffsetToTextContentOffset, Basic) {
  const auto data = GetParam();
  SetBodyInnerHTML(data.text);
  LayoutText* layout_text = GetBasicText();
  const OffsetMapping* mapping = layout_text->GetOffsetMapping();
  ASSERT_TRUE(mapping);
  unsigned start = data.dom_start;
  unsigned end = data.dom_end;
  bool success =
      layout_text->MapDOMOffsetToTextContentOffset(*mapping, &start, &end);
  EXPECT_EQ(data.success, success);
  if (success) {
    EXPECT_EQ(data.text_start, start);
    EXPECT_EQ(data.text_end, end);
  }
}

TEST_F(LayoutTextTest, CharacterAfterWhitespaceCollapsing) {
  SetBodyInnerHTML("a<span id=target> b </span>");
  LayoutText* layout_text = GetLayoutTextById("target");
  EXPECT_EQ(' ', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ('b', layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("a <span id=target> b </span>");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ('b', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ('b', layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("a<span id=target> </span>b");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ(' ', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(' ', layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("a <span id=target> </span>b");
  layout_text = GetLayoutTextById("target");
  DCHECK(!layout_text->HasInlineFragments());
  EXPECT_EQ(0, layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(0, layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML(
      "<span style='white-space: pre'>a <span id=target> </span>b</span>");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ(' ', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(' ', layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("<span id=target>Hello </span> <span>world</span>");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ('H', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(' ', layout_text->LastCharacterAfterWhitespaceCollapsing());
  layout_text =
      To<LayoutText>(GetLayoutObjectByElementId("target")->NextSibling());
  DCHECK(!layout_text->HasInlineFragments());
  EXPECT_EQ(0, layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(0, layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("<b id=target>&#x1F34C;_&#x1F34D;</b>");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ(0x1F34C, layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(0x1F34D, layout_text->LastCharacterAfterWhitespaceCollapsing());
}

TEST_F(LayoutTextTest, CaretMinMaxOffset) {
  SetBasicBody("foo");
  EXPECT_EQ(0, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(3, GetBasicText()->CaretMaxOffset());

  SetBasicBody("  foo");
  EXPECT_EQ(2, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(5, GetBasicText()->CaretMaxOffset());

  SetBasicBody("foo  ");
  EXPECT_EQ(0, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(3, GetBasicText()->CaretMaxOffset());

  SetBasicBody(" foo  ");
  EXPECT_EQ(1, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(4, GetBasicText()->CaretMaxOffset());
}

TEST_F(LayoutTextTest, ResolvedTextLength) {
  SetBasicBody("foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("  foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody(" foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());
}

TEST_F(LayoutTextTest, ContainsCaretOffset) {
  // This test records the behavior introduced in crrev.com/e3eb4e
  SetBasicBody(" foo   bar ");
  // text_content = "foo bar"
  // offset mapping unit:
  //  [0] = C DOM:0-1 TC:0-0
  //  [1] = I DOM:1-5 TC:0-4 "foo "
  //  [2] = C DOM:5-7 TC:4-4
  //  [3] = I DOM:7-10 TC:4-7 "bar"
  //  [4] = C DOM:10-11 TC:7-7
  EXPECT_EQ("---", GetSnapCode("| foo   bar "));
  EXPECT_EQ("BC-", GetSnapCode(" |foo   bar "));
  EXPECT_EQ("BCA", GetSnapCode(" f|oo   bar "));
  EXPECT_EQ("BCA", GetSnapCode(" fo|o   bar "));
  EXPECT_EQ("BCA", GetSnapCode(" foo|   bar "));
  EXPECT_EQ("-CA", GetSnapCode(" foo |  bar "));
  EXPECT_EQ("---", GetSnapCode(" foo  | bar "));
  EXPECT_EQ("BC-", GetSnapCode(" foo   |bar "));
  EXPECT_EQ("BCA", GetSnapCode(" foo   b|ar "));
  EXPECT_EQ("BCA", GetSnapCode(" foo   ba|r "));
  EXPECT_EQ("-CA", GetSnapCode(" foo   bar| "));
  EXPECT_EQ("---", GetSnapCode(" foo   bar |"));
  EXPECT_EQ("--_", GetSnapCode(*GetBasicText(), 12));  // out of range
}

TEST_F(LayoutTextTest, ContainsCaretOffsetInPre) {
  // These tests record the behavior introduced in crrev.com/e3eb4e
  InsertStyleElement("#target {white-space: pre; }");

  SetBasicBody("foo   bar");
  EXPECT_EQ("BC-", GetSnapCode("|foo   bar"));
  EXPECT_EQ("BCA", GetSnapCode("f|oo   bar"));
  EXPECT_EQ("BCA", GetSnapCode("fo|o   bar"));
  EXPECT_EQ("BCA", GetSnapCode("foo|   bar"));
  EXPECT_EQ("BCA", GetSnapCode("foo |  bar"));
  EXPECT_EQ("BCA", GetSnapCode("foo  | bar"));
  EXPECT_EQ("BCA", GetSnapCode("foo   |bar"));
  EXPECT_EQ("BCA", GetSnapCode("foo   b|ar"));
  EXPECT_EQ("BCA", GetSnapCode("foo   ba|r"));
  EXPECT_EQ("-CA", GetSnapCode("foo   bar|"));

  SetBasicBody("abc\n");
  // text_content = "abc\n"
  // offset mapping unit:
  //  [0] I DOM:0-4 TC:0-4 "abc\n"
  EXPECT_EQ("BC-", GetSnapCode("|abc\n"));
  EXPECT_EQ("BCA", GetSnapCode("a|bc\n"));
  EXPECT_EQ("BCA", GetSnapCode("ab|c\n"));
  EXPECT_EQ("BCA", GetSnapCode("abc|\n"));
  EXPECT_EQ("--A", GetSnapCode("abc\n|"));

  SetBasicBody("foo\nbar");
  EXPECT_EQ("BC-", GetSnapCode("|foo\nbar"));
  EXPECT_EQ("BCA", GetSnapCode("f|oo\nbar"));
  EXPECT_EQ("BCA", GetSnapCode("fo|o\nbar"));
  EXPECT_EQ("BCA", GetSnapCode("foo|\nbar"));
  EXPECT_EQ("BCA", GetSnapCode("foo\n|bar"));
  EXPECT_EQ("BCA", GetSnapCode("foo\nb|ar"));
  EXPECT_EQ("BCA", GetSnapCode("foo\nba|r"));
  EXPECT_EQ("-CA", GetSnapCode("foo\nbar|"));
}

TEST_F(LayoutTextTest, ContainsCaretOffsetInPreLine) {
  InsertStyleElement("#target {white-space: pre-line; }");

  SetBasicBody("ab \n cd");
  // text_content = "ab\ncd"
  // offset mapping unit:
  //  [0] I DOM:0-2 TC:0-2 "ab"
  //  [1] C DOM:2-3 TC:2-2
  //  [2] I DOM:3-4 TC:2-3 "\n"
  //  [3] C DOM:4-5 TC:3-3
  //  [4] I DOM:5-7 TC:3-5 "cd"
  EXPECT_EQ("BC-", GetSnapCode("|ab \n cd"));
  EXPECT_EQ("BCA", GetSnapCode("a|b \n cd"));
  // Before collapsed trailing space.
  EXPECT_EQ("-CA", GetSnapCode("ab| \n cd"));
  // After first trailing space.
  EXPECT_EQ("BC-", GetSnapCode("ab |\n cd"));
  // Before collapsed leading space.
  EXPECT_EQ("--A", GetSnapCode("ab \n| cd"));
  // After collapsed leading space.
  EXPECT_EQ("BC-", GetSnapCode("ab \n |cd"));

  SetBasicBody("ab  \n  cd");
  // text_content = "ab\ncd"
  // offset mapping unit:
  //  [0] I DOM:0-2 TC:0-2 "ab"
  //  [1] C DOM:2-4 TC:2-2
  //  [2] I DOM:4-5 TC:2-3 "\n"
  //  [3] C DOM:5-7 TC:3-3
  //  [4] I DOM:7-9 TC:3-5 "cd"
  EXPECT_EQ("BC-", GetSnapCode("|ab  \n  cd"));
  EXPECT_EQ("BCA", GetSnapCode("a|b  \n  cd"));
  // Before collapsed trailing space.
  EXPECT_EQ("-CA", GetSnapCode("ab|  \n  cd"));
  // After first trailing space.
  EXPECT_EQ("---", GetSnapCode("ab | \n  cd"));
  // After collapsed trailing space.
  EXPECT_EQ("BC-", GetSnapCode("ab  |\n  cd"));
  // Before collapsed leading space.
  EXPECT_EQ("--A", GetSnapCode("ab  \n|  cd"));
  // After collapsed leading space.
  EXPECT_EQ("---", GetSnapCode("ab  \n | cd"));
  EXPECT_EQ("BC-", GetSnapCode("ab  \n  |cd"));
  EXPECT_EQ("BCA", GetSnapCode("ab  \n  c|d"));
  EXPECT_EQ("-CA", GetSnapCode("ab  \n  cd|"));

  SetBasicBody("a\n\nb");
  EXPECT_EQ("BC-", GetSnapCode("|a\n\nb"));
  EXPECT_EQ("BCA", GetSnapCode("a|\n\nb"));
  EXPECT_EQ("BCA", GetSnapCode("a\n|\nb"));
  EXPECT_EQ("BCA", GetSnapCode("a\n\n|b"));
  EXPECT_EQ("-CA", GetSnapCode("a\n\nb|"));

  SetBasicBody("a \n \n b");
  // text_content = "a\n\nb"
  // offset mapping unit:
  //  [0] = I DOM:0-1 TC:0-1 "a"
  //  [1] = C DOM:1-2 TC:1-1
  //  [2] = I DOM:2-3 TC:1-2 "\n"
  //  [3] = C DOM:3-4 TC:2-2
  //  [4] = I DOM:4-5 TC:2-3 "\n"
  //  [5] = C DOM:5-6 TC:3-3
  //  [6] = I DOM:6-7 TC:3-4 "b"
  EXPECT_EQ("BC-", GetSnapCode("|a \n \n b"));
  // Before collapsed trailing space.
  EXPECT_EQ("-CA", GetSnapCode("a| \n \n b"));
  // After first trailing space.
  EXPECT_EQ("BC-", GetSnapCode("a |\n \n b"));
  // Before leading collapsed space.
  EXPECT_EQ("--A", GetSnapCode("a \n| \n b"));
  // After first trailing space.
  EXPECT_EQ("BC-", GetSnapCode("a \n |\n b"));
  // Before collapsed leading space.
  EXPECT_EQ("--A", GetSnapCode("a \n \n| b"));
  // After collapsed leading space.
  EXPECT_EQ("BC-", GetSnapCode("a \n \n |b"));
  EXPECT_EQ("-CA", GetSnapCode("a \n \n b|"));

  SetBasicBody("a \n  \n b");
  // text_content = "a\n\nb"
  // offset mapping unit:
  //  [0] = I DOM:0-1 TC:0-1 "a"
  //  [1] = C DOM:1-2 TC:1-1
  //  [2] = I DOM:2-3 TC:1-2 "\n"
  //  [3] = C DOM:3-5 TC:2-2
  //  [4] = I DOM:5-6 TC:2-3 "\n"
  //  [5] = C DOM:6-7 TC:3-3
  //  [6] = I DOM:7-8 TC:3-4 "b"
  EXPECT_EQ("BC-", GetSnapCode("|a \n  \n b"));
  // Before collapsed trailing space.
  EXPECT_EQ("-CA", GetSnapCode("a| \n  \n b"));
  // After first trailing space.
  EXPECT_EQ("BC-", GetSnapCode("a |\n  \n b"));
  // Before collapsed leading space.
  EXPECT_EQ("--A", GetSnapCode("a \n|  \n b"));
  // After first trailing and in leading space.
  EXPECT_EQ("---", GetSnapCode("a \n | \n b"));
  EXPECT_EQ("BC-", GetSnapCode("a \n  |\n b"));
  // before collapsed leading space.
  EXPECT_EQ("--A", GetSnapCode("a \n  \n| b"));
  // After collapsed leading space.
  EXPECT_EQ("BC-", GetSnapCode("a \n  \n |b"));
  EXPECT_EQ("-CA", GetSnapCode("a \n  \n b|"));
}

TEST_F(LayoutTextTest, ContainsCaretOffsetWithTrailingSpace) {
  SetBodyInnerHTML("<div id=target>ab<br>cd</div>");
  const auto& text_ab = *GetLayoutTextById("target");
  const auto& layout_br = *To<LayoutText>(text_ab.NextSibling());
  const auto& text_cd = *To<LayoutText>(layout_br.NextSibling());

  EXPECT_EQ("BC-", GetSnapCode(text_ab, "|ab<br>"));
  EXPECT_EQ("BCA", GetSnapCode(text_ab, "a|b<br>"));
  EXPECT_EQ("-CA", GetSnapCode(text_ab, "ab|<br>"));
  EXPECT_EQ("BC-", GetSnapCode(layout_br, 0));
  EXPECT_EQ("--A", GetSnapCode(layout_br, 1));
  EXPECT_EQ("BC-", GetSnapCode(text_cd, "|cd"));
  EXPECT_EQ("BCA", GetSnapCode(text_cd, "c|d"));
  EXPECT_EQ("-CA", GetSnapCode(text_cd, "cd|"));
}

TEST_F(LayoutTextTest, ContainsCaretOffsetWithTrailingSpace1) {
  SetBodyInnerHTML("<div id=target>ab <br> cd</div>");
  const auto& text_ab = *GetLayoutTextById("target");
  const auto& layout_br = *To<LayoutText>(text_ab.NextSibling());
  const auto& text_cd = *To<LayoutText>(layout_br.NextSibling());

  // text_content = "ab\ncd"
  // offset mapping unit:
  //  [0] I DOM:0-2 TC:0-2 "ab"
  //  [1] C DOM:2-3 TC:2-2
  //  [2] I DOM:0-1 TC:2-3 "\n" <br>
  //  [3] C DOM:0-1 TC:3-3
  //  [4] I DOM:1-3 TC:3-5 "cd"
  EXPECT_EQ("BC-", GetSnapCode(text_ab, "|ab <br>"));
  EXPECT_EQ("BCA", GetSnapCode(text_ab, "a|b <br>"));
  // Before after first trailing space.
  EXPECT_EQ("-CA", GetSnapCode(text_ab, "ab| <br>"));
  // After first trailing space.
  EXPECT_EQ("---", GetSnapCode(text_ab, "ab |<br>"));
  EXPECT_EQ("BC-", GetSnapCode(layout_br, 0));
  EXPECT_EQ("--A", GetSnapCode(layout_br, 1));
  EXPECT_EQ("---", GetSnapCode(text_cd, "| cd"));
  EXPECT_EQ("BC-", GetSnapCode(text_cd, " |cd"));
  EXPECT_EQ("BCA", GetSnapCode(text_cd, " c|d"));
  EXPECT_EQ("-CA", GetSnapCode(text_cd, " cd|"));
}

TEST_F(LayoutTextTest, ContainsCaretOffsetWithTrailingSpace2) {
  SetBodyInnerHTML("<div id=target>ab  <br>  cd</div>");
  const auto& text_ab = *GetLayoutTextById("target");
  const auto& layout_br = *To<LayoutText>(text_ab.NextSibling());
  const auto& text_cd = *To<LayoutText>(layout_br.NextSibling());

  // text_content = "ab\ncd"
  // offset mapping unit:
  //  [0] I DOM:0-2 TC:0-2 "ab"
  //  [1] C DOM:2-4 TC:2-2
  //  [2] I DOM:0-1 TC:2-3 "\n" <br>
  //  [3] C DOM:0-2 TC:3-3
  //  [4] I DOM:2-4 TC:3-5 "cd"
  EXPECT_EQ("BC-", GetSnapCode(text_ab, "|ab  <br>"));
  EXPECT_EQ("BCA", GetSnapCode(text_ab, "a|b  <br>"));
  // After first trailing space.
  EXPECT_EQ("-CA", GetSnapCode(text_ab, "ab|  <br>"));
  // After first trailing space.
  EXPECT_EQ("---", GetSnapCode(text_ab, "ab | <br>"));
  EXPECT_EQ("---", GetSnapCode(text_ab, "ab  |<br>"));
  // Before <br>.
  EXPECT_EQ("BC-", GetSnapCode(layout_br, 0));
  // After <br>.
  EXPECT_EQ("--A", GetSnapCode(layout_br, 1));
  EXPECT_EQ("---", GetSnapCode(text_cd, "|  cd"));
  EXPECT_EQ("---", GetSnapCode(text_cd, " | cd"));
  EXPECT_EQ("BC-", GetSnapCode(text_cd, "  |cd"));
  EXPECT_EQ("BCA", GetSnapCode(text_cd, "  c|d"));
  EXPECT_EQ("-CA", GetSnapCode(text_cd, "  cd|"));
}

TEST_F(LayoutTextTest, ContainsCaretOffsetWithTrailingSpace3) {
  SetBodyInnerHTML("<div id=target>a<br>   <br>b<br></div>");
  const auto& text_a = *GetLayoutTextById("target");
  const auto& layout_br1 = *To<LayoutText>(text_a.NextSibling());
  const auto& text_space = *To<LayoutText>(layout_br1.NextSibling());
  EXPECT_EQ(1u, text_space.TransformedTextLength());
  const auto& layout_br2 = *To<LayoutText>(text_space.NextSibling());
  const auto& text_b = *To<LayoutText>(layout_br2.NextSibling());
  // Note: the last <br> doesn't have layout object.

  // text_content = "a\n \nb"
  // offset mapping unit:
  //  [0] I DOM:0-1 TC:0-1 "a"
  EXPECT_EQ("BC-", GetSnapCode(text_a, "|a<br>"));
  EXPECT_EQ("-CA", GetSnapCode(text_a, "a|<br>"));
  EXPECT_EQ("-CA", GetSnapCode(text_a, "a|<br>"));
  EXPECT_EQ("BC-", GetSnapCode(layout_br1, 0));
  EXPECT_EQ("--A", GetSnapCode(layout_br1, 1));
  EXPECT_EQ("BC-", GetSnapCode(text_space, 0));
  EXPECT_EQ("--A", GetSnapCode(text_space, 1));
  EXPECT_EQ("BC-", GetSnapCode(layout_br2, 0));
  EXPECT_EQ("-CA", GetSnapCode(layout_br2, 1));
  EXPECT_EQ("BC-", GetSnapCode(text_b, "|b<br>"));
  EXPECT_EQ("--A", GetSnapCode(text_b, "b|<br>"));
}

TEST_F(LayoutTextTest, GetTextBoxInfoWithCollapsedWhiteSpace) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>pre { font: 10px/1 Ahem; white-space: pre-line; }</style>
    <pre id=target> abc  def
    xyz   </pre>)HTML");
  const LayoutText& layout_text = *GetLayoutTextById("target");

  const auto& results = layout_text.GetTextBoxInfo();

  ASSERT_EQ(4u, results.size());

  EXPECT_EQ(1u, results[0].dom_start_offset);
  EXPECT_EQ(4u, results[0].dom_length);
  EXPECT_EQ(PhysicalRect(0, 0, 40, 10), results[0].local_rect);

  EXPECT_EQ(6u, results[1].dom_start_offset);
  EXPECT_EQ(3u, results[1].dom_length);
  EXPECT_EQ(PhysicalRect(40, 0, 30, 10), results[1].local_rect);

  EXPECT_EQ(9u, results[2].dom_start_offset);
  EXPECT_EQ(1u, results[2].dom_length);
  EXPECT_EQ(PhysicalRect(70, 0, 0, 10), results[2].local_rect);

  EXPECT_EQ(14u, results[3].dom_start_offset);
  EXPECT_EQ(3u, results[3].dom_length);
  EXPECT_EQ(PhysicalRect(0, 10, 30, 10), results[3].local_rect);
}

TEST_F(LayoutTextTest, GetTextBoxInfoWithGeneratedContent) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      div::before { content: '  a   bc'; }
      div::first-letter { font-weight: bold; }
      div { font: 10px/1 Ahem; }
    </style>
    <div id="target">XYZ</div>)HTML");
  const Element& target = *GetElementById("target");
  const Element& before =
      *GetElementById("target")->GetPseudoElement(kPseudoIdBefore);
  const auto& layout_text_xyz =
      *To<LayoutText>(target.firstChild()->GetLayoutObject());
  const auto& layout_text_remaining =
      To<LayoutText>(*before.GetLayoutObject()->SlowLastChild());
  const LayoutText& layout_text_first_letter =
      *layout_text_remaining.GetFirstLetterPart();

  auto boxes_xyz = layout_text_xyz.GetTextBoxInfo();
  EXPECT_EQ(1u, boxes_xyz.size());
  EXPECT_EQ(0u, boxes_xyz[0].dom_start_offset);
  EXPECT_EQ(3u, boxes_xyz[0].dom_length);
  EXPECT_EQ(PhysicalRect(40, 0, 30, 10), boxes_xyz[0].local_rect);

  auto boxes_first_letter = layout_text_first_letter.GetTextBoxInfo();
  EXPECT_EQ(1u, boxes_first_letter.size());
  EXPECT_EQ(2u, boxes_first_letter[0].dom_start_offset);
  EXPECT_EQ(1u, boxes_first_letter[0].dom_length);
  EXPECT_EQ(PhysicalRect(0, 0, 10, 10), boxes_first_letter[0].local_rect);

  auto boxes_remaining = layout_text_remaining.GetTextBoxInfo();
  EXPECT_EQ(2u, boxes_remaining.size());
  EXPECT_EQ(0u, boxes_remaining[0].dom_start_offset);
  EXPECT_EQ(1u, boxes_remaining[0].dom_length) << "two spaces to one space";
  EXPECT_EQ(PhysicalRect(10, 0, 10, 10), boxes_remaining[0].local_rect);
  EXPECT_EQ(3u, boxes_remaining[1].dom_start_offset);
  EXPECT_EQ(2u, boxes_remaining[1].dom_length);
  EXPECT_EQ(PhysicalRect(20, 0, 20, 10), boxes_remaining[1].local_rect);
}

// For http://crbug.com/985488
TEST_F(LayoutTextTest, GetTextBoxInfoWithHidden) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        font: 10px/1 Ahem;
        overflow-x: hidden;
        white-space: nowrap;
        width: 9ch;
      }
    </style>
    <div id="target">  abcde  fghij  </div>
  )HTML");
  const Element& target = *GetElementById("target");
  const LayoutText& layout_text =
      *To<Text>(target.firstChild())->GetLayoutObject();

  auto boxes = layout_text.GetTextBoxInfo();
  EXPECT_EQ(2u, boxes.size());

  EXPECT_EQ(2u, boxes[0].dom_start_offset);
  EXPECT_EQ(6u, boxes[0].dom_length);
  EXPECT_EQ(PhysicalRect(0, 0, 60, 10), boxes[0].local_rect);

  EXPECT_EQ(9u, boxes[1].dom_start_offset);
  EXPECT_EQ(5u, boxes[1].dom_length);
  EXPECT_EQ(PhysicalRect(60, 0, 50, 10), boxes[1].local_rect);
}

// For http://crbug.com/985488
TEST_F(LayoutTextTest, GetTextBoxInfoWithEllipsis) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        font: 10px/1 Ahem;
        overflow-x: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        width: 9ch;
      }
    </style>
    <div id="target">  abcde  fghij  </div>
  )HTML");
  const Element& target = *GetElementById("target");
  const LayoutText& layout_text =
      *To<Text>(target.firstChild())->GetLayoutObject();

  auto boxes = layout_text.GetTextBoxInfo();
  EXPECT_EQ(2u, boxes.size());

  EXPECT_EQ(2u, boxes[0].dom_start_offset);
  EXPECT_EQ(6u, boxes[0].dom_length);
  EXPECT_EQ(PhysicalRect(0, 0, 60, 10), boxes[0].local_rect);

  EXPECT_EQ(9u, boxes[1].dom_start_offset);
  EXPECT_EQ(5u, boxes[1].dom_length);
  EXPECT_EQ(PhysicalRect(60, 0, 50, 10), boxes[1].local_rect);
}

// For http://crbug.com/1003413
TEST_F(LayoutTextTest, GetTextBoxInfoWithEllipsisForPseudoAfter) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      #sample {
        box-sizing: border-box;
        font: 10px/1 Ahem;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        width: 5ch;
      }
      b::after { content: ","; }
    </style>
    <div id=sample><b id=target>abc</b><b>xyz</b></div>
  )HTML");
  const Element& target = *GetElementById("target");
  const Element& after = *target.GetPseudoElement(kPseudoIdAfter);
  // Set |layout_text| to "," in <pseudo::after>,</pseudo::after>
  const auto& layout_text =
      *To<LayoutText>(after.GetLayoutObject()->SlowFirstChild());

  auto boxes = layout_text.GetTextBoxInfo();
  EXPECT_EQ(1u, boxes.size());

  EXPECT_EQ(0u, boxes[0].dom_start_offset);
  EXPECT_EQ(1u, boxes[0].dom_length);
  EXPECT_EQ(PhysicalRect(30, 0, 10, 10), boxes[0].local_rect);
}

// Test the specialized code path in |PlainText| for when |!GetNode()|.
TEST_F(LayoutTextTest, PlainTextInPseudo) {
  SetBodyInnerHTML(String(R"HTML(
    <style>
    :root {
      font-family: monospace;
      font-size: 10px;
    }
    #before_parent::before {
      display: inline-block;
      width: 5ch;
      content: "123 456";
    }
    #before_parent_cjk::before {
      display: inline-block;
      width: 5ch;
      content: "123)HTML") +
                   String(u"\u4E00") + R"HTML(456";
    }
    </style>
    <div id="before_parent"></div>
    <div id="before_parent_cjk"></div>
  )HTML");

  const auto GetPlainText = [](const LayoutObject* parent) {
    const LayoutObject* before = parent->SlowFirstChild();
    EXPECT_TRUE(before->IsBeforeContent());
    const auto* before_text = To<LayoutText>(before->SlowFirstChild());
    EXPECT_FALSE(before_text->GetNode());
    return before_text->PlainText();
  };

  const LayoutObject* before_parent =
      GetLayoutObjectByElementId("before_parent");
  EXPECT_EQ("123 456", GetPlainText(before_parent));
  const LayoutObject* before_parent_cjk =
      GetLayoutObjectByElementId("before_parent_cjk");
  EXPECT_EQ(String(u"123\u4E00456"), GetPlainText(before_parent_cjk));
}

TEST_F(LayoutTextTest, IsBeforeAfterNonCollapsedCharacterNoLineWrap) {
  // Basic tests
  SetBasicBody("foo");
  EXPECT_EQ("BC-", GetSnapCode("|foo"));
  EXPECT_EQ("BCA", GetSnapCode("f|oo"));
  EXPECT_EQ("BCA", GetSnapCode("fo|o"));
  EXPECT_EQ("-CA", GetSnapCode("foo|"));

  // Consecutive spaces are collapsed into one
  SetBasicBody("f   bar");
  EXPECT_EQ("BC-", GetSnapCode("|f   bar"));
  EXPECT_EQ("BCA", GetSnapCode("f|   bar"));
  EXPECT_EQ("-CA", GetSnapCode("f |  bar"));
  EXPECT_EQ("---", GetSnapCode("f  | bar"));
  EXPECT_EQ("BC-", GetSnapCode("f   |bar"));
  EXPECT_EQ("BCA", GetSnapCode("f   b|ar"));
  EXPECT_EQ("BCA", GetSnapCode("f   ba|r"));
  EXPECT_EQ("-CA", GetSnapCode("f   bar|"));

  // Leading spaces in a block are collapsed
  SetBasicBody("  foo");
  EXPECT_EQ("---", GetSnapCode("|  foo"));
  EXPECT_EQ("---", GetSnapCode(" | foo"));
  EXPECT_EQ("BC-", GetSnapCode("  |foo"));
  EXPECT_EQ("BCA", GetSnapCode("  f|oo"));
  EXPECT_EQ("BCA", GetSnapCode("  fo|o"));
  EXPECT_EQ("-CA", GetSnapCode("  foo|"));

  // Trailing spaces in a block are collapsed
  SetBasicBody("foo  ");
  EXPECT_EQ("BC-", GetSnapCode("|foo  "));
  EXPECT_EQ("BCA", GetSnapCode("f|oo  "));
  EXPECT_EQ("BCA", GetSnapCode("fo|o  "));
  EXPECT_EQ("-CA", GetSnapCode("foo|  "));
  EXPECT_EQ("---", GetSnapCode("foo | "));
  EXPECT_EQ("---", GetSnapCode("foo  |"));

  // Non-collapsed space at node end
  SetBasicBody("foo <span>bar</span>");
  EXPECT_EQ("BC-", GetSnapCode("|foo "));
  EXPECT_EQ("BCA", GetSnapCode("f|oo "));
  EXPECT_EQ("BCA", GetSnapCode("fo|o "));
  EXPECT_EQ("BCA", GetSnapCode("foo| "));
  EXPECT_EQ("-CA", GetSnapCode("foo |"));

  // Non-collapsed space at node start
  SetBasicBody("foo<span id=bar> bar</span>");
  EXPECT_EQ("BC-", GetSnapCode("bar", "| bar"));
  EXPECT_EQ("BCA", GetSnapCode("bar", " |bar"));
  EXPECT_EQ("BCA", GetSnapCode("bar", " b|ar"));
  EXPECT_EQ("BCA", GetSnapCode("bar", " ba|r"));
  EXPECT_EQ("-CA", GetSnapCode("bar", " bar|"));

  // Consecutive spaces across nodes
  SetBasicBody("foo <span id=bar> bar</span>");
  // text_content = "foo bar"
  // [0] I DOM:0-4 TC:0-4 "foo "
  // [1] C DOM:0-1 TC:4-4 " bar"
  // [2] I DOM:1-4 TC:4-7 " bar"
  EXPECT_EQ("BC-", GetSnapCode("|foo "));
  EXPECT_EQ("BCA", GetSnapCode("f|oo "));
  EXPECT_EQ("BCA", GetSnapCode("fo|o "));
  EXPECT_EQ("BCA", GetSnapCode("foo| "));
  EXPECT_EQ("-CA", GetSnapCode("foo |"));
  EXPECT_EQ("---", GetSnapCode("bar", "| bar"));
  EXPECT_EQ("BC-", GetSnapCode("bar", " |bar"));
  EXPECT_EQ("BCA", GetSnapCode("bar", " b|ar"));
  EXPECT_EQ("BCA", GetSnapCode("bar", " ba|r"));
  EXPECT_EQ("-CA", GetSnapCode("bar", " bar|"));

  // Non-collapsed whitespace text node
  SetBasicBody("foo<span id=space> </span>bar");
  EXPECT_EQ("BC-", GetSnapCode("space", "| "));
  EXPECT_EQ("-CA", GetSnapCode("space", " |"));

  // Collapsed whitespace text node
  SetBasicBody("foo <span id=space> </span>bar");
  EXPECT_EQ("---", GetSnapCode("space", "| "));
  EXPECT_EQ("---", GetSnapCode("space", " |"));
}

TEST_F(LayoutTextTest, IsBeforeAfterNonCollapsedLineWrapSpace) {
  LoadAhem();

  // Note: Because we can place a caret before soft line wrap, "ab| cd",
  // |GetSnapCode()| should return "BC-" for both NG and legacy.

  // Line wrapping inside node
  SetAhemBody("ab  cd", 2);
  // text_content = "ab cd"
  // [0] I DOM:0-3 TC:0-3 "ab "
  // [1] C DOM:3-4 TC:3-3 " "
  // [2] I DOM:4-6 TC:3-5 "cd"
  EXPECT_EQ("BC-", GetSnapCode("|ab  cd"));
  EXPECT_EQ("BCA", GetSnapCode("a|b  cd"));
  EXPECT_EQ("BCA", GetSnapCode("ab|  cd"));
  // After soft line wrap.
  EXPECT_EQ("-CA", GetSnapCode("ab | cd"));
  EXPECT_EQ("BC-", GetSnapCode("ab  |cd"));
  EXPECT_EQ("BCA", GetSnapCode("ab  c|d"));
  EXPECT_EQ("-CA", GetSnapCode("ab  cd|"));

  // Line wrapping at node start
  // text_content = "xx"
  // [0] I DOM:0-2 TC:0-2 "xx"
  // [1] I DOM:0-1 TC:2-3 " "
  // [2] C DOM:1-2 TC:3-3 " "
  // [3] I DOM:2-3 TC:3-5 "xx"
  SetAhemBody("ab<span id=span>  cd</span>", 2);
  // Before soft line wrap.
  EXPECT_EQ("BC-", GetSnapCode("span", "|  cd"));
  // After soft line wrap.
  EXPECT_EQ("-CA", GetSnapCode("span", " | cd"));
  EXPECT_EQ("BC-", GetSnapCode("span", "  |cd"));
  EXPECT_EQ("BCA", GetSnapCode("span", "  c|d"));
  EXPECT_EQ("-CA", GetSnapCode("span", "  cd|"));

  // Line wrapping at node end
  SetAhemBody("ab  <span>cd</span>", 2);
  // text_content = "ab cd"
  // [0] I DOM:0-3 TC:0-3 "ab "
  // [1] C DOM:3-4 TC:3-3 " "
  // [2] I DOM:0-2 TC:3-5 "cd"
  EXPECT_EQ("BC-", GetSnapCode("|ab "));
  EXPECT_EQ("BCA", GetSnapCode("a|b "));
  // Before soft line wrap.
  EXPECT_EQ("BCA", GetSnapCode("ab|  "));
  // After soft line wrap.
  EXPECT_EQ("-CA", GetSnapCode("ab | "));
  EXPECT_EQ("---", GetSnapCode("ab  |"));

  // Entire node as line wrapping
  SetAhemBody("ab<span id=space>  </span>cd", 2);
  // text_content = "ab cd"
  // [0] I DOM:0-2 TC:0-2 "ab"
  // [1] I DOM:0-1 TC:2-3 " "
  // [2] C DOM:1-2 TC:3-3 " "
  // [3] I DOM:0-2 TC:3-5 "cd"

  // Before soft line wrap.
  EXPECT_EQ("BC-", GetSnapCode("space", "|  "));
  // After soft line wrap.
  EXPECT_EQ("-CA", GetSnapCode("space", " | "));
  EXPECT_EQ("---", GetSnapCode("space", "  |"));
}

TEST_F(LayoutTextTest, IsBeforeAfterNonCollapsedCharacterBR) {
  SetBasicBody("<br>");
  EXPECT_EQ("BC-", GetSnapCode(*GetBasicText(), 0));
  EXPECT_EQ("--A", GetSnapCode(*GetBasicText(), 1));
}

TEST_F(LayoutTextTest, AbsoluteQuads) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { margin: 0 }
    div {
      font: 10px/1 Ahem;
      width: 5em;
    }
    </style>
    <div>012<span id=target>345 67</span></div>
  )HTML");
  LayoutText* layout_text = GetLayoutTextById("target");
  Vector<gfx::QuadF> quads;
  layout_text->AbsoluteQuads(quads);
  EXPECT_THAT(quads,
              testing::ElementsAre(gfx::QuadF(gfx::RectF(30, 0, 30, 10)),
                                   gfx::QuadF(gfx::RectF(0, 10, 20, 10))));
}

TEST_F(LayoutTextTest, AbsoluteQuadsVRL) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { margin: 0 }
    div {
      font: 10px/1 Ahem;
      width: 10em;
      height: 5em;
      writing-mode: vertical-rl;
    }
    </style>
    <div>012<span id=target>345 67</span></div>
  )HTML");
  LayoutText* layout_text = GetLayoutTextById("target");
  Vector<gfx::QuadF> quads;
  layout_text->AbsoluteQuads(quads);
  EXPECT_THAT(quads,
              testing::ElementsAre(gfx::QuadF(gfx::RectF(90, 30, 10, 30)),
                                   gfx::QuadF(gfx::RectF(80, 0, 10, 20))));
}

TEST_F(LayoutTextTest, PhysicalLinesBoundingBox) {
  LoadAhem();
  SetBasicBody(
      "<style>"
      "div {"
      "  font-family:Ahem;"
      "  font-size: 13px;"
      "  line-height: 19px;"
      "  padding: 3px;"
      "}"
      "</style>"
      "<div id=div>"
      "  012"
      "  <span id=one>345</span>"
      "  <br>"
      "  <span style='padding: 20px'>"
      "    <span id=two style='padding: 5px'>678</span>"
      "  </span>"
      "</div>");
  // Layout NG Physical Fragment Tree
  // Box offset:3,3 size:778x44
  //   LineBox offset:3,3 size:91x19
  //     Text offset:0,3 size:52x13 start: 0 end: 4
  //     Box offset:52,3 size:39x13
  //       Text offset:0,0 size:39x13 start: 4 end: 7
  //       Text offset:91,3 size:0x13 start: 7 end: 8
  //   LineBox offset:3,22 size:89x19
  //     Box offset:0,-17 size:89x53
  //       Box offset:20,15 size:49x23
  //         Text offset:5,5 size:39x13 start: 8 end: 11
  const Element& div = *GetElementById("div");
  const Element& one = *GetElementById("one");
  const Element& two = *GetElementById("two");
  EXPECT_EQ(PhysicalRect(3, 6, 52, 13),
            To<LayoutText>(div.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(55, 6, 39, 13),
            To<LayoutText>(one.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(28, 25, 39, 13),
            To<LayoutText>(two.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
}

TEST_F(LayoutTextTest, PhysicalLinesBoundingBoxTextCombine) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 100px/130px Ahem; }"
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div>a<c id=target>01234</c>b</div>");
  const auto& target = *GetElementById("target");
  const auto& text_a = *To<Text>(target.previousSibling())->GetLayoutObject();
  const auto& text_01234 = *To<Text>(target.firstChild())->GetLayoutObject();
  const auto& text_b = *To<Text>(target.nextSibling())->GetLayoutObject();

  //   LayoutBlockFlow {HTML} at (0,0) size 800x600
  //     LayoutBlockFlow {BODY} at (8,8) size 784x584
  //       LayoutBlockFlow {DIV} at (0,0) size 130x300
  //         LayoutText {#text} at (15,0) size 100x100
  //           text run at (15,0) width 100: "a"
  //         LayoutInline {C} at (15,100) size 100x100
  //           LayoutTextCombine (anonymous) at (15,100) size 100x100
  //             LayoutText {#text} at (-5,0) size 110x100
  //               text run at (0,0) width 500: "01234"
  //         LayoutText {#text} at (15,200) size 100x100
  //           text run at (15,200) width 100: "b"
  //

  EXPECT_EQ(PhysicalRect(15, 0, 100, 100), text_a.PhysicalLinesBoundingBox());
  // Note: Width 110 comes from |100px * kTextCombineMargin| in
  // |LayoutTextCombine::DesiredWidth()|.
  EXPECT_EQ(PhysicalRect(-5, 0, 110, 100),
            text_01234.PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(15, 200, 100, 100), text_b.PhysicalLinesBoundingBox());
}

TEST_F(LayoutTextTest, PhysicalLinesBoundingBoxVerticalRL) {
  LoadAhem();
  SetBasicBody(R"HTML(
    <style>
    div {
      font-family:Ahem;
      font-size: 13px;
      line-height: 19px;
      padding: 3px;
      writing-mode: vertical-rl;
    }
    </style>
    <div id=div>
      012
      <span id=one>345</span>
      <br>
      <span style='padding: 20px'>
        <span id=two style='padding: 5px'>678</span>
      </span>
    </div>
  )HTML");
  // Similar to the previous test, with logical coordinates converted to
  // physical coordinates.
  const Element& div = *GetElementById("div");
  const Element& one = *GetElementById("one");
  const Element& two = *GetElementById("two");
  EXPECT_EQ(PhysicalRect(25, 3, 13, 52),
            To<LayoutText>(div.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(25, 55, 13, 39),
            To<LayoutText>(one.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(6, 28, 13, 39),
            To<LayoutText>(two.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
}

TEST_F(LayoutTextTest, WordBreakElement) {
  SetBasicBody("foo <wbr> bar");

  const Element* wbr = GetDocument().QuerySelector(AtomicString("wbr"));
  DCHECK(wbr->GetLayoutObject()->IsText());
  const auto* layout_wbr = To<LayoutText>(wbr->GetLayoutObject());

  EXPECT_EQ(0u, layout_wbr->ResolvedTextLength());
  EXPECT_EQ(0, layout_wbr->CaretMinOffset());
  EXPECT_EQ(0, layout_wbr->CaretMaxOffset());
}

TEST_F(LayoutTextTest, LocalSelectionRect) {
  LoadAhem();
  // TODO(yoichio): Fix LayoutNG incompatibility.
  EXPECT_EQ(PhysicalRect(10, 0, 50, 10), GetSelectionRectFor("f^oo ba|r"));
  EXPECT_EQ(PhysicalRect(0, 0, 40, 20),
            GetSelectionRectFor("<div style='width: 2em'>f^oo ba|r</div>"));
  EXPECT_EQ(PhysicalRect(30, 0, 10, 10),
            GetSelectionRectFor("foo^<br id='target'>|bar"));
  EXPECT_EQ(PhysicalRect(10, 0, 20, 10), GetSelectionRectFor("f^oo<br>b|ar"));
  EXPECT_EQ(PhysicalRect(10, 0, 30, 10),
            GetSelectionRectFor("<div>f^oo</div><div>b|ar</div>"));
  EXPECT_EQ(PhysicalRect(30, 0, 10, 10), GetSelectionRectFor("foo^ |bar"));
  EXPECT_EQ(PhysicalRect(0, 0, 0, 0), GetSelectionRectFor("^ |foo"));
  EXPECT_EQ(PhysicalRect(0, 0, 0, 0),
            GetSelectionRectFor("fo^o<wbr id='target'>ba|r"));
  EXPECT_EQ(
      PhysicalRect(0, 0, 10, 10),
      GetSelectionRectFor("<style>:first-letter { float: right}</style>^fo|o"));
  // Since we don't paint trimed white spaces on LayoutNG,  we don't need fix
  // this case.
  EXPECT_EQ(PhysicalRect(0, 0, 0, 0), GetSelectionRectFor("foo^ |"));
}

TEST_F(LayoutTextTest, LocalSelectionRectLineBreak) {
  LoadAhem();
  EXPECT_EQ(PhysicalRect(30, 0, 10, 10),
            GetSelectionRectFor("f^oo<br id='target'><br>ba|r"));
  EXPECT_EQ(PhysicalRect(0, 10, 10, 10),
            GetSelectionRectFor("f^oo<br><br id='target'>ba|r"));
}

TEST_F(LayoutTextTest, LocalSelectionRectLineBreakPre) {
  LoadAhem();
  EXPECT_EQ(
      PhysicalRect(30, 0, 10, 10),
      GetSelectionRectFor("<div style='white-space:pre;'>foo^\n|\nbar</div>"));
  EXPECT_EQ(
      PhysicalRect(0, 10, 10, 10),
      GetSelectionRectFor("<div style='white-space:pre;'>foo\n^\n|bar</div>"));
}

TEST_F(LayoutTextTest, LocalSelectionRectRTL) {
  LoadAhem();
  // TODO(yoichio) : Fix LastLogicalLeafIgnoringLineBreak so that 'foo' is the
  // last fragment.
  EXPECT_EQ(PhysicalRect(-10, 0, 30, 20),
            GetSelectionRectFor("<div style='width: 2em' dir=rtl>"
                                "f^oo ba|r baz</div>"));
  EXPECT_EQ(PhysicalRect(0, 0, 40, 20),
            GetSelectionRectFor("<div style='width: 2em' dir=ltr>"
                                "f^oo ba|r baz</div>"));
}

TEST_F(LayoutTextTest, LocalSelectionRectVertical) {
  LoadAhem();
  EXPECT_EQ(
      PhysicalRect(0, 0, 20, 40),
      GetSelectionRectFor("<div style='writing-mode: vertical-lr; height: 2em'>"
                          "f^oo ba|r baz</div>"));
  EXPECT_EQ(
      PhysicalRect(10, 0, 20, 40),
      GetSelectionRectFor("<div style='writing-mode: vertical-rl; height: 2em'>"
                          "f^oo ba|r baz</div>"));
}

TEST_F(LayoutTextTest, LocalSelectionRectVerticalRTL) {
  LoadAhem();
  // TODO(yoichio): Investigate diff (maybe soft line break treatment).
  EXPECT_EQ(PhysicalRect(0, -10, 20, 30),
            GetSelectionRectFor(
                "<div style='writing-mode: vertical-lr; height: 2em' dir=rtl>"
                "f^oo ba|r baz</div>"));
  EXPECT_EQ(PhysicalRect(10, -10, 20, 30),
            GetSelectionRectFor(
                "<div style='writing-mode: vertical-rl; height: 2em' dir=rtl>"
                "f^oo ba|r baz</div>"));
}

TEST_F(LayoutTextTest, LocalSelectionRectLineHeight) {
  LoadAhem();
  EXPECT_EQ(PhysicalRect(10, 0, 10, 50),
            GetSelectionRectFor("<div style='line-height: 50px; width:1em;'>"
                                "f^o|o bar baz</div>"));
  EXPECT_EQ(PhysicalRect(10, 50, 10, 50),
            GetSelectionRectFor("<div style='line-height: 50px; width:1em;'>"
                                "foo b^a|r baz</div>"));
  EXPECT_EQ(PhysicalRect(10, 100, 10, 50),
            GetSelectionRectFor("<div style='line-height: 50px; width:1em;'>"
                                "foo bar b^a|</div>"));
}

TEST_F(LayoutTextTest, LocalSelectionRectNegativeLeading) {
  LoadAhem();
  SetSelectionAndUpdateLayoutSelection(R"HTML(
    <div id="container" style="font: 10px/10px Ahem">
      ^
      <span id="span" style="display: inline-block; line-height: 1px">
        Text
      </span>
      |
    </div>
  )HTML");
  LayoutObject* span = GetLayoutObjectByElementId("span");
  LayoutObject* text = span->SlowFirstChild();
  EXPECT_EQ(PhysicalRect(0, -5, 40, 10), text->LocalSelectionVisualRect());
}

TEST_F(LayoutTextTest, LocalSelectionRectLineHeightVertical) {
  LoadAhem();
  EXPECT_EQ(PhysicalRect(0, 10, 50, 10),
            GetSelectionRectFor("<div style='line-height: 50px; height:1em; "
                                "writing-mode:vertical-lr'>"
                                "f^o|o bar baz</div>"));
  EXPECT_EQ(PhysicalRect(50, 10, 50, 10),
            GetSelectionRectFor("<div style='line-height: 50px; height:1em; "
                                "writing-mode:vertical-lr'>"
                                "foo b^a|r baz</div>"));
  EXPECT_EQ(PhysicalRect(100, 10, 50, 10),
            GetSelectionRectFor("<div style='line-height: 50px; height:1em; "
                                "writing-mode:vertical-lr'>"
                                "foo bar b^a|z</div>"));
}

TEST_F(LayoutTextTest, PositionForPointAtLeading) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body {
      margin: 0;
      font-size: 10px;
      line-height: 3;
      font-family: Ahem;
    }
    #container {
      width: 5ch;
    }
    </style>
    <div id="container">line1 line2</div>
  )HTML");
  LayoutObject* container = GetLayoutObjectByElementId("container");
  auto* text = To<LayoutText>(container->SlowFirstChild());
  // The 1st line is at {0, 0}x{50,30} and 2nd line is {0,30}x{50,30}, with
  // 10px half-leading, 10px text, and  10px half-leading. {10, 30} is the
  // middle of the two lines, at the half-leading.

  // line 1
  // Note: All |PositionForPoint()| should return "line1"[1].
  EXPECT_EQ(Position(text->GetNode(), 1),
            text->PositionForPoint({10, 0}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 1),
            text->PositionForPoint({10, 5}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 1),
            text->PositionForPoint({10, 10}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 1),
            text->PositionForPoint({10, 15}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 1),
            text->PositionForPoint({10, 20}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 1),
            text->PositionForPoint({10, 25}).GetPosition());
  // line 2
  EXPECT_EQ(Position(text->GetNode(), 7),
            text->PositionForPoint({10, 30}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 7),
            text->PositionForPoint({10, 35}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 7),
            text->PositionForPoint({10, 40}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 7),
            text->PositionForPoint({10, 45}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 7),
            text->PositionForPoint({10, 50}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 7),
            text->PositionForPoint({10, 55}).GetPosition());
}

// https://crbug.com/2654312
TEST_F(LayoutTextTest, FloatFirstLetterPlainText) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div::first-letter { float: left; }
    </style>
    <div id="target">Foo</div>
  )HTML");

  LayoutText* text =
      To<LayoutText>(GetElementById("target")->firstChild()->GetLayoutObject());
  EXPECT_EQ("Foo", text->PlainText());
}

TEST_F(LayoutTextTest, SetTextWithOffsetAppendBidi) {
  SetBodyInnerHTML(u"<div dir=rtl id=target>\u05D0\u05D1\u05BC\u05D2</div>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.appendData(u"\u05D0\u05D1\u05BC\u05D2");

  EXPECT_EQ(
      "*{'\u05D0\u05D1\u05BC\u05D2\u05D0\u05D1\u05BC\u05D2', "
      "ShapeResult=0+8 #glyphs=6}\n",
      GetItemsAsString(*text.GetLayoutObject(), 6));
}

TEST_F(LayoutTextTest, SetTextWithOffsetAppendControl) {
  SetBodyInnerHTML(u"<pre id=target>a</pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  // Note: "\n" is control character instead of text character.
  text.appendData("\nX");

  EXPECT_EQ(
      "*{'a', ShapeResult=0+1}\n"
      "*{'X', ShapeResult=2+1}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetAppendCollapseWhiteSpace) {
  SetBodyInnerHTML(u"<p id=target>abc </p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.appendData("XYZ");

  EXPECT_EQ("*{'abc XYZ', ShapeResult=0+7}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetAppend) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.appendData("xyz");

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XYZxyz', ShapeResult=3+6}\n"
      "{'def', ShapeResult=9+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1213235
TEST_F(LayoutTextTest, SetTextWithOffsetAppendEmojiWithZWJ) {
  // Compose "Woman Shrugging"
  //    U+1F937 Shrug (U+D83E U+0xDD37)
  //    U+200D  ZWJ
  //    U+2640  Female Sign
  //    U+FE0F  Variation Selector-16
  SetBodyInnerHTML(
      u"<pre id=target>&#x1F937;</pre>"
      "<p id=checker>&#x1F937;&#x200D;&#x2640;&#xFE0F</p>");

  // Check whether we have "Woman Shrug glyph or not.
  const auto& checker = *To<LayoutText>(
      GetElementById("checker")->firstChild()->GetLayoutObject());
  if (CountNumberOfGlyphs(checker) != 1) {
    return;
  }

  Text& text = To<Text>(*GetElementById("target")->firstChild());
  UpdateAllLifecyclePhasesForTest();
  text.appendData(u"\u200D");
  EXPECT_EQ("*{'\U0001F937\u200D', ShapeResult=0+3 #glyphs=2}\n",
            GetItemsAsString(*text.GetLayoutObject(), 2));

  UpdateAllLifecyclePhasesForTest();
  text.appendData(u"\u2640");
  EXPECT_EQ("*{'\U0001F937\u200D\u2640', ShapeResult=0+4 #glyphs=1}\n",
            GetItemsAsString(*text.GetLayoutObject(), 1));

  UpdateAllLifecyclePhasesForTest();
  text.appendData(u"\uFE0F");
  EXPECT_EQ("*{'\U0001F937\u200D\u2640\uFE0F', ShapeResult=0+5 #glyphs=1}\n",
            GetItemsAsString(*text.GetLayoutObject(), 1));
}

TEST_F(LayoutTextTest, SetTextWithOffsetDelete) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>xXYZyz<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.deleteData(1, 3, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'xyz', ShapeResult=3+3}\n"
      "{'def', ShapeResult=6+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetDeleteCollapseWhiteSpace) {
  SetBodyInnerHTML(u"<p id=target>ab  XY  cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(4, 2, ASSERT_NO_EXCEPTION);  // remove "XY"

  EXPECT_EQ("*{'ab cd', ShapeResult=0+5}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetDeleteCollapseWhiteSpaceEnd) {
  SetBodyInnerHTML(u"<p id=target>a bc</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(2, 2, ASSERT_NO_EXCEPTION);  // remove "bc"

  EXPECT_EQ("*{'a', ShapeResult=0+1}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1253931
TEST_F(LayoutTextTest, SetTextWithOffsetCopyItemBefore) {
  SetBodyInnerHTML(u"<p id=target><img> a</p>");

  auto& target = *GetElementById("target");
  const auto& text = *To<Text>(target.lastChild());

  target.appendChild(Text::Create(GetDocument(), "YuGFkVSKiG"));
  UpdateAllLifecyclePhasesForTest();

  // Combine Text nodes "a " and "YuGFkVSKiG".
  target.normalize();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("*{' aYuGFkVSKiG', ShapeResult=1+12}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

// web_tests/external/wpt/editing/run/delete.html?993-993
// web_tests/external/wpt/editing/run/forwarddelete.html?1193-1193
TEST_F(LayoutTextTest, SetTextWithOffsetDeleteNbspInPreWrap) {
  InsertStyleElement("#target { white-space:pre-wrap; }");
  SetBodyInnerHTML(u"<p id=target>&nbsp; abc</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(0, 1, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "*{' ', ShapeResult=0+1}\n"
      "*{'abc', ShapeResult=2+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetDeleteRTL) {
  SetBodyInnerHTML(u"<p id=target dir=rtl>0 234</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(2, 2, ASSERT_NO_EXCEPTION);  // remove "23"

  EXPECT_EQ(
      "*{'0', ShapeResult=0+1}\n"
      "*{' ', ShapeResult=1+1}\n"
      "*{'4', ShapeResult=2+1}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1000685
TEST_F(LayoutTextTest, SetTextWithOffsetDeleteRTL2) {
  SetBodyInnerHTML(u"<p id=target dir=rtl>0(xy)5</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(0, 1, ASSERT_NO_EXCEPTION);  // remove "0"

  EXPECT_EQ(
      "*{'(', ShapeResult=0+1}\n"
      "*{'xy', ShapeResult=1+2}\n"
      "*{')', ShapeResult=3+1}\n"
      "*{'5', ShapeResult=4+1}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// editing/deleting/delete_ws_fixup.html
TEST_F(LayoutTextTest, SetTextWithOffsetDeleteThenNonCollapse) {
  SetBodyInnerHTML(u"<div id=target>abc def<b> </b>ghi</div>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(4, 3, ASSERT_NO_EXCEPTION);  // remove "def"

  EXPECT_EQ(
      "*{'abc ', ShapeResult=0+4}\n"
      "{''}\n"
      "{'ghi', ShapeResult=4+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// editing/deleting/delete_ws_fixup.html
TEST_F(LayoutTextTest, SetTextWithOffsetDeleteThenNonCollapse2) {
  SetBodyInnerHTML(u"<div id=target>abc def<b> X </b>ghi</div>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(4, 3, ASSERT_NO_EXCEPTION);  // remove "def"

  EXPECT_EQ(
      "*{'abc ', ShapeResult=0+4}\n"
      "{'X ', ShapeResult=4+2}\n"
      "{'ghi', ShapeResult=6+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1039143
TEST_F(LayoutTextTest, SetTextWithOffsetDeleteWithBidiControl) {
  // In text content, we have bidi control codes:
  // U+2066 U+2069 \n U+2066 abc U+2066
  SetBodyInnerHTML(u"<pre><b id=target dir=ltr>\nabc</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(0, 1, ASSERT_NO_EXCEPTION);  // remove "\n"

  // FirstLetterPseudoElement::FirstLetterLength() change (due to \n removed)
  // makes ShouldUpdateLayoutByReattaching() (in text.cc) return true.
  EXPECT_TRUE(text.GetForceReattachLayoutTree());
}

// http://crbug.com/1125262
TEST_F(LayoutTextTest, SetTextWithOffsetDeleteWithGeneratedBreakOpportunity) {
  InsertStyleElement("#target { white-space:nowrap; }");
  SetBodyInnerHTML(u"<p><b><i id=target>ab\n</i>\n</b>\n</div>");
  // We have two ZWS for "</i>\n" and "</b>\n".
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(2, 1, ASSERT_NO_EXCEPTION);  // remove "\n"

  EXPECT_EQ(
      "*{'ab', ShapeResult=0+2}\n"
      "{''}\n"
      "{''}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1123251
TEST_F(LayoutTextTest, SetTextWithOffsetEditingTextCollapsedSpace) {
  SetBodyInnerHTML(u"<p id=target></p>");
  // Simulate: insertText("A") + InsertHTML("X ")
  Text& text = *GetDocument().CreateEditingTextNode("AX ");
  GetElementById("target")->appendChild(&text);
  UpdateAllLifecyclePhasesForTest();

  text.replaceData(0, 2, " ", ASSERT_NO_EXCEPTION);

  EXPECT_EQ("*{''}\n", GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetInsert) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.insertData(1, "xyz", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XxyzYZ', ShapeResult=3+6}\n"
      "{'def', ShapeResult=9+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetInsertAfterSpace) {
  SetBodyInnerHTML(u"<p id=target>ab cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.insertData(3, " XYZ ", ASSERT_NO_EXCEPTION);

  EXPECT_EQ("*{'ab XYZ cd', ShapeResult=0+9}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetInserBeforetSpace) {
  SetBodyInnerHTML(u"<p id=target>ab cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.insertData(2, " XYZ ", ASSERT_NO_EXCEPTION);

  EXPECT_EQ("*{'ab XYZ cd', ShapeResult=0+9}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

// https://crbug.com/1391668
TEST_F(LayoutTextTest, SetTextWithOffsetInsertSameCharacters) {
  LoadAhem();
  InsertStyleElement("body { font: 10px/15px Ahem; } b { font-size: 50px; }");
  SetBodyInnerHTML(u"<p><b id=target>a</b>aa</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.insertData(0, "aa", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "*{'aaa', ShapeResult=0+3 width=150}\n"
      "{'aa', ShapeResult=3+2 width=20}\n",
      GetItemsAsString(*text.GetLayoutObject(), 0, kIncludeSnappedWidth));
}

TEST_F(LayoutTextTest, SetTextWithOffsetNoRelocation) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  // Note: |CharacterData::setData()| is implementation of Node::setNodeValue()
  // for |CharacterData|.
  text.setData("xyz");

  EXPECT_EQ("LayoutText has NeedsCollectInlines",
            GetItemsAsString(*text.GetLayoutObject()))
      << "There are no optimization for setData()";
}

TEST_F(LayoutTextTest, SetTextWithOffsetPrepend) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.insertData(1, "xyz", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XxyzYZ', ShapeResult=3+6}\n"
      "{'def', ShapeResult=9+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetReplace) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZW<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.replaceData(1, 2, "yz", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XyzW', ShapeResult=3+4}\n"
      "{'def', ShapeResult=7+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetReplaceCollapseWhiteSpace) {
  SetBodyInnerHTML(u"<p id=target>ab  XY  cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.replaceData(4, 2, " ", ASSERT_NO_EXCEPTION);  // replace "XY" to " "

  EXPECT_EQ("*{'ab cd', ShapeResult=0+5}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetReplaceToExtend) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZW<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.replaceData(1, 2, "xyz", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XxyzW', ShapeResult=3+5}\n"
      "{'def', ShapeResult=8+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetReplaceToShrink) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZW<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.replaceData(1, 2, "y", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XyW', ShapeResult=3+3}\n"
      "{'def', ShapeResult=6+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutTextTest, SetTextWithOffsetToEmpty) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  // Note: |CharacterData::setData()| is implementation of Node::setNodeValue()
  // for |CharacterData|.
  // Note: |setData()| detaches layout object from |Text| node since
  // |Text::TextLayoutObjectIsNeeded()| returns false for empty text.
  text.setData("");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(nullptr, text.GetLayoutObject());
}

}  // namespace blink
