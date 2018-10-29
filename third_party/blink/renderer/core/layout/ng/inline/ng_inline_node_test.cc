// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class NGInlineNodeForTest : public NGInlineNode {
 public:
  using NGInlineNode::NGInlineNode;

  std::string Text() const { return Data().text_content.Utf8().data(); }
  Vector<NGInlineItem>& Items() { return MutableData()->items; }
  static Vector<NGInlineItem>& Items(NGInlineNodeData& data) {
    return data.items;
  }

  void Append(const String& text,
              const ComputedStyle* style = nullptr,
              LayoutObject* layout_object = nullptr) {
    NGInlineNodeData* data = MutableData();
    unsigned start = data->text_content.length();
    data->text_content.append(text);
    data->items.push_back(NGInlineItem(NGInlineItem::kText, start,
                                       start + text.length(), style,
                                       layout_object));
    data->is_empty_inline_ = false;
  }

  void Append(UChar character) {
    NGInlineNodeData* data = MutableData();
    data->text_content.append(character);
    unsigned end = data->text_content.length();
    data->items.push_back(
        NGInlineItem(NGInlineItem::kBidiControl, end - 1, end, nullptr));
    data->is_bidi_enabled_ = true;
    data->is_empty_inline_ = false;
  }

  void ClearText() {
    NGInlineNodeData* data = MutableData();
    data->text_content = String();
    data->items.clear();
    data->is_empty_inline_ = true;
  }

  void SegmentText() {
    NGInlineNodeData* data = MutableData();
    data->is_bidi_enabled_ = true;
    NGInlineNode::SegmentText(data);
  }

  void CollectInlines() { NGInlineNode::CollectInlines(MutableData()); }
  void ShapeText() { NGInlineNode::ShapeText(MutableData()); }
};

class NGInlineNodeTest : public NGLayoutTest {
 protected:
  void SetUp() override {
    NGLayoutTest::SetUp();
    style_ = ComputedStyle::Create();
    style_->GetFont().Update(nullptr);
  }

  void SetupHtml(const char* id, String html) {
    SetBodyInnerHTML(html);
    layout_block_flow_ = ToLayoutNGBlockFlow(GetLayoutObjectByElementId(id));
    layout_object_ = layout_block_flow_->FirstChild();
    style_ = layout_object_ ? layout_object_->Style() : nullptr;
  }

  void UseLayoutObjectAndAhem() {
    // Get Ahem from document. Loading "Ahem.woff" using |createTestFont| fails
    // on linux_chromium_asan_rel_ng.
    LoadAhem();
    SetupHtml("t", "<div id=t style='font:10px Ahem'>test</div>");
  }

  NGInlineNodeForTest CreateInlineNode() {
    if (!layout_block_flow_)
      SetupHtml("t", "<div id=t style='font:10px'>test</div>");
    NGInlineNodeForTest node(layout_block_flow_);
    node.InvalidatePrepareLayoutForTest();
    return node;
  }

  void CreateLine(
      NGInlineNode node,
      Vector<scoped_refptr<const NGPhysicalTextFragment>>* fragments_out) {
    NGPhysicalSize icb_size(LayoutUnit(200), LayoutUnit(200));

    NGConstraintSpace constraint_space =
        NGConstraintSpaceBuilder(WritingMode::kHorizontalTb, icb_size)
            .SetAvailableSize({LayoutUnit::Max(), LayoutUnit(-1)})
            .ToConstraintSpace(WritingMode::kHorizontalTb);
    NGInlineChildLayoutContext context;
    scoped_refptr<NGLayoutResult> result =
        NGInlineLayoutAlgorithm(node, constraint_space,
                                nullptr /* break_token */, &context)
            .Layout();

    const NGPhysicalLineBoxFragment* line =
        ToNGPhysicalLineBoxFragment(result->PhysicalFragment().get());
    for (const auto& child : line->Children()) {
      fragments_out->push_back(ToNGPhysicalTextFragment(child.get()));
    }
  }

  const String& GetText() const {
    NGInlineNodeData* data = layout_block_flow_->GetNGInlineNodeData();
    CHECK(data);
    return data->text_content;
  }

  Vector<NGInlineItem>& Items() {
    NGInlineNodeData* data = layout_block_flow_->GetNGInlineNodeData();
    CHECK(data);
    return NGInlineNodeForTest::Items(*data);
  }

  void ForceLayout() { GetDocument().body()->OffsetTop(); }

  scoped_refptr<const ComputedStyle> style_;
  LayoutNGBlockFlow* layout_block_flow_ = nullptr;
  LayoutObject* layout_object_ = nullptr;
  FontCachePurgePreventer purge_preventer_;
};

#define TEST_ITEM_TYPE_OFFSET(item, type, start, end) \
  EXPECT_EQ(NGInlineItem::type, item.Type());         \
  EXPECT_EQ(start, item.StartOffset());               \
  EXPECT_EQ(end, item.EndOffset())

#define TEST_ITEM_TYPE_OFFSET_LEVEL(item, type, start, end, level) \
  EXPECT_EQ(NGInlineItem::type, item.Type());                      \
  EXPECT_EQ(start, item.StartOffset());                            \
  EXPECT_EQ(end, item.EndOffset());                                \
  EXPECT_EQ(level, item.BidiLevel())

#define TEST_ITEM_OFFSET_DIR(item, start, end, direction) \
  EXPECT_EQ(start, item.StartOffset());                   \
  EXPECT_EQ(end, item.EndOffset());                       \
  EXPECT_EQ(direction, item.Direction())

TEST_F(NGInlineNodeTest, CollectInlinesText) {
  SetupHtml("t", "<div id=t>Hello <span>inline</span> world.</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_FALSE(node.IsBidiEnabled());
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 6u);
  TEST_ITEM_TYPE_OFFSET(items[1], kOpenTag, 6u, 6u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 6u, 12u);
  TEST_ITEM_TYPE_OFFSET(items[3], kCloseTag, 12u, 12u);
  TEST_ITEM_TYPE_OFFSET(items[4], kText, 12u, 19u);
  EXPECT_EQ(5u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesBR) {
  SetupHtml("t", u"<div id=t>Hello<br>World</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_EQ("Hello\nWorld", node.Text());
  EXPECT_FALSE(node.IsBidiEnabled());
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 5u);
  TEST_ITEM_TYPE_OFFSET(items[1], kControl, 5u, 6u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 6u, 11u);
  EXPECT_EQ(3u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesFloat) {
  SetupHtml("t",
            "<div id=t>"
            "abc"
            "<span style='float:right'>DEF</span>"
            "ghi"
            "<span style='float:left'>JKL</span>"
            "mno"
            "</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_EQ(u8"abc\uFFFCghi\uFFFCmno", node.Text())
      << "floats are appeared as an object replacement character";
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(5u, items.size());
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 3u);
  TEST_ITEM_TYPE_OFFSET(items[1], kFloating, 3u, 4u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 4u, 7u);
  TEST_ITEM_TYPE_OFFSET(items[3], kFloating, 7u, 8u);
  TEST_ITEM_TYPE_OFFSET(items[4], kText, 8u, 11u);
}

TEST_F(NGInlineNodeTest, CollectInlinesInlineBlock) {
  SetupHtml("t",
            "<div id=t>"
            "abc<span style='display:inline-block'>DEF</span>jkl"
            "</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_EQ(u8"abc\uFFFCjkl", node.Text())
      << "inline-block is appeared as an object replacement character";
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(3u, items.size());
  TEST_ITEM_TYPE_OFFSET(items[0], kText, 0u, 3u);
  TEST_ITEM_TYPE_OFFSET(items[1], kAtomicInline, 3u, 4u);
  TEST_ITEM_TYPE_OFFSET(items[2], kText, 4u, 7u);
}

TEST_F(NGInlineNodeTest, CollectInlinesUTF16) {
  SetupHtml("t", u"<div id=t>Hello \u3042</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  // |CollectInlines()| sets |IsBidiEnabled()| for any UTF-16 strings.
  EXPECT_TRUE(node.IsBidiEnabled());
  // |SegmentText()| analyzes the string and resets |IsBidiEnabled()| if all
  // characters are LTR.
  node.SegmentText();
  EXPECT_FALSE(node.IsBidiEnabled());
}

TEST_F(NGInlineNodeTest, CollectInlinesRtl) {
  SetupHtml("t", u"<div id=t>Hello \u05E2</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
}

TEST_F(NGInlineNodeTest, CollectInlinesRtlWithSpan) {
  SetupHtml("t", u"<div id=t dir=rtl>\u05E2 <span>\u05E2</span> \u05E2</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[0], kText, 0u, 2u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[1], kOpenTag, 2u, 2u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[2], kText, 2u, 3u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[3], kCloseTag, 3u, 3u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[4], kText, 3u, 5u, 1u);
  EXPECT_EQ(5u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesMixedText) {
  SetupHtml("t", u"<div id=t>Hello, \u05E2 <span>\u05E2</span></div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[0], kText, 0u, 7u, 0u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[1], kText, 7u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[2], kOpenTag, 9u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[3], kText, 9u, 10u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[4], kCloseTag, 10u, 10u, 1u);
  EXPECT_EQ(5u, items.size());
}

TEST_F(NGInlineNodeTest, CollectInlinesMixedTextEndWithON) {
  SetupHtml("t", u"<div id=t>Hello, \u05E2 <span>\u05E2!</span></div>");
  NGInlineNodeForTest node = CreateInlineNode();
  node.CollectInlines();
  EXPECT_TRUE(node.IsBidiEnabled());
  node.SegmentText();
  EXPECT_TRUE(node.IsBidiEnabled());
  Vector<NGInlineItem>& items = node.Items();
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[0], kText, 0u, 7u, 0u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[1], kText, 7u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[2], kOpenTag, 9u, 9u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[3], kText, 9u, 10u, 1u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[4], kText, 10u, 11u, 0u);
  TEST_ITEM_TYPE_OFFSET_LEVEL(items[5], kCloseTag, 11u, 11u, 0u);
  EXPECT_EQ(6u, items.size());
}

TEST_F(NGInlineNodeTest, SegmentASCII) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append("Hello");
  node.SegmentText();
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(1u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 5u, TextDirection::kLtr);
}

TEST_F(NGInlineNodeTest, SegmentHebrew) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA");
  node.SegmentText();
  ASSERT_EQ(1u, node.Items().size());
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(1u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 5u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentSplit1To2) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append(u"Hello \u05E2\u05D1\u05E8\u05D9\u05EA");
  node.SegmentText();
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(2u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 11u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentSplit3To4) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append("Hel");
  node.Append(u"lo \u05E2");
  node.Append(u"\u05D1\u05E8\u05D9\u05EA");
  node.SegmentText();
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(4u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 3u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 3u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[2], 6u, 7u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 7u, 11u, TextDirection::kRtl);
}

TEST_F(NGInlineNodeTest, SegmentBidiOverride) {
  NGInlineNodeForTest node = CreateInlineNode();
  node.Append("Hello ");
  node.Append(kRightToLeftOverrideCharacter);
  node.Append("ABC");
  node.Append(kPopDirectionalFormattingCharacter);
  node.SegmentText();
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(4u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 7u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[2], 7u, 10u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 10u, 11u, TextDirection::kLtr);
}

static NGInlineNodeForTest CreateBidiIsolateNode(NGInlineNodeForTest node,
                                                 const ComputedStyle* style,
                                                 LayoutObject* layout_object) {
  node.Append("Hello ", style, layout_object);
  node.Append(kRightToLeftIsolateCharacter);
  node.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA ", style, layout_object);
  node.Append(kLeftToRightIsolateCharacter);
  node.Append("A", style, layout_object);
  node.Append(kPopDirectionalIsolateCharacter);
  node.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", style, layout_object);
  node.Append(kPopDirectionalIsolateCharacter);
  node.Append(" World", style, layout_object);
  node.SegmentText();
  return node;
}

TEST_F(NGInlineNodeTest, SegmentBidiIsolate) {
  NGInlineNodeForTest node = CreateInlineNode();
  node = CreateBidiIsolateNode(node, style_.get(), layout_object_);
  Vector<NGInlineItem>& items = node.Items();
  ASSERT_EQ(10u, items.size());
  TEST_ITEM_OFFSET_DIR(items[0], 0u, 6u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[1], 6u, 7u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[2], 7u, 13u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[3], 13u, 14u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[4], 14u, 15u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[5], 15u, 16u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[6], 16u, 21u, TextDirection::kRtl);
  TEST_ITEM_OFFSET_DIR(items[7], 21u, 22u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[8], 22u, 23u, TextDirection::kLtr);
  TEST_ITEM_OFFSET_DIR(items[9], 23u, 28u, TextDirection::kLtr);
}

#define TEST_TEXT_FRAGMENT(fragment, start_offset, end_offset) \
  EXPECT_EQ(start_offset, fragment->StartOffset());            \
  EXPECT_EQ(end_offset, fragment->EndOffset());

TEST_F(NGInlineNodeTest, CreateLineBidiIsolate) {
  UseLayoutObjectAndAhem();
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetLineHeight(Length(1, kFixed));
  style->GetFont().Update(nullptr);
  NGInlineNodeForTest node = CreateInlineNode();
  node = CreateBidiIsolateNode(node, style.get(), layout_object_);
  node.ShapeText();
  Vector<scoped_refptr<const NGPhysicalTextFragment>> fragments;
  CreateLine(node, &fragments);
  ASSERT_EQ(6u, fragments.size());
  TEST_TEXT_FRAGMENT(fragments[0], 0u, 6u);
  TEST_TEXT_FRAGMENT(fragments[1], 16u, 21u);
  TEST_TEXT_FRAGMENT(fragments[2], 14u, 15u);
  TEST_TEXT_FRAGMENT(fragments[3], 7u, 13u);
  TEST_TEXT_FRAGMENT(fragments[4], 22u, 23u);
  TEST_TEXT_FRAGMENT(fragments[5], 23u, 28u);
}

TEST_F(NGInlineNodeTest, MinMaxSize) {
  LoadAhem();
  SetupHtml("t", "<div id=t style='font:10px Ahem'>AB CDEF</div>");
  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSize sizes =
      node.ComputeMinMaxSize(WritingMode::kHorizontalTb, MinMaxSizeInput());
  EXPECT_EQ(40, sizes.min_size);
  EXPECT_EQ(70, sizes.max_size);
}

TEST_F(NGInlineNodeTest, MinMaxSizeElementBoundary) {
  LoadAhem();
  SetupHtml("t", "<div id=t style='font:10px Ahem'>A B<span>C D</span></div>");
  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSize sizes =
      node.ComputeMinMaxSize(WritingMode::kHorizontalTb, MinMaxSizeInput());
  // |min_content| should be the width of "BC" because there is an element
  // boundary between "B" and "C" but no break opportunities.
  EXPECT_EQ(20, sizes.min_size);
  EXPECT_EQ(60, sizes.max_size);
}

TEST_F(NGInlineNodeTest, MinMaxSizeFloats) {
  LoadAhem();
  SetupHtml("t", R"HTML(
    <style>
      #left { float: left; width: 50px; }
    </style>
    <div id=t style="font: 10px Ahem">
      XXX <div id="left"></div> XXXX
    </div>
  )HTML");

  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSize sizes =
      node.ComputeMinMaxSize(WritingMode::kHorizontalTb, MinMaxSizeInput());

  EXPECT_EQ(50, sizes.min_size);
  EXPECT_EQ(130, sizes.max_size);
}

TEST_F(NGInlineNodeTest, MinMaxSizeFloatsClearance) {
  LoadAhem();
  SetupHtml("t", R"HTML(
    <style>
      #left { float: left; width: 40px; }
      #right { float: right; clear: left; width: 50px; }
    </style>
    <div id=t style="font: 10px Ahem">
      XXX <div id="left"></div><div id="right"></div><div id="left"></div> XXX
    </div>
  )HTML");

  NGInlineNodeForTest node = CreateInlineNode();
  MinMaxSize sizes =
      node.ComputeMinMaxSize(WritingMode::kHorizontalTb, MinMaxSizeInput());

  EXPECT_EQ(50, sizes.min_size);
  EXPECT_EQ(160, sizes.max_size);
}

TEST_F(NGInlineNodeTest, InvalidateAddSpan) {
  SetupHtml("t", "<div id=t>before</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  Element* parent = ToElement(layout_block_flow_->GetNode());
  Element* span = GetDocument().CreateRawElement(HTMLNames::spanTag);
  parent->appendChild(span);

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, open/close items should be added.
  ForceLayout();
  EXPECT_EQ(item_count_before + 2, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateRemoveSpan) {
  SetupHtml("t", "<div id=t><span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->remove();
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, InvalidateAddInnerSpan) {
  SetupHtml("t", "<div id=t><span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  Element* parent = GetElementById("x");
  ASSERT_TRUE(parent);
  Element* span = GetDocument().CreateRawElement(HTMLNames::spanTag);
  parent->appendChild(span);

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, open/close items should be added.
  ForceLayout();
  EXPECT_EQ(item_count_before + 2, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateRemoveInnerSpan) {
  SetupHtml("t", "<div id=t><span><span id=x></span></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->remove();
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, InvalidateSetText) {
  SetupHtml("t", "<div id=t>before</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  LayoutText* text = ToLayoutText(layout_block_flow_->FirstChild());
  text->SetText(String("after").Impl());
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, InvalidateSetTextWithOffset) {
  SetupHtml("t", "<div id=t>before</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  LayoutText* text = ToLayoutText(layout_block_flow_->FirstChild());
  text->SetTextWithOffset(String("after").Impl(), 1, 4);
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, InvalidateAddAbsolute) {
  SetupHtml("t",
            "<style>span { position: absolute; }</style>"
            "<div id=t>before</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  Element* parent = ToElement(layout_block_flow_->GetNode());
  Element* span = GetDocument().CreateRawElement(HTMLNames::spanTag);
  parent->appendChild(span);

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, an OOF item should be added.
  ForceLayout();
  EXPECT_EQ(item_count_before + 1, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateRemoveAbsolute) {
  SetupHtml("t",
            "<style>span { position: absolute; }</style>"
            "<div id=t>before<span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->remove();
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, InvalidateChangeToAbsolute) {
  SetupHtml("t",
            "<style>#y { position: absolute; }</style>"
            "<div id=t>before<span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->SetIdAttribute("y");

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, an open/close items should be replaced with an
  // OOF item.
  ForceLayout();
  EXPECT_EQ(item_count_before - 1, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateChangeFromAbsolute) {
  SetupHtml("t",
            "<style>#x { position: absolute; }</style>"
            "<div id=t>before<span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->SetIdAttribute("y");

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, an OOF item should be replaced with open/close
  // items..
  ForceLayout();
  EXPECT_EQ(item_count_before + 1, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateAddFloat) {
  SetupHtml("t",
            "<style>span { float: left; }</style>"
            "<div id=t>before</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  unsigned item_count_before = Items().size();

  Element* parent = ToElement(layout_block_flow_->GetNode());
  Element* span = GetDocument().CreateRawElement(HTMLNames::spanTag);
  parent->appendChild(span);

  // NeedsCollectInlines() is marked during the layout.
  // By re-collecting inlines, an float item should be added.
  ForceLayout();
  EXPECT_EQ(item_count_before + 1, Items().size());
}

TEST_F(NGInlineNodeTest, InvalidateRemoveFloat) {
  SetupHtml("t",
            "<style>span { float: left; }</style>"
            "<div id=t>before<span id=x></span></div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  span->remove();
  EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());
}

TEST_F(NGInlineNodeTest, SpaceRestoredByInsertingWord) {
  SetupHtml("t", "<div id=t>before <span id=x></span> after</div>");
  EXPECT_FALSE(layout_block_flow_->NeedsCollectInlines());
  EXPECT_EQ(String("before after"), GetText());

  Element* span = GetElementById("x");
  ASSERT_TRUE(span);
  Text* text = Text::Create(GetDocument(), "mid");
  span->appendChild(text);
  // EXPECT_TRUE(layout_block_flow_->NeedsCollectInlines());

  ForceLayout();
  EXPECT_EQ(String("before mid after"), GetText());
}

}  // namespace blink
