// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_text.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

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
    Selection().SetSelectionAndEndTyping(selection);
    Selection().CommitAppearanceIfNeeded();
  }

  const LayoutText* FindFirstLayoutText() {
    for (const Node& node :
         NodeTraversal::DescendantsOf(*GetDocument().body())) {
      if (node.GetLayoutObject() && node.GetLayoutObject()->IsText())
        return To<LayoutText>(node.GetLayoutObject());
    }
    NOTREACHED();
    return nullptr;
  }

  PhysicalRect GetSelectionRectFor(const std::string& selection_text) {
    std::stringstream stream;
    stream << "<div style='font: 10px/10px Ahem;'>" << selection_text
           << "</div>";
    SetSelectionAndUpdateLayoutSelection(stream.str());
    const Node* target = GetDocument().getElementById("target");
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
    if (offset <= layout_text.TextLength())
      result[2] = layout_text.IsAfterNonCollapsedCharacter(offset) ? 'A' : '-';
    return result;
  }
};

const char kTacoText[] = "Los Compadres Taco Truck";

// Helper class to run the same test code with and without LayoutNG
class ParameterizedLayoutTextTest : public testing::WithParamInterface<bool>,
                                    private ScopedLayoutNGForTest,
                                    public LayoutTextTest {
 public:
  ParameterizedLayoutTextTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const {
    return RuntimeEnabledFeatures::LayoutNGEnabled();
  }

  // TODO(yosin): Once we release EditingNG, this function is used for
  // specifying legacy specific behavior.
  const char* ValueWithLegacy(const char* ng_text,
                              const char* legacy_text,
                              const char* reason) {
    DCHECK_NE(*reason, 0);
    return LayoutNGEnabled() ? ng_text : legacy_text;
  }
};

INSTANTIATE_TEST_SUITE_P(All, ParameterizedLayoutTextTest, testing::Bool());

}  // namespace

TEST_F(LayoutTextTest, WidthZeroFromZeroLength) {
  SetBasicBody(kTacoText);
  ASSERT_EQ(0, GetBasicText()->Width(0u, 0u, LayoutUnit(), TextDirection::kLtr,
                                     false));
}

TEST_F(LayoutTextTest, WidthMaxFromZeroLength) {
  SetBasicBody(kTacoText);
  ASSERT_EQ(0, GetBasicText()->Width(std::numeric_limits<unsigned>::max(), 0u,
                                     LayoutUnit(), TextDirection::kLtr, false));
}

TEST_F(LayoutTextTest, WidthZeroFromMaxLength) {
  SetBasicBody(kTacoText);
  float width = GetBasicText()->Width(0u, std::numeric_limits<unsigned>::max(),
                                      LayoutUnit(), TextDirection::kLtr, false);
  // Width may vary by platform and we just want to make sure it's something
  // roughly reasonable.
  ASSERT_GE(width, 100.f);
  ASSERT_LE(width, 160.f);
}

TEST_F(LayoutTextTest, WidthMaxFromMaxLength) {
  SetBasicBody(kTacoText);
  ASSERT_EQ(0, GetBasicText()->Width(std::numeric_limits<unsigned>::max(),
                                     std::numeric_limits<unsigned>::max(),
                                     LayoutUnit(), TextDirection::kLtr, false));
}

TEST_F(LayoutTextTest, WidthWithHugeLengthAvoidsOverflow) {
  // The test case from http://crbug.com/647820 uses a 288-length string, so for
  // posterity we follow that closely.
  SetBodyInnerHTML(R"HTML(
    <div
    id='target'>
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    </div>
  )HTML");
  // Width may vary by platform and we just want to make sure it's something
  // roughly reasonable.
  const float width = GetBasicText()->Width(
      23u, 4294967282u, LayoutUnit(2.59375), TextDirection::kRtl, false);
  ASSERT_GE(width, 100.f);
  ASSERT_LE(width, 300.f);
}

TEST_F(LayoutTextTest, WidthFromBeyondLength) {
  SetBasicBody("x");
  ASSERT_EQ(0u, GetBasicText()->Width(1u, 1u, LayoutUnit(), TextDirection::kLtr,
                                      false));
}

TEST_F(LayoutTextTest, WidthLengthBeyondLength) {
  SetBasicBody("x");
  // Width may vary by platform and we just want to make sure it's something
  // roughly reasonable.
  const float width =
      GetBasicText()->Width(0u, 2u, LayoutUnit(), TextDirection::kLtr, false);
  ASSERT_GE(width, 4.f);
  ASSERT_LE(width, 20.f);
}

TEST_F(LayoutTextTest, ContainsOnlyWhitespaceOrNbsp) {
  // Note that '&nbsp' is needed when only including whitespace to force
  // LayoutText creation from the div.
  SetBasicBody("&nbsp");
  // GetWidth will also compute |contains_only_whitespace_|.
  GetBasicText()->Width(0u, 1u, LayoutUnit(), TextDirection::kLtr, false);
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kYes,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());

  SetBasicBody("   \t\t\n \n\t &nbsp \n\t&nbsp\n \t");
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kUnknown,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());
  GetBasicText()->Width(0u, 18u, LayoutUnit(), TextDirection::kLtr, false);
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kYes,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());

  SetBasicBody("abc");
  GetBasicText()->Width(0u, 3u, LayoutUnit(), TextDirection::kLtr, false);
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kNo,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());

  SetBasicBody("  \t&nbsp\nx \n");
  GetBasicText()->Width(0u, 8u, LayoutUnit(), TextDirection::kLtr, false);
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kNo,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());
}

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

struct NGOffsetMappingTestData {
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

std::ostream& operator<<(std::ostream& out, NGOffsetMappingTestData data) {
  return out << "\"" << data.text << "\" " << data.dom_start << ","
             << data.dom_end << " => " << (data.success ? "true " : "false ")
             << data.text_start << "," << data.text_end;
}

class MapDOMOffsetToTextContentOffset
    : public LayoutTextTest,
      private ScopedLayoutNGForTest,
      public testing::WithParamInterface<NGOffsetMappingTestData> {
 public:
  MapDOMOffsetToTextContentOffset() : ScopedLayoutNGForTest(true) {}
};

INSTANTIATE_TEST_SUITE_P(LayoutTextTest,
                         MapDOMOffsetToTextContentOffset,
                         testing::ValuesIn(offset_mapping_test_data));

TEST_P(MapDOMOffsetToTextContentOffset, Basic) {
  const auto data = GetParam();
  SetBodyInnerHTML(data.text);
  LayoutText* layout_text = GetBasicText();
  const NGOffsetMapping* mapping = layout_text->GetNGOffsetMapping();
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

TEST_P(ParameterizedLayoutTextTest, CharacterAfterWhitespaceCollapsing) {
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

TEST_P(ParameterizedLayoutTextTest, CaretMinMaxOffset) {
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

TEST_P(ParameterizedLayoutTextTest, ResolvedTextLength) {
  SetBasicBody("foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("  foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody(" foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());
}

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffset) {
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

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffsetInPre) {
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

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffsetInPreLine) {
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
  EXPECT_EQ(ValueWithLegacy("-CA", "BCA", "before collapsed trailing space"),
            GetSnapCode("ab| \n cd"));
  EXPECT_EQ(ValueWithLegacy("BC-", "BCA", "after first trailing space"),
            GetSnapCode("ab |\n cd"));
  EXPECT_EQ(ValueWithLegacy("--A", "B-A", "before collapsed leading space"),
            GetSnapCode("ab \n| cd"));
  EXPECT_EQ(ValueWithLegacy("BC-", "BCA", "after collapsed leading space"),
            GetSnapCode("ab \n |cd"));

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
  EXPECT_EQ(ValueWithLegacy("-CA", "BCA", "before collapsed trailing space"),
            GetSnapCode("ab|  \n  cd"));
  EXPECT_EQ(ValueWithLegacy("---", "-CA", "after first trailing space"),
            GetSnapCode("ab | \n  cd"));
  EXPECT_EQ(ValueWithLegacy("BC-", "BCA", "after collapsed trailing space"),
            GetSnapCode("ab  |\n  cd"));
  EXPECT_EQ(ValueWithLegacy("--A", "B-A", "before collapsed leading space"),
            GetSnapCode("ab  \n|  cd"));
  EXPECT_EQ(ValueWithLegacy("---", "--A", "after collapsed leading space"),
            GetSnapCode("ab  \n | cd"));
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
  EXPECT_EQ(ValueWithLegacy("-CA", "BCA", "before collapsed trailing space"),
            GetSnapCode("a| \n \n b"));
  EXPECT_EQ(ValueWithLegacy("BC-", "BCA", "after first trailing space"),
            GetSnapCode("a |\n \n b"));
  EXPECT_EQ(ValueWithLegacy("--A", "B-A", "before leading collapsed space"),
            GetSnapCode("a \n| \n b"));
  EXPECT_EQ(ValueWithLegacy("BC-", "BCA", "after first trailing space"),
            GetSnapCode("a \n |\n b"));
  EXPECT_EQ(ValueWithLegacy("--A", "B-A", "before collapsed leading space"),
            GetSnapCode("a \n \n| b"));
  EXPECT_EQ(ValueWithLegacy("BC-", "BCA", "after collapsed leading space"),
            GetSnapCode("a \n \n |b"));
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
  EXPECT_EQ(ValueWithLegacy("-CA", "BCA", "before collapsed trailing space"),
            GetSnapCode("a| \n  \n b"));
  EXPECT_EQ(ValueWithLegacy("BC-", "BCA", "after first trailing space"),
            GetSnapCode("a |\n  \n b"));
  EXPECT_EQ(ValueWithLegacy("--A", "B-A", "before collapsed leading space"),
            GetSnapCode("a \n|  \n b"));
  EXPECT_EQ(ValueWithLegacy("---", "--A",
                            "after first trailing and in leading space"),
            GetSnapCode("a \n | \n b"));
  EXPECT_EQ("BC-", GetSnapCode("a \n  |\n b"));
  EXPECT_EQ(ValueWithLegacy("--A", "B-A", "before collapsed leading space"),
            GetSnapCode("a \n  \n| b"));
  EXPECT_EQ(ValueWithLegacy("BC-", "BCA", "after collapsed leading space"),
            GetSnapCode("a \n  \n |b"));
  EXPECT_EQ("-CA", GetSnapCode("a \n  \n b|"));
}

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffsetWithTrailingSpace) {
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

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffsetWithTrailingSpace1) {
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
  EXPECT_EQ(ValueWithLegacy("-CA", "BCA", "before after first trailing space"),
            GetSnapCode(text_ab, "ab| <br>"));
  EXPECT_EQ(ValueWithLegacy("---", "-CA", "after first trailing space"),
            GetSnapCode(text_ab, "ab |<br>"));
  EXPECT_EQ("BC-", GetSnapCode(layout_br, 0));
  EXPECT_EQ("--A", GetSnapCode(layout_br, 1));
  EXPECT_EQ("---", GetSnapCode(text_cd, "| cd"));
  EXPECT_EQ("BC-", GetSnapCode(text_cd, " |cd"));
  EXPECT_EQ("BCA", GetSnapCode(text_cd, " c|d"));
  EXPECT_EQ("-CA", GetSnapCode(text_cd, " cd|"));
}

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffsetWithTrailingSpace2) {
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
  EXPECT_EQ(ValueWithLegacy("-CA", "BCA", "after first trailing space"),
            GetSnapCode(text_ab, "ab|  <br>"));
  EXPECT_EQ(ValueWithLegacy("---", "-CA", "after first trailing space"),
            GetSnapCode(text_ab, "ab | <br>"));
  EXPECT_EQ("---", GetSnapCode(text_ab, "ab  |<br>"));
  EXPECT_EQ(ValueWithLegacy("BC-", "---", "before <br>"),
            GetSnapCode(layout_br, 0));
  EXPECT_EQ(ValueWithLegacy("--A", "---", "after <br>"),
            GetSnapCode(layout_br, 1));
  EXPECT_EQ("---", GetSnapCode(text_cd, "|  cd"));
  EXPECT_EQ("---", GetSnapCode(text_cd, " | cd"));
  EXPECT_EQ("BC-", GetSnapCode(text_cd, "  |cd"));
  EXPECT_EQ("BCA", GetSnapCode(text_cd, "  c|d"));
  EXPECT_EQ("-CA", GetSnapCode(text_cd, "  cd|"));
}

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffsetWithTrailingSpace3) {
  SetBodyInnerHTML("<div id=target>a<br>   <br>b<br></div>");
  const auto& text_a = *GetLayoutTextById("target");
  const auto& layout_br1 = *To<LayoutText>(text_a.NextSibling());
  const auto& text_space = *To<LayoutText>(layout_br1.NextSibling());
  EXPECT_EQ(1u, text_space.TextLength());
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

TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithCollapsedWhiteSpace) {
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
  EXPECT_EQ(LayoutRect(0, 0, 40, 10), results[0].local_rect);

  EXPECT_EQ(6u, results[1].dom_start_offset);
  EXPECT_EQ(3u, results[1].dom_length);
  EXPECT_EQ(LayoutRect(40, 0, 30, 10), results[1].local_rect);

  EXPECT_EQ(9u, results[2].dom_start_offset);
  EXPECT_EQ(1u, results[2].dom_length);
  EXPECT_EQ(LayoutRect(70, 0, 0, 10), results[2].local_rect);

  EXPECT_EQ(14u, results[3].dom_start_offset);
  EXPECT_EQ(3u, results[3].dom_length);
  EXPECT_EQ(LayoutRect(0, 10, 30, 10), results[3].local_rect);
}

TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithGeneratedContent) {
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
  EXPECT_EQ(LayoutRect(40, 0, 30, 10), boxes_xyz[0].local_rect);

  auto boxes_first_letter = layout_text_first_letter.GetTextBoxInfo();
  EXPECT_EQ(1u, boxes_first_letter.size());
  EXPECT_EQ(2u, boxes_first_letter[0].dom_start_offset);
  EXPECT_EQ(1u, boxes_first_letter[0].dom_length);
  EXPECT_EQ(LayoutRect(0, 0, 10, 10), boxes_first_letter[0].local_rect);

  auto boxes_remaining = layout_text_remaining.GetTextBoxInfo();
  EXPECT_EQ(2u, boxes_remaining.size());
  EXPECT_EQ(0u, boxes_remaining[0].dom_start_offset);
  EXPECT_EQ(1u, boxes_remaining[0].dom_length) << "two spaces to one space";
  EXPECT_EQ(LayoutRect(10, 0, 10, 10), boxes_remaining[0].local_rect);
  EXPECT_EQ(3u, boxes_remaining[1].dom_start_offset);
  EXPECT_EQ(2u, boxes_remaining[1].dom_length);
  EXPECT_EQ(LayoutRect(20, 0, 20, 10), boxes_remaining[1].local_rect);
}

// For http://crbug.com/985488
TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithHidden) {
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
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), boxes[0].local_rect);

  EXPECT_EQ(9u, boxes[1].dom_start_offset);
  EXPECT_EQ(5u, boxes[1].dom_length);
  EXPECT_EQ(LayoutRect(60, 0, 50, 10), boxes[1].local_rect);
}

// For http://crbug.com/985488
TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithEllipsis) {
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
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), boxes[0].local_rect);

  EXPECT_EQ(9u, boxes[1].dom_start_offset);
  EXPECT_EQ(5u, boxes[1].dom_length);
  EXPECT_EQ(LayoutRect(60, 0, 50, 10), boxes[1].local_rect);
}

// For http://crbug.com/1003413
TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithEllipsisForPseudoAfter) {
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
  EXPECT_EQ(LayoutRect(30, 0, 10, 10), boxes[0].local_rect);
}

// Test the specialized code path in |PlainText| for when |!GetNode()|.
TEST_P(ParameterizedLayoutTextTest, PlainTextInPseudo) {
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

TEST_P(ParameterizedLayoutTextTest,
       IsBeforeAfterNonCollapsedCharacterNoLineWrap) {
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

TEST_P(ParameterizedLayoutTextTest, IsBeforeAfterNonCollapsedLineWrapSpace) {
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
  EXPECT_EQ(ValueWithLegacy("-CA", "--A", "after soft line wrap"),
            GetSnapCode("ab | cd"));
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
  EXPECT_EQ(ValueWithLegacy("BC-", "---", "before soft line wrap"),
            GetSnapCode("span", "|  cd"));
  EXPECT_EQ(ValueWithLegacy("-CA", "---", "after soft line wrap"),
            GetSnapCode("span", " | cd"));
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
  EXPECT_EQ(ValueWithLegacy("BCA", "-CA", "before soft line wrap"),
            GetSnapCode("ab|  "));
  EXPECT_EQ(ValueWithLegacy("-CA", "---", "after soft line wrap"),
            GetSnapCode("ab | "));
  EXPECT_EQ("---", GetSnapCode("ab  |"));

  // Entire node as line wrapping
  SetAhemBody("ab<span id=space>  </span>cd", 2);
  // text_content = "ab cd"
  // [0] I DOM:0-2 TC:0-2 "ab"
  // [1] I DOM:0-1 TC:2-3 " "
  // [2] C DOM:1-2 TC:3-3 " "
  // [3] I DOM:0-2 TC:3-5 "cd"
  EXPECT_EQ(ValueWithLegacy("BC-", "---", "before soft line wrap"),
            GetSnapCode("space", "|  "));
  EXPECT_EQ(ValueWithLegacy("-CA", "---", "after soft line wrap"),
            GetSnapCode("space", " | "));
  EXPECT_EQ("---", GetSnapCode("space", "  |"));
}

TEST_P(ParameterizedLayoutTextTest, IsBeforeAfterNonCollapsedCharacterBR) {
  SetBasicBody("<br>");
  EXPECT_EQ("BC-", GetSnapCode(*GetBasicText(), 0));
  EXPECT_EQ("--A", GetSnapCode(*GetBasicText(), 1));
}

TEST_P(ParameterizedLayoutTextTest, AbsoluteQuads) {
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

TEST_P(ParameterizedLayoutTextTest, AbsoluteQuadsVRL) {
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

TEST_P(ParameterizedLayoutTextTest, PhysicalLinesBoundingBox) {
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
  const Element& div = *GetDocument().getElementById("div");
  const Element& one = *GetDocument().getElementById("one");
  const Element& two = *GetDocument().getElementById("two");
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

TEST_P(ParameterizedLayoutTextTest, PhysicalLinesBoundingBoxTextCombine) {
  ScopedLayoutNGForTest enable_layout_ng(true);
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

  //   LayoutNGBlockFlow {HTML} at (0,0) size 800x600
  //     LayoutNGBlockFlow {BODY} at (8,8) size 784x584
  //       LayoutNGBlockFlow {DIV} at (0,0) size 130x300
  //         LayoutText {#text} at (15,0) size 100x100
  //           text run at (15,0) width 100: "a"
  //         LayoutInline {C} at (15,100) size 100x100
  //           LayoutNGTextCombine (anonymous) at (15,100) size 100x100
  //             LayoutText {#text} at (-5,0) size 110x100
  //               text run at (0,0) width 500: "01234"
  //         LayoutText {#text} at (15,200) size 100x100
  //           text run at (15,200) width 100: "b"
  //

  EXPECT_EQ(PhysicalRect(15, 0, 100, 100), text_a.PhysicalLinesBoundingBox());
  // Note: Width 110 comes from |100px * kTextCombineMargin| in
  // |LayoutNGTextCombine::DesiredWidth()|.
  EXPECT_EQ(PhysicalRect(-5, 0, 110, 100),
            text_01234.PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(15, 200, 100, 100), text_b.PhysicalLinesBoundingBox());
}

TEST_P(ParameterizedLayoutTextTest, PhysicalLinesBoundingBoxVerticalRL) {
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
  const Element& div = *GetDocument().getElementById("div");
  const Element& one = *GetDocument().getElementById("one");
  const Element& two = *GetDocument().getElementById("two");
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

TEST_P(ParameterizedLayoutTextTest, WordBreakElement) {
  SetBasicBody("foo <wbr> bar");

  const Element* wbr = GetDocument().QuerySelector("wbr");
  DCHECK(wbr->GetLayoutObject()->IsText());
  const auto* layout_wbr = To<LayoutText>(wbr->GetLayoutObject());

  EXPECT_EQ(0u, layout_wbr->ResolvedTextLength());
  EXPECT_EQ(0, layout_wbr->CaretMinOffset());
  EXPECT_EQ(0, layout_wbr->CaretMaxOffset());
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRect) {
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
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(0, 0, 0, 0)
                              : PhysicalRect(30, 0, 10, 10),
            GetSelectionRectFor("foo^ |"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectLineBreak) {
  LoadAhem();
  EXPECT_EQ(PhysicalRect(30, 0, 10, 10),
            GetSelectionRectFor("f^oo<br id='target'><br>ba|r"));
  EXPECT_EQ(PhysicalRect(0, 10, 10, 10),
            GetSelectionRectFor("f^oo<br><br id='target'>ba|r"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectLineBreakPre) {
  LoadAhem();
  EXPECT_EQ(
      PhysicalRect(30, 0, 10, 10),
      GetSelectionRectFor("<div style='white-space:pre;'>foo^\n|\nbar</div>"));
  EXPECT_EQ(
      PhysicalRect(0, 10, 10, 10),
      GetSelectionRectFor("<div style='white-space:pre;'>foo\n^\n|bar</div>"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectRTL) {
  LoadAhem();
  // TODO(yoichio) : Fix LastLogicalLeafIgnoringLineBreak so that 'foo' is the
  // last fragment.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(-10, 0, 30, 20)
                              : PhysicalRect(-10, 0, 40, 20),
            GetSelectionRectFor("<div style='width: 2em' dir=rtl>"
                                "f^oo ba|r baz</div>"));
  EXPECT_EQ(PhysicalRect(0, 0, 40, 20),
            GetSelectionRectFor("<div style='width: 2em' dir=ltr>"
                                "f^oo ba|r baz</div>"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectVertical) {
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

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectVerticalRTL) {
  LoadAhem();
  // TODO(yoichio): Investigate diff (maybe soft line break treatment).
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(0, -10, 20, 30)
                              : PhysicalRect(0, -10, 20, 40),
            GetSelectionRectFor(
                "<div style='writing-mode: vertical-lr; height: 2em' dir=rtl>"
                "f^oo ba|r baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(10, -10, 20, 30)
                              : PhysicalRect(10, -10, 20, 40),
            GetSelectionRectFor(
                "<div style='writing-mode: vertical-rl; height: 2em' dir=rtl>"
                "f^oo ba|r baz</div>"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectLineHeight) {
  LoadAhem();
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(10, 0, 10, 50)
                              : PhysicalRect(10, 20, 10, 10),
            GetSelectionRectFor("<div style='line-height: 50px; width:1em;'>"
                                "f^o|o bar baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(10, 50, 10, 50)
                              : PhysicalRect(10, 30, 10, 50),
            GetSelectionRectFor("<div style='line-height: 50px; width:1em;'>"
                                "foo b^a|r baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(10, 100, 10, 50)
                              : PhysicalRect(10, 80, 10, 50),
            GetSelectionRectFor("<div style='line-height: 50px; width:1em;'>"
                                "foo bar b^a|</div>"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectNegativeLeading) {
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
  EXPECT_EQ(PhysicalRect(0, -5, LayoutNGEnabled() ? 40 : 50, 10),
            text->LocalSelectionVisualRect());
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectLineHeightVertical) {
  LoadAhem();
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(0, 10, 50, 10)
                              : PhysicalRect(20, 10, 50, 10),
            GetSelectionRectFor("<div style='line-height: 50px; height:1em; "
                                "writing-mode:vertical-lr'>"
                                "f^o|o bar baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(50, 10, 50, 10)
                              : PhysicalRect(70, 10, 50, 10),
            GetSelectionRectFor("<div style='line-height: 50px; height:1em; "
                                "writing-mode:vertical-lr'>"
                                "foo b^a|r baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(100, 10, 50, 10)
                              : PhysicalRect(120, 10, 10, 10),
            GetSelectionRectFor("<div style='line-height: 50px; height:1em; "
                                "writing-mode:vertical-lr'>"
                                "foo bar b^a|z</div>"));
}

TEST_P(ParameterizedLayoutTextTest, VisualRectInDocumentSVGTspan) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin:0px;
        font: 20px/20px Ahem;
      }
    </style>
    <svg>
      <text x="10" y="50" width="100">
        <tspan id="target" dx="15" dy="25">tspan</tspan>
      </text>
    </svg>
  )HTML");

  auto* target =
      To<LayoutText>(GetLayoutObjectByElementId("target")->SlowFirstChild());
  const int ascent = 16;
  PhysicalRect expected(10 + 15, 50 + 25 - ascent, 20 * 5, 20);
  EXPECT_EQ(expected, target->VisualRectInDocument());
  EXPECT_EQ(expected, target->VisualRectInDocument(kUseGeometryMapper));
}

TEST_P(ParameterizedLayoutTextTest, VisualRectInDocumentSVGTspanTB) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin:0px;
        font: 20px/20px Ahem;
      }
    </style>
    <svg>
      <text x="50" y="10" width="100" writing-mode="tb">
        <tspan id="target" dx="15" dy="25">tspan</tspan>
      </text>
    </svg>
  )HTML");

  auto* target =
      To<LayoutText>(GetLayoutObjectByElementId("target")->SlowFirstChild());
  PhysicalRect expected(50 + 15 - 20 / 2, 10 + 25, 20, 20 * 5);
  EXPECT_EQ(expected, target->VisualRectInDocument());
  EXPECT_EQ(expected, target->VisualRectInDocument(kUseGeometryMapper));
}

TEST_P(ParameterizedLayoutTextTest, PositionForPointAtLeading) {
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
  EXPECT_EQ(Position(text->GetNode(), LayoutNGEnabled() ? 1 : 7),
            text->PositionForPoint({10, 0}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), LayoutNGEnabled() ? 1 : 7),
            text->PositionForPoint({10, 5}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 1),
            text->PositionForPoint({10, 10}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), 1),
            text->PositionForPoint({10, 15}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), LayoutNGEnabled() ? 1 : 7),
            text->PositionForPoint({10, 20}).GetPosition());
  EXPECT_EQ(Position(text->GetNode(), LayoutNGEnabled() ? 1 : 7),
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
TEST_P(ParameterizedLayoutTextTest, FloatFirstLetterPlainText) {
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

}  // namespace blink
