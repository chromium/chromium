// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_selection.h"

#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_adjuster.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

#define LOREM_IPSUM                                                            \
  "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod "  \
  "tempor "                                                                    \
  "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, "     \
  "quis nostrud "                                                              \
  "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. "     \
  "Duis aute irure "                                                           \
  "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat "    \
  "nulla pariatur."                                                            \
  "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia " \
  "deserunt "                                                                  \
  "mollit anim id est laborum."

namespace blink {

class VisibleSelectionTest : public EditingTestBase {
 protected:
  // Helper function to set the VisibleSelection base/extent.
  template <typename Strategy>
  void SetSelection(VisibleSelectionTemplate<Strategy>& selection, int base) {
    SetSelection(selection, base, base);
  }

  // Helper function to set the VisibleSelection base/extent.
  template <typename Strategy>
  void SetSelection(VisibleSelectionTemplate<Strategy>& selection,
                    int base,
                    int extend) {
    Node* node = GetDocument().body()->firstChild();
    selection = CreateVisibleSelection(
        typename SelectionTemplate<Strategy>::Builder(selection.AsSelection())
            .Collapse(PositionTemplate<Strategy>(node, base))
            .Extend(PositionTemplate<Strategy>(node, extend))
            .Build());
  }

  std::string GetWordSelectionText(const std::string&);

  std::string ComputeVisibleSelection(const std::string& selection_text) {
    Selection().SetSelection(SetSelectionTextToBody(selection_text),
                             SetSelectionOptions());
    const VisibleSelection& visible =
        Selection().ComputeVisibleSelectionInDOMTree();
    return GetSelectionTextFromBody(visible.AsSelection());
  }
};

std::string VisibleSelectionTest::GetWordSelectionText(
    const std::string& selection_text) {
  const PositionInFlatTree position =
      ToPositionInFlatTree(SetSelectionTextToBody(selection_text).Base());
  return GetSelectionTextInFlatTreeFromBody(
      CreateVisibleSelectionWithGranularity(
          SelectionInFlatTree::Builder().Collapse(position).Build(),
          TextGranularity::kWord)
          .AsSelection());
}

static void TestFlatTreePositionsToEqualToDOMTreePositions(
    const VisibleSelection& selection,
    const VisibleSelectionInFlatTree& selection_in_flat_tree) {
  // Since DOM tree positions can't be map to flat tree version, e.g.
  // shadow root, not distributed node, we map a position in flat tree
  // to DOM tree position.
  EXPECT_EQ(selection.Start(),
            ToPositionInDOMTree(selection_in_flat_tree.Start()));
  EXPECT_EQ(selection.End(), ToPositionInDOMTree(selection_in_flat_tree.End()));
  EXPECT_EQ(selection.Base(),
            ToPositionInDOMTree(selection_in_flat_tree.Base()));
  EXPECT_EQ(selection.Extent(),
            ToPositionInDOMTree(selection_in_flat_tree.Extent()));
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy> ExpandUsingGranularity(
    const VisibleSelectionTemplate<Strategy>& selection,
    TextGranularity granularity) {
  return CreateVisibleSelectionWithGranularity(
      typename SelectionTemplate<Strategy>::Builder()
          .SetBaseAndExtent(selection.Base(), selection.Extent())
          .Build(),
      granularity);
}

TEST_F(VisibleSelectionTest, expandUsingGranularity) {
  const char* body_content =
      "<span id=host><a id=one>1</a><a id=two>22</a></span>";
  const char* shadow_content =
      "<p><b id=three>333</b><content select=#two></content><b "
      "id=four>4444</b><span id=space>  </span><content "
      "select=#one></content><b id=five>55555</b></p>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = shadow_root->getElementById("three")->firstChild();
  Node* four = shadow_root->getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();
  Node* space = shadow_root->getElementById("space")->firstChild();

  VisibleSelection selection;
  VisibleSelectionInFlatTree selection_in_flat_tree;

  // From a position at distributed node
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(one, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(one, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(five, 5), selection.Start());
  EXPECT_EQ(Position(five, 5), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(one, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(five, 5), selection_in_flat_tree.End());

  // From a position at distributed node
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(two, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(two, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(space, 0), selection.Start());
  EXPECT_EQ(Position(five, 5), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(three, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(four, 4), selection_in_flat_tree.End());

  // From a position at node in shadow tree
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(three, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(three, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(three, 0), selection.Start());
  EXPECT_EQ(Position(four, 4), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(three, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(four, 4), selection_in_flat_tree.End());

  // From a position at node in shadow tree
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(four, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(four, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(three, 0), selection.Start());
  EXPECT_EQ(Position(four, 4), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(three, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(four, 4), selection_in_flat_tree.End());

  // From a position at node in shadow tree
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(five, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(five, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(five, 0), selection.Start());
  EXPECT_EQ(Position(five, 5), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(one, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(five, 5), selection_in_flat_tree.End());
}

// For http://wkb.ug/32622
TEST_F(VisibleSelectionTest, ExpandUsingGranularityWithEmptyCell) {
  const SelectionInDOMTree& selection_in_dom_tree = SetSelectionTextToBody(
      "<div contentEditable><table cellspacing=0><tr>"
      "<td id='first' width='50' height='25pt'>|</td>"
      "<td id='second' width='50' height='25pt'></td>"
      "</tr></table></div>");
  const VisibleSelectionInFlatTree& selection =
      CreateVisibleSelectionWithGranularity(
          ConvertToSelectionInFlatTree(selection_in_dom_tree),
          TextGranularity::kWord);
  EXPECT_EQ(
      "<div contenteditable><table cellspacing=\"0\"><tbody><tr>"
      "<td height=\"25pt\" id=\"first\" width=\"50\">|</td>"
      "<td height=\"25pt\" id=\"second\" width=\"50\"></td>"
      "</tr></tbody></table></div>",
      GetSelectionTextInFlatTreeFromBody(selection.AsSelection()));
}

TEST_F(VisibleSelectionTest, Initialisation) {
  SetBodyContent(LOREM_IPSUM);

  VisibleSelection selection;
  VisibleSelectionInFlatTree selection_in_flat_tree;
  SetSelection(selection, 0);
  SetSelection(selection_in_flat_tree, 0);

  EXPECT_FALSE(selection.IsNone());
  EXPECT_FALSE(selection_in_flat_tree.IsNone());
  EXPECT_TRUE(selection.IsCaret());
  EXPECT_TRUE(selection_in_flat_tree.IsCaret());

  Range* range = CreateRange(FirstEphemeralRangeOf(selection));
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(0u, range->endOffset());
  EXPECT_EQ("", range->GetText());
  TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                 selection_in_flat_tree);

  const VisibleSelection no_selection =
      CreateVisibleSelection(SelectionInDOMTree::Builder().Build());
  EXPECT_TRUE(no_selection.IsNone());
}

TEST_F(VisibleSelectionTest, FirstLetter) {
  SetBodyContent(
      "<style>p::first-letter { font-color: red; }</style>"
      "<p>abc def</p>");
  const Element* sample = GetDocument().QuerySelector("p");
  const SelectionInDOMTree selection =
      SelectionInDOMTree::Builder()
          .Collapse(Position(sample->firstChild(), 0))
          .Extend(Position(sample->firstChild(), 3))
          .Build();
  const VisibleSelection visible_selection = CreateVisibleSelection(selection);

  EXPECT_EQ(selection, visible_selection.AsSelection());
}

TEST_F(VisibleSelectionTest, FirstLetterCollapsedWhitespace) {
  SetBodyContent(
      "<style>p::first-letter { font-color: red; }</style>"
      "<p>  abc def</p>");
  const Element* sample = GetDocument().QuerySelector("p");
  const SelectionInDOMTree selection =
      SelectionInDOMTree::Builder()
          .Collapse(Position(sample->firstChild(), 0))
          .Extend(Position(sample->firstChild(), 5))
          .Build();
  const VisibleSelection visible_selection = CreateVisibleSelection(selection);

  EXPECT_EQ(SelectionInDOMTree::Builder()
                .Collapse(Position(sample->firstChild(), 2))
                .Extend(Position(sample->firstChild(), 5))
                .Build(),
            visible_selection.AsSelection())
      << "VisibleSelection doesn't contains collapsed whitespaces";
}

TEST_F(VisibleSelectionTest, FirstLetterPartial) {
  SetBodyContent(
      "<style>p::first-letter { font-color: red; }</style>"
      "<p>((a))bc def</p>");
  const Element* sample = GetDocument().QuerySelector("p");
  const SelectionInDOMTree selection =
      SelectionInDOMTree::Builder()
          .Collapse(Position(sample->firstChild(), 1))
          .Extend(Position(sample->firstChild(), 4))
          .Build();
  const VisibleSelection visible_selection = CreateVisibleSelection(selection);

  EXPECT_EQ(selection, visible_selection.AsSelection())
      << "Select '(a)' of '((a))";
}

TEST_F(VisibleSelectionTest, FirstLetterTextTransform) {
  SetBodyContent(
      "<style>p::first-letter { text-transform: uppercase; }</style>"
      "<p>\u00DFbc def</p>");  // uppercase(U+00DF) = "SS"
  const Element* sample = GetDocument().QuerySelector("p");
  const SelectionInDOMTree selection =
      SelectionInDOMTree::Builder()
          .Collapse(Position(sample->firstChild(), 0))
          .Extend(Position(sample->firstChild(), 3))
          .Build();
  const VisibleSelection visible_selection = CreateVisibleSelection(selection);

  EXPECT_EQ(selection, visible_selection.AsSelection());
}

TEST_F(VisibleSelectionTest, FirstLetterVisibilityHidden) {
  SetBodyContent(
      "<style>p::first-letter { visibility: hidden; }</style>"
      "<p>abc def</p>");
  const Element* sample = GetDocument().QuerySelector("p");
  const SelectionInDOMTree selection =
      SelectionInDOMTree::Builder()
          .Collapse(Position(sample->firstChild(), 0))
          .Extend(Position(sample->firstChild(), 3))
          .Build();
  const VisibleSelection visible_selection = CreateVisibleSelection(selection);

  EXPECT_EQ(SelectionInDOMTree::Builder()
                .Collapse(Position(sample->firstChild(), 1))
                .Extend(Position(sample->firstChild(), 3))
                .Build(),
            visible_selection.AsSelection())
      << "Exclude first-letter part since it is visibility::hidden";
}

// For http://crbug.com/695317
TEST_F(VisibleSelectionTest, SelectAllWithInputElement) {
  SetBodyContent("<input>123");
  Element* const html_element = GetDocument().documentElement();
  Element* const input = GetDocument().QuerySelector("input");
  Node* const last_child = GetDocument().body()->lastChild();

  const VisibleSelection& visible_selection_in_dom_tree =
      CreateVisibleSelection(
          SelectionInDOMTree::Builder()
              .Collapse(Position::FirstPositionInNode(*html_element))
              .Extend(Position::LastPositionInNode(*html_element))
              .Build());
  EXPECT_EQ(SelectionInDOMTree::Builder()
                .Collapse(Position::BeforeNode(*input))
                .Extend(Position(last_child, 3))
                .Build(),
            visible_selection_in_dom_tree.AsSelection());

  const VisibleSelectionInFlatTree& visible_selection_in_flat_tree =
      CreateVisibleSelection(
          SelectionInFlatTree::Builder()
              .Collapse(PositionInFlatTree::FirstPositionInNode(*html_element))
              .Extend(PositionInFlatTree::LastPositionInNode(*html_element))
              .Build());
  EXPECT_EQ(SelectionInFlatTree::Builder()
                .Collapse(PositionInFlatTree::BeforeNode(*input))
                .Extend(PositionInFlatTree(last_child, 3))
                .Build(),
            visible_selection_in_flat_tree.AsSelection());
}

TEST_F(VisibleSelectionTest, GetWordSelectionTextWithTextSecurity) {
  InsertStyleElement("s {-webkit-text-security:disc;}");
  // Note: |CreateVisibleSelectionWithGranularity()| considers security
  // characters as a sequence "x".
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("|abc<s>foo bar</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("a|bc<s>foo bar</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc|<s>foo bar</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>|foo bar</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>f|oo bar</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>fo|o bar</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>foo| bar</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>foo |bar</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>foo b|ar</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>foo ba|r</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>foo bar|</s>baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>foo bar</s>|baz"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>foo bar</s>b|az"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>foo bar</s>ba|z"));
  EXPECT_EQ("^abc<s>foo bar</s>baz|",
            GetWordSelectionText("abc<s>foo bar</s>baz|"));
}

TEST_F(VisibleSelectionTest, ShadowCrossing) {
  const char* body_content =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadow_content =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* host = body->QuerySelector("#host");
  Element* one = body->QuerySelector("#one");
  Element* six = shadow_root->QuerySelector("#s6");

  VisibleSelection selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position::FirstPositionInNode(*one))
          .Extend(Position::LastPositionInNode(*shadow_root))
          .Build());
  VisibleSelectionInFlatTree selection_in_flat_tree = CreateVisibleSelection(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*one))
          .Extend(PositionInFlatTree::LastPositionInNode(*host))
          .Build());

  EXPECT_EQ(Position(host, PositionAnchorType::kBeforeAnchor),
            selection.Start());
  EXPECT_EQ(Position(host, PositionAnchorType::kBeforeAnchor), selection.End());
  EXPECT_EQ(PositionInFlatTree(one->firstChild(), 0),
            selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(six->firstChild(), 2),
            selection_in_flat_tree.End());
}

TEST_F(VisibleSelectionTest, ShadowV0DistributedNodes) {
  const char* body_content =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadow_content =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* one = body->QuerySelector("#one");
  Element* two = body->QuerySelector("#two");
  Element* five = shadow_root->QuerySelector("#s5");

  VisibleSelection selection =
      CreateVisibleSelection(SelectionInDOMTree::Builder()
                                 .Collapse(Position::FirstPositionInNode(*one))
                                 .Extend(Position::LastPositionInNode(*two))
                                 .Build());
  VisibleSelectionInFlatTree selection_in_flat_tree = CreateVisibleSelection(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*one))
          .Extend(PositionInFlatTree::LastPositionInNode(*two))
          .Build());

  EXPECT_EQ(Position(one->firstChild(), 0), selection.Start());
  EXPECT_EQ(Position(two->firstChild(), 2), selection.End());
  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 0),
            selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 2),
            selection_in_flat_tree.End());
}

TEST_F(VisibleSelectionTest, ShadowNested) {
  const char* body_content =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadow_content =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  const char* shadow_content2 =
      "<span id='s7'>77</span><content></content><span id='s8'>88</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  ShadowRoot* shadow_root2 = CreateShadowRootForElementWithIDAndSetInnerHTML(
      *shadow_root, "s5", shadow_content2);

  // Flat tree is something like below:
  //  <p id="host">
  //    <span id="s4">44</span>
  //    <b id="two">22</b>
  //    <span id="s5"><span id="s7">77>55</span id="s8">88</span>
  //    <b id="one">11</b>
  //    <span id="s6">66</span>
  //  </p>
  Element* body = GetDocument().body();
  Element* host = body->QuerySelector("#host");
  Element* one = body->QuerySelector("#one");
  Element* eight = shadow_root2->QuerySelector("#s8");

  VisibleSelection selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position::FirstPositionInNode(*one))
          .Extend(Position::LastPositionInNode(*shadow_root2))
          .Build());
  VisibleSelectionInFlatTree selection_in_flat_tree = CreateVisibleSelection(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*one))
          .Extend(PositionInFlatTree::AfterNode(*eight))
          .Build());

  EXPECT_EQ(Position(host, PositionAnchorType::kBeforeAnchor),
            selection.Start());
  EXPECT_EQ(Position(host, PositionAnchorType::kBeforeAnchor), selection.End());
  EXPECT_EQ(PositionInFlatTree(eight->firstChild(), 2),
            selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(eight->firstChild(), 2),
            selection_in_flat_tree.End());
}

TEST_F(VisibleSelectionTest, WordGranularity) {
  SetBodyContent(LOREM_IPSUM);

  VisibleSelection selection;
  VisibleSelectionInFlatTree selection_in_flat_tree;

  // Beginning of a word.
  {
    SetSelection(selection, 0);
    SetSelection(selection_in_flat_tree, 0);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(0u, range->startOffset());
    EXPECT_EQ(5u, range->endOffset());
    EXPECT_EQ("Lorem", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // Middle of a word.
  {
    SetSelection(selection, 8);
    SetSelection(selection_in_flat_tree, 8);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(6u, range->startOffset());
    EXPECT_EQ(11u, range->endOffset());
    EXPECT_EQ("ipsum", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // End of a word.
  // FIXME: that sounds buggy, we might want to select the word _before_ instead
  // of the space...
  {
    SetSelection(selection, 5);
    SetSelection(selection_in_flat_tree, 5);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(5u, range->startOffset());
    EXPECT_EQ(6u, range->endOffset());
    EXPECT_EQ(" ", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // Before comma.
  // FIXME: that sounds buggy, we might want to select the word _before_ instead
  // of the comma.
  {
    SetSelection(selection, 26);
    SetSelection(selection_in_flat_tree, 26);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(26u, range->startOffset());
    EXPECT_EQ(27u, range->endOffset());
    EXPECT_EQ(",", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // After comma.
  {
    SetSelection(selection, 27);
    SetSelection(selection_in_flat_tree, 27);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(27u, range->startOffset());
    EXPECT_EQ(28u, range->endOffset());
    EXPECT_EQ(" ", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // When selecting part of a word.
  {
    SetSelection(selection, 0, 1);
    SetSelection(selection_in_flat_tree, 0, 1);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(0u, range->startOffset());
    EXPECT_EQ(5u, range->endOffset());
    EXPECT_EQ("Lorem", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // When selecting part of two words.
  {
    SetSelection(selection, 2, 8);
    SetSelection(selection_in_flat_tree, 2, 8);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(0u, range->startOffset());
    EXPECT_EQ(11u, range->endOffset());
    EXPECT_EQ("Lorem ipsum", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }
}

// This is for crbug.com/627783, simulating restoring selection
// in undo stack.
TEST_F(VisibleSelectionTest, updateIfNeededWithShadowHost) {
  SetBodyContent("<div id=host></div><div id=sample>foo</div>");
  SetShadowContent("<content>", "host");
  Element* sample = GetDocument().getElementById("sample");

  // Simulates saving selection in undo stack.
  VisibleSelection selection =
      CreateVisibleSelection(SelectionInDOMTree::Builder()
                                 .Collapse(Position(sample->firstChild(), 0))
                                 .Build());
  EXPECT_EQ(Position(sample->firstChild(), 0), selection.Start());

  // Simulates modifying DOM tree to invalidate distribution.
  Element* host = GetDocument().getElementById("host");
  host->AppendChild(sample);
  GetDocument().UpdateStyleAndLayout();

  // Simulates to restore selection from undo stack.
  selection = CreateVisibleSelection(selection.AsSelection());
  EXPECT_EQ(Position(sample->firstChild(), 0), selection.Start());
}

// This is a regression test for https://crbug.com/825120
TEST_F(VisibleSelectionTest, BackwardSelectionWithMultipleEmptyBodies) {
  Element* body = GetDocument().body();
  Element* new_body = GetDocument().CreateRawElement(HTMLNames::bodyTag);
  body->appendChild(new_body);
  GetDocument().UpdateStyleAndLayout();

  const SelectionInDOMTree selection =
      SelectionInDOMTree::Builder()
          .Collapse(Position::BeforeNode(*new_body))
          .Extend(Position::BeforeNode(*body))
          .Build();
  const VisibleSelection visible_selection = CreateVisibleSelection(selection);

  EXPECT_EQ("^<body></body>", GetSelectionTextFromBody(selection));
  EXPECT_EQ("|<body></body>",
            GetSelectionTextFromBody(visible_selection.AsSelection()));
}

// Confirm canonicalization.
#define EXPECT_EQ_VS(input, expect) \
  EXPECT_EQ(ComputeVisibleSelection(input), expect)

TEST_F(VisibleSelectionTest, ComputeVisibleSelectionBasic) {
  EXPECT_EQ_VS("fo^o<br>ba|r", "fo^o<br>ba|r");
  EXPECT_EQ_VS("fo|o<br>ba^r", "fo|o<br>ba^r");
  EXPECT_EQ_VS("foo<!--|--><br><!--^-->bar", "foo|<br>^bar");
}

TEST_F(VisibleSelectionTest, ComputeVisibleSelectionBR) {
  EXPECT_EQ_VS("fo^o<br>|", "fo^o|<br>");
  EXPECT_EQ_VS("fo^o<br><br>|", "fo^o<br>|<br>");
  EXPECT_EQ_VS("foo<br>^<br>|", "foo<br>|<br>");
  EXPECT_EQ_VS("foo<!--|--><br>", "foo|<br>");
  EXPECT_EQ_VS("foo<br>|", "foo|<br>");
}

TEST_F(VisibleSelectionTest, ComputeVisibleSelectionCaret) {
  EXPECT_EQ_VS("fo|o", "fo|o");
  EXPECT_EQ_VS("<!--|-->foo", "|foo");
  EXPECT_EQ_VS("foo<!--|-->", "foo|");
  EXPECT_EQ_VS("<div>|</div>", "|<div></div>");
  EXPECT_EQ_VS("<div contenteditable><div>|</div></div>",
               "<div contenteditable><div></div></div>");
  EXPECT_EQ_VS("<div contenteditable>foo<div>|</div>bar</div>",
               "<div contenteditable>foo<div></div>|bar</div>");
  EXPECT_EQ_VS("<div contenteditable>|</div>", "<div contenteditable>|</div>");
}

TEST_F(VisibleSelectionTest, ComputeVisibleSelectionEdgeIsNone) {
  EXPECT_EQ_VS("fo|o<b style=\"display:none;\">b^ar</b>",
               "fo|o^<b style=\"display:none;\">bar</b>");
  EXPECT_EQ_VS("<b style=\"display:none;\">f^oo</b>ba|r",
               "<b style=\"display:none;\">foo</b>^ba|r");
  EXPECT_EQ_VS(
      "<b style=\"display:none;\">f^oo</b>"
      "bar<b style=\"display:none;\">b|az</b>",
      "<b style=\"display:none;\">foo</b>"
      "^bar|<b style=\"display:none;\">baz</b>");
}

TEST_F(VisibleSelectionTest, ComputeVisibleSelectionInsideNone) {
  EXPECT_EQ_VS("foo<b style=\"display:none;\">b^a|r</b>baz",
               "foo|<b style=\"display:none;\">bar</b>baz");
  EXPECT_EQ_VS("<b style=\"display:none;\">b|a^r</b>baz",
               "<b style=\"display:none;\">bar</b>|baz");
}

}  // namespace blink
