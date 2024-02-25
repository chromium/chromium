// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_adjuster.h"

#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"

namespace blink {

class SelectionAdjusterTest : public EditingTestBase {};

// ------------ Shadow boundary adjustment tests --------------
TEST_F(SelectionAdjusterTest, AdjustShadowToCollpasedInDOMTree) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody(
      "<span><template data-mode=\"open\">a|bc</template></span>^");
  const SelectionInDOMTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingShadowBoundaries(
          selection);
  EXPECT_EQ("<span></span>|", GetSelectionTextFromBody(result));
}

TEST_F(SelectionAdjusterTest, AdjustShadowToCollpasedInFlatTree) {
  SetBodyContent("<input value=abc>");
  const auto& input =
      ToTextControl(*GetDocument().QuerySelector(AtomicString("input")));
  const SelectionInFlatTree& selection =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::AfterNode(input))
          .Extend(
              PositionInFlatTree(*input.InnerEditorElement()->firstChild(), 1))
          .Build();
  const SelectionInFlatTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingShadowBoundaries(
          selection);
  EXPECT_EQ("<input value=\"abc\"><div>abc</div></input>|",
            GetSelectionTextInFlatTreeFromBody(result));
}

// ------------ Editing boundary adjustment tests --------------
// Extracted the related part from delete-non-editable-range-crash.html here,
// because the final result in that test was not WAI.
TEST_F(SelectionAdjusterTest, DeleteNonEditableRange) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody(R"HTML(
      <div contenteditable>
        <blockquote>
          <span>^foo<br></span>
          barbarbar
        </blockquote>
        <span contenteditable="false">
          <span contenteditable>|</span>
          <ol>bar</ol>
        </span>
      </div>)HTML");

  const SelectionInDOMTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);

  EXPECT_EQ(R"HTML(
      <div contenteditable>
        <blockquote>
          <span>^foo<br></span>
          barbarbar
        </blockquote>
        |<span contenteditable="false">
          <span contenteditable></span>
          <ol>bar</ol>
        </span>
      </div>)HTML",
            GetSelectionTextFromBody(result));
}

// Extracted the related part from format-block-contenteditable-false.html here,
// because the final result in that test was not WAI.
TEST_F(SelectionAdjusterTest, FormatBlockContentEditableFalse) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody(R"HTML(
      <div contenteditable>
        <h1><i>^foo</i><br><i>baz</i></h1>
        <div contenteditable="false">|bar</div>
      </div>)HTML");

  const SelectionInDOMTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);

  EXPECT_EQ(R"HTML(
      <div contenteditable>
        <h1><i>^foo</i><br><i>baz</i></h1>
        |<div contenteditable="false">bar</div>
      </div>)HTML",
            GetSelectionTextFromBody(result));
}

TEST_F(SelectionAdjusterTest, NestedContentEditableElements) {
  // Select from bar to foo.
  const SelectionInDOMTree& selection = SetSelectionTextToBody(R"HTML(
      <div contenteditable>
        <div contenteditable="false">
          <div contenteditable>
            |foo
          </div>
        </div>
        <br>
        bar^
      </div>)HTML");

  const SelectionInDOMTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);

  EXPECT_EQ(R"HTML(
      <div contenteditable>
        <div contenteditable="false">
          <div contenteditable>
            foo
          </div>
        </div>|
        <br>
        bar^
      </div>)HTML",
            GetSelectionTextFromBody(result));
}

TEST_F(SelectionAdjusterTest, ShadowRootAsRootBoundaryElement) {
  const char* body_content = "<div id='host'></div>";
  const char* shadow_content = "<div id='foo'>foo</div><div id='bar'>bar</div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Element* foo = shadow_root->QuerySelector(AtomicString("#foo"));
  Element* bar = shadow_root->QuerySelector(AtomicString("#bar"));

  // DOM tree selection.
  const SelectionInDOMTree& selection =
      SelectionInDOMTree::Builder()
          .Collapse(Position::FirstPositionInNode(*foo))
          .Extend(Position::LastPositionInNode(*bar))
          .Build();
  const SelectionInDOMTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);

  EXPECT_EQ(Position::FirstPositionInNode(*foo), result.Anchor());
  EXPECT_EQ(Position::LastPositionInNode(*bar), result.Focus());

  // Flat tree selection.
  const SelectionInFlatTree& selection_in_flat_tree =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*foo))
          .Extend(PositionInFlatTree::LastPositionInNode(*bar))
          .Build();
  const SelectionInFlatTree& result_in_flat_tree =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection_in_flat_tree);

  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*foo),
            result_in_flat_tree.Anchor());
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*bar),
            result_in_flat_tree.Focus());
}

TEST_F(SelectionAdjusterTest, ShadowRootAsRootBoundaryElementEditable) {
  const char* body_content = "<div id='host'></div>";
  const char* shadow_content =
      "foo"
      "<div id='bar' contenteditable>bar</div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  const Node* foo = shadow_root->firstChild();
  const Element* bar = shadow_root->QuerySelector(AtomicString("#bar"));

  // Select from foo to bar in DOM tree.
  const SelectionInDOMTree& selection =
      SelectionInDOMTree::Builder()
          .Collapse(Position::FirstPositionInNode(*foo))
          .Extend(Position::LastPositionInNode(*bar))
          .Build();
  const SelectionInDOMTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);

  EXPECT_EQ(Position::FirstPositionInNode(*foo), result.Anchor());
  EXPECT_EQ(Position::BeforeNode(*bar), result.Focus());

  // Select from foo to bar in flat tree.
  const SelectionInFlatTree& selection_in_flat_tree =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*foo))
          .Extend(PositionInFlatTree::LastPositionInNode(*bar))
          .Build();
  const SelectionInFlatTree& result_in_flat_tree =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection_in_flat_tree);

  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*foo),
            result_in_flat_tree.Anchor());
  EXPECT_EQ(PositionInFlatTree::BeforeNode(*bar), result_in_flat_tree.Focus());

  // Select from bar to foo in DOM tree.
  const SelectionInDOMTree& selection2 =
      SelectionInDOMTree::Builder()
          .Collapse(Position::LastPositionInNode(*bar))
          .Extend(Position::FirstPositionInNode(*foo))
          .Build();
  const SelectionInDOMTree& result2 =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection2);

  EXPECT_EQ(Position::LastPositionInNode(*bar), result2.Anchor());
  EXPECT_EQ(Position::FirstPositionInNode(*bar), result2.Focus());

  // Select from bar to foo in flat tree.
  const SelectionInFlatTree& selection_in_flat_tree2 =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::LastPositionInNode(*bar))
          .Extend(PositionInFlatTree::FirstPositionInNode(*foo))
          .Build();
  const SelectionInFlatTree& result_in_flat_tree2 =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection_in_flat_tree2);

  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*bar),
            result_in_flat_tree2.Anchor());
  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*bar),
            result_in_flat_tree2.Focus());
}

TEST_F(SelectionAdjusterTest, ShadowDistributedNodesWithoutEditingBoundary) {
  const char* body_content = R"HTML(
      <div id=host>
        <div id=foo slot=foo>foo</div>
        <div id=bar slot=bar>bar</div>
      </div>)HTML";
  const char* shadow_content = R"HTML(
      <div>
        <div id=s1>111</div>
        <slot name=foo></slot>
        <div id=s2>222</div>
        <slot name=bar></slot>
        <div id=s3>333</div>
      </div>)HTML";
  SetBodyContent(body_content);
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(shadow_content);

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  Element* s1 = shadow_root.QuerySelector(AtomicString("#s1"));

  // Select from 111 to foo.
  const SelectionInFlatTree& selection =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*s1))
          .Extend(PositionInFlatTree::LastPositionInNode(*foo))
          .Build();
  const SelectionInFlatTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);
  EXPECT_EQ(R"HTML(
      <div id="host">
      <div>
        <div id="s1">^111</div>
        <slot name="foo"><div id="foo" slot="foo">foo|</div></slot>
        <div id="s2">222</div>
        <slot name="bar"><div id="bar" slot="bar">bar</div></slot>
        <div id="s3">333</div>
      </div></div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result));

  // Select from foo to 111.
  const SelectionInFlatTree& selection2 =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::LastPositionInNode(*foo))
          .Extend(PositionInFlatTree::FirstPositionInNode(*s1))
          .Build();
  const SelectionInFlatTree& result2 =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection2);
  EXPECT_EQ(R"HTML(
      <div id="host">
      <div>
        <div id="s1">|111</div>
        <slot name="foo"><div id="foo" slot="foo">foo^</div></slot>
        <div id="s2">222</div>
        <slot name="bar"><div id="bar" slot="bar">bar</div></slot>
        <div id="s3">333</div>
      </div></div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result2));
}

// This test is just recording the behavior of current implementation, can be
// changed.
TEST_F(SelectionAdjusterTest, ShadowDistributedNodesWithEditingBoundary) {
  const char* body_content = R"HTML(
      <div contenteditable id=host>
        <div id=foo slot=foo>foo</div>
        <div id=bar slot=bar>bar</div>
      </div>)HTML";
  const char* shadow_content = R"HTML(
      <div>
        <div id=s1>111</div>
        <slot name=foo></slot>
        <div id=s2>222</div>
        <slot name=bar></slot>
        <div id=s3>333</div>
      </div>)HTML";
  SetBodyContent(body_content);
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(shadow_content);

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  Element* bar = GetDocument().getElementById(AtomicString("bar"));
  Element* s1 = shadow_root.QuerySelector(AtomicString("#s1"));
  Element* s2 = shadow_root.QuerySelector(AtomicString("#s2"));

  // Select from 111 to foo.
  const SelectionInFlatTree& selection =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*s1))
          .Extend(PositionInFlatTree::LastPositionInNode(*foo))
          .Build();
  const SelectionInFlatTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);
  EXPECT_EQ(R"HTML(
      <div contenteditable id="host">
      <div>
        <div id="s1">^111</div>
        <slot name="foo">|<div id="foo" slot="foo">foo</div></slot>
        <div id="s2">222</div>
        <slot name="bar"><div id="bar" slot="bar">bar</div></slot>
        <div id="s3">333</div>
      </div></div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result));

  // Select from foo to 111.
  const SelectionInFlatTree& selection2 =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::LastPositionInNode(*foo))
          .Extend(PositionInFlatTree::FirstPositionInNode(*s1))
          .Build();
  const SelectionInFlatTree& result2 =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection2);
  EXPECT_EQ(R"HTML(
      <div contenteditable id="host">
      <div>
        <div id="s1">111</div>
        <slot name="foo"><div id="foo" slot="foo">|foo^</div></slot>
        <div id="s2">222</div>
        <slot name="bar"><div id="bar" slot="bar">bar</div></slot>
        <div id="s3">333</div>
      </div></div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result2));

  // Select from 111 to 222.
  const SelectionInFlatTree& selection3 =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*s1))
          .Extend(PositionInFlatTree::LastPositionInNode(*s2))
          .Build();
  const SelectionInFlatTree& result3 =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection3);
  EXPECT_EQ(R"HTML(
      <div contenteditable id="host">
      <div>
        <div id="s1">^111</div>
        <slot name="foo"><div id="foo" slot="foo">foo</div></slot>
        <div id="s2">222|</div>
        <slot name="bar"><div id="bar" slot="bar">bar</div></slot>
        <div id="s3">333</div>
      </div></div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result3));

  // Select from foo to bar.
  const SelectionInFlatTree& selection4 =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*foo))
          .Extend(PositionInFlatTree::LastPositionInNode(*bar))
          .Build();
  const SelectionInFlatTree& result4 =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection4);
  EXPECT_EQ(R"HTML(
      <div contenteditable id="host">
      <div>
        <div id="s1">111</div>
        <slot name="foo"><div id="foo" slot="foo">^foo|</div></slot>
        <div id="s2">222</div>
        <slot name="bar"><div id="bar" slot="bar">bar</div></slot>
        <div id="s3">333</div>
      </div></div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result4));
}

TEST_F(SelectionAdjusterTest, EditingBoundaryOutsideOfShadowTree) {
  SetBodyContent(R"HTML(
    <div>
      <div id=base>base</div>
      <div id=div1 contenteditable>
        55
        <div id=host></div>
      </div>
    </div>)HTML");
  ShadowRoot* shadow_root =
      SetShadowContent("<div id=extent>extent</div>", "host");
  Element* base = GetDocument().getElementById(AtomicString("base"));
  Element* extent = shadow_root->QuerySelector(AtomicString("#extent"));

  const SelectionInFlatTree& selection =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*base))
          .Extend(PositionInFlatTree::LastPositionInNode(*extent))
          .Build();
  const SelectionInFlatTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);
  EXPECT_EQ(R"HTML(
    <div>
      <div id="base">^base</div>
      |<div contenteditable id="div1">
        55
        <div id="host"><div id="extent">extent</div></div>
      </div>
    </div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result));
}

TEST_F(SelectionAdjusterTest, EditingBoundaryInsideOfShadowTree) {
  SetBodyContent(R"HTML(
    <div>
      <div id=base>base</div>
      <div id=host>foo</div>
    </div>)HTML");
  ShadowRoot* shadow_root = SetShadowContent(R"HTML(
    <div>
      <div>bar</div>
      <div contenteditable id=extent>extent</div>
      <div>baz</div>
    </div>)HTML",
                                             "host");

  Element* base = GetDocument().getElementById(AtomicString("base"));
  Element* extent = shadow_root->QuerySelector(AtomicString("#extent"));

  const SelectionInFlatTree& selection =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*base))
          .Extend(PositionInFlatTree::LastPositionInNode(*extent))
          .Build();
  const SelectionInFlatTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);
  EXPECT_EQ(R"HTML(
    <div>
      <div id="base">^base</div>
      <div id="host">
    <div>
      <div>bar</div>
      |<div contenteditable id="extent">extent</div>
      <div>baz</div>
    </div></div>
    </div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result));
}

// The current behavior of shadow host and shadow tree are editable is we can't
// cross the shadow boundary.
TEST_F(SelectionAdjusterTest, ShadowHostAndShadowTreeAreEditable) {
  SetBodyContent(R"HTML(
    <div contenteditable>
      <div id=foo>foo</div>
      <div id=host></div>
    </div>)HTML");
  ShadowRoot* shadow_root =
      SetShadowContent("<div contenteditable id=bar>bar</div>", "host");

  Element* foo = GetDocument().getElementById(AtomicString("foo"));
  Element* bar = shadow_root->QuerySelector(AtomicString("#bar"));

  // Select from foo to bar.
  const SelectionInFlatTree& selection =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*foo))
          .Extend(PositionInFlatTree::LastPositionInNode(*bar))
          .Build();
  const SelectionInFlatTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);
  EXPECT_EQ(R"HTML(
    <div contenteditable>
      <div id="foo">^foo</div>
      <div id="host">|<div contenteditable id="bar">bar</div></div>
    </div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result));

  // Select from bar to foo.
  const SelectionInFlatTree& selection2 =
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::LastPositionInNode(*bar))
          .Extend(PositionInFlatTree::FirstPositionInNode(*foo))
          .Build();
  const SelectionInFlatTree& result2 =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection2);
  EXPECT_EQ(R"HTML(
    <div contenteditable>
      <div id="foo">foo</div>
      <div id="host"><div contenteditable id="bar">|bar^</div></div>
    </div>)HTML",
            GetSelectionTextInFlatTreeFromBody(result2));
}

TEST_F(SelectionAdjusterTest, AdjustSelectionTypeWithShadow) {
  SetBodyContent("<p id='host'>foo</p>");
  SetShadowContent("bar<slot></slot>", "host");

  Element* host = GetDocument().getElementById(AtomicString("host"));
  const Position& base = Position(host->firstChild(), 0);
  const Position& extent = Position(host, 0);
  const SelectionInDOMTree& selection =
      SelectionInDOMTree::Builder().Collapse(base).Extend(extent).Build();

  // Should not crash
  const SelectionInDOMTree& adjusted =
      SelectionAdjuster::AdjustSelectionType(selection);

  EXPECT_EQ(base, adjusted.Anchor());
  EXPECT_EQ(extent, adjusted.Focus());
}

TEST_F(SelectionAdjusterTest, AdjustShadowWithRootAndHost) {
  SetBodyContent("<div id='host'></div>");
  ShadowRoot* shadow_root = SetShadowContent("", "host");

  Element* host = GetDocument().getElementById(AtomicString("host"));
  const SelectionInDOMTree& selection = SelectionInDOMTree::Builder()
                                            .Collapse(Position(shadow_root, 0))
                                            .Extend(Position(host, 0))
                                            .Build();

  // Should not crash
  const SelectionInDOMTree& result =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingShadowBoundaries(
          selection);

  EXPECT_EQ(Position(shadow_root, 0), result.Anchor());
  EXPECT_EQ(Position(shadow_root, 0), result.Focus());
}

// http://crbug.com/1371268
TEST_F(SelectionAdjusterTest, AdjustSelectionWithNextNonEditableNode) {
  SetBodyContent(R"HTML(
    <div contenteditable=true>
      <div id="one">Paragraph 1</div>
      <div id="two" contenteditable=false>
        <div contenteditable=true>Paragraph 2</div>
      </div>
    </div>)HTML");

  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));
  const SelectionInDOMTree& selection = SelectionInDOMTree::Builder()
                                            .Collapse(Position(one, 0))
                                            .Extend(Position(two, 0))
                                            .Build();
  const SelectionInDOMTree& editing_selection =
      SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
          selection);
  EXPECT_EQ(editing_selection.Anchor(), selection.Anchor());
  EXPECT_EQ(editing_selection.Focus(), Position::BeforeNode(*two));

  const SelectionInDOMTree& adjusted_selection =
      SelectionAdjuster::AdjustSelectionType(editing_selection);
  EXPECT_EQ(adjusted_selection.Anchor(),
            Position::FirstPositionInNode(*one->firstChild()));
  EXPECT_EQ(adjusted_selection.Focus(), editing_selection.Focus());
}

}  // namespace blink
