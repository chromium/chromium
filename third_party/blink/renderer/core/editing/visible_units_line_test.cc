// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"

namespace blink {

static VisiblePosition EndOfLine(const VisiblePosition& position) {
  return CreateVisiblePosition(EndOfLine(position.ToPositionWithAffinity()));
}

static VisiblePositionInFlatTree EndOfLine(
    const VisiblePositionInFlatTree& position) {
  return CreateVisiblePosition(EndOfLine(position.ToPositionWithAffinity()));
}

class VisibleUnitsLineTest : public EditingTestBase {
 protected:
  static PositionWithAffinity PositionWithAffinityInDOMTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return PositionWithAffinity(CanonicalPositionOf(Position(&anchor, offset)),
                                affinity);
  }

  static VisiblePosition CreateVisiblePositionInDOMTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return CreateVisiblePosition(Position(&anchor, offset), affinity);
  }

  static PositionInFlatTreeWithAffinity PositionWithAffinityInFlatTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return PositionInFlatTreeWithAffinity(
        CanonicalPositionOf(PositionInFlatTree(&anchor, offset)), affinity);
  }

  static VisiblePositionInFlatTree CreateVisiblePositionInFlatTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return CreateVisiblePosition(PositionInFlatTree(&anchor, offset), affinity);
  }

  std::string TestEndOfLine(const std::string& input) {
    const Position& caret = SetCaretTextToBody(input);
    const Position& result =
        EndOfLine(CreateVisiblePosition(caret)).DeepEquivalent();
    return GetCaretTextFromBody(result);
  }

  std::string TestLogicalEndOfLine(const std::string& input) {
    const Position& caret = SetCaretTextToBody(input);
    const Position& result =
        LogicalEndOfLine(CreateVisiblePosition(caret)).DeepEquivalent();
    return GetCaretTextFromBody(result);
  }

  std::string TestStartOfLine(const std::string& input) {
    const Position& caret = SetCaretTextToBody(input);
    const Position& result =
        StartOfLine(CreateVisiblePosition(caret)).DeepEquivalent();
    return GetCaretTextFromBody(result);
  }
};

TEST_F(VisibleUnitsLineTest, endOfLine) {
  // Test case:
  // 5555522
  // 666666
  // 117777777
  // 3334444
  const char* body_content =
      "<span id=host><b slot='#one' id=one>11</b><b slot='#two' "
      "id=two>22</b></span><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><slot name='#two'></slot><br><u "
      "id=six>666666</u><br><slot name='#one'></slot><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();
  Node* four = GetDocument().getElementById(AtomicString("four"))->firstChild();
  Node* five = shadow_root->getElementById(AtomicString("five"))->firstChild();
  Node* six = shadow_root->getElementById(AtomicString("six"))->firstChild();
  Node* seven =
      shadow_root->getElementById(AtomicString("seven"))->firstChild();

  EXPECT_EQ(
      Position(seven, 7),
      EndOfLine(CreateVisiblePositionInDOMTree(*one, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(seven, 7),
      EndOfLine(CreateVisiblePositionInFlatTree(*one, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(seven, 7),
      EndOfLine(CreateVisiblePositionInDOMTree(*one, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(seven, 7),
      EndOfLine(CreateVisiblePositionInFlatTree(*one, 1)).DeepEquivalent());

  EXPECT_EQ(Position(two, 2), EndOfLine(CreateVisiblePositionInDOMTree(
                                            *two, 0, TextAffinity::kUpstream))
                                  .DeepEquivalent());
  EXPECT_EQ(
      Position(two, 2),
      EndOfLine(CreateVisiblePositionInDOMTree(*two, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      EndOfLine(CreateVisiblePositionInFlatTree(*two, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(two, 2),
      EndOfLine(CreateVisiblePositionInDOMTree(*two, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      EndOfLine(CreateVisiblePositionInFlatTree(*two, 1)).DeepEquivalent());

  EXPECT_EQ(Position(four, 4),
            EndOfLine(CreateVisiblePositionInDOMTree(*three, 0,
                                                     TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(
      Position(four, 4),
      EndOfLine(CreateVisiblePositionInDOMTree(*three, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 4),
      EndOfLine(CreateVisiblePositionInFlatTree(*three, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(four, 4),
      EndOfLine(CreateVisiblePositionInDOMTree(*four, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 4),
      EndOfLine(CreateVisiblePositionInFlatTree(*four, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(two, 2),
      EndOfLine(CreateVisiblePositionInDOMTree(*five, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      EndOfLine(CreateVisiblePositionInFlatTree(*five, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(six, 6),
      EndOfLine(CreateVisiblePositionInDOMTree(*six, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(six, 6),
      EndOfLine(CreateVisiblePositionInFlatTree(*six, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(seven, 7),
      EndOfLine(CreateVisiblePositionInDOMTree(*seven, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(seven, 7),
      EndOfLine(CreateVisiblePositionInFlatTree(*seven, 1)).DeepEquivalent());
}

TEST_F(VisibleUnitsLineTest, isEndOfLine) {
  // Test case:
  // 5555522
  // 666666
  // 117777777
  // 3334444
  const char* body_content =
      "<span id=host><b slot='#one' id=one>11</b><b slot='#two' "
      "id=two>22</b></span><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><slot name='#two'></slot><br><u "
      "id=six>666666</u><br><slot name='#one'></slot><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();
  Node* four = GetDocument().getElementById(AtomicString("four"))->firstChild();
  Node* five = shadow_root->getElementById(AtomicString("five"))->firstChild();
  Node* six = shadow_root->getElementById(AtomicString("six"))->firstChild();
  Node* seven =
      shadow_root->getElementById(AtomicString("seven"))->firstChild();

  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_TRUE(IsEndOfLine(
      CreateVisiblePositionInFlatTree(*two, 2, TextAffinity::kUpstream)));
  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInFlatTree(*two, 2)));
  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInFlatTree(*two, 2)));

  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInDOMTree(*three, 3)));
  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInFlatTree(*three, 3)));

  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInDOMTree(*four, 4)));
  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInFlatTree(*four, 4)));

  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInFlatTree(*five, 5)));
  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInFlatTree(*five, 5)));

  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInDOMTree(*six, 6)));
  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInFlatTree(*six, 6)));

  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInDOMTree(*seven, 7)));
  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInFlatTree(*seven, 7)));
}

TEST_F(VisibleUnitsLineTest, isLogicalEndOfLine) {
  // Test case:
  // 5555522
  // 666666
  // 117777777
  // 3334444
  const char* body_content =
      "<span id=host><b slot='#one' id=one>11</b><b slot='#two' "
      "id=two>22</b></span><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><slot name='#two'></slot><br><u "
      "id=six>666666</u><br><slot name='#one'></slot><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();
  Node* four = GetDocument().getElementById(AtomicString("four"))->firstChild();
  Node* five = shadow_root->getElementById(AtomicString("five"))->firstChild();
  Node* six = shadow_root->getElementById(AtomicString("six"))->firstChild();
  Node* seven =
      shadow_root->getElementById(AtomicString("seven"))->firstChild();

  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_TRUE(IsLogicalEndOfLine(
      CreateVisiblePositionInDOMTree(*two, 2, TextAffinity::kUpstream)));
  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*two, 2)));
  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*two, 2)));

  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*three, 3)));
  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*three, 3)));

  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*four, 4)));
  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*four, 4)));

  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*five, 5)));
  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*five, 5)));

  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*six, 6)));
  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*six, 6)));

  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*seven, 7)));
  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*seven, 7)));
}

TEST_F(VisibleUnitsLineTest, inSameLine) {
  const char* body_content =
      "<p id='host'>00<b slot='#one' id='one'>11</b><b slot='#two' "
      "id='two'>22</b>33</p>";
  const char* shadow_content =
      "<div><span id='s4'>44</span><slot name='#two'></slot><br><span "
      "id='s5'>55</span><br><slot name='#one'></slot><span "
      "id='s6'>66</span></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* one = body->QuerySelector(AtomicString("#one"));
  Element* two = body->QuerySelector(AtomicString("#two"));
  Element* four = shadow_root->QuerySelector(AtomicString("#s4"));
  Element* five = shadow_root->QuerySelector(AtomicString("#s5"));

  EXPECT_FALSE(InSameLine(PositionWithAffinityInDOMTree(*one, 0),
                          PositionWithAffinityInDOMTree(*two, 0)));
  EXPECT_FALSE(
      InSameLine(PositionWithAffinityInDOMTree(*one->firstChild(), 0),
                 PositionWithAffinityInDOMTree(*two->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(PositionWithAffinityInDOMTree(*one->firstChild(), 0),
                 PositionWithAffinityInDOMTree(*five->firstChild(), 0)));
  EXPECT_TRUE(
      InSameLine(PositionWithAffinityInDOMTree(*two->firstChild(), 0),
                 PositionWithAffinityInDOMTree(*four->firstChild(), 0)));

  EXPECT_FALSE(InSameLine(
      CreateVisiblePositionInDOMTree(*one, 0),
      CreateVisiblePositionInDOMTree(*two, 0, TextAffinity::kUpstream)));
  EXPECT_FALSE(InSameLine(CreateVisiblePositionInDOMTree(*one, 0),
                          CreateVisiblePositionInDOMTree(*two, 0)));
  EXPECT_FALSE(InSameLine(CreateVisiblePositionInDOMTree(*one->firstChild(), 0),
                          CreateVisiblePositionInDOMTree(
                              *two->firstChild(), 0, TextAffinity::kUpstream)));
  EXPECT_FALSE(
      InSameLine(CreateVisiblePositionInDOMTree(*one->firstChild(), 0),
                 CreateVisiblePositionInDOMTree(*two->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(CreateVisiblePositionInDOMTree(*one->firstChild(), 0),
                 CreateVisiblePositionInDOMTree(*five->firstChild(), 0)));
  EXPECT_TRUE(
      InSameLine(CreateVisiblePositionInDOMTree(*two->firstChild(), 0,
                                                TextAffinity::kUpstream),
                 CreateVisiblePositionInDOMTree(*four->firstChild(), 0)));
  EXPECT_TRUE(
      InSameLine(CreateVisiblePositionInDOMTree(*two->firstChild(), 0),
                 CreateVisiblePositionInDOMTree(*four->firstChild(), 0)));

  EXPECT_FALSE(InSameLine(PositionWithAffinityInFlatTree(*one, 0),
                          PositionWithAffinityInFlatTree(*two, 0)));
  EXPECT_FALSE(
      InSameLine(PositionWithAffinityInFlatTree(*one->firstChild(), 0),
                 PositionWithAffinityInFlatTree(*two->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(PositionWithAffinityInFlatTree(*one->firstChild(), 0),
                 PositionWithAffinityInFlatTree(*five->firstChild(), 0)));
  EXPECT_TRUE(
      InSameLine(PositionWithAffinityInFlatTree(*two->firstChild(), 0),
                 PositionWithAffinityInFlatTree(*four->firstChild(), 0)));

  EXPECT_FALSE(InSameLine(CreateVisiblePositionInFlatTree(*one, 0),
                          CreateVisiblePositionInFlatTree(*two, 0)));
  EXPECT_FALSE(
      InSameLine(CreateVisiblePositionInFlatTree(*one->firstChild(), 0),
                 CreateVisiblePositionInFlatTree(*two->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(CreateVisiblePositionInFlatTree(*one->firstChild(), 0),
                 CreateVisiblePositionInFlatTree(*five->firstChild(), 0)));
  EXPECT_TRUE(
      InSameLine(CreateVisiblePositionInFlatTree(*two->firstChild(), 0),
                 CreateVisiblePositionInFlatTree(*four->firstChild(), 0)));
}

TEST_F(VisibleUnitsLineTest, isStartOfLine) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>11</b><b slot='#two' "
      "id=two>22</b></span><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><slot name='#two'></slot><br><u "
      "id=six>666666</u><br><slot name='#one'></slot><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();
  Node* four = GetDocument().getElementById(AtomicString("four"))->firstChild();
  Node* five = shadow_root->getElementById(AtomicString("five"))->firstChild();
  Node* six = shadow_root->getElementById(AtomicString("six"))->firstChild();
  Node* seven =
      shadow_root->getElementById(AtomicString("seven"))->firstChild();

  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInDOMTree(*two, 0)));
  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInFlatTree(*two, 0)));

  EXPECT_TRUE(IsStartOfLine(
      CreateVisiblePositionInDOMTree(*three, 0, TextAffinity::kUpstream)));
  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInDOMTree(*three, 0)));
  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInFlatTree(*three, 0)));

  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInDOMTree(*four, 0)));
  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInFlatTree(*four, 0)));

  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInDOMTree(*five, 0)));
  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInFlatTree(*five, 0)));

  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInDOMTree(*six, 0)));
  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInFlatTree(*six, 0)));

  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInDOMTree(*seven, 0)));
  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInFlatTree(*seven, 0)));
}

TEST_F(VisibleUnitsLineTest, logicalEndOfLine) {
  // Test case:
  // 5555522
  // 666666
  // 117777777
  // 3334444
  const char* body_content =
      "<span id=host><b slot='#one' id=one>11</b><b slot='#two' "
      "id=two>22</b></span><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><slot name='#two'></slot><br><u "
      "id=six>666666</u><br><slot name='#one'></slot><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();
  Node* four = GetDocument().getElementById(AtomicString("four"))->firstChild();
  Node* five = shadow_root->getElementById(AtomicString("five"))->firstChild();
  Node* six = shadow_root->getElementById(AtomicString("six"))->firstChild();
  Node* seven =
      shadow_root->getElementById(AtomicString("seven"))->firstChild();

  EXPECT_EQ(Position(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*one, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*one, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*one, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*one, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(
                                 *two, 0, TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(Position(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*two, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*two, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*two, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*two, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(four, 4),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(
                                 *three, 0, TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(Position(four, 4),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*three, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(four, 4),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*three, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(four, 4),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*four, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(four, 4),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*four, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*five, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*five, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(six, 6),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*six, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(six, 6),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*six, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*seven, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*seven, 1))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsLineTest, logicalStartOfLine) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>11</b><b slot='#two' "
      "id=two>22</b></span><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><slot name='#two'></slot><br><u "
      "id=six>666666</u><br><slot name='#one'></slot><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();
  Node* four = GetDocument().getElementById(AtomicString("four"))->firstChild();
  Node* five = shadow_root->getElementById(AtomicString("five"))->firstChild();
  Node* six = shadow_root->getElementById(AtomicString("six"))->firstChild();
  Node* seven =
      shadow_root->getElementById(AtomicString("seven"))->firstChild();

  EXPECT_EQ(Position(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*one, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*one, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*one, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*one, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(
                                   *two, 0, TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(Position(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*two, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*two, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*two, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*two, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(three, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(
                                   *three, 0, TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(Position(three, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*three, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*three, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(three, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(
                                   *four, 1, TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(Position(three, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*four, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*four, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*five, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*five, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(six, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*six, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(six, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*six, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*seven, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*seven, 1))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsLineTest, startOfLine) {
  // Test case:
  // 5555522
  // 666666
  // 117777777
  // 3334444
  const char* body_content =
      "<span id=host><b slot='#one' id=one>11</b><b slot='#two' "
      "id=two>22</b></span><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><slot name='#two'></slot><br><u "
      "id=six>666666</u><br><slot name='#one'></slot><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();
  Node* four = GetDocument().getElementById(AtomicString("four"))->firstChild();
  Node* five = shadow_root->getElementById(AtomicString("five"))->firstChild();
  Node* six = shadow_root->getElementById(AtomicString("six"))->firstChild();
  Node* seven =
      shadow_root->getElementById(AtomicString("seven"))->firstChild();

  EXPECT_EQ(
      Position(one, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*one, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*one, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(one, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*one, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*one, 1)).DeepEquivalent());

  EXPECT_EQ(Position(five, 0),
            StartOfLine(CreateVisiblePositionInDOMTree(*two, 0,
                                                       TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(
      Position(five, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*two, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*two, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(five, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*two, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*two, 1)).DeepEquivalent());

  EXPECT_EQ(Position(three, 0),
            StartOfLine(CreateVisiblePositionInDOMTree(*three, 0,
                                                       TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(
      Position(three, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*three, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(three, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*three, 1)).DeepEquivalent());

  EXPECT_EQ(Position(three, 0),
            StartOfLine(CreateVisiblePositionInDOMTree(*four, 1,
                                                       TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(
      Position(three, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*four, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(three, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*four, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(five, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*five, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*five, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(six, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*six, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(six, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*six, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(one, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*seven, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*seven, 1)).DeepEquivalent());
}

TEST_F(VisibleUnitsLineTest, EndOfLineWithBidi) {
  LoadAhem();
  InsertStyleElement("p { font: 30px/3 Ahem; }");

  EXPECT_EQ(
      "<p dir=\"ltr\"><bdo dir=\"ltr\">ab cd ef|</bdo></p>",
      TestEndOfLine("<p dir=\"ltr\"><bdo dir=\"ltr\">a|b cd ef</bdo></p>"))
      << "LTR LTR";
  EXPECT_EQ(
      "<p dir=\"ltr\"><bdo dir=\"rtl\">ab cd ef|</bdo></p>",
      TestEndOfLine("<p dir=\"ltr\"><bdo dir=\"rtl\">a|b cd ef</bdo></p>"))
      << "LTR RTL";
  EXPECT_EQ(
      "<p dir=\"rtl\"><bdo dir=\"ltr\">ab cd ef|</bdo></p>",
      TestEndOfLine("<p dir=\"rtl\"><bdo dir=\"ltr\">a|b cd ef</bdo></p>"))
      << "RTL LTR";
  EXPECT_EQ(
      "<p dir=\"rtl\"><bdo dir=\"rtl\">ab cd ef|</bdo></p>",
      TestEndOfLine("<p dir=\"rtl\"><bdo dir=\"rtl\">a|b cd ef</bdo></p>"))
      << "RTL RTL";
}

// http://crbug.com/1136740
TEST_F(VisibleUnitsLineTest, EndOfLineWithHangingSpace) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "font: 30px/3 Ahem;"
      "overflow-wrap: break-word;"
      "white-space: pre-wrap;"
      "width: 4ch;"
      "}");

  // _____ _=Space
  // abcd
  // efgh
  EXPECT_EQ("<p>     |abcdefgh</p>", TestEndOfLine("<p>|     abcdefgh</p>"));
  EXPECT_EQ("<p>     |abcdefgh</p>", TestEndOfLine("<p> |    abcdefgh</p>"));
  EXPECT_EQ("<p>     |abcdefgh</p>", TestEndOfLine("<p>  |   abcdefgh</p>"));
  EXPECT_EQ("<p>     |abcdefgh</p>", TestEndOfLine("<p>   |  abcdefgh</p>"));
  EXPECT_EQ("<p>     |abcdefgh</p>", TestEndOfLine("<p>    | abcdefgh</p>"));
  EXPECT_EQ("<p>     abcd|efgh</p>", TestEndOfLine("<p>     |abcdefgh</p>"));
  EXPECT_EQ("<p>     abcd|efgh</p>", TestEndOfLine("<p>     a|bcdefgh</p>"));

  // __x__ _=Space
  // abcd
  // efgh
  EXPECT_EQ("<p>  x |abcdefgh</p>", TestEndOfLine("<p>|  x abcdefgh</p>"));
  EXPECT_EQ("<p>  x |abcdefgh</p>", TestEndOfLine("<p> | x abcdefgh</p>"));
  EXPECT_EQ("<p>  x |abcdefgh</p>", TestEndOfLine("<p>  x| abcdefgh</p>"));
  EXPECT_EQ("<p>  x |abcdefgh</p>", TestEndOfLine("<p>  x| abcdefgh</p>"));
  EXPECT_EQ("<p>  x abcd|efgh</p>", TestEndOfLine("<p>  x |abcdefgh</p>"));
  EXPECT_EQ("<p>  x abcd|efgh</p>", TestEndOfLine("<p>  x a|bcdefgh</p>"));
}

TEST_F(VisibleUnitsLineTest, EndOfLineWithPositionRelative) {
  LoadAhem();
  InsertStyleElement(
      "b { position:relative; left: 30px; }"
      "p { font: 30px/3 Ahem; }");

  EXPECT_EQ("<p>ab <b>cd</b> <b>ef|</b></p>",
            TestEndOfLine("<p>a|b <b>cd</b> <b>ef</b></p>"));
  EXPECT_EQ(
      "<p><bdo dir=\"rtl\">ab <b>cd</b> <b>ef|</b></bdo></p>",
      TestEndOfLine("<p><bdo dir=\"rtl\">a|b <b>cd</b> <b>ef</b></bdo></p>"));
  EXPECT_EQ("<p dir=\"rtl\">ab <b>cd</b> <b>ef|</b></p>",
            TestEndOfLine("<p dir=\"rtl\">a|b <b>cd</b> <b>ef</b></p>"));
  EXPECT_EQ(
      "<p dir=\"rtl\"><bdo dir=\"rtl\">ab <b>cd</b> <b>ef|</b></bdo></p>",
      TestEndOfLine(
          "<p dir=\"rtl\"><bdo dir=\"rtl\">a|b <b>cd</b> <b>ef</b></bdo></p>"));
}

TEST_F(VisibleUnitsLineTest, EndOfLineWithSoftLineWrap3) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "font: 10px/1 Ahem; width: 3ch; word-break: break-all; }");

  EXPECT_EQ("<div>abc|def</div>", TestEndOfLine("<div>|abcdef</div>"));
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">abc|def</bdo></div>",
      TestEndOfLine("<div dir=\"rtl\"><bdo dir=\"rtl\">|abcdef</bdo></div>"));

  // Note: Both legacy and NG layout don't have text boxes for spaces cause
  // soft line wrap.
  EXPECT_EQ("<div>abc| def ghi</div>",
            TestEndOfLine("<div>|abc def ghi</div>"));
  EXPECT_EQ("<div>abc| def ghi</div>",
            TestEndOfLine("<div>ab|c def ghi</div>"));
  EXPECT_EQ("<div>abc| def ghi</div>",
            TestEndOfLine("<div>abc| def ghi</div>"));
  EXPECT_EQ("<div>abc def| ghi</div>",
            TestEndOfLine("<div>abc |def ghi</div>"));

  EXPECT_EQ("<div dir=\"rtl\"><bdo dir=\"rtl\">abc| def ghi</bdo></div>",
            TestEndOfLine(
                "<div dir=\"rtl\"><bdo dir=\"rtl\">|abc def ghi</bdo></div>"));
  EXPECT_EQ("<div dir=\"rtl\"><bdo dir=\"rtl\">abc| def ghi</bdo></div>",
            TestEndOfLine(
                "<div dir=\"rtl\"><bdo dir=\"rtl\">ab|c def ghi</bdo></div>"));
  EXPECT_EQ("<div dir=\"rtl\"><bdo dir=\"rtl\">abc| def ghi</bdo></div>",
            TestEndOfLine(
                "<div dir=\"rtl\"><bdo dir=\"rtl\">abc| def ghi</bdo></div>"));
  EXPECT_EQ("<div dir=\"rtl\"><bdo dir=\"rtl\">abc def| ghi</bdo></div>",
            TestEndOfLine(
                "<div dir=\"rtl\"><bdo dir=\"rtl\">abc |def ghi</bdo></div>"));

  // On content editable, caret is after a space.
  // Note: Legacy layout has text boxes at end of line for space cause soft line
  // wrap for editable text, e.g.
  //   LayoutText {#text} at (10,9) size 18x32
  //     text run at (10,9) width 18: "abc"
  //     text run at (28,9) width 0: " "
  //     text run at (10,19) width 18: "def"
  //     text run at (28,19) width 0: " "
  //     text run at (10,29) width 18: "ghi"
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestEndOfLine("<div contenteditable>|abc def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestEndOfLine("<div contenteditable>ab|c def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestEndOfLine("<div contenteditable>abc| def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc def |ghi</div>",
            TestEndOfLine("<div contenteditable>abc |def ghi</div>"));
}

TEST_F(VisibleUnitsLineTest, EndOfLineWithSoftLineWrap4) {
  LoadAhem();
  InsertStyleElement("div { font: 10px/1 Ahem; width: 4ch; }");

  EXPECT_EQ("<div>abc| def ghi</div>",
            TestEndOfLine("<div>|abc def ghi</div>"));
  EXPECT_EQ("<div>abc| def ghi</div>",
            TestEndOfLine("<div>ab|c def ghi</div>"));
  EXPECT_EQ("<div>abc| def ghi</div>",
            TestEndOfLine("<div>abc| def ghi</div>"));
  EXPECT_EQ("<div>abc def| ghi</div>",
            TestEndOfLine("<div>abc |def ghi</div>"));

  // On content editable, caret is after a space.
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestEndOfLine("<div contenteditable>|abc def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestEndOfLine("<div contenteditable>ab|c def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestEndOfLine("<div contenteditable>abc| def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc def |ghi</div>",
            TestEndOfLine("<div contenteditable>abc |def ghi</div>"));
}

// http://crbug.com/1169583
TEST_F(VisibleUnitsLineTest, EndOfLineWithWhiteSpacePre) {
  LoadAhem();
  InsertStyleElement("p { font: 10px/1 Ahem; white-space: pre; }");

  EXPECT_EQ("<p dir=\"ltr\"><bdo dir=\"ltr\">ABC DEF|\nGHI JKL</bdo></p>",
            TestEndOfLine(
                "<p dir=\"ltr\"><bdo dir=\"ltr\">ABC| DEF\nGHI JKL</bdo></p>"))
      << "LTR LTR";
  EXPECT_EQ("<p dir=\"ltr\"><bdo dir=\"rtl\">ABC DEF|\nGHI JKL</bdo></p>",
            TestEndOfLine(
                "<p dir=\"ltr\"><bdo dir=\"rtl\">ABC| DEF\nGHI JKL</bdo></p>"))
      << "LTR RTL";
  EXPECT_EQ("<p dir=\"rtl\"><bdo dir=\"ltr\">ABC DEF|\nGHI JKL</bdo></p>",
            TestEndOfLine(
                "<p dir=\"rtl\"><bdo dir=\"ltr\">ABC| DEF\nGHI JKL</bdo></p>"))
      << "RTL LTR";
  EXPECT_EQ("<p dir=\"rtl\"><bdo dir=\"rtl\">ABC DEF|\nGHI JKL</bdo></p>",
            TestEndOfLine(
                "<p dir=\"rtl\"><bdo dir=\"rtl\">ABC| DEF\nGHI JKL</bdo></p>"))
      << "RTL RTL";
}

TEST_F(VisibleUnitsLineTest, LogicalEndOfLineWithSoftLineWrap3) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "font: 10px/1 Ahem; width: 3ch; word-break: break-all; }");

  EXPECT_EQ("<div>abc|def</div>", TestLogicalEndOfLine("<div>|abcdef</div>"));
  EXPECT_EQ("<div dir=\"rtl\"><bdo dir=\"rtl\">abc|def</bdo></div>",
            TestLogicalEndOfLine(
                "<div dir=\"rtl\"><bdo dir=\"rtl\">|abcdef</bdo></div>"));

  EXPECT_EQ("<div>abc| def ghi</div>",
            TestLogicalEndOfLine("<div>|abc def ghi</div>"));
  EXPECT_EQ("<div>abc| def ghi</div>",
            TestLogicalEndOfLine("<div>ab|c def ghi</div>"));
  EXPECT_EQ("<div>abc| def ghi</div>",
            TestLogicalEndOfLine("<div>abc| def ghi</div>"));
  EXPECT_EQ("<div>abc def| ghi</div>",
            TestLogicalEndOfLine("<div>abc |def ghi</div>"));

  // On content editable, caret is after a space.
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestLogicalEndOfLine("<div contenteditable>|abc def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestLogicalEndOfLine("<div contenteditable>ab|c def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestLogicalEndOfLine("<div contenteditable>abc| def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc def |ghi</div>",
            TestLogicalEndOfLine("<div contenteditable>abc |def ghi</div>"));
}

TEST_F(VisibleUnitsLineTest, LogicalEndOfLineWithSoftLineWrap4) {
  LoadAhem();
  InsertStyleElement("div { font: 10px/1 Ahem; width: 4ch; }");

  EXPECT_EQ("<div>abc| def ghi</div>",
            TestLogicalEndOfLine("<div>|abc def ghi</div>"));
  EXPECT_EQ("<div>abc| def ghi</div>",
            TestLogicalEndOfLine("<div>ab|c def ghi</div>"));
  EXPECT_EQ("<div>abc| def ghi</div>",
            TestLogicalEndOfLine("<div>abc| def ghi</div>"));
  EXPECT_EQ("<div>abc def| ghi</div>",
            TestLogicalEndOfLine("<div>abc |def ghi</div>"));

  // On content editable, caret is after a space.
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestLogicalEndOfLine("<div contenteditable>|abc def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestLogicalEndOfLine("<div contenteditable>ab|c def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc |def ghi</div>",
            TestLogicalEndOfLine("<div contenteditable>abc| def ghi</div>"));
  EXPECT_EQ("<div contenteditable>abc def |ghi</div>",
            TestLogicalEndOfLine("<div contenteditable>abc |def ghi</div>"));
}

TEST_F(VisibleUnitsLineTest, InSameLineSkippingEmptyEditableDiv) {
  // This test records the InSameLine() results in
  // editing/selection/skip-over-contenteditable.html
  SetBodyContent(
      "<p id=foo>foo</p>"
      "<div contenteditable></div>"
      "<p id=bar>bar</p>");
  const Node* const foo = GetElementById("foo")->firstChild();
  const Node* const bar = GetElementById("bar")->firstChild();

  EXPECT_TRUE(InSameLine(
      PositionWithAffinity(Position(foo, 3), TextAffinity::kDownstream),
      PositionWithAffinity(Position(foo, 3), TextAffinity::kUpstream)));
  EXPECT_FALSE(InSameLine(
      PositionWithAffinity(Position(bar, 0), TextAffinity::kDownstream),
      PositionWithAffinity(Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_TRUE(InSameLine(
      PositionWithAffinity(Position(bar, 3), TextAffinity::kDownstream),
      PositionWithAffinity(Position(bar, 3), TextAffinity::kUpstream)));
  EXPECT_FALSE(InSameLine(
      PositionWithAffinity(Position(foo, 0), TextAffinity::kDownstream),
      PositionWithAffinity(Position(bar, 0), TextAffinity::kDownstream)));
}

TEST_F(VisibleUnitsLineTest, InSameLineWithMixedEditability) {
  SelectionInDOMTree selection =
      SetSelectionTextToBody("<span contenteditable>f^oo</span>b|ar");

  PositionWithAffinity position1(selection.Anchor());
  PositionWithAffinity position2(selection.Focus());
  // "Same line" is restricted by editability boundaries.
  EXPECT_FALSE(InSameLine(position1, position2));
}

TEST_F(VisibleUnitsLineTest, InSameLineWithGeneratedZeroWidthSpace) {
  LoadAhem();
  InsertStyleElement(
      "p { font: 10px/1 Ahem; }"
      "p { width: 4ch; white-space: pre-wrap;");
  // We have ZWS before "abc" due by "pre-wrap".
  const Position& after_zws = SetCaretTextToBody("<p id=t>    |abcd</p>");
  const PositionWithAffinity after_zws_down =
      PositionWithAffinity(after_zws, TextAffinity::kDownstream);
  const PositionWithAffinity after_zws_up =
      PositionWithAffinity(after_zws, TextAffinity::kUpstream);

  EXPECT_EQ(
      PositionWithAffinity(Position(*GetElementById("t")->firstChild(), 8),
                           TextAffinity::kUpstream),
      EndOfLine(after_zws_down));
  EXPECT_EQ(after_zws_up, EndOfLine(after_zws_up));
  EXPECT_FALSE(InSameLine(after_zws_up, after_zws_down));
}

// http://crbug.com/1183269
TEST_F(VisibleUnitsLineTest, InSameLineWithSoftLineWrap) {
  LoadAhem();
  InsertStyleElement(
      "p { font: 10px/1 Ahem; }"
      "p { width: 3ch; }");
  // Note: "contenteditable" adds
  //    line-break: after-white-space;
  //    overflow-wrap: break-word;
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("<p contenteditable id=t>abc |xyz</p>");
  EXPECT_FALSE(InSameLine(
      PositionWithAffinity(selection.Anchor(), TextAffinity::kUpstream),
      PositionWithAffinity(selection.Anchor(), TextAffinity::kDownstream)));
}

TEST_F(VisibleUnitsLineTest, InSameLineWithZeroWidthSpace) {
  LoadAhem();
  InsertStyleElement(
      "p { font: 10px/1 Ahem; }"
      "p { width: 4ch; }");
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("<p id=t>abcd^\u200B|wxyz</p>");

  const Position& after_zws = selection.Focus();
  const PositionWithAffinity after_zws_down =
      PositionWithAffinity(after_zws, TextAffinity::kDownstream);
  const PositionWithAffinity after_zws_up =
      PositionWithAffinity(after_zws, TextAffinity::kUpstream);

  const Position& before_zws = selection.Anchor();
  const PositionWithAffinity before_zws_down =
      PositionWithAffinity(before_zws, TextAffinity::kDownstream);
  const PositionWithAffinity before_zws_up =
      PositionWithAffinity(before_zws, TextAffinity::kUpstream);

  EXPECT_EQ(
      PositionWithAffinity(Position(*GetElementById("t")->firstChild(), 9),
                           TextAffinity::kUpstream),
      EndOfLine(after_zws_down));
  EXPECT_EQ(after_zws_up, EndOfLine(after_zws_up));
  EXPECT_FALSE(InSameLine(after_zws_up, after_zws_down));

  EXPECT_EQ(after_zws_up, EndOfLine(before_zws_down));
  EXPECT_EQ(after_zws_up, EndOfLine(before_zws_up));
  EXPECT_TRUE(InSameLine(before_zws_up, before_zws_down));
}

// https://issues.chromium.org/issues/41497469
TEST_F(VisibleUnitsLineTest, InSameLineWithInlineBlock) {
  SetBodyContent(
      "<span id=one>start</span>"
      "<span id=two style='display: inline-block;'>test</span>"
      "<span id=three>end</span>");

  const PositionWithAffinity position =
      PositionWithAffinity(Position(*GetElementById("two")->firstChild(), 0),
                           TextAffinity::kUpstream);
  EXPECT_TRUE(InSameLine(
      position,
      PositionWithAffinity(Position(*GetElementById("one")->firstChild(), 0),
                           TextAffinity::kUpstream)));
  EXPECT_TRUE(InSameLine(
      position,
      PositionWithAffinity(Position(*GetElementById("three")->firstChild(), 0),
                           TextAffinity::kUpstream)));
}

// http://crbug.com/1358235
TEST_F(VisibleUnitsLineTest, StartOfLineBeforeEmptyLine) {
  LoadAhem();
  InsertStyleElement("p { font: 30px/3 Ahem; }");

  EXPECT_EQ("<p dir=\"ltr\">abc<br>|<br>xyz<br></p>",
            TestStartOfLine("<p dir=\"ltr\">abc<br>|<br>xyz<br></p>"));
  EXPECT_EQ("<p dir=\"ltr\">abc<br><br>|<br>xyz<br></p>",
            TestStartOfLine("<p dir=\"ltr\">abc<br><br>|<br>xyz<br></p>"));
  EXPECT_EQ("<p dir=\"ltr\">abc<br>|<br><br>xyz<br></p>",
            TestStartOfLine("<p dir=\"ltr\">abc<br>|<br><br>xyz<br></p>"));

  EXPECT_EQ("<p dir=\"rtl\">abc<br>|<br>xyz<br></p>",
            TestStartOfLine("<p dir=\"rtl\">abc<br>|<br>xyz<br></p>"));
  EXPECT_EQ("<p dir=\"rtl\">abc<br>|<br><br>xyz<br></p>",
            TestStartOfLine("<p dir=\"rtl\">abc<br>|<br><br>xyz<br></p>"));
  EXPECT_EQ("<p dir=\"rtl\">abc<br><br>|<br>xyz<br></p>",
            TestStartOfLine("<p dir=\"rtl\">abc<br><br>|<br>xyz<br></p>"));
}

TEST_F(VisibleUnitsLineTest, StartOfLineWithBidi) {
  LoadAhem();
  InsertStyleElement("p { font: 30px/3 Ahem; }");

  EXPECT_EQ(
      "<p dir=\"ltr\"><bdo dir=\"ltr\">|abc xyz</bdo></p>",
      TestStartOfLine("<p dir=\"ltr\"><bdo dir=\"ltr\">abc |xyz</bdo></p>"))
      << "LTR LTR";
  EXPECT_EQ(
      "<p dir=\"ltr\"><bdo dir=\"rtl\">|abc xyz</bdo></p>",
      TestStartOfLine("<p dir=\"ltr\"><bdo dir=\"rtl\">abc |xyz</bdo></p>"))
      << "LTR RTL";
  EXPECT_EQ(
      "<p dir=\"rtl\"><bdo dir=\"ltr\">|abc xyz</bdo></p>",
      TestStartOfLine("<p dir=\"rtl\"><bdo dir=\"ltr\">abc |xyz</bdo></p>"))
      << "RTL LTR";
  EXPECT_EQ(
      "<p dir=\"rtl\"><bdo dir=\"rtl\">|abc xyz</bdo></p>",
      TestStartOfLine("<p dir=\"rtl\"><bdo dir=\"rtl\">abc |xyz</bdo></p>"))
      << "RTL RTL";
}

TEST_F(VisibleUnitsLineTest, StartOfLineWithPositionRelative) {
  LoadAhem();
  InsertStyleElement(
      "b { position:relative; left: -100px; }"
      "p { font: 30px/3 Ahem; }");

  EXPECT_EQ("<p><b>|abc</b> xyz</p>", TestStartOfLine("<p><b>abc</b> |xyz</p>"))
      << "LTR-LTR";
  EXPECT_EQ("<p dir=\"rtl\"><b>|abc</b> xyz</p>",
            TestStartOfLine("<p dir=\"rtl\"><b>abc</b> |xyz</p>"))
      << "RTL-LTR";
  EXPECT_EQ("<p><bdo dir=\"rtl\"><b>|abc</b> xyz</bdo></p>",
            TestStartOfLine("<p><bdo dir=\"rtl\"><b>abc</b> |xyz</bdo></p>"))
      << "LTR-RTL";
  EXPECT_EQ("<p dir=\"rtl\"><bdo dir=\"rtl\"><b>|abc</b> xyz</bdo></p>",
            TestStartOfLine(
                "<p dir=\"rtl\"><bdo  dir=\"rtl\"><b>abc</b> |xyz</bdo></p>"))
      << "RTL-RTL";
}

// https://crbug.com/947462
TEST_F(VisibleUnitsLineTest, TextOverflowEllipsis1) {
  LoadAhem();
  InsertStyleElement(R"HTML(
    div {
      width: 40px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      font: 10px/10px Ahem;
    })HTML");
  SetBodyContent("<div>foo foo</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();
  EXPECT_EQ(
      Position(text, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*text, 6)).DeepEquivalent());
  EXPECT_EQ(
      Position(text, 7),
      EndOfLine(CreateVisiblePositionInDOMTree(*text, 6)).DeepEquivalent());
}

// https://crbug.com/1177753
TEST_F(VisibleUnitsLineTest, TextOverflowEllipsis2) {
  InsertStyleElement(R"HTML(
    div {
      overflow: scroll;
      text-overflow: ellipsis;
      white-space: nowrap;
      width: 50px;
      direction: rtl;
    }
    span {
      display: inline-block;
      width: 75px; /* Something bigger than 50px */
    })HTML");
  SetBodyContent("<div><span>x</span>&#x20;</div>");
  Element* span = GetDocument().QuerySelector(AtomicString("span"));

  // Should not crash
  const PositionWithAffinity& start_of_line =
      StartOfLine(PositionWithAffinity(Position(span, 1)));

  EXPECT_EQ(PositionWithAffinity(Position::BeforeNode(*span)), start_of_line);
}

// https://crbug.com/1181451
TEST_F(VisibleUnitsLineTest, InSameLineWithBidiReordering) {
  InsertStyleElement("div { display: inline-block; width: 75% }");
  SetBodyContent(
      "<span dir='rtl'>"
      "<span dir='ltr'>a&#x20;</span>&#x20;"
      "<div></div><div></div>"
      "</span>");
  Element* span = GetDocument().QuerySelector(AtomicString("span > span"));
  PositionWithAffinity p1(Position(span->nextSibling(), 0));
  PositionWithAffinity p2(Position(span->firstChild(), 2));

  // Should not crash.
  EXPECT_EQ(true, InSameLine(p1, p2));
}

}  // namespace blink
