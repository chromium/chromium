// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/position_iterator.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"

namespace blink {

using ::testing::ElementsAre;

class PositionIteratorTest : public EditingTestBase {
 protected:
  std::vector<std::string> ScanBackward(const char* selection_text) {
    return ScanBackward(SetCaretTextToBody(selection_text));
  }

  std::vector<std::string> ScanBackwardInFlatTree(const char* selection_text) {
    return ScanBackward(
        ToPositionInFlatTree(SetCaretTextToBody(selection_text)));
  }

  std::vector<std::string> ScanForward(const char* selection_text) {
    return ScanForward(SetCaretTextToBody(selection_text));
  }

  std::vector<std::string> ScanForwardInFlatTree(const char* selection_text) {
    return ScanForward(
        ToPositionInFlatTree(SetCaretTextToBody(selection_text)));
  }

  template <typename Strategy>
  std::vector<std::string> ScanBackward(
      const PositionTemplate<Strategy>& start) {
    std::vector<std::string> positions;
    for (PositionIteratorAlgorithm<Strategy> it(start); !it.AtStart();
         it.Decrement()) {
      positions.push_back(ToString(it));
    }
    return positions;
  }

  template <typename Strategy>
  std::vector<std::string> ScanForward(
      const PositionTemplate<Strategy>& start) {
    std::vector<std::string> positions;
    for (PositionIteratorAlgorithm<Strategy> it(start); !it.AtEnd();
         it.Increment()) {
      positions.push_back(ToString(it));
    }
    return positions;
  }

 private:
  template <typename Strategy>
  static std::string ToString(const PositionIteratorAlgorithm<Strategy>& it) {
    const PositionTemplate<Strategy> position1 = it.ComputePosition();
    const PositionTemplate<Strategy> position2 = it.DeprecatedComputePosition();
    std::ostringstream os;
    os << (it.AtStart() ? "S" : "-") << (it.AtStartOfNode() ? "S" : "-")
       << (it.AtEnd() ? "E" : "-") << (it.AtEndOfNode() ? "E" : "-") << " "
       << it.GetNode();
    if (IsA<Text>(it.GetNode())) {
      os << "@" << it.OffsetInTextNode();
    } else if (EditingIgnoresContent(*it.GetNode())) {
      os << "@" << (it.AtStartOfNode() ? "0" : "1");
    }
    os << " " << position1;
    if (position1 != position2)
      os << " " << position2;
    return os.str();
  }
};

TEST_F(PositionIteratorTest, DecrementFromInputElementAfterChildren) {
  // FlatTree is "ABC" <input><div>123</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree::LastPositionInNode(input_element)),
      ElementsAre("---E INPUT@1 INPUT@afterAnchor",
                  // Note: `DeprecatedComputePosition()` should return
                  // `INPUT@beforeAnchor`.
                  "-S-E INPUT@0 INPUT@beforeAnchor INPUT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromInInputElementAfterNode) {
  // FlatTree is "ABC" <input><div>123</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree::AfterNode(input_element)),
      ElementsAre("---E INPUT@1 INPUT@afterAnchor",
                  // Note: `DeprecatedComputePosition()` should return
                  // `INPUT@beforeAnchor`.
                  "-S-E INPUT@0 INPUT@beforeAnchor INPUT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromInInputElementBeforeNode) {
  // FlatTree is "ABC" <input><div>123</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree::BeforeNode(input_element)),
      ElementsAre("-S-- INPUT@0 INPUT@offsetInAnchor[0] INPUT@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromInInputElementInnerEditorAfterNode) {
  // FlatTree is "ABC" <input><div>"123"</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  // `PositionIterator` stops at `<input>`.
  EXPECT_THAT(
      ScanBackward(
          PositionInFlatTree::AfterNode(*input_element.InnerEditorElement())),
      ElementsAre("---E DIV (editable) DIV (editable)@afterChildren",
                  "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
                  "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
                  "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
                  "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
                  "-S-- DIV (editable) DIV (editable)@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromInInputElementOffset0) {
  // FlatTree is "ABC" <input><div>"123"</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree(input_element, 0)),
      ElementsAre("-S-- INPUT@0 INPUT@offsetInAnchor[0] INPUT@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromInInputElementOffset1) {
  // FlatTree is "ABC" <input><div>"123"</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree(input_element, 1)),
      ElementsAre("---E INPUT@1 INPUT@afterAnchor",
                  // Note: `DeprecatedComputePosition()` should return
                  // `INPUT@beforeAnchor`.
                  "-S-E INPUT@0 INPUT@beforeAnchor INPUT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromObjectElementAfterChildren) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree::LastPositionInNode(object_element)),
      ElementsAre("---E OBJECT@1 OBJECT@afterAnchor",
                  "-S-E SLOT SLOT@beforeAnchor SLOT@offsetInAnchor[0]",
                  "-S-- OBJECT@0 OBJECT@offsetInAnchor[0] OBJECT@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromObjectElementAfterNode) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree::AfterNode(object_element)),
      ElementsAre("---E OBJECT@1 OBJECT@afterAnchor",
                  "-S-E SLOT SLOT@beforeAnchor SLOT@offsetInAnchor[0]",
                  "-S-- OBJECT@0 OBJECT@offsetInAnchor[0] OBJECT@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromObjectElementBeforeNode) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree::BeforeNode(object_element)),
      ElementsAre("-S-- OBJECT@0 OBJECT@offsetInAnchor[0] OBJECT@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromObjectElementOffset0) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree(object_element, 0)),
      ElementsAre("-S-- OBJECT@0 OBJECT@offsetInAnchor[0] OBJECT@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromObjectElementOffset1) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree(object_element, 1)),
      ElementsAre("---E OBJECT@1 OBJECT@afterAnchor",
                  "-S-E SLOT SLOT@beforeAnchor SLOT@offsetInAnchor[0]",
                  "-S-- OBJECT@0 OBJECT@offsetInAnchor[0] OBJECT@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromSelectElementAfterChildren) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  for (const Node* node = &select_element; node;
       node = FlatTreeTraversal::Next(*node))
    DVLOG(0) << node << " " << FlatTreeTraversal::Parent(*node);
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree::LastPositionInNode(select_element)),
      ElementsAre("---E SELECT@1 SELECT@afterAnchor",
                  "---E SELECT@1 SELECT@afterAnchor",
                  "---E SELECT@1 SELECT@afterAnchor",
                  "---E SELECT@1 SELECT@afterAnchor",
                  "---E SELECT@1 SELECT@afterAnchor",
                  "-S-E SELECT@0 SELECT@beforeAnchor SELECT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromSelectElementAfterNode) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree::AfterNode(select_element)),
      ElementsAre("---E SELECT@1 SELECT@afterAnchor",
                  "---E SELECT@1 SELECT@afterAnchor",
                  "---E SELECT@1 SELECT@afterAnchor",
                  "---E SELECT@1 SELECT@afterAnchor",
                  "---E SELECT@1 SELECT@afterAnchor",
                  "-S-E SELECT@0 SELECT@beforeAnchor SELECT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromSelectElementBeforeNode) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree::BeforeNode(select_element)),
      ElementsAre("-S-- SELECT@0 SELECT@offsetInAnchor[0] SELECT@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromSelectElementOffset0) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree(select_element, 0)),
      ElementsAre("-S-- SELECT@0 SELECT@offsetInAnchor[0] SELECT@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementFromSelectElementOffset1) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  EXPECT_THAT(
      ScanBackward(PositionInFlatTree(select_element, 1)),
      ElementsAre("---- SELECT@1 SELECT@offsetInAnchor[1] SELECT@beforeAnchor",
                  "---E DIV DIV@afterChildren",
                  "-S-E #text \"\"@0 #text \"\"@offsetInAnchor[0]",
                  "-S-- DIV DIV@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementWithBrInOption) {
  const char* selection_text = "<option><br></option>|";

  // `<br>` is not associated to `LayoutObject`.
  EXPECT_THAT(ScanBackward(selection_text),
              ElementsAre("---E BODY BODY@afterChildren",
                          "---E OPTION OPTION@afterChildren",
                          "---E BR@1 BR@afterAnchor",
                          "-S-- OPTION OPTION@offsetInAnchor[0]",
                          "-S-- BODY BODY@offsetInAnchor[0]",
                          "---- HTML HTML@offsetInAnchor[1]",
                          "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                          "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementWithCollapsedSpace) {
  const char* selection_text = "<p> abc </p>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre("---E BODY BODY@afterChildren", "---E P P@afterChildren",
                  "---E #text \" abc \"@5 #text \" abc \"@offsetInAnchor[5]",
                  "---- #text \" abc \"@4 #text \" abc \"@offsetInAnchor[4]",
                  "---- #text \" abc \"@3 #text \" abc \"@offsetInAnchor[3]",
                  "---- #text \" abc \"@2 #text \" abc \"@offsetInAnchor[2]",
                  "---- #text \" abc \"@1 #text \" abc \"@offsetInAnchor[1]",
                  "-S-- #text \" abc \"@0 #text \" abc \"@offsetInAnchor[0]",
                  "-S-- P P@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementWithCommentEmpty) {
  const char* selection_text = "<p>a<br>b<br><!----><br>c</p>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre("---E BODY BODY@afterChildren", "---E P P@afterChildren",
                  "---E #text \"c\"@1 #text \"c\"@offsetInAnchor[1]",
                  "-S-- #text \"c\"@0 #text \"c\"@offsetInAnchor[0]",
                  "---- P P@offsetInAnchor[6]", "---E BR@1 BR@afterAnchor",
                  "-S-- BR@0 BR@beforeAnchor", "---- P P@offsetInAnchor[5]",
                  // Empty comment returns true for `AtStartNode()` and
                  // `AtEndOfNode()`.
                  "-S-E #comment@0 #comment@beforeAnchor",
                  "---- P P@offsetInAnchor[4]", "---E BR@1 BR@afterAnchor",
                  "-S-- BR@0 BR@beforeAnchor", "---- P P@offsetInAnchor[3]",
                  "---E #text \"b\"@1 #text \"b\"@offsetInAnchor[1]",
                  "-S-- #text \"b\"@0 #text \"b\"@offsetInAnchor[0]",
                  "---- P P@offsetInAnchor[2]", "---E BR@1 BR@afterAnchor",
                  "-S-- BR@0 BR@beforeAnchor", "---- P P@offsetInAnchor[1]",
                  "---E #text \"a\"@1 #text \"a\"@offsetInAnchor[1]",
                  "-S-- #text \"a\"@0 #text \"a\"@offsetInAnchor[0]",
                  "-S-- P P@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementWithCommentNotEmpty) {
  const char* selection_text = "<p>a<br>b<br><!--XYZ--><br>c</p>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre("---E BODY BODY@afterChildren", "---E P P@afterChildren",
                  "---E #text \"c\"@1 #text \"c\"@offsetInAnchor[1]",
                  "-S-- #text \"c\"@0 #text \"c\"@offsetInAnchor[0]",
                  "---- P P@offsetInAnchor[6]", "---E BR@1 BR@afterAnchor",
                  "-S-- BR@0 BR@beforeAnchor", "---- P P@offsetInAnchor[5]",
                  // Not-empty comment returns true for `AtEndOfNode()`.
                  "---E #comment@1 #comment@afterAnchor",
                  "---- P P@offsetInAnchor[4]", "---E BR@1 BR@afterAnchor",
                  "-S-- BR@0 BR@beforeAnchor", "---- P P@offsetInAnchor[3]",
                  "---E #text \"b\"@1 #text \"b\"@offsetInAnchor[1]",
                  "-S-- #text \"b\"@0 #text \"b\"@offsetInAnchor[0]",
                  "---- P P@offsetInAnchor[2]", "---E BR@1 BR@afterAnchor",
                  "-S-- BR@0 BR@beforeAnchor", "---- P P@offsetInAnchor[1]",
                  "---E #text \"a\"@1 #text \"a\"@offsetInAnchor[1]",
                  "-S-- #text \"a\"@0 #text \"a\"@offsetInAnchor[0]",
                  "-S-- P P@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementWithInlineElemnt) {
  const char* selection_text = "<p><a><b>ABC</b></a><i><s>DEF</s></i></p>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre("---E BODY BODY@afterChildren", "---E P P@afterChildren",
                  "---E I I@afterChildren", "---E S S@afterChildren",
                  "---E #text \"DEF\"@3 #text \"DEF\"@offsetInAnchor[3]",
                  "---- #text \"DEF\"@2 #text \"DEF\"@offsetInAnchor[2]",
                  "---- #text \"DEF\"@1 #text \"DEF\"@offsetInAnchor[1]",
                  "-S-- #text \"DEF\"@0 #text \"DEF\"@offsetInAnchor[0]",
                  "-S-- S S@offsetInAnchor[0]", "-S-- I I@offsetInAnchor[0]",
                  "---- P P@offsetInAnchor[1]", "---E A A@afterChildren",
                  "---E B B@afterChildren",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "-S-- B B@offsetInAnchor[0]", "-S-- A A@offsetInAnchor[0]",
                  "-S-- P P@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, decrementWithInputElement) {
  const char* const selection_text = "123<input id=target value='abc'>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre("---E BODY BODY@afterChildren",
                  "---E INPUT id=\"target\"@1 INPUT id=\"target\"@afterAnchor",
                  "-S-- INPUT id=\"target\"@0 INPUT id=\"target\"@beforeAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
                  "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
                  "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
                  "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
                  "-S-- BODY BODY@offsetInAnchor[0]",
                  "---- HTML HTML@offsetInAnchor[1]",
                  "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML HTML@offsetInAnchor[0]"));

  EXPECT_THAT(
      ScanBackwardInFlatTree(selection_text),
      ElementsAre(
          "---E BODY BODY@afterChildren",
          "---E INPUT id=\"target\"@1 INPUT id=\"target\"@afterAnchor",
          "-S-E INPUT id=\"target\"@0 INPUT id=\"target\"@beforeAnchor INPUT "
          "id=\"target\"@afterAnchor",
          "---- BODY BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY BODY@offsetInAnchor[0]",
          "---- HTML HTML@offsetInAnchor[1]",
          "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, DecrementWithNoChildren) {
  const char* const selection_text = "abc<br>def<img><br>|";
  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre(
          "---E BODY BODY@afterChildren", "---E BR@1 BR@afterAnchor",
          "-S-- BR@0 BR@beforeAnchor", "---- BODY BODY@offsetInAnchor[4]",
          "---E IMG@1 IMG@afterAnchor", "-S-- IMG@0 IMG@beforeAnchor",
          "---- BODY BODY@offsetInAnchor[3]",
          "---E #text \"def\"@3 #text \"def\"@offsetInAnchor[3]",
          "---- #text \"def\"@2 #text \"def\"@offsetInAnchor[2]",
          "---- #text \"def\"@1 #text \"def\"@offsetInAnchor[1]",
          "-S-- #text \"def\"@0 #text \"def\"@offsetInAnchor[0]",
          "---- BODY BODY@offsetInAnchor[2]", "---E BR@1 BR@afterAnchor",
          "-S-- BR@0 BR@beforeAnchor", "---- BODY BODY@offsetInAnchor[1]",
          "---E #text \"abc\"@3 #text \"abc\"@offsetInAnchor[3]",
          "---- #text \"abc\"@2 #text \"abc\"@offsetInAnchor[2]",
          "---- #text \"abc\"@1 #text \"abc\"@offsetInAnchor[1]",
          "-S-- #text \"abc\"@0 #text \"abc\"@offsetInAnchor[0]",
          "-S-- BODY BODY@offsetInAnchor[0]",
          "---- HTML HTML@offsetInAnchor[1]",
          "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, decrementWithSelectElement) {
  const char* const selection_text =
      "123<select id=target><option>1</option><option>2</option></select>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre(
          "---E BODY BODY@afterChildren",
          "---E SELECT id=\"target\"@1 SELECT id=\"target\"@afterAnchor",
          "-S-E SELECT id=\"target\"@0 SELECT id=\"target\"@beforeAnchor "
          "SELECT id=\"target\"@afterAnchor",
          "---- BODY BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY BODY@offsetInAnchor[0]",
          "---- HTML HTML@offsetInAnchor[1]",
          "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML HTML@offsetInAnchor[0]"));

  EXPECT_THAT(
      ScanBackwardInFlatTree(selection_text),
      ElementsAre(
          "---E BODY BODY@afterChildren",
          "---E SELECT id=\"target\"@1 SELECT id=\"target\"@afterAnchor",
          // Note: `DeprecatedComputePosition()` should return
          // `SELECT@beforeAnchor`.
          "-S-E SELECT id=\"target\"@0 SELECT id=\"target\"@beforeAnchor "
          "SELECT id=\"target\"@afterAnchor",
          "---- BODY BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY BODY@offsetInAnchor[0]",
          "---- HTML HTML@offsetInAnchor[1]",
          "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML HTML@offsetInAnchor[0]"));
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, decrementWithTextAreaElement) {
  const char* const selection_text = "123<textarea id=target>456</textarea>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre(
          "---E BODY BODY@afterChildren",
          "---E TEXTAREA id=\"target\"@1 TEXTAREA id=\"target\"@afterAnchor",
          // Note: `DeprecatedComputePosition()` should return
          // `SELECT@beforeAnchor`.
          "-S-E TEXTAREA id=\"target\"@0 TEXTAREA id=\"target\"@beforeAnchor "
          "TEXTAREA id=\"target\"@afterAnchor",
          "---- BODY BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY BODY@offsetInAnchor[0]",
          "---- HTML HTML@offsetInAnchor[1]",
          "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML HTML@offsetInAnchor[0]"));

  EXPECT_THAT(
      ScanBackwardInFlatTree(selection_text),
      ElementsAre(
          "---E BODY BODY@afterChildren",
          "---E TEXTAREA id=\"target\"@1 TEXTAREA id=\"target\"@afterAnchor",
          // Note: `DeprecatedComputePosition()` should return
          // `SELECT@beforeAnchor`.
          "-S-E TEXTAREA id=\"target\"@0 TEXTAREA id=\"target\"@beforeAnchor "
          "TEXTAREA id=\"target\"@afterAnchor",
          "---- BODY BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY BODY@offsetInAnchor[0]",
          "---- HTML HTML@offsetInAnchor[1]",
          "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML HTML@offsetInAnchor[0]"));
}

// http://crbug.com/1392758
TEST_F(PositionIteratorTest, DecrementWithTextAreaFromAfterChildren) {
  SetBodyContent("<textarea>abc</textarea>");
  const Element& body = *GetDocument().body();
  const auto expectation = ElementsAre(
      "---E BODY BODY@afterChildren", "---E TEXTAREA@1 TEXTAREA@afterAnchor",
      "-S-E TEXTAREA@0 TEXTAREA@beforeAnchor TEXTAREA@afterAnchor",
      "-S-- BODY BODY@offsetInAnchor[0]", "---- HTML HTML@offsetInAnchor[1]",
      "-S-E HEAD HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
      "-S-- HTML HTML@offsetInAnchor[0]");

  EXPECT_THAT(ScanBackward(Position(body, 1)), expectation);
  EXPECT_THAT(ScanBackward(Position::LastPositionInNode(body)), expectation);
}

// ---

TEST_F(PositionIteratorTest, IncrementFromInputElementAfterChildren) {
  // FlatTree is "ABC" <input><div>123</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree::LastPositionInNode(input_element)),
      ElementsAre(
          "---E INPUT@1 INPUT@afterAnchor", "---- BODY BODY@offsetInAnchor[2]",
          "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
          "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
          "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
          "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
          "---E BODY BODY@afterChildren", "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromInputElementAfterNode) {
  // FlatTree is "ABC" <input><div>123</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree::AfterNode(input_element)),
      ElementsAre(
          "---E INPUT@1 INPUT@afterAnchor", "---- BODY BODY@offsetInAnchor[2]",
          "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
          "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
          "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
          "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
          "---E BODY BODY@afterChildren", "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromInputElementBeforeNode) {
  // FlatTree is "ABC" <input><div>123</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree::BeforeNode(input_element)),
      ElementsAre("-S-- INPUT@0 INPUT@offsetInAnchor[0] INPUT@beforeAnchor",
                  "-S-- DIV (editable) DIV (editable)@offsetInAnchor[0]",
                  "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
                  "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
                  "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
                  "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
                  "---E DIV (editable) DIV (editable)@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromInputElementOffset0) {
  // FlatTree is "ABC" <input><div>123</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree(input_element, 0)),
      ElementsAre("-S-- INPUT@0 INPUT@offsetInAnchor[0] INPUT@beforeAnchor",
                  "-S-- DIV (editable) DIV (editable)@offsetInAnchor[0]",
                  "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
                  "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
                  "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
                  "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
                  "---E DIV (editable) DIV (editable)@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromInputElementOffset1) {
  // FlatTree is "ABC" <input><div>123</div></input> "XYZ".
  SetBodyContent("ABC<input value=123>XYZ");
  const auto& input_element =
      *To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree(input_element, 1)),
      ElementsAre(
          "---E INPUT@1 INPUT@afterAnchor", "---- BODY BODY@offsetInAnchor[2]",
          "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
          "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
          "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
          "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
          "---E BODY BODY@afterChildren", "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromObjectElementAfterChildren) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree::LastPositionInNode(object_element)),
      ElementsAre("---E OBJECT@1 OBJECT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[2]",
                  "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
                  "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
                  "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
                  "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
                  "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromObjectElementAfterNode) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree::AfterNode(object_element)),
      ElementsAre("---E OBJECT@1 OBJECT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[2]",
                  "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
                  "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
                  "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
                  "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
                  "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromObjectElementBeforeNode) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree::BeforeNode(object_element)),
      ElementsAre("-S-- OBJECT@0 OBJECT@offsetInAnchor[0] OBJECT@beforeAnchor",
                  "-S-E SLOT SLOT@beforeAnchor SLOT@offsetInAnchor[0]",
                  "---E OBJECT@1 OBJECT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[2]",
                  "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
                  "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
                  "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
                  "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
                  "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromObjectElementOffset0) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree(object_element, 0)),
      ElementsAre("-S-- OBJECT@0 OBJECT@offsetInAnchor[0] OBJECT@beforeAnchor",
                  "-S-E SLOT SLOT@beforeAnchor SLOT@offsetInAnchor[0]",
                  "---E OBJECT@1 OBJECT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[2]",
                  "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
                  "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
                  "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
                  "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
                  "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromObjectElementOffset1) {
  // FlatTree is "ABC" <object><slot></slot></object> "XYZ".
  SetBodyContent("ABC<object></object>XYZ");
  const auto& object_element = *To<HTMLObjectElement>(
      GetDocument().QuerySelector(AtomicString("object")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree(object_element, 1)),
      ElementsAre("---E OBJECT@1 OBJECT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[2]",
                  "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
                  "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
                  "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
                  "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
                  "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromSelectElementAfterChildren) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree::LastPositionInNode(select_element)),
      ElementsAre("---E SELECT@1 SELECT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[2]",
                  "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
                  "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
                  "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
                  "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
                  "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromSelectElementAfterNode) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree::AfterNode(select_element)),
      ElementsAre("---E SELECT@1 SELECT@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[2]",
                  "-S-- #text \"XYZ\"@0 #text \"XYZ\"@offsetInAnchor[0]",
                  "---- #text \"XYZ\"@1 #text \"XYZ\"@offsetInAnchor[1]",
                  "---- #text \"XYZ\"@2 #text \"XYZ\"@offsetInAnchor[2]",
                  "---E #text \"XYZ\"@3 #text \"XYZ\"@offsetInAnchor[3]",
                  "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromSelectElementBeforeNode) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree::BeforeNode(select_element)),
      ElementsAre("-S-- SELECT@0 SELECT@offsetInAnchor[0] SELECT@beforeAnchor",
                  "-S-- DIV DIV@offsetInAnchor[0]",
                  "-S-E #text \"\"@0 #text \"\"@offsetInAnchor[0]",
                  "---E DIV DIV@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromSelectElementOffset0) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree(select_element, 0)),
      ElementsAre("-S-- SELECT@0 SELECT@offsetInAnchor[0] SELECT@beforeAnchor",
                  "-S-- DIV DIV@offsetInAnchor[0]",
                  "-S-E #text \"\"@0 #text \"\"@offsetInAnchor[0]",
                  "---E DIV DIV@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementFromSelectElementOffset1) {
  // FlatTree is "ABC"
  // <select><div>""</div><slot><option></option></slot></select> "XYZ".
  SetBodyContent("ABC<select><option></option></select>XYZ");
  const auto& select_element = *To<HTMLSelectElement>(
      GetDocument().QuerySelector(AtomicString("select")));
  EXPECT_THAT(
      ScanForward(PositionInFlatTree(select_element, 1)),
      ElementsAre("---- SELECT@1 SELECT@offsetInAnchor[1] SELECT@beforeAnchor",
                  "-S-- SLOT id=\"select-options\" SLOT "
                  "id=\"select-options\"@offsetInAnchor[0]",
                  "-S-- OPTION OPTION@offsetInAnchor[0]",
                  "-S-E SLOT SLOT@beforeAnchor SLOT@offsetInAnchor[0]",
                  "---E OPTION OPTION@afterChildren",
                  "---E SLOT id=\"select-options\" SLOT "
                  "id=\"select-options\"@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementWithCollapsedSpace) {
  const char* selection_text = "|<p> abc </p>";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre("-S-- BODY BODY@offsetInAnchor[0]",
                  "-S-- P P@offsetInAnchor[0]",
                  "-S-- #text \" abc \"@0 #text \" abc \"@offsetInAnchor[0]",
                  "---- #text \" abc \"@1 #text \" abc \"@offsetInAnchor[1]",
                  "---- #text \" abc \"@2 #text \" abc \"@offsetInAnchor[2]",
                  "---- #text \" abc \"@3 #text \" abc \"@offsetInAnchor[3]",
                  "---- #text \" abc \"@4 #text \" abc \"@offsetInAnchor[4]",
                  "---E #text \" abc \"@5 #text \" abc \"@offsetInAnchor[5]",
                  "---E P P@afterChildren", "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementWithCommentEmpty) {
  const char* selection_text = "|<p>a<br>b<!---->c</p>";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre(
          "-S-- BODY BODY@offsetInAnchor[0]", "-S-- P P@offsetInAnchor[0]",
          "-S-- #text \"a\"@0 #text \"a\"@offsetInAnchor[0]",
          "---E #text \"a\"@1 #text \"a\"@offsetInAnchor[1]",
          "---- P P@offsetInAnchor[1]", "-S-- BR@0 BR@beforeAnchor",
          "---E BR@1 BR@afterAnchor", "---- P P@offsetInAnchor[2]",
          "-S-- #text \"b\"@0 #text \"b\"@offsetInAnchor[0]",
          "---E #text \"b\"@1 #text \"b\"@offsetInAnchor[1]",
          "---- P P@offsetInAnchor[3]",
          // `At{Start,End}OfNode()` return false for empty comment.
          "-S-E #comment@0 #comment@beforeAnchor", "---- P P@offsetInAnchor[4]",
          "-S-- #text \"c\"@0 #text \"c\"@offsetInAnchor[0]",
          "---E #text \"c\"@1 #text \"c\"@offsetInAnchor[1]",
          "---E P P@afterChildren", "---E BODY BODY@afterChildren",
          "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementWithCommentNonEmpty) {
  const char* selection_text = "|<p>a<br>b<!--XYZ-->c</p>";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre(
          "-S-- BODY BODY@offsetInAnchor[0]", "-S-- P P@offsetInAnchor[0]",
          "-S-- #text \"a\"@0 #text \"a\"@offsetInAnchor[0]",
          "---E #text \"a\"@1 #text \"a\"@offsetInAnchor[1]",
          "---- P P@offsetInAnchor[1]", "-S-- BR@0 BR@beforeAnchor",
          "---E BR@1 BR@afterAnchor", "---- P P@offsetInAnchor[2]",
          "-S-- #text \"b\"@0 #text \"b\"@offsetInAnchor[0]",
          "---E #text \"b\"@1 #text \"b\"@offsetInAnchor[1]",
          "---- P P@offsetInAnchor[3]",
          // `AtEndOfNode()` returns false for not-empty comment.
          "-S-- #comment@0 #comment@beforeAnchor", "---- P P@offsetInAnchor[4]",
          "-S-- #text \"c\"@0 #text \"c\"@offsetInAnchor[0]",
          "---E #text \"c\"@1 #text \"c\"@offsetInAnchor[1]",
          "---E P P@afterChildren", "---E BODY BODY@afterChildren",
          "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementWithInlineElemnt) {
  const char* selection_text = "|<p><a><b>ABC</b></a>DEF<i><s>GHI</s></i></p>";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre("-S-- BODY BODY@offsetInAnchor[0]",
                  "-S-- P P@offsetInAnchor[0]", "-S-- A A@offsetInAnchor[0]",
                  "-S-- B B@offsetInAnchor[0]",
                  "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
                  "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
                  "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
                  "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
                  "---E B B@afterChildren", "---E A A@afterChildren",
                  "---- P P@offsetInAnchor[1]",
                  "-S-- #text \"DEF\"@0 #text \"DEF\"@offsetInAnchor[0]",
                  "---- #text \"DEF\"@1 #text \"DEF\"@offsetInAnchor[1]",
                  "---- #text \"DEF\"@2 #text \"DEF\"@offsetInAnchor[2]",
                  "---E #text \"DEF\"@3 #text \"DEF\"@offsetInAnchor[3]",
                  "---- P P@offsetInAnchor[2]", "-S-- I I@offsetInAnchor[0]",
                  "-S-- S S@offsetInAnchor[0]",
                  "-S-- #text \"GHI\"@0 #text \"GHI\"@offsetInAnchor[0]",
                  "---- #text \"GHI\"@1 #text \"GHI\"@offsetInAnchor[1]",
                  "---- #text \"GHI\"@2 #text \"GHI\"@offsetInAnchor[2]",
                  "---E #text \"GHI\"@3 #text \"GHI\"@offsetInAnchor[3]",
                  "---E S S@afterChildren", "---E I I@afterChildren",
                  "---E P P@afterChildren", "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, incrementWithInputElement) {
  const char* selection_text = "|<input id=target value='abc'>123";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre("-S-- BODY BODY@offsetInAnchor[0]",
                  "-S-- INPUT id=\"target\"@0 INPUT id=\"target\"@beforeAnchor",
                  "---E INPUT id=\"target\"@1 INPUT id=\"target\"@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
                  "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
                  "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
                  "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
                  "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));

  EXPECT_THAT(
      ScanForwardInFlatTree(selection_text),
      ElementsAre("-S-- BODY BODY@offsetInAnchor[0]",
                  // Note: `DeprecatedComputePosition()` should return
                  // `INPUT@beforeAnchor`.
                  "-S-E INPUT id=\"target\"@0 INPUT id=\"target\"@beforeAnchor "
                  "INPUT id=\"target\"@afterAnchor",
                  "---E INPUT id=\"target\"@1 INPUT id=\"target\"@afterAnchor",
                  "---- BODY BODY@offsetInAnchor[1]",
                  "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
                  "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
                  "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
                  "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
                  "---E BODY BODY@afterChildren",
                  "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, IncrementWithNoChildren) {
  const char* const selection_text = "|abc<br>def<img><br>";
  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre(
          "-S-- #text \"abc\"@0 #text \"abc\"@offsetInAnchor[0]",
          "---- #text \"abc\"@1 #text \"abc\"@offsetInAnchor[1]",
          "---- #text \"abc\"@2 #text \"abc\"@offsetInAnchor[2]",
          "---E #text \"abc\"@3 #text \"abc\"@offsetInAnchor[3]",
          "---- BODY BODY@offsetInAnchor[1]", "-S-- BR@0 BR@beforeAnchor",
          "---E BR@1 BR@afterAnchor", "---- BODY BODY@offsetInAnchor[2]",
          "-S-- #text \"def\"@0 #text \"def\"@offsetInAnchor[0]",
          "---- #text \"def\"@1 #text \"def\"@offsetInAnchor[1]",
          "---- #text \"def\"@2 #text \"def\"@offsetInAnchor[2]",
          "---E #text \"def\"@3 #text \"def\"@offsetInAnchor[3]",
          "---- BODY BODY@offsetInAnchor[3]", "-S-- IMG@0 IMG@beforeAnchor",
          "---E IMG@1 IMG@afterAnchor", "---- BODY BODY@offsetInAnchor[4]",
          "-S-- BR@0 BR@beforeAnchor", "---E BR@1 BR@afterAnchor",
          "---E BODY BODY@afterChildren", "---E HTML HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, incrementWithSelectElement) {
  const char* selection_text =
      "|<select id=target><option>1</option><option>2</option></select>123";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre(
          "-S-- BODY BODY@offsetInAnchor[0]",
          // Note: `DeprecatedComputePosition()` should return
          // `SELECT@beforeAnchor`.
          "-S-E SELECT id=\"target\"@0 SELECT id=\"target\"@beforeAnchor "
          "SELECT id=\"target\"@afterAnchor",
          "---E SELECT id=\"target\"@1 SELECT id=\"target\"@afterAnchor",
          "---- BODY BODY@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---E BODY BODY@afterChildren", "---E HTML HTML@afterChildren"));

  EXPECT_THAT(
      ScanForwardInFlatTree(selection_text),
      ElementsAre(
          "-S-- BODY BODY@offsetInAnchor[0]",
          // Note: `DeprecatedComputePosition()` should return
          // `SELECT@beforeAnchor`.
          "-S-E SELECT id=\"target\"@0 SELECT id=\"target\"@beforeAnchor "
          "SELECT id=\"target\"@afterAnchor",
          "---E SELECT id=\"target\"@1 SELECT id=\"target\"@afterAnchor",
          "---- BODY BODY@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---E BODY BODY@afterChildren", "---E HTML HTML@afterChildren"));
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, incrementWithTextAreaElement) {
  const char* selection_text = "|<textarea id=target>123</textarea>456";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre(
          "-S-- BODY BODY@offsetInAnchor[0]",
          // Note: `DeprecatedComputePosition()` should return
          // `TEXTAREA@beforeAnchor`.
          "-S-E TEXTAREA id=\"target\"@0 TEXTAREA id=\"target\"@beforeAnchor "
          "TEXTAREA id=\"target\"@afterAnchor",
          "---E TEXTAREA id=\"target\"@1 TEXTAREA id=\"target\"@afterAnchor",
          "---- BODY BODY@offsetInAnchor[1]",
          "-S-- #text \"456\"@0 #text \"456\"@offsetInAnchor[0]",
          "---- #text \"456\"@1 #text \"456\"@offsetInAnchor[1]",
          "---- #text \"456\"@2 #text \"456\"@offsetInAnchor[2]",
          "---E #text \"456\"@3 #text \"456\"@offsetInAnchor[3]",
          "---E BODY BODY@afterChildren", "---E HTML HTML@afterChildren"));

  EXPECT_THAT(
      ScanForwardInFlatTree(selection_text),
      ElementsAre(
          "-S-- BODY BODY@offsetInAnchor[0]",
          // Note: `DeprecatedComputePosition()` should return
          // `TEXTAREA@beforeAnchor`.
          "-S-E TEXTAREA id=\"target\"@0 TEXTAREA id=\"target\"@beforeAnchor "
          "TEXTAREA id=\"target\"@afterAnchor",
          "---E TEXTAREA id=\"target\"@1 TEXTAREA id=\"target\"@afterAnchor",
          "---- BODY BODY@offsetInAnchor[1]",
          "-S-- #text \"456\"@0 #text \"456\"@offsetInAnchor[0]",
          "---- #text \"456\"@1 #text \"456\"@offsetInAnchor[1]",
          "---- #text \"456\"@2 #text \"456\"@offsetInAnchor[2]",
          "---E #text \"456\"@3 #text \"456\"@offsetInAnchor[3]",
          "---E BODY BODY@afterChildren", "---E HTML HTML@afterChildren"));
}

// For http://crbug.com/1248744
TEST_F(PositionIteratorTest, nullPosition) {
  PositionIterator dom_iterator((Position()));
  PositionIteratorInFlatTree flat_iterator((PositionInFlatTree()));

  EXPECT_EQ(Position(), dom_iterator.ComputePosition());
  EXPECT_EQ(PositionInFlatTree(), flat_iterator.ComputePosition());

  EXPECT_EQ(Position(), dom_iterator.DeprecatedComputePosition());
  EXPECT_EQ(PositionInFlatTree(), flat_iterator.DeprecatedComputePosition());

  dom_iterator.Increment();
  flat_iterator.Increment();

  EXPECT_EQ(Position(), dom_iterator.ComputePosition());
  EXPECT_EQ(PositionInFlatTree(), flat_iterator.ComputePosition());

  dom_iterator.Decrement();
  flat_iterator.Decrement();

  EXPECT_EQ(Position(), dom_iterator.ComputePosition());
  EXPECT_EQ(PositionInFlatTree(), flat_iterator.ComputePosition());
}

}  // namespace blink
