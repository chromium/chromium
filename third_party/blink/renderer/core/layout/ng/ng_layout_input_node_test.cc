// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

#if DCHECK_IS_ON()

class NGLayoutInputNodeTest : public RenderingTest {
 public:
  String DumpAll(const NGLayoutInputNode* target = nullptr) const {
    NGBlockNode root_node(GetDocument().GetLayoutView());
    return root_node.DumpNodeTree(target);
  }
  NGBlockNode BlockNodeFromId(const char* id) {
    auto* box = DynamicTo<LayoutBox>(GetLayoutObjectByElementId(id));
    return NGBlockNode(box);
  }
};

TEST_F(NGLayoutInputNodeTest, DumpBasic) {
  SetBodyInnerHTML(R"HTML(
    <div id="block"><span>Hello world!</span></div>
  )HTML");
  String dump = DumpAll();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  NGBlockNode: LayoutView #document
    NGBlockNode: LayoutNGBlockFlow HTML
      NGBlockNode: LayoutNGBlockFlow BODY
        NGBlockNode: LayoutNGBlockFlow DIV id='block'
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "Hello world!"
            InlineItem CloseTag. LayoutInline SPAN
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGLayoutInputNodeTest, DumpBlockInInline) {
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
  NGBlockNode inner = BlockNodeFromId("inner");
  String dump = inner.DumpNodeTreeFromRoot();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  NGBlockNode: LayoutView #document
    NGBlockNode: LayoutNGBlockFlow HTML
      NGBlockNode: LayoutNGBlockFlow BODY
        NGBlockNode: LayoutNGBlockFlow DIV id='block'
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "\n        Hello world!\n        "
            InlineItem BlockInInline. LayoutNGBlockFlow (anonymous)
              NGBlockNode: LayoutNGBlockFlow DIV id='blockininline'
*               NGBlockNode: LayoutNGBlockFlow DIV id='inner'
                  InlineNode
                    InlineItem Text. "Hello trouble!"
            InlineItem CloseTag. LayoutInline SPAN
            InlineItem Text. "\n     "
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGLayoutInputNodeTest, DumpInlineBlockInInline) {
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
  NGBlockNode inner = BlockNodeFromId("inner");
  String dump = inner.DumpNodeTreeFromRoot();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  NGBlockNode: LayoutView #document
    NGBlockNode: LayoutNGBlockFlow HTML
      NGBlockNode: LayoutNGBlockFlow BODY
        NGBlockNode: LayoutNGBlockFlow DIV id='block'
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "\n        Hello world!\n        "
            InlineItem AtomicInline. LayoutNGBlockFlow DIV id='inlineblock'
*             NGBlockNode: LayoutNGBlockFlow DIV id='inner'
                InlineNode
                  InlineItem Text. "Hello Janus!"
            InlineItem Text. "\n      "
            InlineItem CloseTag. LayoutInline SPAN
            InlineItem Text. "\n     "
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGLayoutInputNodeTest, DumpFloatInInline) {
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
  NGBlockNode inner = BlockNodeFromId("inner");
  String dump = inner.DumpNodeTreeFromRoot();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  NGBlockNode: LayoutView #document
    NGBlockNode: LayoutNGBlockFlow HTML
      NGBlockNode: LayoutNGBlockFlow BODY
        NGBlockNode: LayoutNGBlockFlow DIV id='block'
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "\n        Hello world!\n        "
            InlineItem Floating. LayoutNGBlockFlow (floating) DIV id='float'
*             NGBlockNode: LayoutNGBlockFlow DIV id='inner'
                InlineNode
                  InlineItem Text. "Hello Hermes!"
            InlineItem CloseTag. LayoutInline SPAN
            InlineItem Text. "\n     "
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGLayoutInputNodeTest, DumpAbsposInInline) {
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
  NGBlockNode inner = BlockNodeFromId("inner");
  String dump = inner.DumpNodeTreeFromRoot();
  String expectation = R"DUMP(.:: Layout input node tree ::.
  NGBlockNode: LayoutView #document
    NGBlockNode: LayoutNGBlockFlow HTML
      NGBlockNode: LayoutNGBlockFlow BODY
        NGBlockNode: LayoutNGBlockFlow DIV id='block'
          InlineNode
            InlineItem OpenTag. LayoutInline SPAN
            InlineItem Text. "\n        Hello world!\n        "
            InlineItem OutOfFlowPositioned. LayoutNGBlockFlow (positioned) DIV id='abspos'
*             NGBlockNode: LayoutNGBlockFlow DIV id='inner'
                InlineNode
                  InlineItem Text. "Hello Thor!"
            InlineItem CloseTag. LayoutInline SPAN
            InlineItem Text. "\n     "
)DUMP";
  EXPECT_EQ(expectation, dump);
}

#endif  // DCHECK_IS_ON()

}  // anonymous namespace

}  // namespace blink
