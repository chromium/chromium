// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

class NGPhysicalTextFragmentTest : public NGLayoutTest {
 public:
  NGPhysicalTextFragmentTest() : NGLayoutTest() {}

 protected:
  Vector<scoped_refptr<const NGPhysicalTextFragment>>
  CollectTextFragmentsInContainer(const char* container_id) {
    const Element* container = GetElementById(container_id);
    DCHECK(container) << container_id;
    const LayoutObject* layout_object = container->GetLayoutObject();
    DCHECK(layout_object) << container;
    DCHECK(layout_object->IsLayoutBlockFlow()) << container;
    const auto* root_fragment =
        To<LayoutBlockFlow>(layout_object)->CurrentFragment();
    DCHECK(root_fragment) << container;

    Vector<scoped_refptr<const NGPhysicalTextFragment>> result;
    for (const auto& child :
         NGInlineFragmentTraversal::DescendantsOf(*root_fragment)) {
      if (auto* text_child_fragment =
              DynamicTo<NGPhysicalTextFragment>(child.fragment.get()))
        result.push_back(text_child_fragment);
    }
    return result;
  }

  static std::string GetText(const NGPhysicalTextFragment& fragment) {
    return fragment.Text().ToString().Utf8();
  }
};

TEST_F(NGPhysicalTextFragmentTest, LocalRect) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font: 10px/1 Ahem;
      width: 5em;
    }
    </style>
    <div id=container>01234 67890</div>
  )HTML");
  auto text_fragments = CollectTextFragmentsInContainer("container");
  ASSERT_EQ(2u, text_fragments.size());
  EXPECT_EQ(PhysicalRect(20, 0, 20, 10), text_fragments[1]->LocalRect(8, 10));
}

TEST_F(NGPhysicalTextFragmentTest, LocalRectRTL) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font: 10px/1 Ahem;
      width: 10em;
      direction: rtl;
      unicode-bidi: bidi-override;
    }
    </style>
    <div id=container>0123456789 123456789</div>
  )HTML");
  auto text_fragments = CollectTextFragmentsInContainer("container");
  ASSERT_EQ(2u, text_fragments.size());
  // The 2nd line starts at 12, because the div has a bidi-control.
  EXPECT_EQ(12u, text_fragments[1]->StartOffset());
  // TODO(layout-dev): Investigate whether this is correct.
  // EXPECT_EQ(PhysicalRect(50, 0, 20, 10),
  //          text_fragments[1]->LocalRect(14, 16));
}

TEST_F(NGPhysicalTextFragmentTest, LocalRectVLR) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font: 10px/1 Ahem;
      height: 5em;
      writing-mode: vertical-lr;
    }
    </style>
    <div id=container>01234 67890</div>
  )HTML");
  auto text_fragments = CollectTextFragmentsInContainer("container");
  ASSERT_EQ(2u, text_fragments.size());
  EXPECT_EQ(PhysicalRect(0, 20, 10, 20), text_fragments[1]->LocalRect(8, 10));
}

TEST_F(NGPhysicalTextFragmentTest, LocalRectVRL) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font: 10px/1 Ahem;
      height: 5em;
      writing-mode: vertical-rl;
    }
    </style>
    <div id=container>01234 67890</div>
  )HTML");
  auto text_fragments = CollectTextFragmentsInContainer("container");
  ASSERT_EQ(2u, text_fragments.size());
  EXPECT_EQ(PhysicalRect(0, 20, 10, 20), text_fragments[1]->LocalRect(8, 10));
}

TEST_F(NGPhysicalTextFragmentTest, NormalTextIsNotAnonymousText) {
  SetBodyInnerHTML("<div id=div>text</div>");

  auto text_fragments = CollectTextFragmentsInContainer("div");
  ASSERT_EQ(1u, text_fragments.size());

  const NGPhysicalTextFragment& text = *text_fragments[0];
  EXPECT_FALSE(text.IsGeneratedText());
}

TEST_F(NGPhysicalTextFragmentTest, FirstLetterIsNotAnonymousText) {
  SetBodyInnerHTML(
      "<style>::first-letter {color:red}</style>"
      "<div id=div>text</div>");

  auto text_fragments = CollectTextFragmentsInContainer("div");
  ASSERT_EQ(2u, text_fragments.size());

  const NGPhysicalTextFragment& first_letter = *text_fragments[0];
  const NGPhysicalTextFragment& remaining_text = *text_fragments[1];
  EXPECT_FALSE(first_letter.IsGeneratedText());
  EXPECT_FALSE(remaining_text.IsGeneratedText());
}

TEST_F(NGPhysicalTextFragmentTest, BeforeAndAfterAreAnonymousText) {
  SetBodyInnerHTML(
      "<style>::before{content:'x'} ::after{content:'x'}</style>"
      "<div id=div>text</div>");

  auto text_fragments = CollectTextFragmentsInContainer("div");
  ASSERT_EQ(3u, text_fragments.size());

  const NGPhysicalTextFragment& before = *text_fragments[0];
  const NGPhysicalTextFragment& text = *text_fragments[1];
  const NGPhysicalTextFragment& after = *text_fragments[2];
  EXPECT_TRUE(before.IsGeneratedText());
  EXPECT_FALSE(text.IsGeneratedText());
  EXPECT_TRUE(after.IsGeneratedText());
}

TEST_F(NGPhysicalTextFragmentTest, Ellipsis) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
      <style>
      #sample {
        font: 10px/1 Ahem;
        overflow: hidden;
        text-overflow: ellipsis;
        width: 4ch;
      }
      </style>
      <p id="sample">abcdef</p>
  )HTML");
  auto text_fragments = CollectTextFragmentsInContainer("sample");
  ASSERT_EQ(3u, text_fragments.size());

  const NGPhysicalTextFragment& hidden = *text_fragments[0];
  const NGPhysicalTextFragment& truncated = *text_fragments[1];
  const NGPhysicalTextFragment& ellipsis = *text_fragments[2];

  EXPECT_EQ(NGPhysicalTextFragment::kNormalText, hidden.TextType());
  EXPECT_FALSE(hidden.IsGeneratedText());
  EXPECT_TRUE(hidden.IsHiddenForPaint());
  EXPECT_EQ(u8"abcdef", GetText(hidden));

  EXPECT_EQ(NGPhysicalTextFragment::kNormalText, truncated.TextType());
  EXPECT_FALSE(truncated.IsGeneratedText());
  EXPECT_FALSE(truncated.IsHiddenForPaint());
  EXPECT_EQ(u8"abc", GetText(truncated));

  EXPECT_EQ(NGPhysicalTextFragment::kGeneratedText, ellipsis.TextType());
  EXPECT_TRUE(ellipsis.IsGeneratedText());
  EXPECT_FALSE(ellipsis.IsHiddenForPaint());
  EXPECT_EQ(u8"\u2026", GetText(ellipsis));
}

TEST_F(NGPhysicalTextFragmentTest, ListMarkerIsGeneratedText) {
  SetBodyInnerHTML(
      "<ol style='list-style-position:inside'>"
      "<li id=list>text</li>"
      "</ol>");

  auto text_fragments = CollectTextFragmentsInContainer("list");
  ASSERT_EQ(2u, text_fragments.size());

  const NGPhysicalTextFragment& marker = *text_fragments[0];
  const NGPhysicalTextFragment& text = *text_fragments[1];
  EXPECT_TRUE(marker.IsGeneratedText());
  EXPECT_FALSE(text.IsGeneratedText());
}

TEST_F(NGPhysicalTextFragmentTest, SoftHyphen) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    #sample {
      font: 10px/1 Ahem;
      width: 3ch;
    }
    </style>
    <p id="sample">abc&shy;def</p>
  )HTML");
  auto text_fragments = CollectTextFragmentsInContainer("sample");
  ASSERT_EQ(3u, text_fragments.size());

  const NGPhysicalTextFragment& abc = *text_fragments[0];
  const NGPhysicalTextFragment& shy = *text_fragments[1];
  const NGPhysicalTextFragment& def = *text_fragments[2];
  EXPECT_EQ(NGPhysicalTextFragment::kNormalText, abc.TextType());
  // Note: ShapeResult::RunInfo.width_ == 0 for U+00AD
  EXPECT_EQ(u8"abc\u00AD", GetText(abc));
  EXPECT_EQ(NGPhysicalTextFragment::kGeneratedText, shy.TextType());
  // Note: |ComputedStyle::HypenString()| returns "-" or U+2010 based on
  // glyph availability.
  if (GetText(shy) != "-")
    EXPECT_EQ(u8"\u2010", GetText(shy));
  EXPECT_EQ(NGPhysicalTextFragment::kNormalText, def.TextType());
}

TEST_F(NGPhysicalTextFragmentTest, QuotationMarksAreAnonymousText) {
  SetBodyInnerHTML("<div id=div><q>text</q></div>");

  auto text_fragments = CollectTextFragmentsInContainer("div");
  ASSERT_EQ(3u, text_fragments.size());

  const NGPhysicalTextFragment& open_quote = *text_fragments[0];
  const NGPhysicalTextFragment& text = *text_fragments[1];
  const NGPhysicalTextFragment& closed_quote = *text_fragments[2];
  EXPECT_TRUE(open_quote.IsGeneratedText());
  EXPECT_FALSE(text.IsGeneratedText());
  EXPECT_TRUE(closed_quote.IsGeneratedText());
}

TEST_F(NGPhysicalTextFragmentTest, TextOffsetForPointForTabulation) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      white-space: pre;
      font-family: Ahem;
      font-size: 10px;
      tab-size: 8;
    }
    </style>
    <div id="container">&#09;</div>
  )HTML");
  auto text_fragments = CollectTextFragmentsInContainer("container");
  ASSERT_EQ(1u, text_fragments.size());
  const NGPhysicalTextFragment& text = *text_fragments[0];
  EXPECT_EQ(0u, text.TextOffsetForPoint({LayoutUnit(), LayoutUnit()}));
  EXPECT_EQ(0u, text.TextOffsetForPoint({LayoutUnit(39), LayoutUnit()}));
  EXPECT_EQ(1u, text.TextOffsetForPoint({LayoutUnit(41), LayoutUnit()}));
  EXPECT_EQ(1u, text.TextOffsetForPoint({LayoutUnit(80), LayoutUnit()}));
}

TEST_F(NGPhysicalTextFragmentTest, TextOffsetForPointForTabulationRtl) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      white-space: pre;
      font-family: Ahem;
      font-size: 10px;
      tab-size: 8;
    }
    </style>
    <div id="container" dir="rtl">&#09;</div>
  )HTML");
  auto text_fragments = CollectTextFragmentsInContainer("container");
  ASSERT_EQ(1u, text_fragments.size());
  const NGPhysicalTextFragment& text = *text_fragments[0];
  EXPECT_EQ(1u, text.TextOffsetForPoint({LayoutUnit(), LayoutUnit()}));
  EXPECT_EQ(1u, text.TextOffsetForPoint({LayoutUnit(39), LayoutUnit()}));
  EXPECT_EQ(0u, text.TextOffsetForPoint({LayoutUnit(41), LayoutUnit()}));
  EXPECT_EQ(0u, text.TextOffsetForPoint({LayoutUnit(80), LayoutUnit()}));
}

}  // namespace blink
