// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class LayoutNGTextCombineTest : public NGLayoutTest,
                                private ScopedLayoutNGTextCombineForTest {
 protected:
  LayoutNGTextCombineTest() : ScopedLayoutNGTextCombineForTest(true) {}
};

TEST_F(LayoutNGTextCombineTest, AppendChild) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  GetElementById("combine")->appendChild(Text::Create(GetDocument(), "Z"));
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, BoxBoundary) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>X<b>Y</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "Y"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, DeleteDataToEmpty) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())
      ->deleteData(0, 2, ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, InsertBefore) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  auto& combine = *GetElementById("combine");
  combine.insertBefore(Text::Create(GetDocument(), "Z"), combine.firstChild());
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "Z"
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1215026
TEST_F(LayoutNGTextCombineTest, LegacyQuote) {
  InsertStyleElement(
      "q { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root><q>ab</q></div>");
  auto& root = *GetElementById("root");

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutInline Q
  |  +--LayoutInline ::before
  |  |  +--LayoutQuote (anonymous)
  |  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  |  +--LayoutTextFragment (anonymous) ("\"")
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "ab"
  |  +--LayoutInline ::after
  |  |  +--LayoutQuote (anonymous)
  |  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  |  +--LayoutTextFragment (anonymous) ("\"")
)DUMP",
            ToSimpleLayoutTree(*root.GetLayoutObject()));

  // Force legacy layout
  root.SetStyleShouldForceLegacyLayout(true);
  GetDocument().documentElement()->SetForceReattachLayoutTree();
  RunDocumentLifecycle();

  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="root"
  +--LayoutInline Q
  |  +--LayoutInline ::before
  |  |  +--LayoutQuote (anonymous)
  |  |  |  +--LayoutTextFragment (anonymous) ("\"")
  |  +--LayoutTextCombine #text "ab"
  |  +--LayoutInline ::after
  |  |  +--LayoutQuote (anonymous)
  |  |  |  +--LayoutTextFragment (anonymous) ("\"")
)DUMP",
            ToSimpleLayoutTree(*root.GetLayoutObject()))
      << "No more LayoutNGTextCombine";
}

TEST_F(LayoutNGTextCombineTest, MultipleTextNode) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>X<!-- -->Y</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
  |  |  +--LayoutText #text "Y"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, Nested) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, RemoveChildCombine) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  GetElementById("combine")->remove();
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, RemoveChildToEmpty) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  GetElementById("combine")->firstChild()->remove();
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, SetDataToEmpty) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())->setData("");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "We should not have a wrapper.";
}

TEST_F(LayoutNGTextCombineTest, SplitText) {
  V8TestingScope scope;

  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())
      ->splitText(1, ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
  |  |  +--LayoutText #text "Y"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, SplitTextAtZero) {
  V8TestingScope scope;

  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY</c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())
      ->splitText(0, ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no empty LayoutText.";
}

TEST_F(LayoutNGTextCombineTest, SplitTextBeforeBox) {
  V8TestingScope scope;

  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY<b>Z</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  To<Text>(GetElementById("combine")->firstChild())
      ->splitText(1, ASSERT_NO_EXCEPTION);
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "X"
  |  |  +--LayoutText #text "Y"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, StyleToTextCombineUprightAll) {
  InsertStyleElement("div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no wrapper.";

  GetElementById("combine")->setAttribute("style", "text-combine-upright: all");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine" style="text-combine-upright: all"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no wrapper.";
}

TEST_F(LayoutNGTextCombineTest, StyleToTextCombineUprightNone) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  GetElementById("combine")->setAttribute("style",
                                          "text-combine-upright: none");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine" style="text-combine-upright: none"
  |  +--LayoutInline B
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no wrapper.";
}

TEST_F(LayoutNGTextCombineTest, StyleToHorizontalWritingMode) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  root.setAttribute("style", "writing-mode: horizontal-tb");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root" style="writing-mode: horizontal-tb"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object))
      << "There are no wrapper.";
}

TEST_F(LayoutNGTextCombineTest, StyleToVerticalWritingMode) {
  InsertStyleElement("c { text-combine-upright: all; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine><b>XY</b></c>de</div>");
  auto& root = *GetElementById("root");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(root.GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));

  root.setAttribute("style", "writing-mode: vertical-rl");
  RunDocumentLifecycle();
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root" style="writing-mode: vertical-rl"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutInline B
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText #text "XY"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, WithBR) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY<br>Z</c>de</div>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  |  +--LayoutBR BR
  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

// http://crbug.com/1060007
TEST_F(LayoutNGTextCombineTest, WithMarker) {
  InsertStyleElement(
      "li { text-combine-upright: all; }"
      "p {"
      "  counter-increment: my-counter;"
      "  display: list-item;"
      "  writing-mode: vertical-rl;"
      "}"
      "p::marker {"
      "  content: '<' counter(my-counter) '>';"
      "  text-combine-upright: all;"
      "}");
  SetBodyInnerHTML("<p id=root>ab</p>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());
  EXPECT_EQ(R"DUMP(
LayoutNGListItem P id="root"
  +--LayoutNGOutsideListMarker ::marker
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutTextFragment (anonymous) ("<")
  |  |  +--LayoutCounter (anonymous) "1"
  |  |  +--LayoutTextFragment (anonymous) (">")
  +--LayoutText #text "ab"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, WithOrderedList) {
  InsertStyleElement(
      "li { text-combine-upright: all; }"
      "ol { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<ol id=root><li>ab</li></ol>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow OL id="root"
  +--LayoutNGListItem LI
  |  +--LayoutNGOutsideListMarker ::marker
  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  +--LayoutText (anonymous) "1. "
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "ab"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, WithQuote) {
  InsertStyleElement(
      "q { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root><q>XY</q></div>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());
  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutInline Q
  |  +--LayoutInline ::before
  |  |  +--LayoutQuote (anonymous)
  |  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  |  +--LayoutTextFragment (anonymous) ("\"")
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  +--LayoutInline ::after
  |  |  +--LayoutQuote (anonymous)
  |  |  |  +--LayoutNGTextCombine (anonymous)
  |  |  |  |  +--LayoutTextFragment (anonymous) ("\"")
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

TEST_F(LayoutNGTextCombineTest, WithWordBreak) {
  InsertStyleElement(
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>ab<c id=combine>XY<wbr>Z</c>de</div>");
  const auto& root_layout_object =
      *To<LayoutNGBlockFlow>(GetElementById("root")->GetLayoutObject());

  EXPECT_EQ(R"DUMP(
LayoutNGBlockFlow DIV id="root"
  +--LayoutText #text "ab"
  +--LayoutInline C id="combine"
  |  +--LayoutNGTextCombine (anonymous)
  |  |  +--LayoutText #text "XY"
  |  |  +--LayoutWordBreak WBR
  |  |  +--LayoutText #text "Z"
  +--LayoutText #text "de"
)DUMP",
            ToSimpleLayoutTree(root_layout_object));
}

}  // namespace blink
