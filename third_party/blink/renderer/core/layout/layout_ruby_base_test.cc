// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"

#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutRubyBaseTest : public RenderingTest {};

// crbug.com/1503372

TEST_F(LayoutRubyBaseTest, AddChildNoBlockChildren) {
  SetBodyInnerHTML(R"HTML(
      <ruby id="target">abc<span style="display:table-cell"></span></ruby>
      )HTML");
  auto* run_box = To<LayoutRubyColumn>(
      GetLayoutObjectByElementId("target")->SlowFirstChild());
  auto* base_box = run_box->RubyBase();
  // Adding a table-cell should not move the prior Text to an anonymous block.
  EXPECT_TRUE(base_box->FirstChild()->IsText());
  EXPECT_EQ(EDisplay::kInlineTable,
            base_box->FirstChild()->NextSibling()->StyleRef().Display());
}

// crbug.com/1510269

TEST_F(LayoutRubyBaseTest, AddImageNoBlockChildren) {
  SetBodyInnerHTML(R"HTML(
<style> .c7 { content: url(data:text/plain,foo); }</style>
<ruby id="target">abc</ruby>)HTML");
  Element* caption = GetDocument().CreateRawElement(html_names::kCaptionTag);
  caption->setAttribute(html_names::kClassAttr, AtomicString("c7"));
  GetElementById("target")->appendChild(caption);
  UpdateAllLifecyclePhasesForTest();

  auto* run_box = To<LayoutRubyColumn>(
      GetLayoutObjectByElementId("target")->SlowFirstChild());
  auto* base_box = run_box->RubyBase();
  // Adding a LayoutImage with display:table-caption should not move the prior
  // Text to an anonymous block.
  EXPECT_TRUE(base_box->FirstChild()->IsText());
  LayoutObject* caption_box = base_box->FirstChild()->NextSibling();
  ASSERT_TRUE(caption_box);
  EXPECT_TRUE(caption_box->IsImage());
  EXPECT_EQ(EDisplay::kTableCaption, caption_box->StyleRef().Display());
  EXPECT_TRUE(caption_box->IsInline());
}

// crbug.com/1513853

TEST_F(LayoutRubyBaseTest, AddSpecialWithTableInternalDisplayNoBlockChildren) {
  SetBodyInnerHTML(R"HTML(<ruby id="target">abc</ruby>)HTML");
  auto* input = GetDocument().CreateRawElement(html_names::kInputTag);
  input->setAttribute(html_names::kStyleAttr,
                      AtomicString("display:table-column; appearance:none"));
  GetElementById("target")->appendChild(input);
  UpdateAllLifecyclePhasesForTest();

  auto* base_box = To<LayoutRubyColumn>(
                       GetLayoutObjectByElementId("target")->SlowFirstChild())
                       ->RubyBase();
  // Adding a table-column should not move the prior Text to an anonymous block.
  EXPECT_TRUE(base_box->FirstChild()->IsText());
  // The input is not wrapped by an inline-table though it has
  // display:table-column.
  auto* layout_special = base_box->FirstChild()->NextSibling();
  ASSERT_TRUE(layout_special);
  EXPECT_EQ(EDisplay::kTableColumn, layout_special->StyleRef().Display());
  EXPECT_TRUE(layout_special->IsInline());
}

// crbug.com/1514152

TEST_F(LayoutRubyBaseTest, ChangeToRubyNoBlockChildren) {
  SetBodyInnerHTML(R"HTML(<div id="target"><p></div>)HTML");
  GetElementById("target")->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                                   CSSValueID::kRuby);
  UpdateAllLifecyclePhasesForTest();

  auto* base_box = To<LayoutRubyColumn>(
                       GetLayoutObjectByElementId("target")->SlowFirstChild())
                       ->RubyBase();
  // <p> should be inlinified.
  EXPECT_TRUE(base_box->FirstChild()->IsInline()) << base_box->FirstChild();
}

}  // namespace blink
