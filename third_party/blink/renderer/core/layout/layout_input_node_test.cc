// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_input_node.h"

#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

#if DCHECK_IS_ON()

class LayoutInputNodeTest : public RenderingTest {
 public:
  String DumpAll(const LayoutInputNode* target = nullptr) const {
    BlockNode root_node(GetDocument().GetLayoutView());
    return root_node.DumpNodeTree(target);
  }
  BlockNode BlockNodeFromId(const char* id) {
    auto* box = DynamicTo<LayoutBox>(GetLayoutObjectByElementId(id));
    return BlockNode(box);
  }
};

TEST_F(LayoutInputNodeTest, DumpBasic) {
  SetBodyInnerHTML(R"HTML(
    <div id="block"><span>Hello world!</span></div>
  )HTML");
  String dump = DumpAll();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  BlockNode: LayoutView #document
    BlockNode: LayoutBlockFlow HTML
      BlockNode: LayoutBlockFlow BODY
        BlockNode: LayoutBlockFlow (children-inline) DIV id="block"
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "Hello world!"
            InlineItem CloseTag. LayoutInline SPAN
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(LayoutInputNodeTest, DumpBlockInInline) {
  SetBodyInnerHTML(R"HTML(
    <div id="block">
      <span>
        Hello world!
        <div id="blockininline">
          <div id="inner">Hello trouble!</div>
        </div>
      </span>
     </div>
  )HTML");
  BlockNode inner = BlockNodeFromId("inner");
  String dump = inner.DumpNodeTreeFromRoot();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  BlockNode: LayoutView #document
    BlockNode: LayoutBlockFlow HTML
      BlockNode: LayoutBlockFlow BODY
        BlockNode: LayoutBlockFlow (children-inline) DIV id="block"
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "\n        Hello world!\n        "
            InlineItem BlockInInline. LayoutBlockFlow (anonymous)
              BlockNode: LayoutBlockFlow DIV id="blockininline"
*               BlockNode: LayoutBlockFlow (children-inline) DIV id="inner"
                  InlineNode
                    InlineItem Text. "Hello trouble!"
            InlineItem CloseTag. LayoutInline SPAN
            InlineItem Text. "\n     "
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(LayoutInputNodeTest, DumpInlineBlockInInline) {
  SetBodyInnerHTML(R"HTML(
    <div id="block">
      <span>
        Hello world!
        <div id="inlineblock" style="display:inline-block;">
          <div id="inner">Hello Janus!</div>
        </div>
      </span>
     </div>
  )HTML");
  BlockNode inner = BlockNodeFromId("inner");
  String dump = inner.DumpNodeTreeFromRoot();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  BlockNode: LayoutView #document
    BlockNode: LayoutBlockFlow HTML
      BlockNode: LayoutBlockFlow BODY
        BlockNode: LayoutBlockFlow (children-inline) DIV id="block"
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "\n        Hello world!\n        "
            InlineItem AtomicInline. LayoutBlockFlow (inline) DIV id="inlineblock" style="display:inline-block;"
*             BlockNode: LayoutBlockFlow (children-inline) DIV id="inner"
                InlineNode
                  InlineItem Text. "Hello Janus!"
            InlineItem Text. "\n      "
            InlineItem CloseTag. LayoutInline SPAN
            InlineItem Text. "\n     "
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(LayoutInputNodeTest, DumpFloatInInline) {
  SetBodyInnerHTML(R"HTML(
    <div id="block">
      <span>
        Hello world!
        <div id="float" style="float:left;">
          <div id="inner">Hello Hermes!</div>
        </div>
      </span>
     </div>
  )HTML");
  BlockNode inner = BlockNodeFromId("inner");
  String dump = inner.DumpNodeTreeFromRoot();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  BlockNode: LayoutView #document
    BlockNode: LayoutBlockFlow HTML
      BlockNode: LayoutBlockFlow BODY
        BlockNode: LayoutBlockFlow (children-inline) DIV id="block"
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "\n        Hello world!\n        "
            InlineItem Floating. LayoutBlockFlow (floating) DIV id="float" style="float:left;"
*             BlockNode: LayoutBlockFlow (children-inline) DIV id="inner"
                InlineNode
                  InlineItem Text. "Hello Hermes!"
            InlineItem CloseTag. LayoutInline SPAN
            InlineItem Text. "\n     "
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(LayoutInputNodeTest, DumpAbsposInInline) {
  SetBodyInnerHTML(R"HTML(
    <div id="block">
      <span>
        Hello world!
        <div id="abspos" style="position:absolute;">
          <div id="inner">Hello Thor!</div>
        </div>
      </span>
     </div>
  )HTML");
  BlockNode inner = BlockNodeFromId("inner");
  String dump = inner.DumpNodeTreeFromRoot();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  BlockNode: LayoutView #document
    BlockNode: LayoutBlockFlow HTML
      BlockNode: LayoutBlockFlow BODY
        BlockNode: LayoutBlockFlow (children-inline) DIV id="block"
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "\n        Hello world!\n        "
            InlineItem OutOfFlowPositioned. LayoutBlockFlow (positioned) DIV id="abspos" style="position:absolute;"
*             BlockNode: LayoutBlockFlow (children-inline) DIV id="inner"
                InlineNode
                  InlineItem Text. "Hello Thor!"
            InlineItem CloseTag. LayoutInline SPAN
            InlineItem Text. "\n     "
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(LayoutInputNodeTest, DumpRelposInline) {
  SetBodyInnerHTML(R"HTML(
    <span style="position:relative;">Hello world!</span>
  )HTML");
  String dump = DumpAll();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  BlockNode: LayoutView #document
    BlockNode: LayoutBlockFlow HTML
      BlockNode: LayoutBlockFlow (children-inline) BODY
        InlineNode
          InlineItem OpenTag. LayoutInline (relative positioned) SPAN style="position:relative;"
          InlineItem Text. "Hello world!"
          InlineItem CloseTag. LayoutInline (relative positioned) SPAN style="position:relative;"
          InlineItem Text. "\n  "
)DUMP";
  EXPECT_EQ(expectation, dump);
}

#endif  // DCHECK_IS_ON()

}  // anonymous namespace

}  // namespace blink
