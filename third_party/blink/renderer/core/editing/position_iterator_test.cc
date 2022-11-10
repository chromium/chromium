// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/position_iterator.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

using ::testing::ElementsAre;

class PositionIteratorTest : public EditingTestBase {
 protected:
  std::vector<std::string> ScanBackward(const char* selection_text) {
    return ScanBackwardTemplate<EditingStrategy>(selection_text);
  }

  std::vector<std::string> ScanBackwardInFlatTree(const char* selection_text) {
    return ScanBackwardTemplate<EditingInFlatTreeStrategy>(selection_text);
  }

  std::vector<std::string> ScanForward(const char* selection_text) {
    return ScanForwardTemplate<EditingStrategy>(selection_text);
  }

  std::vector<std::string> ScanForwardInFlatTree(const char* selection_text) {
    return ScanForwardTemplate<EditingInFlatTreeStrategy>(selection_text);
  }

 private:
  template <typename Strategy>
  std::vector<std::string> ScanBackwardTemplate(const char* selection_text) {
    const Position start = SetCaretTextToBody(selection_text);
    std::vector<std::string> positions;
    for (PositionIteratorAlgorithm<Strategy> it(
             FromPositionInDOMTree<Strategy>(start));
         !it.AtStart(); it.Decrement()) {
      positions.push_back(ToString(it));
    }
    return positions;
  }

  template <typename Strategy>
  std::vector<std::string> ScanForwardTemplate(const char* selection_text) {
    const Position start = SetCaretTextToBody(selection_text);
    std::vector<std::string> positions;
    for (PositionIteratorAlgorithm<Strategy> it(
             FromPositionInDOMTree<Strategy>(start));
         !it.AtEnd(); it.Increment()) {
      positions.push_back(ToString(it));
    }
    return positions;
  }

  template <typename Strategy>
  static std::string ToString(const PositionIteratorAlgorithm<Strategy>& it) {
    const PositionTemplate<Strategy> position1 = it.ComputePosition();
    const PositionTemplate<Strategy> position2 = it.DeprecatedComputePosition();
    std::ostringstream os;
    os << (it.AtStart() ? "S" : "-") << (it.AtStartOfNode() ? "S" : "-")
       << (it.AtEnd() ? "E" : "-") << (it.AtEndOfNode() ? "E" : "-") << " "
       << it.GetNode() << "@" << it.OffsetInLeafNode() << " " << position1;
    if (position1 != position2)
      os << " " << position2;
    return os.str();
  }
};

TEST_F(PositionIteratorTest, DecrementWithInlineElemnt) {
  const char* selection_text = "<p><a><b>ABC</b></a><i><s>DEF</s></i></p>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre(
          "---E BODY@1 BODY@afterChildren", "---E P@0 P@afterChildren",
          "---E I@0 I@afterChildren", "---E S@0 S@afterChildren",
          "---E #text \"DEF\"@3 #text \"DEF\"@offsetInAnchor[3]",
          "---- #text \"DEF\"@2 #text \"DEF\"@offsetInAnchor[2]",
          "---- #text \"DEF\"@1 #text \"DEF\"@offsetInAnchor[1]",
          "-S-- #text \"DEF\"@0 #text \"DEF\"@offsetInAnchor[0]",
          "-S-- S@0 S@offsetInAnchor[0]", "-S-- I@0 I@offsetInAnchor[0]",
          "---- P@0 P@offsetInAnchor[1]", "---E A@0 A@afterChildren",
          "---E B@0 B@afterChildren",
          "---E #text \"ABC\"@3 #text \"ABC\"@offsetInAnchor[3]",
          "---- #text \"ABC\"@2 #text \"ABC\"@offsetInAnchor[2]",
          "---- #text \"ABC\"@1 #text \"ABC\"@offsetInAnchor[1]",
          "-S-- #text \"ABC\"@0 #text \"ABC\"@offsetInAnchor[0]",
          "-S-- B@0 B@offsetInAnchor[0]", "-S-- A@0 A@offsetInAnchor[0]",
          "-S-- P@0 P@offsetInAnchor[0]", "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---- HTML@0 HTML@offsetInAnchor[1]",
          "-S-E HEAD@0 HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML@0 HTML@offsetInAnchor[0]"));
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, decrementWithInputElement) {
  const char* const selection_text = "123<input id=target value='abc'>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre("---E BODY@2 BODY@afterChildren",
                  "---E INPUT id=\"target\"@1 INPUT id=\"target\"@afterAnchor",
                  "-S-- INPUT id=\"target\"@0 INPUT id=\"target\"@beforeAnchor",
                  "---- BODY@1 BODY@offsetInAnchor[1]",
                  "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
                  "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
                  "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
                  "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
                  "-S-- BODY@0 BODY@offsetInAnchor[0]",
                  "---- HTML@0 HTML@offsetInAnchor[1]",
                  "-S-E HEAD@0 HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
                  "-S-- HTML@0 HTML@offsetInAnchor[0]"));

  EXPECT_THAT(
      ScanBackwardInFlatTree(selection_text),
      ElementsAre(
          "---E BODY@2 BODY@afterChildren",
          "---E INPUT id=\"target\"@1 INPUT id=\"target\"@afterAnchor",
          "---E INPUT id=\"target\"@0 INPUT id=\"target\"@beforeAnchor INPUT "
          "id=\"target\"@afterAnchor",
          "---- BODY@1 BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---- HTML@0 HTML@offsetInAnchor[1]",
          "-S-E HEAD@0 HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML@0 HTML@offsetInAnchor[0]"));
}

TEST_F(PositionIteratorTest, decrementWithSelectElement) {
  const char* const selection_text =
      "123<select id=target><option>1</option><option>2</option></select>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre(
          "---E BODY@2 BODY@afterChildren",
          "---E SELECT id=\"target\"@1 SELECT id=\"target\"@afterAnchor",
          "---E SELECT id=\"target\"@0 SELECT id=\"target\"@beforeAnchor "
          "SELECT id=\"target\"@afterAnchor",
          "---- BODY@1 BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---- HTML@0 HTML@offsetInAnchor[1]",
          "-S-E HEAD@0 HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML@0 HTML@offsetInAnchor[0]"));

  EXPECT_THAT(
      ScanBackwardInFlatTree(selection_text),
      ElementsAre(
          "---E BODY@2 BODY@afterChildren",
          "---E SELECT id=\"target\"@1 SELECT id=\"target\"@afterAnchor",
          "---E SELECT id=\"target\"@0 SELECT id=\"target\"@beforeAnchor "
          "SELECT id=\"target\"@afterAnchor",
          "---- BODY@1 BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---- HTML@0 HTML@offsetInAnchor[1]",
          "-S-E HEAD@0 HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML@0 HTML@offsetInAnchor[0]"));
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, decrementWithTextAreaElement) {
  const char* const selection_text = "123<textarea id=target>456</textarea>|";

  EXPECT_THAT(
      ScanBackward(selection_text),
      ElementsAre(
          "---E BODY@2 BODY@afterChildren",
          "---E TEXTAREA id=\"target\"@1 TEXTAREA id=\"target\"@afterAnchor",
          "---E TEXTAREA id=\"target\"@0 TEXTAREA id=\"target\"@beforeAnchor "
          "TEXTAREA id=\"target\"@afterAnchor",
          "---- BODY@1 BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---- HTML@0 HTML@offsetInAnchor[1]",
          "-S-E HEAD@0 HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML@0 HTML@offsetInAnchor[0]"));

  EXPECT_THAT(
      ScanBackwardInFlatTree(selection_text),
      ElementsAre(
          "---E BODY@2 BODY@afterChildren",
          "---E TEXTAREA id=\"target\"@1 TEXTAREA id=\"target\"@afterAnchor",
          "---E TEXTAREA id=\"target\"@0 TEXTAREA id=\"target\"@beforeAnchor "
          "TEXTAREA id=\"target\"@afterAnchor",
          "---- BODY@1 BODY@offsetInAnchor[1]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---- HTML@0 HTML@offsetInAnchor[1]",
          "-S-E HEAD@0 HEAD@beforeAnchor HEAD@offsetInAnchor[0]",
          "-S-- HTML@0 HTML@offsetInAnchor[0]"));
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, incrementWithInputElement) {
  const char* selection_text = "|<input id=target value='abc'>123";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre("-S-- BODY@0 BODY@offsetInAnchor[0]",
                  "-S-- INPUT id=\"target\"@0 INPUT id=\"target\"@beforeAnchor",
                  "---E INPUT id=\"target\"@1 INPUT id=\"target\"@afterAnchor",
                  "---- BODY@1 BODY@offsetInAnchor[1]",
                  "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
                  "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
                  "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
                  "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
                  "---E BODY@2 BODY@afterChildren",
                  "---E HTML@2 HTML@afterChildren"));

  EXPECT_THAT(
      ScanForwardInFlatTree(selection_text),
      ElementsAre("-S-- BODY@0 BODY@offsetInAnchor[0]",
                  "---E INPUT id=\"target\"@0 INPUT id=\"target\"@beforeAnchor "
                  "INPUT id=\"target\"@afterAnchor",
                  "---E INPUT id=\"target\"@1 INPUT id=\"target\"@afterAnchor",
                  "---- BODY@1 BODY@offsetInAnchor[1]",
                  "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
                  "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
                  "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
                  "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
                  "---E BODY@2 BODY@afterChildren",
                  "---E HTML@2 HTML@afterChildren"));
}

TEST_F(PositionIteratorTest, incrementWithSelectElement) {
  const char* selection_text =
      "|<select id=target><option>1</option><option>2</option></select>123";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre(
          "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---E SELECT id=\"target\"@0 SELECT id=\"target\"@beforeAnchor "
          "SELECT id=\"target\"@afterAnchor",
          "---E SELECT id=\"target\"@1 SELECT id=\"target\"@afterAnchor",
          "---- BODY@1 BODY@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---E BODY@2 BODY@afterChildren", "---E HTML@2 HTML@afterChildren"));

  EXPECT_THAT(
      ScanForwardInFlatTree(selection_text),
      ElementsAre(
          "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---E SELECT id=\"target\"@0 SELECT id=\"target\"@beforeAnchor "
          "SELECT id=\"target\"@afterAnchor",
          "---E SELECT id=\"target\"@1 SELECT id=\"target\"@afterAnchor",
          "---- BODY@1 BODY@offsetInAnchor[1]",
          "-S-- #text \"123\"@0 #text \"123\"@offsetInAnchor[0]",
          "---- #text \"123\"@1 #text \"123\"@offsetInAnchor[1]",
          "---- #text \"123\"@2 #text \"123\"@offsetInAnchor[2]",
          "---E #text \"123\"@3 #text \"123\"@offsetInAnchor[3]",
          "---E BODY@2 BODY@afterChildren", "---E HTML@2 HTML@afterChildren"));
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, incrementWithTextAreaElement) {
  const char* selection_text = "|<textarea id=target>123</textarea>456";

  EXPECT_THAT(
      ScanForward(selection_text),
      ElementsAre(
          "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---E TEXTAREA id=\"target\"@0 TEXTAREA id=\"target\"@beforeAnchor "
          "TEXTAREA id=\"target\"@afterAnchor",
          "---E TEXTAREA id=\"target\"@1 TEXTAREA id=\"target\"@afterAnchor",
          "---- BODY@1 BODY@offsetInAnchor[1]",
          "-S-- #text \"456\"@0 #text \"456\"@offsetInAnchor[0]",
          "---- #text \"456\"@1 #text \"456\"@offsetInAnchor[1]",
          "---- #text \"456\"@2 #text \"456\"@offsetInAnchor[2]",
          "---E #text \"456\"@3 #text \"456\"@offsetInAnchor[3]",
          "---E BODY@2 BODY@afterChildren", "---E HTML@2 HTML@afterChildren"));

  EXPECT_THAT(
      ScanForwardInFlatTree(selection_text),
      ElementsAre(
          "-S-- BODY@0 BODY@offsetInAnchor[0]",
          "---E TEXTAREA id=\"target\"@0 TEXTAREA id=\"target\"@beforeAnchor "
          "TEXTAREA id=\"target\"@afterAnchor",
          "---E TEXTAREA id=\"target\"@1 TEXTAREA id=\"target\"@afterAnchor",
          "---- BODY@1 BODY@offsetInAnchor[1]",
          "-S-- #text \"456\"@0 #text \"456\"@offsetInAnchor[0]",
          "---- #text \"456\"@1 #text \"456\"@offsetInAnchor[1]",
          "---- #text \"456\"@2 #text \"456\"@offsetInAnchor[2]",
          "---E #text \"456\"@3 #text \"456\"@offsetInAnchor[3]",
          "---E BODY@2 BODY@afterChildren", "---E HTML@2 HTML@afterChildren"));
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
