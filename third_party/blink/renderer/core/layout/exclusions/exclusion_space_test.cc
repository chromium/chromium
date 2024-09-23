// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/exclusions/exclusion_space.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

using ::testing::ElementsAre;

#define TEST_OPPORTUNITY(opp, expected_start_offset, expected_end_offset) \
  EXPECT_EQ(expected_start_offset, opp.rect.start_offset);                \
  EXPECT_EQ(expected_end_offset, opp.rect.end_offset)

struct ExclusionSpaceForTesting {
  explicit ExclusionSpaceForTesting(float available_inline_size)
      : available_inline_size(available_inline_size) {}

  ExclusionSpace exclusion_space;
  LayoutUnit available_inline_size;

  Vector<LayoutUnit> InitialLetterClearanceOffset() const {
    return {exclusion_space.InitialLetterClearanceOffset(EClear::kBoth),
            exclusion_space.InitialLetterClearanceOffset(EClear::kLeft),
            exclusion_space.InitialLetterClearanceOffset(EClear::kRight)};
  }

  void Add(const ExclusionArea* exclusion) { exclusion_space.Add(exclusion); }

  void AddForFloat(float inline_start,
                   float block_start,
                   float inline_end,
                   float block_end,
                   EFloat float_type = EFloat::kLeft) {
    exclusion_space.Add(ExclusionArea::Create(
        BfcRect(BfcOffset(LayoutUnit(inline_start), LayoutUnit(block_start)),
                BfcOffset(LayoutUnit(inline_end), LayoutUnit(block_end))),
        EFloat::kLeft, /*is_hidden_for_paint*/ false));
  }

  void AddForInitialLetterBox(float inline_start,
                              float block_start,
                              float inline_end,
                              float block_end,
                              EFloat float_type = EFloat::kLeft) {
    exclusion_space.Add(ExclusionArea::CreateForInitialLetterBox(
        BfcRect(BfcOffset(LayoutUnit(inline_start), LayoutUnit(block_start)),
                BfcOffset(LayoutUnit(inline_end), LayoutUnit(block_end))),
        EFloat::kLeft, /*is_hidden_for_paint*/ false));
  }

  LayoutOpportunityVector AllLayoutOpportunities(float inline_offset,
                                                 float block_offset) const {
    return exclusion_space.AllLayoutOpportunities(
        BfcOffset(LayoutUnit(inline_offset), LayoutUnit(block_offset)),
        available_inline_size);
  }

  LayoutOpportunity FindLayoutOpportunity(float inline_offset,
                                          float block_offset,
                                          float minimal_inline_size) {
    return exclusion_space.FindLayoutOpportunity(
        BfcOffset(LayoutUnit(inline_offset), LayoutUnit(block_offset)),
        available_inline_size, LayoutUnit(minimal_inline_size));
  }
};

LayoutOpportunity CreateLayoutOpportunity(float inline_start,
                                          float block_start,
                                          float inline_end,
                                          float block_end = LayoutUnit::Max()) {
  return LayoutOpportunity(
      BfcRect(BfcOffset(LayoutUnit(inline_start), LayoutUnit(block_start)),
              BfcOffset(LayoutUnit(inline_end), LayoutUnit(block_end))));
}

// Tests that an empty exclusion space returns exactly one layout opportunity
// each one, and sized appropriately given the area.
TEST(ExclusionSpaceTest, Empty) {
  test::TaskEnvironment task_environment;
  ExclusionSpace exclusion_space;

  LayoutOpportunityVector opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(), LayoutUnit()},
      /* available_size */ LayoutUnit(100));

  EXPECT_EQ(1u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(), LayoutUnit()),
                   BfcOffset(LayoutUnit(100), LayoutUnit::Max()));

  opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(-30), LayoutUnit(-100)},
      /* available_size */ LayoutUnit(50));

  EXPECT_EQ(1u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0],
                   BfcOffset(LayoutUnit(-30), LayoutUnit(-100)),
                   BfcOffset(LayoutUnit(20), LayoutUnit::Max()));

  opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(30), LayoutUnit(100)},
      /* available_size */ LayoutUnit(50));

  EXPECT_EQ(1u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(30), LayoutUnit(100)),
                   BfcOffset(LayoutUnit(80), LayoutUnit::Max()));
}

TEST(ExclusionSpaceTest, SingleExclusion) {
  test::TaskEnvironment task_environment;
  ExclusionSpace exclusion_space;

  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(10), LayoutUnit(20)),
                                    BfcOffset(LayoutUnit(60), LayoutUnit(90))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));

  LayoutOpportunityVector opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(), LayoutUnit()},
      /* available_size */ LayoutUnit(100));

  EXPECT_EQ(3u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(), LayoutUnit()),
                   BfcOffset(LayoutUnit(100), LayoutUnit(20)));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(60), LayoutUnit()),
                   BfcOffset(LayoutUnit(100), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[2], BfcOffset(LayoutUnit(), LayoutUnit(90)),
                   BfcOffset(LayoutUnit(100), LayoutUnit::Max()));

  opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(-10), LayoutUnit(-100)},
      /* available_size */ LayoutUnit(100));

  EXPECT_EQ(3u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0],
                   BfcOffset(LayoutUnit(-10), LayoutUnit(-100)),
                   BfcOffset(LayoutUnit(90), LayoutUnit(20)));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(60), LayoutUnit(-100)),
                   BfcOffset(LayoutUnit(90), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[2], BfcOffset(LayoutUnit(-10), LayoutUnit(90)),
                   BfcOffset(LayoutUnit(90), LayoutUnit::Max()));

  // This will still produce three opportunities, with a zero-width RHS
  // opportunity.
  opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(10), LayoutUnit(10)},
      /* available_size */ LayoutUnit(50));

  EXPECT_EQ(3u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(10), LayoutUnit(10)),
                   BfcOffset(LayoutUnit(60), LayoutUnit(20)));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(60), LayoutUnit(10)),
                   BfcOffset(LayoutUnit(60), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[2], BfcOffset(LayoutUnit(10), LayoutUnit(90)),
                   BfcOffset(LayoutUnit(60), LayoutUnit::Max()));

  // This will also produce three opportunities, as the RHS opportunity outside
  // the search area creates a zero-width opportunity.
  opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(10), LayoutUnit(10)},
      /* available_size */ LayoutUnit(49));

  EXPECT_EQ(3u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(10), LayoutUnit(10)),
                   BfcOffset(LayoutUnit(59), LayoutUnit(20)));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(60), LayoutUnit(10)),
                   BfcOffset(LayoutUnit(60), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[2], BfcOffset(LayoutUnit(10), LayoutUnit(90)),
                   BfcOffset(LayoutUnit(59), LayoutUnit::Max()));
}

TEST(ExclusionSpaceTest, TwoExclusions) {
  test::TaskEnvironment task_environment;
  ExclusionSpace exclusion_space;

  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(), LayoutUnit()),
                                    BfcOffset(LayoutUnit(150), LayoutUnit(75))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  exclusion_space.Add(ExclusionArea::Create(
      BfcRect(BfcOffset(LayoutUnit(100), LayoutUnit(75)),
              BfcOffset(LayoutUnit(400), LayoutUnit(150))),
      EFloat::kRight, /*is_hidden_for_paint*/ false));

  LayoutOpportunityVector opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(), LayoutUnit()},
      /* available_size */ LayoutUnit(400));

  EXPECT_EQ(3u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(150), LayoutUnit()),
                   BfcOffset(LayoutUnit(400), LayoutUnit(75)));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(), LayoutUnit(75)),
                   BfcOffset(LayoutUnit(100), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[2], BfcOffset(LayoutUnit(), LayoutUnit(150)),
                   BfcOffset(LayoutUnit(400), LayoutUnit::Max()));
}

// Tests the "solid edge" behaviour. When "NEW" is added a new layout
// opportunity shouldn't be created above it.
//
// NOTE: This is the same example given in the code.
//
//    0 1 2 3 4 5 6 7 8
// 0  +---+  X----X+---+
//    |xxx|  .     |xxx|
// 10 |xxx|  .     |xxx|
//    +---+  .     +---+
// 20        .     .
//      +---+. .+---+
// 30   |xxx|   |NEW|
//      |xxx|   +---+
// 40   +---+
TEST(ExclusionSpaceTest, SolidEdges) {
  test::TaskEnvironment task_environment;
  ExclusionSpace exclusion_space;

  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(), LayoutUnit()),
                                    BfcOffset(LayoutUnit(20), LayoutUnit(15))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(65), LayoutUnit()),
                                    BfcOffset(LayoutUnit(85), LayoutUnit(15))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));
  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(10), LayoutUnit(25)),
                                    BfcOffset(LayoutUnit(30), LayoutUnit(40))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(50), LayoutUnit(25)),
                                    BfcOffset(LayoutUnit(70), LayoutUnit(35))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));

  LayoutOpportunityVector opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(), LayoutUnit()},
      /* available_size */ LayoutUnit(80));

  EXPECT_EQ(5u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(20), LayoutUnit()),
                   BfcOffset(LayoutUnit(65), LayoutUnit(25)));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(30), LayoutUnit()),
                   BfcOffset(LayoutUnit(50), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[2], BfcOffset(LayoutUnit(), LayoutUnit(15)),
                   BfcOffset(LayoutUnit(80), LayoutUnit(25)));
  TEST_OPPORTUNITY(opportunites[3], BfcOffset(LayoutUnit(30), LayoutUnit(35)),
                   BfcOffset(LayoutUnit(80), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[4], BfcOffset(LayoutUnit(), LayoutUnit(40)),
                   BfcOffset(LayoutUnit(80), LayoutUnit::Max()));
}

// Tests that if a new exclusion doesn't overlap with a shelf, we don't add a
// new layout opportunity.
//
// NOTE: This is the same example given in the code.
//
//    0 1 2 3 4 5 6 7 8
// 0  +---+X------X+---+
//    |xxx|        |xxx|
// 10 |xxx|        |xxx|
//    +---+        +---+
// 20
//                  +---+
// 30               |NEW|
//                  +---+
TEST(ExclusionSpaceTest, OverlappingWithShelf) {
  test::TaskEnvironment task_environment;
  ExclusionSpace exclusion_space;

  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(), LayoutUnit()),
                                    BfcOffset(LayoutUnit(20), LayoutUnit(15))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(65), LayoutUnit()),
                                    BfcOffset(LayoutUnit(85), LayoutUnit(15))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));
  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(70), LayoutUnit(25)),
                                    BfcOffset(LayoutUnit(90), LayoutUnit(35))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));

  LayoutOpportunityVector opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(), LayoutUnit()},
      /* available_size */ LayoutUnit(80));

  EXPECT_EQ(4u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(20), LayoutUnit()),
                   BfcOffset(LayoutUnit(65), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(), LayoutUnit(15)),
                   BfcOffset(LayoutUnit(80), LayoutUnit(25)));
  TEST_OPPORTUNITY(opportunites[2], BfcOffset(LayoutUnit(), LayoutUnit(15)),
                   BfcOffset(LayoutUnit(70), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[3], BfcOffset(LayoutUnit(), LayoutUnit(35)),
                   BfcOffset(LayoutUnit(80), LayoutUnit::Max()));
}

// Tests that a shelf is properly inserted between two other shelves.
//
// Additionally tests that an inserted exclusion is correctly inserted in a
// shelve's line_left_edges/line_right_edges list.
//
// NOTE: This is the same example given in the code.
//
//    0 1 2 3 4 5 6 7 8
// 0  +-----+X----X+---+
//    |xxxxx|      |xxx|
// 10 +-----+      |xxx|
//      +---+      |xxx|
// 20   |NEW|      |xxx|
//    X-----------X|xxx|
// 30              |xxx|
//    X----------------X
TEST(ExclusionSpaceTest, InsertBetweenShelves) {
  test::TaskEnvironment task_environment;
  ExclusionSpace exclusion_space;

  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(), LayoutUnit()),
                                    BfcOffset(LayoutUnit(30), LayoutUnit(10))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(65), LayoutUnit()),
                                    BfcOffset(LayoutUnit(85), LayoutUnit(35))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));
  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(10), LayoutUnit(15)),
                                    BfcOffset(LayoutUnit(30), LayoutUnit(25))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));

  LayoutOpportunityVector opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(30), LayoutUnit(15)},
      /* available_size */ LayoutUnit(30));

  // NOTE: This demonstrates a quirk when querying the exclusion space for
  // opportunities. The exclusion space may return multiple exclusions of
  // exactly the same (or growing) size. This quirk still produces correct
  // results for code which uses it as the exclusions grow or keep the same
  // size.
  EXPECT_EQ(3u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(30), LayoutUnit(15)),
                   BfcOffset(LayoutUnit(60), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(30), LayoutUnit(25)),
                   BfcOffset(LayoutUnit(60), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[2], BfcOffset(LayoutUnit(30), LayoutUnit(35)),
                   BfcOffset(LayoutUnit(60), LayoutUnit::Max()));
}

TEST(ExclusionSpaceTest, InitialLetterBasic) {
  test::TaskEnvironment task_environment;
  constexpr LayoutUnit kAvailableInlineSize = LayoutUnit(300);
  ExclusionSpaceForTesting exclusion_space(kAvailableInlineSize);

  // <!doctype html>
  //  <style>
  //  body { font-size: 20px; }
  //  .drop::first-letter { initial-letter: 3; }
  //  .sample {
  //      border: solid green 1px;
  //      margin-bottom: 5px;
  //      width: 300px;
  //  }
  //
  //  *::first-letter {
  //      color: red;
  //      background: yellow;
  //  }
  //
  //  </style>
  //  <<div class="sample drop">
  //  Drop<br>line1<br>line2<br>line3<br>line4<br>line5<br>
  //  </div>
  exclusion_space.AddForInitialLetterBox(9, 9, 73, 73);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 32),
              ElementsAre(CreateLayoutOpportunity(73, 32, 309),
                          CreateLayoutOpportunity(9, 73, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 55),
              ElementsAre(CreateLayoutOpportunity(73, 55, 309),
                          CreateLayoutOpportunity(9, 73, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 78),
              ElementsAre(CreateLayoutOpportunity(9, 78, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 101),
              ElementsAre(CreateLayoutOpportunity(9, 101, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 124),
              ElementsAre(CreateLayoutOpportunity(9, 124, 309)));

  EXPECT_THAT(exclusion_space.InitialLetterClearanceOffset(),
              ElementsAre(LayoutUnit(73), LayoutUnit(73), LayoutUnit::Min()));
}

TEST(ExclusionSpaceTest, InitialLetterDirectionRight) {
  test::TaskEnvironment task_environment;
  constexpr LayoutUnit kAvailableInlineSize = LayoutUnit(300);
  ExclusionSpaceForTesting exclusion_space(kAvailableInlineSize);

  // <!doctype html>
  //  <style>
  //  body { font-size: 20px; }
  //  .drop::first-letter { initial-letter: 3; }
  //  .sample {
  //      border: solid green 1px;
  //      margin-bottom: 5px;
  //      width: 300px;
  //  }
  //
  //  *::first-letter {
  //      color: red;
  //      background: yellow;
  //  }
  //
  //  </style>
  //  <<div dir="rtl" class="sample drop">
  // <float id="float1"></float>
  //  <float id="float2" style="float:right; width:100px"></float>
  //  Drop<br>line1<br>line2<br>line3<br>line4<br>line5<br>
  //  </div>
  exclusion_space.AddForFloat(9, 9, 59, 59);

  EXPECT_THAT(exclusion_space.FindLayoutOpportunity(9, 9, 100),
              CreateLayoutOpportunity(59, 9, 309));
  exclusion_space.AddForFloat(209, 9, 309, 59, EFloat::kRight);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 9),
              ElementsAre(CreateLayoutOpportunity(309, 9, 309),
                          CreateLayoutOpportunity(9, 59, 309)));
  exclusion_space.AddForInitialLetterBox(117, 9, 182, 73, EFloat::kRight);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 32),
              ElementsAre(CreateLayoutOpportunity(309, 32, 309),
                          CreateLayoutOpportunity(182, 59, 309),
                          CreateLayoutOpportunity(9, 73, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 55),
              ElementsAre(CreateLayoutOpportunity(309, 55, 309),
                          CreateLayoutOpportunity(182, 59, 309),
                          CreateLayoutOpportunity(9, 73, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 78),
              ElementsAre(CreateLayoutOpportunity(9, 78, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 101),
              ElementsAre(CreateLayoutOpportunity(9, 101, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 124),
              ElementsAre(CreateLayoutOpportunity(9, 124, 309)));

  EXPECT_THAT(exclusion_space.InitialLetterClearanceOffset(),
              ElementsAre(LayoutUnit(73), LayoutUnit(73), LayoutUnit::Min()));
}

TEST(ExclusionSpaceTest, InitialLetterFloatLeft1) {
  test::TaskEnvironment task_environment;
  constexpr LayoutUnit kAvailableInlineSize = LayoutUnit(300);
  ExclusionSpaceForTesting exclusion_space(kAvailableInlineSize);

  // <!doctype html>
  //  <style>
  //  body { font-size: 20px; }
  //  .drop::first-letter { initial-letter: 3; }
  //  .sample {
  //      border: solid green 1px;
  //      margin-bottom: 5px;
  //      width: 300px;
  //  }
  //
  //  *::first-letter {
  //      color: red;
  //      background: yellow;
  //  }
  //
  //  float {
  //    float: left;
  //    width: 50px;
  //    height: 50px;
  //  }
  //
  //  </style>
  //  <<div class="sample drop">
  //  <float id="float1"></float>
  //  Drop<br>line1<br>line2<br>line3<br>line4<br>line5<br>
  //  </div>
  exclusion_space.AddForFloat(9, 59, 59, 59);
  exclusion_space.AddForInitialLetterBox(59, 9, 73, 73);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 32),
              ElementsAre(CreateLayoutOpportunity(73, 32, 309),
                          CreateLayoutOpportunity(73, 59, 309),
                          CreateLayoutOpportunity(9, 73, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 55),
              ElementsAre(CreateLayoutOpportunity(73, 55, 309),
                          CreateLayoutOpportunity(73, 59, 309),
                          CreateLayoutOpportunity(9, 73, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 78),
              ElementsAre(CreateLayoutOpportunity(9, 78, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 101),
              ElementsAre(CreateLayoutOpportunity(9, 101, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 124),
              ElementsAre(CreateLayoutOpportunity(9, 124, 309)));

  EXPECT_THAT(exclusion_space.InitialLetterClearanceOffset(),
              ElementsAre(LayoutUnit(73), LayoutUnit(73), LayoutUnit::Min()));
}

TEST(ExclusionSpaceTest, InitialLetterFloatLeft2) {
  test::TaskEnvironment task_environment;
  constexpr LayoutUnit kAvailableInlineSize = LayoutUnit(300);
  ExclusionSpaceForTesting exclusion_space(kAvailableInlineSize);

  // <!doctype html>
  //  <style>
  //  body { font-size: 20px; }
  //  .drop::first-letter { initial-letter: 3; }
  //  .sample {
  //      border: solid green 1px;
  //      margin-bottom: 5px;
  //      width: 300px;
  //  }
  //
  //  *::first-letter {
  //      color: red;
  //      background: yellow;
  //  }
  //
  //  float {
  //    float: left;
  //    width: 50px;
  //    height: 50px;
  //  }
  //
  //  </style>
  //  <<div class="sample drop">
  //  <float id="float1"></float>
  //  <float id="float2" style="width:100px"></float>
  //  Drop<br>line1<br>line2<br>line3<br>line4<br>line5<br>
  //  </div>
  exclusion_space.AddForFloat(9, 59, 59, 59);
  exclusion_space.AddForFloat(59, 59, 159, 59);
  exclusion_space.AddForInitialLetterBox(159, 9, 223, 73);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 32),
              ElementsAre(CreateLayoutOpportunity(223, 32, 309),
                          CreateLayoutOpportunity(223, 59, 309),
                          CreateLayoutOpportunity(9, 73, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 55),
              ElementsAre(CreateLayoutOpportunity(223, 55, 309),
                          CreateLayoutOpportunity(223, 59, 309),
                          CreateLayoutOpportunity(9, 73, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 78),
              ElementsAre(CreateLayoutOpportunity(9, 78, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 101),
              ElementsAre(CreateLayoutOpportunity(9, 101, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 124),
              ElementsAre(CreateLayoutOpportunity(9, 124, 309)));

  EXPECT_THAT(exclusion_space.InitialLetterClearanceOffset(),
              ElementsAre(LayoutUnit(73), LayoutUnit(73), LayoutUnit::Min()));
}

TEST(ExclusionSpaceTest, InitialLetterFloatLeft2ClearLeft) {
  test::TaskEnvironment task_environment;
  constexpr LayoutUnit kAvailableInlineSize = LayoutUnit(300);
  ExclusionSpaceForTesting exclusion_space(kAvailableInlineSize);

  // <!doctype html>
  //  <style>
  //  body { font-size: 20px; }
  //  .drop::first-letter { initial-letter: 3; }
  //  .sample {
  //      border: solid green 1px;
  //      margin-bottom: 5px;
  //      width: 300px;
  //  }
  //
  //  *::first-letter {
  //      color: red;
  //      background: yellow;
  //  }
  //
  //  float {
  //    float: left;
  //    width: 50px;
  //    height: 50px;
  //  }
  //
  //  </style>
  //  <<div class="sample drop">
  //  <float id="float1"></float>
  //  <float id="float2" style="clear:left; width:100px"></float>
  //  Drop<br>line1<br>line2<br>line3<br>line4<br>line5<br>
  //  </div>
  exclusion_space.AddForFloat(9, 59, 59, 59);
  exclusion_space.AddForFloat(9, 59, 109, 109);
  exclusion_space.AddForInitialLetterBox(109, 9, 223, 73);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 32),
              ElementsAre(CreateLayoutOpportunity(223, 32, 309),
                          CreateLayoutOpportunity(109, 73, 309),
                          CreateLayoutOpportunity(9, 109, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 55),
              ElementsAre(CreateLayoutOpportunity(223, 55, 309),
                          CreateLayoutOpportunity(109, 73, 309),
                          CreateLayoutOpportunity(9, 109, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 78),
              ElementsAre(CreateLayoutOpportunity(109, 78, 309),
                          CreateLayoutOpportunity(9, 109, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 101),
              ElementsAre(CreateLayoutOpportunity(109, 101, 309),
                          CreateLayoutOpportunity(9, 109, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 124),
              ElementsAre(CreateLayoutOpportunity(9, 124, 309)));

  EXPECT_THAT(exclusion_space.InitialLetterClearanceOffset(),
              ElementsAre(LayoutUnit(73), LayoutUnit(73), LayoutUnit::Min()));
}

TEST(ExclusionSpaceTest, InitialLetterFloatLeftAndRight) {
  test::TaskEnvironment task_environment;
  constexpr LayoutUnit kAvailableInlineSize = LayoutUnit(300);
  ExclusionSpaceForTesting exclusion_space(kAvailableInlineSize);

  // <!doctype html>
  //  <style>
  //  body { font-size: 20px; }
  //  .drop::first-letter { initial-letter: 3; }
  //  .sample {
  //      border: solid green 1px;
  //      margin-bottom: 5px;
  //      width: 300px;
  //  }
  //
  //  *::first-letter {
  //      color: red;
  //      background: yellow;
  //  }
  //
  //  float {
  //    float: left;
  //    width: 50px;
  //    height: 50px;
  //  }
  //
  //  </style>
  //  <<div class="sample drop">
  //  <float id="float1"></float>
  //  <float id="float2" style="float:right"></float>
  //  Drop<br>line1<br>line2<br>line3<br>line4<br>line5<br>
  //  </div>
  exclusion_space.AddForFloat(9, 59, 59, 59);

  EXPECT_THAT(exclusion_space.FindLayoutOpportunity(9, 9, 100),
              CreateLayoutOpportunity(9, 9, 309, 59));

  exclusion_space.AddForFloat(9, 59, 109, 109, EFloat::kRight);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 9),
              ElementsAre(CreateLayoutOpportunity(9, 9, 309, 59),
                          CreateLayoutOpportunity(109, 9, 309),
                          CreateLayoutOpportunity(9, 109, 309)));
  exclusion_space.AddForInitialLetterBox(59, 9, 123, 73);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 32),
              ElementsAre(CreateLayoutOpportunity(123, 32, 309),
                          CreateLayoutOpportunity(109, 73, 309),
                          CreateLayoutOpportunity(9, 109, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 55),
              ElementsAre(CreateLayoutOpportunity(123, 55, 309),
                          CreateLayoutOpportunity(109, 73, 309),
                          CreateLayoutOpportunity(9, 109, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 78),
              ElementsAre(CreateLayoutOpportunity(109, 78, 309),
                          CreateLayoutOpportunity(9, 109, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 101),
              ElementsAre(CreateLayoutOpportunity(109, 101, 309),
                          CreateLayoutOpportunity(9, 109, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 124),
              ElementsAre(CreateLayoutOpportunity(9, 124, 309)));

  EXPECT_THAT(exclusion_space.InitialLetterClearanceOffset(),
              ElementsAre(LayoutUnit(73), LayoutUnit(73), LayoutUnit::Min()));
}

TEST(ExclusionSpaceTest, InitialLetterFloatLeftAfterBreak) {
  test::TaskEnvironment task_environment;
  constexpr LayoutUnit kAvailableInlineSize = LayoutUnit(300);
  ExclusionSpaceForTesting exclusion_space(kAvailableInlineSize);

  // <!doctype html>
  //  <style>
  //  body { font-size: 20px; }
  //  .drop::first-letter { initial-letter: 3; }
  //  .sample {
  //      border: solid green 1px;
  //      margin-bottom: 5px;
  //      width: 300px;
  //  }
  //
  //  *::first-letter {
  //      color: red;
  //      background: yellow;
  //  }
  //
  //  float {
  //    float: left;
  //    width: 50px;
  //    height: 50px;
  //  }
  //
  //  </style>
  //  <<div class="sample drop">
  //  Drop<br>line1<br>
  //  <float id="float1"></float>line2<br>line3<br>line4<br>line5<br></div>
  //  </div>
  exclusion_space.AddForInitialLetterBox(9, 9, 223, 73);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 32),
              ElementsAre(CreateLayoutOpportunity(223, 32, 309),
                          CreateLayoutOpportunity(9, 73, 309)));

  EXPECT_THAT(exclusion_space.FindLayoutOpportunity(9, 73, 50),
              CreateLayoutOpportunity(9, 73, 309));
  exclusion_space.AddForFloat(9, 73, 59, 123);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 55),
              ElementsAre(CreateLayoutOpportunity(223, 55, 309),
                          CreateLayoutOpportunity(59, 73, 309),
                          CreateLayoutOpportunity(9, 123, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 78),
              ElementsAre(CreateLayoutOpportunity(59, 78, 309),
                          CreateLayoutOpportunity(9, 123, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 101),
              ElementsAre(CreateLayoutOpportunity(59, 101, 309),
                          CreateLayoutOpportunity(9, 123, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 124),
              ElementsAre(CreateLayoutOpportunity(9, 124, 309)));

  EXPECT_THAT(exclusion_space.InitialLetterClearanceOffset(),
              ElementsAre(LayoutUnit(73), LayoutUnit(73), LayoutUnit::Min()));
}

TEST(ExclusionSpaceTest, InitialLetterFloatRight2) {
  test::TaskEnvironment task_environment;
  constexpr LayoutUnit kAvailableInlineSize = LayoutUnit(300);
  ExclusionSpaceForTesting exclusion_space(kAvailableInlineSize);

  // <!doctype html>
  //  <style>
  //  body { font-size: 20px; }
  //  .drop::first-letter { initial-letter: 3; }
  //  .sample {
  //      border: solid green 1px;
  //      margin-bottom: 5px;
  //      width: 300px;
  //  }
  //
  //  *::first-letter {
  //      color: red;
  //      background: yellow;
  //  }
  //
  //  float {
  //    float: left;
  //    width: 50px;
  //    height: 50px;
  //  }
  //
  //  </style>
  //  <<div class="sample drop">
  //  <float id="float1" style="float:right"></float>
  //  <float id="float2" style="float:right; width: 200px;"></float>
  //  Drop<br>line1<br>line2<br>line3<br>line4<br>line5<br></div>
  //  </div>
  exclusion_space.AddForFloat(259, 9, 309, 59, EFloat::kRight);
  EXPECT_THAT(exclusion_space.FindLayoutOpportunity(9, 9, 200),
              CreateLayoutOpportunity(9, 59, 309));

  exclusion_space.AddForFloat(59, 9, 259, 59, EFloat::kRight);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 9),
              ElementsAre(CreateLayoutOpportunity(309, 9, 309),
                          CreateLayoutOpportunity(9, 59, 309)));

  exclusion_space.AddForInitialLetterBox(9, 59, 73, 123);

  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 82),
              ElementsAre(CreateLayoutOpportunity(73, 82, 309),
                          CreateLayoutOpportunity(9, 123, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 105),
              ElementsAre(CreateLayoutOpportunity(73, 105, 309),
                          CreateLayoutOpportunity(9, 123, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 128),
              ElementsAre(CreateLayoutOpportunity(9, 128, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 151),
              ElementsAre(CreateLayoutOpportunity(9, 151, 309)));
  EXPECT_THAT(exclusion_space.AllLayoutOpportunities(9, 174),
              ElementsAre(CreateLayoutOpportunity(9, 174, 309)));

  EXPECT_THAT(exclusion_space.InitialLetterClearanceOffset(),
              ElementsAre(LayoutUnit(123), LayoutUnit(123), LayoutUnit::Min()));
}

TEST(ExclusionSpaceTest, ZeroInlineSizeOpportunity) {
  test::TaskEnvironment task_environment;
  ExclusionSpace exclusion_space;

  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(), LayoutUnit()),
                                    BfcOffset(LayoutUnit(100), LayoutUnit(10))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));

  LayoutOpportunityVector opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(), LayoutUnit()},
      /* available_size */ LayoutUnit(100));

  EXPECT_EQ(2u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(100), LayoutUnit()),
                   BfcOffset(LayoutUnit(100), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(), LayoutUnit(10)),
                   BfcOffset(LayoutUnit(100), LayoutUnit::Max()));
}

TEST(ExclusionSpaceTest, NegativeInlineSizeOpportunityLeft) {
  test::TaskEnvironment task_environment;
  ExclusionSpace exclusion_space;

  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(), LayoutUnit()),
                                    BfcOffset(LayoutUnit(120), LayoutUnit(10))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));

  LayoutOpportunityVector opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(), LayoutUnit()},
      /* available_size */ LayoutUnit(100));

  EXPECT_EQ(2u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(120), LayoutUnit()),
                   BfcOffset(LayoutUnit(120), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(), LayoutUnit(10)),
                   BfcOffset(LayoutUnit(100), LayoutUnit::Max()));
}

TEST(ExclusionSpaceTest, NegativeInlineSizeOpportunityRight) {
  test::TaskEnvironment task_environment;
  ExclusionSpace exclusion_space;

  exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(-20), LayoutUnit()),
                                    BfcOffset(LayoutUnit(100), LayoutUnit(10))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));

  LayoutOpportunityVector opportunites = exclusion_space.AllLayoutOpportunities(
      /* offset */ {LayoutUnit(), LayoutUnit()},
      /* available_size */ LayoutUnit(100));

  EXPECT_EQ(2u, opportunites.size());
  TEST_OPPORTUNITY(opportunites[0], BfcOffset(LayoutUnit(), LayoutUnit()),
                   BfcOffset(LayoutUnit(), LayoutUnit::Max()));
  TEST_OPPORTUNITY(opportunites[1], BfcOffset(LayoutUnit(), LayoutUnit(10)),
                   BfcOffset(LayoutUnit(100), LayoutUnit::Max()));
}

TEST(ExclusionSpaceTest, PreInitialization) {
  test::TaskEnvironment task_environment;
  ExclusionSpace original_exclusion_space;

  original_exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(), LayoutUnit()),
                                    BfcOffset(LayoutUnit(20), LayoutUnit(15))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  original_exclusion_space.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(65), LayoutUnit()),
                                    BfcOffset(LayoutUnit(85), LayoutUnit(15))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));

  ExclusionSpace exclusion_space1;
  exclusion_space1.PreInitialize(original_exclusion_space);
  EXPECT_NE(original_exclusion_space, exclusion_space1);

  exclusion_space1.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(), LayoutUnit()),
                                    BfcOffset(LayoutUnit(20), LayoutUnit(15))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  EXPECT_NE(original_exclusion_space, exclusion_space1);

  // Adding the same exclusions will make the spaces equal.
  exclusion_space1.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(65), LayoutUnit()),
                                    BfcOffset(LayoutUnit(85), LayoutUnit(15))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));
  EXPECT_EQ(original_exclusion_space, exclusion_space1);

  // Adding a third exclusion will make the spaces non-equal.
  exclusion_space1.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(10), LayoutUnit(25)),
                                    BfcOffset(LayoutUnit(30), LayoutUnit(40))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  EXPECT_NE(original_exclusion_space, exclusion_space1);

  ExclusionSpace exclusion_space2;
  exclusion_space2.PreInitialize(original_exclusion_space);
  EXPECT_NE(original_exclusion_space, exclusion_space2);

  exclusion_space2.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(), LayoutUnit()),
                                    BfcOffset(LayoutUnit(20), LayoutUnit(15))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  EXPECT_NE(original_exclusion_space, exclusion_space2);

  // Adding a different second exclusion will make the spaces non-equal.
  exclusion_space2.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(10), LayoutUnit(25)),
                                    BfcOffset(LayoutUnit(30), LayoutUnit(40))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  EXPECT_NE(original_exclusion_space, exclusion_space2);
}

TEST(ExclusionSpaceTest, MergeExclusionSpacesNoPreviousExclusions) {
  test::TaskEnvironment task_environment;
  ExclusionSpace old_input;
  ExclusionSpace old_output = old_input;

  old_output.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(10), LayoutUnit(25)),
                                    BfcOffset(LayoutUnit(30), LayoutUnit(40))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));

  ExclusionSpace new_input;

  ExclusionSpace new_output = ExclusionSpace::MergeExclusionSpaces(
      old_output, old_input, new_input,
      /* offset_delta */ {LayoutUnit(10), LayoutUnit(20)});

  // To check the equality pre-initialize a new exclusion space with the
  // |new_output|, and add the expected exclusions.
  ExclusionSpace expected;
  expected.PreInitialize(new_output);
  expected.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(20), LayoutUnit(45)),
                                    BfcOffset(LayoutUnit(40), LayoutUnit(60))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));

  EXPECT_EQ(expected, new_output);
}

TEST(ExclusionSpaceTest, MergeExclusionSpacesPreviousExclusions) {
  test::TaskEnvironment task_environment;
  ExclusionSpace old_input;
  old_input.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(20), LayoutUnit(45)),
                                    BfcOffset(LayoutUnit(40), LayoutUnit(60))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));

  ExclusionSpace old_output = old_input;
  old_output.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(100), LayoutUnit(45)),
                                    BfcOffset(LayoutUnit(140), LayoutUnit(60))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));

  ExclusionSpace new_input;
  new_input.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(20), LayoutUnit(45)),
                                    BfcOffset(LayoutUnit(40), LayoutUnit(50))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));

  ExclusionSpace new_output = ExclusionSpace::MergeExclusionSpaces(
      old_output, old_input, new_input,
      /* offset_delta */ {LayoutUnit(10), LayoutUnit(20)});

  // To check the equality pre-initialize a new exclusion space with the
  // |new_output|, and add the expected exclusions.
  ExclusionSpace expected;
  expected.PreInitialize(new_output);
  expected.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(20), LayoutUnit(45)),
                                    BfcOffset(LayoutUnit(40), LayoutUnit(50))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  expected.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(110), LayoutUnit(65)),
                                    BfcOffset(LayoutUnit(150), LayoutUnit(80))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));

  EXPECT_EQ(expected, new_output);
}

TEST(ExclusionSpaceTest, MergeExclusionSpacesNoOutputExclusions) {
  test::TaskEnvironment task_environment;
  ExclusionSpace old_input;
  old_input.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(20), LayoutUnit(45)),
                                    BfcOffset(LayoutUnit(40), LayoutUnit(60))),
                            EFloat::kLeft, /*is_hidden_for_paint*/ false));
  old_input.Add(
      ExclusionArea::Create(BfcRect(BfcOffset(LayoutUnit(100), LayoutUnit(45)),
                                    BfcOffset(LayoutUnit(140), LayoutUnit(60))),
                            EFloat::kRight, /*is_hidden_for_paint*/ false));

  ExclusionSpace old_output = old_input;

  ExclusionSpace new_input;
  ExclusionSpace new_output = ExclusionSpace::MergeExclusionSpaces(
      old_output, old_input, new_input,
      /* offset_delta */ {LayoutUnit(10), LayoutUnit(20)});

  ExclusionSpace expected;
  EXPECT_EQ(expected, new_output);
}

}  // namespace
}  // namespace blink
