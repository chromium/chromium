// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// The spec turned into a discussion that may change. Put this logic on hold
// until CSSWG resolves the issue.
// https://github.com/w3c/csswg-drafts/issues/337
#define SEGMENT_BREAK_TRANSFORMATION_FOR_EAST_ASIAN_WIDTH 0

// Helper functions to use |EXPECT_EQ()| for |NGOffsetMappingUnit| and its span.
Vector<NGOffsetMappingUnit> ToVector(
    const base::span<const NGOffsetMappingUnit>& range) {
  Vector<NGOffsetMappingUnit> units;
  for (const auto& unit : range)
    units.push_back(unit);
  return units;
}

bool operator==(const NGOffsetMappingUnit& unit,
                const NGOffsetMappingUnit& other) {
  return unit.GetType() == other.GetType() &&
         unit.GetLayoutObject() == other.GetLayoutObject() &&
         unit.DOMStart() == other.DOMStart() &&
         unit.DOMEnd() == other.DOMEnd() &&
         unit.TextContentStart() == other.TextContentStart() &&
         unit.TextContentEnd() == other.TextContentEnd();
}

bool operator!=(const NGOffsetMappingUnit& unit,
                const NGOffsetMappingUnit& other) {
  return !operator==(unit, other);
}

void PrintTo(const NGOffsetMappingUnit& unit, std::ostream& ostream) {
  static const char* kTypeNames[] = {"Identity", "Collapsed", "Expanded"};
  ostream << "{" << kTypeNames[static_cast<unsigned>(unit.GetType())] << " "
          << unit.GetLayoutObject() << " dom=" << unit.DOMStart() << "-"
          << unit.DOMEnd() << " tc=" << unit.TextContentStart() << "-"
          << unit.TextContentEnd() << "}";
}

bool operator==(const Vector<NGOffsetMappingUnit>& units1,
                const Vector<NGOffsetMappingUnit>& units2) {
  if (units1.size() != units2.size())
    return false;
  auto* it2 = units2.begin();
  for (const auto& unit1 : units1) {
    if (unit1 != *it2)
      return false;
    ++it2;
  }
  return true;
}

bool operator==(const Vector<NGOffsetMappingUnit>& units,
                const base::span<const NGOffsetMappingUnit>& range) {
  return units == ToVector(range);
}

void PrintTo(const Vector<NGOffsetMappingUnit>& units, std::ostream& ostream) {
  ostream << "[";
  const char* comma = "";
  for (const auto& unit : units) {
    ostream << comma;
    PrintTo(unit, ostream);
    comma = ", ";
  }
  ostream << "]";
}

void PrintTo(const base::span<const NGOffsetMappingUnit>& range,
             std::ostream& ostream) {
  PrintTo(ToVector(range), ostream);
}

class NGOffsetMappingTest : public NGLayoutTest {
 protected:
  static const auto kCollapsed = NGOffsetMappingUnitType::kCollapsed;
  static const auto kIdentity = NGOffsetMappingUnitType::kIdentity;
  static const auto kExpanded = NGOffsetMappingUnitType::kExpanded;

  void SetUp() override {
    NGLayoutTest::SetUp();
    style_ = ComputedStyle::Create();
    style_->GetFont().Update(nullptr);
  }

  void SetupHtml(const char* id, String html) {
    SetBodyInnerHTML(html);
    layout_block_flow_ = To<LayoutBlockFlow>(GetLayoutObjectByElementId(id));
    DCHECK(layout_block_flow_->IsLayoutNGMixin());
    layout_object_ = layout_block_flow_->FirstChild();
    style_ = layout_object_->Style();
  }

  const NGOffsetMapping& GetOffsetMapping() const {
    const NGOffsetMapping* map =
        NGInlineNode(layout_block_flow_).ComputeOffsetMappingIfNeeded();
    CHECK(map);
    return *map;
  }

  String GetCollapsedIndexes() const {
    const NGOffsetMapping& mapping = GetOffsetMapping();
    const EphemeralRange block_range =
        EphemeralRange::RangeOfContents(*layout_block_flow_->GetNode());

    StringBuilder result;
    for (const Node& node : block_range.Nodes()) {
      if (!node.IsTextNode())
        continue;

      Vector<unsigned> collapsed_indexes;
      for (const auto& unit : mapping.GetMappingUnitsForDOMRange(
               EphemeralRange::RangeOfContents(node))) {
        if (unit.GetType() != NGOffsetMappingUnitType::kCollapsed)
          continue;
        for (unsigned i = unit.DOMStart(); i < unit.DOMEnd(); ++i)
          collapsed_indexes.push_back(i);
      }

      result.Append('{');
      bool first = true;
      for (unsigned index : collapsed_indexes) {
        if (!first)
          result.Append(", ");
        result.AppendNumber(index);
        first = false;
      }
      result.Append('}');
    }
    return result.ToString();
  }

  String TestCollapsingWithCSSWhiteSpace(String text, String whitespace) {
    StringBuilder html;
    html.Append("<div id=t style=\"white-space:");
    html.Append(whitespace);
    html.Append("\">");
    html.Append(text);
    html.Append("</div>");
    SetupHtml("t", html.ToString());
    return GetCollapsedIndexes();
  }

  String TestCollapsing(Vector<String> text) {
    StringBuilder html;
    html.Append("<div id=t>");
    for (unsigned i = 0; i < text.size(); ++i) {
      if (i)
        html.Append("<!---->");
      html.Append(text[i]);
    }
    html.Append("</div>");
    SetupHtml("t", html.ToString());
    return GetCollapsedIndexes();
  }

  String TestCollapsing(String text) {
    return TestCollapsing(Vector<String>({text}));
  }

  String TestCollapsing(String text, String text2) {
    return TestCollapsing(Vector<String>({text, text2}));
  }

  String TestCollapsing(String text, String text2, String text3) {
    return TestCollapsing(Vector<String>({text, text2, text3}));
  }

  bool IsOffsetMappingStored() const {
    return layout_block_flow_->GetNGInlineNodeData()->offset_mapping.get();
  }

  const LayoutText* GetLayoutTextUnder(const char* parent_id) {
    Element* parent = GetDocument().getElementById(parent_id);
    return ToLayoutText(parent->firstChild()->GetLayoutObject());
  }

  const NGOffsetMappingUnit* GetUnitForPosition(
      const Position& position) const {
    return GetOffsetMapping().GetMappingUnitForPosition(position);
  }

  base::Optional<unsigned> GetTextContentOffset(
      const Position& position) const {
    return GetOffsetMapping().GetTextContentOffset(position);
  }

  Position StartOfNextNonCollapsedContent(const Position& position) const {
    return GetOffsetMapping().StartOfNextNonCollapsedContent(position);
  }

  Position EndOfLastNonCollapsedContent(const Position& position) const {
    return GetOffsetMapping().EndOfLastNonCollapsedContent(position);
  }

  bool IsBeforeNonCollapsedContent(const Position& position) const {
    return GetOffsetMapping().IsBeforeNonCollapsedContent(position);
  }

  bool IsAfterNonCollapsedContent(const Position& position) const {
    return GetOffsetMapping().IsAfterNonCollapsedContent(position);
  }

  Position GetFirstPosition(unsigned offset) const {
    return GetOffsetMapping().GetFirstPosition(offset);
  }

  Position GetLastPosition(unsigned offset) const {
    return GetOffsetMapping().GetLastPosition(offset);
  }

  scoped_refptr<const ComputedStyle> style_;
  LayoutBlockFlow* layout_block_flow_ = nullptr;
  LayoutObject* layout_object_ = nullptr;
  FontCachePurgePreventer purge_preventer_;
};

TEST_F(NGOffsetMappingTest, CollapseSpaces) {
  String input("text text  text   text");
  EXPECT_EQ("{10, 16, 17}", TestCollapsingWithCSSWhiteSpace(input, "normal"));
  EXPECT_EQ("{10, 16, 17}", TestCollapsingWithCSSWhiteSpace(input, "nowrap"));
  EXPECT_EQ("{10, 16, 17}",
            TestCollapsingWithCSSWhiteSpace(input, "-webkit-nowrap"));
  EXPECT_EQ("{10, 16, 17}", TestCollapsingWithCSSWhiteSpace(input, "pre-line"));
  EXPECT_EQ("{}", TestCollapsingWithCSSWhiteSpace(input, "pre"));
  EXPECT_EQ("{}", TestCollapsingWithCSSWhiteSpace(input, "pre-wrap"));
}

TEST_F(NGOffsetMappingTest, CollapseTabs) {
  String input("text text \ttext \t\ttext");
  EXPECT_EQ("{10, 16, 17}", TestCollapsingWithCSSWhiteSpace(input, "normal"));
  EXPECT_EQ("{10, 16, 17}", TestCollapsingWithCSSWhiteSpace(input, "nowrap"));
  EXPECT_EQ("{10, 16, 17}",
            TestCollapsingWithCSSWhiteSpace(input, "-webkit-nowrap"));
  EXPECT_EQ("{10, 16, 17}", TestCollapsingWithCSSWhiteSpace(input, "pre-line"));
  EXPECT_EQ("{}", TestCollapsingWithCSSWhiteSpace(input, "pre"));
  EXPECT_EQ("{}", TestCollapsingWithCSSWhiteSpace(input, "pre-wrap"));
}

TEST_F(NGOffsetMappingTest, CollapseNewLines) {
  String input("text\ntext \n text\n\ntext");
  EXPECT_EQ("{10, 11, 17}", TestCollapsingWithCSSWhiteSpace(input, "normal"));
  EXPECT_EQ("{10, 11, 17}", TestCollapsingWithCSSWhiteSpace(input, "nowrap"));
  EXPECT_EQ("{10, 11, 17}",
            TestCollapsingWithCSSWhiteSpace(input, "-webkit-nowrap"));
  EXPECT_EQ("{9, 11}", TestCollapsingWithCSSWhiteSpace(input, "pre-line"));
  EXPECT_EQ("{}", TestCollapsingWithCSSWhiteSpace(input, "pre"));
  EXPECT_EQ("{}", TestCollapsingWithCSSWhiteSpace(input, "pre-wrap"));
}

TEST_F(NGOffsetMappingTest, CollapseNewlinesAsSpaces) {
  EXPECT_EQ("{}", TestCollapsing("text\ntext"));
  EXPECT_EQ("{5}", TestCollapsing("text\n\ntext"));
  EXPECT_EQ("{5, 6, 7}", TestCollapsing("text \n\n text"));
  EXPECT_EQ("{5, 6, 7, 8}", TestCollapsing("text \n \n text"));
}

TEST_F(NGOffsetMappingTest, CollapseAcrossElements) {
  EXPECT_EQ("{}{0}", TestCollapsing("text ", " text"))
      << "Spaces are collapsed even when across elements.";
}

TEST_F(NGOffsetMappingTest, CollapseLeadingSpaces) {
  EXPECT_EQ("{0, 1}", TestCollapsing("  text"));
  // TODO(xiaochengh): Currently, LayoutText of trailing whitespace nodes are
  // omitted, so we can't verify the following cases. Get around it and make the
  // following tests work. EXPECT_EQ("{0}{}", TestCollapsing(" ", "text"));
  // EXPECT_EQ("{0}{0}", TestCollapsing(" ", " text"));
}

TEST_F(NGOffsetMappingTest, CollapseTrailingSpaces) {
  EXPECT_EQ("{4, 5}", TestCollapsing("text  "));
  EXPECT_EQ("{}{0}", TestCollapsing("text", " "));
  // TODO(xiaochengh): Get around whitespace LayoutText omission, and make the
  // following test cases work.
  // EXPECT_EQ("{4}{0}", TestCollapsing("text ", " "));
}

// TODO(xiaochengh): Get around whitespace LayoutText omission, and make the
// following test cases work.
TEST_F(NGOffsetMappingTest, DISABLED_CollapseAllSpaces) {
  EXPECT_EQ("{0, 1}", TestCollapsing("  "));
  EXPECT_EQ("{0, 1}{0, 1}", TestCollapsing("  ", "  "));
  EXPECT_EQ("{0, 1}{0}", TestCollapsing("  ", "\n"));
  EXPECT_EQ("{0}{0, 1}", TestCollapsing("\n", "  "));
}

TEST_F(NGOffsetMappingTest, CollapseLeadingNewlines) {
  EXPECT_EQ("{0}", TestCollapsing("\ntext"));
  EXPECT_EQ("{0, 1}", TestCollapsing("\n\ntext"));
  // TODO(xiaochengh): Get around whitespace LayoutText omission, and make the
  // following test cases work.
  // EXPECT_EQ("{0}{}", TestCollapsing("\n", "text"));
  // EXPECT_EQ("{0, 1}{}", TestCollapsing("\n\n", "text"));
  // EXPECT_EQ("{0, 1}{}", TestCollapsing(" \n", "text"));
  // EXPECT_EQ("{0}{0}", TestCollapsing("\n", " text"));
  // EXPECT_EQ("{0, 1}{0}", TestCollapsing("\n\n", " text"));
  // EXPECT_EQ("{0, 1}{0}", TestCollapsing(" \n", " text"));
  // EXPECT_EQ("{0}{0}", TestCollapsing("\n", "\ntext"));
  // EXPECT_EQ("{0, 1}{0}", TestCollapsing("\n\n", "\ntext"));
  // EXPECT_EQ("{0, 1}{0}", TestCollapsing(" \n", "\ntext"));
}

TEST_F(NGOffsetMappingTest, CollapseTrailingNewlines) {
  EXPECT_EQ("{4}", TestCollapsing("text\n"));
  EXPECT_EQ("{}{0}", TestCollapsing("text", "\n"));
  // TODO(xiaochengh): Get around whitespace LayoutText omission, and make the
  // following test cases work.
  // EXPECT_EQ("{4}{0}", TestCollapsing("text\n", "\n"));
  // EXPECT_EQ("{4}{0}", TestCollapsing("text\n", " "));
  // EXPECT_EQ("{4}{0}", TestCollapsing("text ", "\n"));
}

TEST_F(NGOffsetMappingTest, CollapseNewlineAcrossElements) {
  EXPECT_EQ("{}{0}", TestCollapsing("text ", "\ntext"));
  EXPECT_EQ("{}{0, 1}", TestCollapsing("text ", "\n text"));
  EXPECT_EQ("{}{}{0}", TestCollapsing("text", " ", "\ntext"));
}

TEST_F(NGOffsetMappingTest, CollapseBeforeAndAfterNewline) {
  EXPECT_EQ("{4, 5, 7, 8}",
            TestCollapsingWithCSSWhiteSpace("text  \n  text", "pre-line"))
      << "Spaces before and after newline are removed.";
}

TEST_F(NGOffsetMappingTest,
       CollapsibleSpaceAfterNonCollapsibleSpaceAcrossElements) {
  SetupHtml("t",
            "<div id=t>"
            "<span style=\"white-space:pre-wrap\">text </span>"
            " text"
            "</div>");
  EXPECT_EQ("{}{}", GetCollapsedIndexes())
      << "The whitespace in constructions like '<span style=\"white-space: "
         "pre-wrap\">text <span><span> text</span>' does not collapse.";
}

TEST_F(NGOffsetMappingTest, CollapseZeroWidthSpaces) {
  EXPECT_EQ("{5}", TestCollapsing(u"text\u200B\ntext"))
      << "Newline is removed if the character before is ZWS.";
  EXPECT_EQ("{4}", TestCollapsing(u"text\n\u200Btext"))
      << "Newline is removed if the character after is ZWS.";
  EXPECT_EQ("{5}", TestCollapsing(u"text\u200B\n\u200Btext"))
      << "Newline is removed if the character before/after is ZWS.";

  EXPECT_EQ("{4}{}", TestCollapsing(u"text\n", u"\u200Btext"))
      << "Newline is removed if the character after across elements is ZWS.";
  EXPECT_EQ("{}{0}", TestCollapsing(u"text\u200B", u"\ntext"))
      << "Newline is removed if the character before is ZWS even across "
         "elements.";

  EXPECT_EQ("{4, 5}{}", TestCollapsing(u"text \n", u"\u200Btext"))
      << "Collapsible space before newline does not affect the result.";
  EXPECT_EQ("{5}{}", TestCollapsing(u"text\u200B\n", u" text"))
      << "Collapsible space after newline is removed even when the "
         "newline was removed.";
  EXPECT_EQ("{5}{0}", TestCollapsing(u"text\u200B ", u"\ntext"))
      << "A white space sequence containing a segment break before or after "
         "a zero width space is collapsed to a zero width space.";
}

#if SEGMENT_BREAK_TRANSFORMATION_FOR_EAST_ASIAN_WIDTH
TEST_F(NGOffsetMappingTest, CollapseEastAsianWidth) {
  EXPECT_EQ("{1}", TestCollapsing(u"\u4E00\n\u4E00"))
      << "Newline is removed when both sides are Wide.";

  EXPECT_EQ("{}", TestCollapsing(u"\u4E00\nA"))
      << "Newline is not removed when after is Narrow.";
  EXPECT_EQ("{}", TestCollapsing(u"A\n\u4E00"))
      << "Newline is not removed when before is Narrow.";

  EXPECT_EQ("{1}{}", TestCollapsing(u"\u4E00\n", u"\u4E00"))
      << "Newline at the end of elements is removed when both sides are Wide.";
  EXPECT_EQ("{}{0}", TestCollapsing(u"\u4E00", u"\n\u4E00"))
      << "Newline at the beginning of elements is removed "
         "when both sides are Wide.";
}
#endif

#define TEST_UNIT(unit, type, owner, dom_start, dom_end, text_content_start, \
                  text_content_end)                                          \
  EXPECT_EQ(type, unit.GetType());                                           \
  EXPECT_EQ(owner, &unit.GetOwner());                                        \
  EXPECT_EQ(dom_start, unit.DOMStart());                                     \
  EXPECT_EQ(dom_end, unit.DOMEnd());                                         \
  EXPECT_EQ(text_content_start, unit.TextContentStart());                    \
  EXPECT_EQ(text_content_end, unit.TextContentEnd())

#define TEST_RANGE(ranges, owner, start, end) \
  ASSERT_TRUE(ranges.Contains(owner));        \
  EXPECT_EQ(start, ranges.at(owner).first);   \
  EXPECT_EQ(end, ranges.at(owner).second)

TEST_F(NGOffsetMappingTest, StoredResult) {
  SetupHtml("t", "<div id=t>foo</div>");
  EXPECT_FALSE(IsOffsetMappingStored());
  GetOffsetMapping();
  EXPECT_TRUE(IsOffsetMappingStored());
}

TEST_F(NGOffsetMappingTest, NGInlineFormattingContextOf) {
  SetBodyInnerHTML(
      "<div id=container>"
      "  foo"
      "  <span id=inline-block style='display:inline-block'>blah</span>"
      "  <span id=inline-span>bar</span>"
      "</div>");

  const Element* container = GetElementById("container");
  const Element* inline_block = GetElementById("inline-block");
  const Element* inline_span = GetElementById("inline-span");
  const Node* blah = inline_block->firstChild();
  const Node* foo = inline_block->previousSibling();
  const Node* bar = inline_span->firstChild();

  EXPECT_EQ(nullptr,
            NGInlineFormattingContextOf(Position::BeforeNode(*container)));
  EXPECT_EQ(nullptr,
            NGInlineFormattingContextOf(Position::AfterNode(*container)));

  const LayoutObject* container_object = container->GetLayoutObject();
  EXPECT_EQ(container_object, NGInlineFormattingContextOf(Position(foo, 0)));
  EXPECT_EQ(container_object, NGInlineFormattingContextOf(Position(bar, 0)));
  EXPECT_EQ(container_object,
            NGInlineFormattingContextOf(Position::BeforeNode(*inline_block)));
  EXPECT_EQ(container_object,
            NGInlineFormattingContextOf(Position::AfterNode(*inline_block)));

  const LayoutObject* inline_block_object = inline_block->GetLayoutObject();
  EXPECT_EQ(inline_block_object,
            NGInlineFormattingContextOf(Position(blah, 0)));
}

TEST_F(NGOffsetMappingTest, OneTextNode) {
  SetupHtml("t", "<div id=t>foo</div>");
  const Node* foo_node = layout_object_->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

  EXPECT_EQ("foo", result.GetText());

  ASSERT_EQ(1u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 3u, 0u, 3u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 0)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 1)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 2)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 3)));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));

  EXPECT_EQ(Position(foo_node, 0), GetFirstPosition(0));
  EXPECT_EQ(Position(foo_node, 1), GetFirstPosition(1));
  EXPECT_EQ(Position(foo_node, 2), GetFirstPosition(2));
  EXPECT_EQ(Position(foo_node, 3), GetFirstPosition(3));

  EXPECT_EQ(Position(foo_node, 0), GetLastPosition(0));
  EXPECT_EQ(Position(foo_node, 1), GetLastPosition(1));
  EXPECT_EQ(Position(foo_node, 2), GetLastPosition(2));
  EXPECT_EQ(Position(foo_node, 3), GetLastPosition(3));

  EXPECT_EQ(Position(foo_node, 0),
            StartOfNextNonCollapsedContent(Position(foo_node, 0)));
  EXPECT_EQ(Position(foo_node, 1),
            StartOfNextNonCollapsedContent(Position(foo_node, 1)));
  EXPECT_EQ(Position(foo_node, 2),
            StartOfNextNonCollapsedContent(Position(foo_node, 2)));
  EXPECT_TRUE(StartOfNextNonCollapsedContent(Position(foo_node, 3)).IsNull());

  EXPECT_TRUE(EndOfLastNonCollapsedContent(Position(foo_node, 0)).IsNull());
  EXPECT_EQ(Position(foo_node, 1),
            EndOfLastNonCollapsedContent(Position(foo_node, 1)));
  EXPECT_EQ(Position(foo_node, 2),
            EndOfLastNonCollapsedContent(Position(foo_node, 2)));
  EXPECT_EQ(Position(foo_node, 3),
            EndOfLastNonCollapsedContent(Position(foo_node, 3)));

  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(foo_node, 0)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(foo_node, 1)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(foo_node, 2)));
  EXPECT_FALSE(
      IsBeforeNonCollapsedContent(Position(foo_node, 3)));  // false at node end

  // false at node start
  EXPECT_FALSE(IsAfterNonCollapsedContent(Position(foo_node, 0)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(foo_node, 1)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(foo_node, 2)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(foo_node, 3)));
}

TEST_F(NGOffsetMappingTest, TwoTextNodes) {
  SetupHtml("t", "<div id=t>foo<span id=s>bar</span></div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* bar = GetLayoutTextUnder("s");
  const Node* foo_node = foo->GetNode();
  const Node* bar_node = bar->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

  EXPECT_EQ("foobar", result.GetText());

  ASSERT_EQ(2u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 3u, 0u, 3u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, bar_node,
            0u, 3u, 3u, 6u);

  ASSERT_EQ(2u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);
  TEST_RANGE(result.GetRanges(), bar_node, 1u, 2u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 0)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 1)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 2)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 3)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(bar_node, 0)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(bar_node, 1)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(bar_node, 2)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(bar_node, 3)));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(bar_node, 0)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(bar_node, 1)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(bar_node, 2)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(bar_node, 3)));

  EXPECT_EQ(Position(foo_node, 3), GetFirstPosition(3));
  EXPECT_EQ(Position(bar_node, 0), GetLastPosition(3));

  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(foo_node, 0)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(foo_node, 1)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(foo_node, 2)));
  EXPECT_FALSE(
      IsBeforeNonCollapsedContent(Position(foo_node, 3)));  // false at node end

  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(bar_node, 0)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(bar_node, 1)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(bar_node, 2)));
  EXPECT_FALSE(
      IsBeforeNonCollapsedContent(Position(bar_node, 3)));  // false at node end

  // false at node start
  EXPECT_FALSE(IsAfterNonCollapsedContent(Position(foo_node, 0)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(foo_node, 1)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(foo_node, 2)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(foo_node, 3)));

  // false at node start
  EXPECT_FALSE(IsAfterNonCollapsedContent(Position(bar_node, 0)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(bar_node, 1)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(bar_node, 2)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(bar_node, 3)));
}

TEST_F(NGOffsetMappingTest, BRBetweenTextNodes) {
  SetupHtml("t", u"<div id=t>foo<br>bar</div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutText* br = ToLayoutText(foo->NextSibling());
  const LayoutText* bar = ToLayoutText(br->NextSibling());
  const Node* foo_node = foo->GetNode();
  const Node* br_node = br->GetNode();
  const Node* bar_node = bar->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

  EXPECT_EQ("foo\nbar", result.GetText());

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 3u, 0u, 3u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, br_node,
            0u, 1u, 3u, 4u);
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, bar_node,
            0u, 3u, 4u, 7u);

  ASSERT_EQ(3u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);
  TEST_RANGE(result.GetRanges(), br_node, 1u, 2u);
  TEST_RANGE(result.GetRanges(), bar_node, 2u, 3u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 0)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 1)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 2)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 3)));
  EXPECT_EQ(&result.GetUnits()[1],
            GetUnitForPosition(Position::BeforeNode(*br_node)));
  EXPECT_EQ(&result.GetUnits()[1],
            GetUnitForPosition(Position::AfterNode(*br_node)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 0)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 1)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 2)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 3)));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position::BeforeNode(*br_node)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position::AfterNode(*br_node)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(bar_node, 0)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(bar_node, 1)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(bar_node, 2)));
  EXPECT_EQ(7u, *GetTextContentOffset(Position(bar_node, 3)));

  EXPECT_EQ(Position(foo_node, 3), GetFirstPosition(3));
  EXPECT_EQ(Position::BeforeNode(*br_node), GetLastPosition(3));
  EXPECT_EQ(Position::AfterNode(*br_node), GetFirstPosition(4));
  EXPECT_EQ(Position(bar_node, 0), GetLastPosition(4));
}

TEST_F(NGOffsetMappingTest, OneTextNodeWithCollapsedSpace) {
  SetupHtml("t", "<div id=t>foo  bar</div>");
  const Node* node = layout_object_->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

  EXPECT_EQ("foo bar", result.GetText());

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, node, 0u,
            4u, 0u, 4u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kCollapsed, node, 4u,
            5u, 4u, 4u);
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, node, 5u,
            8u, 4u, 7u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), node, 0u, 3u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(node, 0)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(node, 1)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(node, 2)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(node, 3)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(node, 4)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(node, 5)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(node, 6)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(node, 7)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(node, 8)));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(node, 3)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(node, 4)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(node, 5)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(node, 6)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(node, 7)));
  EXPECT_EQ(7u, *GetTextContentOffset(Position(node, 8)));

  EXPECT_EQ(Position(node, 4), GetFirstPosition(4));
  EXPECT_EQ(Position(node, 5), GetLastPosition(4));

  EXPECT_EQ(Position(node, 3),
            StartOfNextNonCollapsedContent(Position(node, 3)));
  EXPECT_EQ(Position(node, 5),
            StartOfNextNonCollapsedContent(Position(node, 4)));
  EXPECT_EQ(Position(node, 5),
            StartOfNextNonCollapsedContent(Position(node, 5)));

  EXPECT_EQ(Position(node, 3), EndOfLastNonCollapsedContent(Position(node, 3)));
  EXPECT_EQ(Position(node, 4), EndOfLastNonCollapsedContent(Position(node, 4)));
  EXPECT_EQ(Position(node, 4), EndOfLastNonCollapsedContent(Position(node, 5)));

  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(node, 0)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(node, 1)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(node, 2)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(node, 3)));
  EXPECT_FALSE(IsBeforeNonCollapsedContent(Position(node, 4)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(node, 5)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(node, 6)));
  EXPECT_TRUE(IsBeforeNonCollapsedContent(Position(node, 7)));
  EXPECT_FALSE(IsBeforeNonCollapsedContent(Position(node, 8)));

  EXPECT_FALSE(IsAfterNonCollapsedContent(Position(node, 0)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(node, 1)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(node, 2)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(node, 3)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(node, 4)));
  EXPECT_FALSE(IsAfterNonCollapsedContent(Position(node, 5)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(node, 6)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(node, 7)));
  EXPECT_TRUE(IsAfterNonCollapsedContent(Position(node, 8)));
}

TEST_F(NGOffsetMappingTest, FullyCollapsedWhiteSpaceNode) {
  SetupHtml("t",
            "<div id=t>"
            "<span id=s1>foo </span>"
            " "
            "<span id=s2>bar</span>"
            "</div>");
  const LayoutText* foo = GetLayoutTextUnder("s1");
  const LayoutText* bar = GetLayoutTextUnder("s2");
  const LayoutText* space = ToLayoutText(layout_object_->NextSibling());
  const Node* foo_node = foo->GetNode();
  const Node* bar_node = bar->GetNode();
  const Node* space_node = space->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

  EXPECT_EQ("foo bar", result.GetText());

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 4u, 0u, 4u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kCollapsed,
            space_node, 0u, 1u, 4u, 4u);
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, bar_node,
            0u, 3u, 4u, 7u);

  ASSERT_EQ(3u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);
  TEST_RANGE(result.GetRanges(), space_node, 1u, 2u);
  TEST_RANGE(result.GetRanges(), bar_node, 2u, 3u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 0)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 1)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 2)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 3)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 4)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(space_node, 0)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(space_node, 1)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 0)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 1)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 2)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 3)));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(foo_node, 4)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(space_node, 0)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(space_node, 1)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(bar_node, 0)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(bar_node, 1)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(bar_node, 2)));
  EXPECT_EQ(7u, *GetTextContentOffset(Position(bar_node, 3)));

  EXPECT_EQ(Position(foo_node, 4), GetFirstPosition(4));
  EXPECT_EQ(Position(bar_node, 0), GetLastPosition(4));

  EXPECT_TRUE(EndOfLastNonCollapsedContent(Position(space_node, 1u)).IsNull());
  EXPECT_TRUE(
      StartOfNextNonCollapsedContent(Position(space_node, 0u)).IsNull());
}

TEST_F(NGOffsetMappingTest, ReplacedElement) {
  SetupHtml("t", "<div id=t>foo <img> bar</div>");
  const LayoutText* foo = ToLayoutText(layout_object_);
  const LayoutObject* img = foo->NextSibling();
  const LayoutText* bar = ToLayoutText(img->NextSibling());
  const Node* foo_node = foo->GetNode();
  const Node* img_node = img->GetNode();
  const Node* bar_node = bar->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 4u, 0u, 4u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, img_node,
            0u, 1u, 4u, 5u);
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, bar_node,
            0u, 4u, 5u, 9u);

  ASSERT_EQ(3u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 1u);
  TEST_RANGE(result.GetRanges(), img_node, 1u, 2u);
  TEST_RANGE(result.GetRanges(), bar_node, 2u, 3u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 0)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 1)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 2)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 3)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 4)));
  EXPECT_EQ(&result.GetUnits()[1],
            GetUnitForPosition(Position::BeforeNode(*img_node)));
  EXPECT_EQ(&result.GetUnits()[1],
            GetUnitForPosition(Position::AfterNode(*img_node)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 0)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 1)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 2)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 3)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(bar_node, 4)));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(3u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position(foo_node, 4)));
  EXPECT_EQ(4u, *GetTextContentOffset(Position::BeforeNode(*img_node)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position::AfterNode(*img_node)));
  EXPECT_EQ(5u, *GetTextContentOffset(Position(bar_node, 0)));
  EXPECT_EQ(6u, *GetTextContentOffset(Position(bar_node, 1)));
  EXPECT_EQ(7u, *GetTextContentOffset(Position(bar_node, 2)));
  EXPECT_EQ(8u, *GetTextContentOffset(Position(bar_node, 3)));
  EXPECT_EQ(9u, *GetTextContentOffset(Position(bar_node, 4)));

  EXPECT_EQ(Position(foo_node, 4), GetFirstPosition(4));
  EXPECT_EQ(Position::BeforeNode(*img_node), GetLastPosition(4));
  EXPECT_EQ(Position::AfterNode(*img_node), GetFirstPosition(5));
  EXPECT_EQ(Position(bar_node, 0), GetLastPosition(5));
}

TEST_F(NGOffsetMappingTest, FirstLetter) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>foo</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* foo_node = div->firstChild();
  const NGOffsetMapping& result = GetOffsetMapping();

  ASSERT_EQ(2u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo_node,
            0u, 1u, 0u, 1u);
  // first leter and remaining text are always in different mapping units.
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, foo_node,
            1u, 3u, 1u, 3u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 2u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 0)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(foo_node, 1)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(foo_node, 2)));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 2)));

  EXPECT_EQ(Position(foo_node, 1), GetFirstPosition(1));
  EXPECT_EQ(Position(foo_node, 1), GetLastPosition(1));
}

TEST_F(NGOffsetMappingTest, FirstLetterWithLeadingSpace) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>  foo</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* foo_node = div->firstChild();
  const NGOffsetMapping& result = GetOffsetMapping();

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kCollapsed, foo_node,
            0u, 2u, 0u, 0u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, foo_node,
            2u, 3u, 0u, 1u);
  // first leter and remaining text are always in different mapping units.
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, foo_node,
            3u, 5u, 1u, 3u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 3u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 0)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(foo_node, 1)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(foo_node, 2)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(foo_node, 3)));
  EXPECT_EQ(&result.GetUnits()[2], GetUnitForPosition(Position(foo_node, 4)));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 0)));
  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 1)));
  EXPECT_EQ(0u, *GetTextContentOffset(Position(foo_node, 2)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(foo_node, 3)));
  EXPECT_EQ(2u, *GetTextContentOffset(Position(foo_node, 4)));

  EXPECT_EQ(Position(foo_node, 0), GetFirstPosition(0));
  EXPECT_EQ(Position(foo_node, 2), GetLastPosition(0));
}

TEST_F(NGOffsetMappingTest, FirstLetterWithoutRemainingText) {
  SetupHtml("t",
            "<style>div:first-letter{color:red}</style>"
            "<div id=t>  f</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* text_node = div->firstChild();
  const NGOffsetMapping& result = GetOffsetMapping();

  ASSERT_EQ(2u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kCollapsed,
            text_node, 0u, 2u, 0u, 0u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, text_node,
            2u, 3u, 0u, 1u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), text_node, 0u, 2u);

  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(text_node, 0)));
  EXPECT_EQ(&result.GetUnits()[0], GetUnitForPosition(Position(text_node, 1)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(text_node, 2)));
  EXPECT_EQ(&result.GetUnits()[1], GetUnitForPosition(Position(text_node, 3)));

  EXPECT_EQ(0u, *GetTextContentOffset(Position(text_node, 0)));
  EXPECT_EQ(0u, *GetTextContentOffset(Position(text_node, 1)));
  EXPECT_EQ(0u, *GetTextContentOffset(Position(text_node, 2)));
  EXPECT_EQ(1u, *GetTextContentOffset(Position(text_node, 3)));

  EXPECT_EQ(Position(text_node, 0), GetFirstPosition(0));
  EXPECT_EQ(Position(text_node, 2), GetLastPosition(0));
}

TEST_F(NGOffsetMappingTest, FirstLetterInDifferentBlock) {
  SetupHtml("t",
            "<style>:first-letter{float:right}</style><div id=t>foo</div>");
  Element* div = GetDocument().getElementById("t");
  const Node* text_node = div->firstChild();

  auto* mapping0 = NGOffsetMapping::GetFor(Position(text_node, 0));
  auto* mapping1 = NGOffsetMapping::GetFor(Position(text_node, 1));
  auto* mapping2 = NGOffsetMapping::GetFor(Position(text_node, 2));
  auto* mapping3 = NGOffsetMapping::GetFor(Position(text_node, 3));

  ASSERT_TRUE(mapping0);
  ASSERT_TRUE(mapping1);
  ASSERT_TRUE(mapping2);
  ASSERT_TRUE(mapping3);

  // GetNGOffsetmappingFor() returns different mappings for offset 0 and other
  // offsets, because first-letter is laid out in a different block.
  EXPECT_NE(mapping0, mapping1);
  EXPECT_EQ(mapping1, mapping2);
  EXPECT_EQ(mapping2, mapping3);

  const NGOffsetMapping& first_letter_result = *mapping0;
  ASSERT_EQ(1u, first_letter_result.GetUnits().size());
  TEST_UNIT(first_letter_result.GetUnits()[0],
            NGOffsetMappingUnitType::kIdentity, text_node, 0u, 1u, 0u, 1u);
  ASSERT_EQ(1u, first_letter_result.GetRanges().size());
  TEST_RANGE(first_letter_result.GetRanges(), text_node, 0u, 1u);

  const NGOffsetMapping& remaining_text_result = *mapping1;
  ASSERT_EQ(1u, remaining_text_result.GetUnits().size());
  TEST_UNIT(remaining_text_result.GetUnits()[0],
            NGOffsetMappingUnitType::kIdentity, text_node, 1u, 3u, 1u, 3u);
  ASSERT_EQ(1u, remaining_text_result.GetRanges().size());
  TEST_RANGE(remaining_text_result.GetRanges(), text_node, 0u, 1u);

  EXPECT_EQ(
      &first_letter_result.GetUnits()[0],
      first_letter_result.GetMappingUnitForPosition(Position(text_node, 0)));
  EXPECT_EQ(
      &remaining_text_result.GetUnits()[0],
      remaining_text_result.GetMappingUnitForPosition(Position(text_node, 1)));
  EXPECT_EQ(
      &remaining_text_result.GetUnits()[0],
      remaining_text_result.GetMappingUnitForPosition(Position(text_node, 2)));
  EXPECT_EQ(
      &remaining_text_result.GetUnits()[0],
      remaining_text_result.GetMappingUnitForPosition(Position(text_node, 3)));

  EXPECT_EQ(0u,
            *first_letter_result.GetTextContentOffset(Position(text_node, 0)));
  EXPECT_EQ(
      1u, *remaining_text_result.GetTextContentOffset(Position(text_node, 1)));
  EXPECT_EQ(
      2u, *remaining_text_result.GetTextContentOffset(Position(text_node, 2)));
  EXPECT_EQ(
      3u, *remaining_text_result.GetTextContentOffset(Position(text_node, 3)));

  EXPECT_EQ(Position(text_node, 1), first_letter_result.GetFirstPosition(1));
  EXPECT_EQ(Position(text_node, 1), first_letter_result.GetLastPosition(1));
  EXPECT_EQ(Position(text_node, 1), remaining_text_result.GetFirstPosition(1));
  EXPECT_EQ(Position(text_node, 1), remaining_text_result.GetLastPosition(1));
}

TEST_F(NGOffsetMappingTest, WhiteSpaceTextNodeWithoutLayoutText) {
  SetupHtml("t", "<div id=t> <span>foo</span></div>");
  Element* div = GetDocument().getElementById("t");
  const Node* text_node = div->firstChild();

  EXPECT_TRUE(EndOfLastNonCollapsedContent(Position(text_node, 1u)).IsNull());
  EXPECT_TRUE(StartOfNextNonCollapsedContent(Position(text_node, 0u)).IsNull());
}

TEST_F(NGOffsetMappingTest, ContainerWithGeneratedContent) {
  SetupHtml("t",
            "<style>#s::before{content:'bar'} #s::after{content:'baz'}</style>"
            "<div id=t><span id=s>foo</span></div>");
  const Element* span = GetElementById("s");
  const Node* text = span->firstChild();
  const LayoutObject& before = *span->GetPseudoElement(kPseudoIdBefore)
                                    ->GetLayoutObject()
                                    ->SlowFirstChild();
  const LayoutObject& after = *span->GetPseudoElement(kPseudoIdAfter)
                                   ->GetLayoutObject()
                                   ->SlowFirstChild();
  const NGOffsetMapping& result = GetOffsetMapping();

  EXPECT_EQ((Vector<NGOffsetMappingUnit>{
                NGOffsetMappingUnit(kIdentity, before, 0u, 3u, 0u, 3u),
                NGOffsetMappingUnit(kIdentity, *text->GetLayoutObject(), 0u, 3u,
                                    3u, 6u),
                NGOffsetMappingUnit(kIdentity, after, 0u, 3u, 6u, 9u)}),
            result.GetUnits());

  // Verify |GetMappingUnitsForLayoutObject()| for ::before and ::after
  EXPECT_EQ((Vector<NGOffsetMappingUnit>{
                NGOffsetMappingUnit(kIdentity, before, 0u, 3u, 0u, 3u)}),
            result.GetMappingUnitsForLayoutObject(before));
  EXPECT_EQ((Vector<NGOffsetMappingUnit>{
                NGOffsetMappingUnit(kIdentity, after, 0u, 3u, 6u, 9u)}),
            result.GetMappingUnitsForLayoutObject(after));
}

TEST_F(NGOffsetMappingTest,
       ContainerWithGeneratedContentWithCollapsedWhitespace) {
  SetupHtml("t",
            "<style>"
            "#t::before { content: '  a   bc'; }"
            "#t::first-letter { font-weight: bold; }"
            "</style><div id=t>def</div>");
  const Element& target = *GetElementById("t");
  const LayoutText& remaining_part =
      *ToLayoutText(target.GetPseudoElement(kPseudoIdBefore)
                        ->GetLayoutObject()
                        ->SlowLastChild());
  const LayoutObject& first_letter_part = *remaining_part.GetFirstLetterPart();
  const NGOffsetMapping& result = GetOffsetMapping();
  const LayoutText& target_text =
      ToLayoutText(*target.firstChild()->GetLayoutObject());

  EXPECT_EQ(
      (Vector<NGOffsetMappingUnit>{
          NGOffsetMappingUnit(kCollapsed, first_letter_part, 0u, 2u, 0u, 0u),
          NGOffsetMappingUnit(kIdentity, first_letter_part, 2u, 3u, 0u, 1u),
          NGOffsetMappingUnit(kIdentity, remaining_part, 0u, 1u, 1u, 2u),
          NGOffsetMappingUnit(kCollapsed, remaining_part, 1u, 3u, 2u, 2u),
          NGOffsetMappingUnit(kIdentity, remaining_part, 3u, 5u, 2u, 4u),
          NGOffsetMappingUnit(kIdentity, target_text, 0u, 3u, 4u, 7u)}),
      result.GetUnits());

  // Verify |GetMappingUnitsForLayoutObject()| for ::first-letter
  EXPECT_EQ(
      (Vector<NGOffsetMappingUnit>{
          NGOffsetMappingUnit(kCollapsed, first_letter_part, 0u, 2u, 0u, 0u),
          NGOffsetMappingUnit(kIdentity, first_letter_part, 2u, 3u, 0u, 1u)}),
      result.GetMappingUnitsForLayoutObject(first_letter_part));
  EXPECT_EQ(
      (Vector<NGOffsetMappingUnit>{
          NGOffsetMappingUnit(kIdentity, remaining_part, 0u, 1u, 1u, 2u),
          NGOffsetMappingUnit(kCollapsed, remaining_part, 1u, 3u, 2u, 2u),
          NGOffsetMappingUnit(kIdentity, remaining_part, 3u, 5u, 2u, 4u)}),
      result.GetMappingUnitsForLayoutObject(remaining_part));
}

TEST_F(NGOffsetMappingTest, Table) {
  SetupHtml("t", "<table><tr><td id=t>  foo  </td></tr></table>");

  const Node* foo_node = layout_object_->GetNode();
  const NGOffsetMapping& result = GetOffsetMapping();

  EXPECT_EQ("foo", result.GetText());

  ASSERT_EQ(3u, result.GetUnits().size());
  TEST_UNIT(result.GetUnits()[0], NGOffsetMappingUnitType::kCollapsed, foo_node,
            0u, 2u, 0u, 0u);
  TEST_UNIT(result.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, foo_node,
            2u, 5u, 0u, 3u);
  TEST_UNIT(result.GetUnits()[2], NGOffsetMappingUnitType::kCollapsed, foo_node,
            5u, 7u, 3u, 3u);

  ASSERT_EQ(1u, result.GetRanges().size());
  TEST_RANGE(result.GetRanges(), foo_node, 0u, 3u);
}

TEST_F(NGOffsetMappingTest, GetMappingForInlineBlock) {
  SetupHtml("t",
            "<div id=t>foo"
            "<span style='display: inline-block' id=span> bar </span>"
            "baz</div>");

  const Element* div = GetElementById("t");
  const Element* span = GetElementById("span");

  const NGOffsetMapping* div_mapping =
      NGOffsetMapping::GetFor(Position(div->firstChild(), 0));
  const NGOffsetMapping* span_mapping =
      NGOffsetMapping::GetFor(Position(span->firstChild(), 0));

  // NGOffsetMapping::GetFor for Before/AfterAnchor of an inline block should
  // return the mapping of the containing block, not of the inline block itself.

  const NGOffsetMapping* span_before_mapping =
      NGOffsetMapping::GetFor(Position::BeforeNode(*span));
  EXPECT_EQ(div_mapping, span_before_mapping);
  EXPECT_NE(span_mapping, span_before_mapping);

  const NGOffsetMapping* span_after_mapping =
      NGOffsetMapping::GetFor(Position::AfterNode(*span));
  EXPECT_EQ(div_mapping, span_after_mapping);
  EXPECT_NE(span_mapping, span_after_mapping);
}

TEST_F(NGOffsetMappingTest, NoWrapSpaceAndCollapsibleSpace) {
  SetupHtml("t",
            "<div id=t>"
            "<span style='white-space: nowrap' id=span>foo </span>"
            " bar"
            "</div>");

  const Element* span = GetElementById("span");
  const Node* foo = span->firstChild();
  const Node* bar = span->nextSibling();
  const NGOffsetMapping& mapping = GetOffsetMapping();

  // NGInlineItemsBuilder inserts a ZWS to indicate break opportunity.
  EXPECT_EQ(String(u"foo \u200Bbar"), mapping.GetText());

  // Should't map any character in DOM to the generated ZWS.
  ASSERT_EQ(3u, mapping.GetUnits().size());
  TEST_UNIT(mapping.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, foo, 0u,
            4u, 0u, 4u);
  TEST_UNIT(mapping.GetUnits()[1], NGOffsetMappingUnitType::kCollapsed, bar, 0u,
            1u, 5u, 5u);
  TEST_UNIT(mapping.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, bar, 1u,
            4u, 5u, 8u);
}

TEST_F(NGOffsetMappingTest, BiDiAroundForcedBreakInPreLine) {
  SetupHtml("t",
            "<div id=t style='white-space: pre-line'>"
            "<bdo dir=rtl id=bdo>foo\nbar</bdo></div>");

  const Node* text = GetElementById("bdo")->firstChild();
  const NGOffsetMapping& mapping = GetOffsetMapping();

  EXPECT_EQ(String(u"\u202Efoo\u202C"
                   u"\n"
                   u"\u202Ebar\u202C"),
            mapping.GetText());

  // Offset mapping should skip generated BiDi control characters.
  ASSERT_EQ(3u, mapping.GetUnits().size());
  TEST_UNIT(mapping.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, text, 0u,
            3u, 1u, 4u);  // "foo"
  TEST_UNIT(mapping.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, text, 3u,
            4u, 5u, 6u);  // "\n"
  TEST_UNIT(mapping.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, text, 4u,
            7u, 7u, 10u);  // "bar"
  TEST_RANGE(mapping.GetRanges(), text, 0u, 3u);
}

TEST_F(NGOffsetMappingTest, BiDiAroundForcedBreakInPreWrap) {
  SetupHtml("t",
            "<div id=t style='white-space: pre-wrap'>"
            "<bdo dir=rtl id=bdo>foo\nbar</bdo></div>");

  const Node* text = GetElementById("bdo")->firstChild();
  const NGOffsetMapping& mapping = GetOffsetMapping();

  EXPECT_EQ(String(u"\u202Efoo\u202C"
                   u"\n"
                   u"\u202Ebar\u202C"),
            mapping.GetText());

  // Offset mapping should skip generated BiDi control characters.
  ASSERT_EQ(3u, mapping.GetUnits().size());
  TEST_UNIT(mapping.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, text, 0u,
            3u, 1u, 4u);  // "foo"
  TEST_UNIT(mapping.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, text, 3u,
            4u, 5u, 6u);  // "\n"
  TEST_UNIT(mapping.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, text, 4u,
            7u, 7u, 10u);  // "bar"
  TEST_RANGE(mapping.GetRanges(), text, 0u, 3u);
}

TEST_F(NGOffsetMappingTest, BiDiAroundForcedBreakInPre) {
  SetupHtml("t",
            "<div id=t style='white-space: pre'>"
            "<bdo dir=rtl id=bdo>foo\nbar</bdo></div>");

  const Node* text = GetElementById("bdo")->firstChild();
  const NGOffsetMapping& mapping = GetOffsetMapping();

  EXPECT_EQ(String(u"\u202Efoo\u202C"
                   u"\n"
                   u"\u202Ebar\u202C"),
            mapping.GetText());

  // Offset mapping should skip generated BiDi control characters.
  ASSERT_EQ(3u, mapping.GetUnits().size());
  TEST_UNIT(mapping.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, text, 0u,
            3u, 1u, 4u);  // "foo"
  TEST_UNIT(mapping.GetUnits()[1], NGOffsetMappingUnitType::kIdentity, text, 3u,
            4u, 5u, 6u);  // "\n"
  TEST_UNIT(mapping.GetUnits()[2], NGOffsetMappingUnitType::kIdentity, text, 4u,
            7u, 7u, 10u);  // "bar"
  TEST_RANGE(mapping.GetRanges(), text, 0u, 3u);
}

TEST_F(NGOffsetMappingTest, SoftHyphen) {
  LoadAhem();
  SetupHtml(
      "t",
      "<div id=t style='font: 10px/10px Ahem; width: 40px'>abc&shy;def</div>");

  const Node* text = GetElementById("t")->firstChild();
  const NGOffsetMapping& mapping = GetOffsetMapping();

  // Line wrapping and hyphenation are oblivious to offset mapping.
  ASSERT_EQ(1u, mapping.GetUnits().size());
  TEST_UNIT(mapping.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, text, 0u,
            7u, 0u, 7u);
  TEST_RANGE(mapping.GetRanges(), text, 0u, 1u);
}

// For http://crbug.com/965353
TEST_F(NGOffsetMappingTest, PreWrapAndReusing) {
  // Note: "white-space: break-space" yields same result.
  SetupHtml("t", "<p id='t' style='white-space: pre-wrap'>abc</p>");
  Element& target = *GetDocument().getElementById("t");

  // Change to <p id=t>abc xyz</p>
  Text& text = *Text::Create(GetDocument(), " xyz");
  target.appendChild(&text);
  UpdateAllLifecyclePhasesForTest();

  // Change to <p id=t> xyz</p>. We attempt to reuse " xyz".
  target.firstChild()->remove();
  UpdateAllLifecyclePhasesForTest();

  const NGOffsetMapping& mapping = GetOffsetMapping();
  EXPECT_EQ(String(u" \u200Bxyz"), mapping.GetText())
      << "We have ZWS after leading preserved space.";
  EXPECT_EQ((Vector<NGOffsetMappingUnit>{
                NGOffsetMappingUnit(kIdentity, *text.GetLayoutObject(), 0u, 1u,
                                    0u, 1u),
                NGOffsetMappingUnit(kIdentity, *text.GetLayoutObject(), 1u, 4u,
                                    2u, 5u),
            }),
            mapping.GetUnits());
}

TEST_F(NGOffsetMappingTest, RestoreTrailingCollapsibleSpaceReplace) {
  // A space inside <b> is collapsed by during handling "\n" then it is restored
  // by handling a newline. Restored space is removed at end of block.
  // When RestoreTrailingCollapsibleSpace(), units are:
  //  0: kIdentity text in <a>, dom=0,1 content=0,1
  //  1: kCollapsed text in <b>, dom=0,1, content=2,2
  //  2: kCollapsed "\n", dom=0,1, content=2,2
  // layout_text is a child of <b> and offset is 2
  SetupHtml("t",
            "<div id=t>"
            "<a style='white-space: pre-wrap;'> </a><b> </b>\n<i> </i>"
            "</div>");
  const NGOffsetMapping& result = GetOffsetMapping();
  const LayoutObject& layout_object_a = *layout_object_;
  const LayoutObject& layout_object_b = *layout_object_a.NextSibling();
  const LayoutObject& newline = *layout_object_b.NextSibling();
  const LayoutObject& layout_object_i = *newline.NextSibling();
  EXPECT_EQ(
      (Vector<NGOffsetMappingUnit>{
          NGOffsetMappingUnit(kIdentity, *layout_object_a.SlowFirstChild(), 0u,
                              1u, 0u, 1u),
          NGOffsetMappingUnit(kCollapsed, *layout_object_b.SlowFirstChild(), 0u,
                              1u, 2u, 2u),
          NGOffsetMappingUnit(kCollapsed, newline, 0u, 1u, 2u, 2u),
          NGOffsetMappingUnit(kCollapsed, *layout_object_i.SlowFirstChild(), 0u,
                              1u, 2u, 2u),
      }),
      result.GetUnits());
}

TEST_F(NGOffsetMappingTest, RestoreTrailingCollapsibleSpaceReplaceKeep) {
  // A space inside <b> is collapsed by during handling "\n" then it is restored
  // by handling a newline.
  // When RestoreTrailingCollapsibleSpace(), units are:
  //  0: kIdentity text in <a>, dom=0,1 content=0,1
  //  1: kCollapsed text in <b>, dom=0,1, content=2,2
  //  2: kCollapsed "\n", dom=0,1, content=2,2
  // layout_text is a child of <b> and offset is 2
  SetupHtml("t",
            "<div id=t>"
            "<a style='white-space: pre-wrap;'> </a><b> </b>\n<i>x</i>"
            "</div>");
  const NGOffsetMapping& result = GetOffsetMapping();
  const LayoutObject& layout_object_a = *layout_object_;
  const LayoutObject& layout_object_b = *layout_object_a.NextSibling();
  const LayoutObject& newline = *layout_object_b.NextSibling();
  const LayoutObject& layout_object_i = *newline.NextSibling();
  EXPECT_EQ(
      (Vector<NGOffsetMappingUnit>{
          NGOffsetMappingUnit(kIdentity, *layout_object_a.SlowFirstChild(), 0u,
                              1u, 0u, 1u),
          NGOffsetMappingUnit(kIdentity, *layout_object_b.SlowFirstChild(), 0u,
                              1u, 2u, 3u),
          NGOffsetMappingUnit(kCollapsed, newline, 0u, 1u, 3u, 3u),
          NGOffsetMappingUnit(kIdentity, *layout_object_i.SlowFirstChild(), 0u,
                              1u, 3u, 4u),
      }),
      result.GetUnits());
}

TEST_F(NGOffsetMappingTest, RestoreTrailingCollapsibleSpaceNone) {
  SetupHtml("t",
            "<div id=t>"
            "<a>x</a><b>   </b>\n<i>y</i>"
            "</div>");
  const NGOffsetMapping& result = GetOffsetMapping();
  const LayoutObject& layout_object_a = *layout_object_;
  const LayoutObject& layout_object_b = *layout_object_a.NextSibling();
  const LayoutObject& newline = *layout_object_b.NextSibling();
  const LayoutObject& layout_object_i = *newline.NextSibling();
  EXPECT_EQ(
      (Vector<NGOffsetMappingUnit>{
          NGOffsetMappingUnit(kIdentity, *layout_object_a.SlowFirstChild(), 0u,
                              1u, 0u, 1u),
          // We take the first space character.
          NGOffsetMappingUnit(kIdentity, *layout_object_b.SlowFirstChild(), 0u,
                              1u, 1u, 2u),
          NGOffsetMappingUnit(kCollapsed, *layout_object_b.SlowFirstChild(), 1u,
                              3u, 2u, 2u),
          NGOffsetMappingUnit(kCollapsed, newline, 0u, 1u, 2u, 2u),
          NGOffsetMappingUnit(kIdentity, *layout_object_i.SlowFirstChild(), 0u,
                              1u, 2u, 3u),
      }),
      result.GetUnits());
}

TEST_F(NGOffsetMappingTest, RestoreTrailingCollapsibleSpaceSplit) {
  // Spaces inside <b> is collapsed by during handling "\n" then it is restored
  // by handling a newline. Restored space is removed at end of block.
  // When RestoreTrailingCollapsibleSpace(), units are:
  //  0: kIdentity text in <a>, dom=0,1 content=0,1
  //  1: kCollapsed text in <b>, dom=0,3, content=2,2
  //  2: kCollapsed "\n", dom=0,1 content=3,3
  // layout_text is a child of <b> and offset is 2
  SetupHtml("t",
            "<div id=t>"
            "<a style='white-space: pre-wrap;'> </a><b>   </b>\n<i> </i>"
            "</div>");
  const NGOffsetMapping& result = GetOffsetMapping();
  const LayoutObject& layout_object_a = *layout_object_;
  const LayoutObject& layout_object_b = *layout_object_a.NextSibling();
  const LayoutObject& newline = *layout_object_b.NextSibling();
  const LayoutObject& layout_object_i = *newline.NextSibling();
  EXPECT_EQ(
      (Vector<NGOffsetMappingUnit>{
          NGOffsetMappingUnit(kIdentity, *layout_object_a.SlowFirstChild(), 0u,
                              1u, 0u, 1u),
          NGOffsetMappingUnit(kCollapsed, *layout_object_b.SlowFirstChild(), 0u,
                              3u, 2u, 2u),
          NGOffsetMappingUnit(kCollapsed, newline, 0u, 1u, 2u, 2u),
          NGOffsetMappingUnit(kCollapsed, *layout_object_i.SlowFirstChild(), 0u,
                              1u, 2u, 2u),
      }),
      result.GetUnits());
}

TEST_F(NGOffsetMappingTest, RestoreTrailingCollapsibleSpaceSplitKeep) {
  // Spaces inside <b> is collapsed by during handling "\n" then it is restored
  // by handling a space in <i>.
  // When RestoreTrailingCollapsibleSpace(), units are:
  //  0: kIdentity text in <a>, dom=0,1 content=0,1
  //  1: kCollapsed text in <b>, dom=0,3, content=2,2
  //  2: kCollapsed "\n", dom=0,1 content=3,3
  // layout_text is a child of <b> and offset is 2
  SetupHtml("t",
            "<div id=t>"
            "<a style='white-space: pre-wrap;'> </a><b>   </b>\n<i>x</i>"
            "</div>");
  const NGOffsetMapping& result = GetOffsetMapping();
  const LayoutObject& layout_object_a = *layout_object_;
  const LayoutObject& layout_object_b = *layout_object_a.NextSibling();
  const LayoutObject& newline = *layout_object_b.NextSibling();
  const LayoutObject& layout_object_i = *newline.NextSibling();
  EXPECT_EQ(
      (Vector<NGOffsetMappingUnit>{
          NGOffsetMappingUnit(kIdentity, *layout_object_a.SlowFirstChild(), 0u,
                              1u, 0u, 1u),
          NGOffsetMappingUnit(kIdentity, *layout_object_b.SlowFirstChild(), 0u,
                              1u, 2u, 3u),
          NGOffsetMappingUnit(kCollapsed, *layout_object_b.SlowFirstChild(), 1u,
                              3u, 3u, 3u),
          NGOffsetMappingUnit(kCollapsed, newline, 0u, 1u, 3u, 3u),
          NGOffsetMappingUnit(kIdentity, *layout_object_i.SlowFirstChild(), 0u,
                              1u, 3u, 4u),
      }),
      result.GetUnits());
}

TEST_F(NGOffsetMappingTest, TextOverflowEllipsis) {
  LoadAhem();
  SetupHtml("t",
            "<div id=t style='font: 10px/10px Ahem; width: 30px; overflow: "
            "hidden; text-overflow: ellipsis'>123456</div>");

  const Node* text = GetElementById("t")->firstChild();
  const NGOffsetMapping& mapping = GetOffsetMapping();

  // Ellipsis is oblivious to offset mapping.
  ASSERT_EQ(1u, mapping.GetUnits().size());
  TEST_UNIT(mapping.GetUnits()[0], NGOffsetMappingUnitType::kIdentity, text, 0u,
            6u, 0u, 6u);
  TEST_RANGE(mapping.GetRanges(), text, 0u, 1u);
}

// https://crbug.com/967106
TEST_F(NGOffsetMappingTest, StartOfNextNonCollapsedContentWithPseudo) {
  // The white spaces are necessary for bug repro. Do not remove them.
  SetupHtml("t", R"HTML(
    <style>span#quote::before { content: '"'}</style>
    <div id=t>
      <span>foo </span>
      <span id=quote>bar</span>
    </div>)HTML");

  const Element* quote = GetElementById("quote");
  const Node* text = quote->previousSibling();
  const Position position = Position::FirstPositionInNode(*text);

  EXPECT_EQ(Position(),
            GetOffsetMapping().StartOfNextNonCollapsedContent(position));
}

// https://crbug.com/967106
TEST_F(NGOffsetMappingTest, EndOfLastNonCollapsedContentWithPseudo) {
  // The white spaces are necessary for bug repro. Do not remove them.
  SetupHtml("t", R"HTML(
    <style>span#quote::after { content: '" '}</style>
    <div id=t>
      <span id=quote>foo</span>
      <span>bar</span>
    </div>)HTML");

  const Element* quote = GetElementById("quote");
  const Node* text = quote->nextSibling();
  const Position position = Position::LastPositionInNode(*text);

  EXPECT_EQ(Position(),
            GetOffsetMapping().EndOfLastNonCollapsedContent(position));
}

TEST_F(NGOffsetMappingTest, WordBreak) {
  SetupHtml("t", "<div id=t>a<wbr>b</div>");

  const LayoutObject& text_a = *layout_object_;
  const LayoutObject& wbr = *text_a.NextSibling();
  const LayoutObject& text_b = *wbr.NextSibling();
  const NGOffsetMapping& result = GetOffsetMapping();

  EXPECT_EQ((Vector<NGOffsetMappingUnit>{
                NGOffsetMappingUnit(kIdentity, text_a, 0u, 1u, 0u, 1u),
                NGOffsetMappingUnit(kIdentity, wbr, 0u, 1u, 1u, 2u),
                NGOffsetMappingUnit(kIdentity, text_b, 0u, 1u, 2u, 3u)}),
            result.GetUnits());

  EXPECT_EQ((Vector<NGOffsetMappingUnit>{
                NGOffsetMappingUnit(kIdentity, wbr, 0u, 1u, 1u, 2u)}),
            result.GetMappingUnitsForLayoutObject(wbr));
}

// Test |GetOffsetMapping| which is available both for LayoutNG and for legacy.
class NGOffsetMappingGetterTest : public RenderingTest,
                                  public testing::WithParamInterface<bool>,
                                  private ScopedLayoutNGForTest {
 public:
  NGOffsetMappingGetterTest() : ScopedLayoutNGForTest(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(NGOffsetMappingTest,
                         NGOffsetMappingGetterTest,
                         testing::Bool());

TEST_P(NGOffsetMappingGetterTest, Get) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      Whitespaces   in this text   should be   collapsed.
    </div>
  )HTML");
  auto* layout_block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  DCHECK(layout_block_flow->ChildrenInline());

  // For the purpose of this test, ensure this is laid out by each layout
  // engine.
  DCHECK_EQ(layout_block_flow->IsLayoutNGMixin(), GetParam());

  const NGOffsetMapping* mapping =
      NGInlineNode::GetOffsetMapping(layout_block_flow);
  EXPECT_TRUE(mapping);

  const String& text_content = mapping->GetText();
  EXPECT_EQ(text_content, "Whitespaces in this text should be collapsed.");
}

}  // namespace blink
