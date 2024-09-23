// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/try_tactic_transform.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TryTacticTransformTest : public testing::Test {};

constexpr TryTacticList Tactics(TryTactic t0,
                                TryTactic t1 = TryTactic::kNone,
                                TryTactic t2 = TryTactic::kNone) {
  return TryTacticList{t0, t1, t2};
}

enum LogicalSide {
  kBlockStart,
  kBlockEnd,
  kInlineEnd,
  kInlineStart,
};

using LogicalSides = TryTacticTransform::LogicalSides<LogicalSide>;

bool operator==(const LogicalSides& a, const LogicalSides& b) {
  return a.inline_start == b.inline_start && a.inline_end == b.inline_end &&
         a.block_start == b.block_start && a.block_end == b.block_end;
}

LogicalSides InitialLogicalSides() {
  return LogicalSides{
      .inline_start = LogicalSide::kInlineStart,
      .inline_end = LogicalSide::kInlineEnd,
      .block_start = LogicalSide::kBlockStart,
      .block_end = LogicalSide::kBlockEnd,
  };
}

LogicalSides TransformLogicalSides(TryTacticList tactic_list) {
  TryTacticTransform transform(tactic_list);
  return transform.Transform(InitialLogicalSides());
}

TEST_F(TryTacticTransformTest, Equality) {
  EXPECT_EQ(TryTacticTransform(Tactics(TryTactic::kNone)),
            TryTacticTransform(Tactics(TryTactic::kNone)));
  EXPECT_EQ(TryTacticTransform(Tactics(TryTactic::kFlipBlock)),
            TryTacticTransform(Tactics(TryTactic::kFlipBlock)));
  EXPECT_NE(TryTacticTransform(Tactics(TryTactic::kFlipInline)),
            TryTacticTransform(Tactics(TryTactic::kFlipBlock)));
  EXPECT_NE(TryTacticTransform(Tactics(TryTactic::kFlipBlock)),
            TryTacticTransform(Tactics(TryTactic::kFlipInline)));
}

// First test that tactics that overlap produce the same transforms:
//
// (See TryTacticTransform).
//
// block                  (1)
// inline                 (2)
// block inline           (3)
// start                  (4)
// block start            (5)
// inline start           (6)
// block inline start     (7)
//
// inline block           (=>3)
// block start inline     (=>4)
// inline start block     (=>4)
// start inline           (=>5)
// start block            (=>6)
// inline block start     (=>7)
// start block inline     (=>7)
// start inline block     (=>7)

// (3)
TEST_F(TryTacticTransformTest, BlockInlineEquality) {
  TryTacticTransform expected = TryTacticTransform(
      Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline));
  EXPECT_EQ(expected, TryTacticTransform(Tactics(TryTactic::kFlipInline,
                                                 TryTactic::kFlipBlock)));
}

// (4)
TEST_F(TryTacticTransformTest, StartEquality) {
  TryTacticTransform expected =
      TryTacticTransform(Tactics(TryTactic::kFlipStart));
  EXPECT_EQ(expected, TryTacticTransform(Tactics(TryTactic::kFlipBlock,
                                                 TryTactic::kFlipStart,
                                                 TryTactic::kFlipInline)));
  EXPECT_EQ(expected, TryTacticTransform(Tactics(TryTactic::kFlipInline,
                                                 TryTactic::kFlipStart,
                                                 TryTactic::kFlipBlock)));
}

// (5)
TEST_F(TryTacticTransformTest, BlockStartEquality) {
  TryTacticTransform expected =
      TryTacticTransform(Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart));
  EXPECT_EQ(expected, TryTacticTransform(Tactics(TryTactic::kFlipStart,
                                                 TryTactic::kFlipInline)));
}

// (6)
TEST_F(TryTacticTransformTest, InlineStartEquality) {
  TryTacticTransform expected = TryTacticTransform(
      Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart));
  EXPECT_EQ(expected, TryTacticTransform(Tactics(TryTactic::kFlipStart,
                                                 TryTactic::kFlipBlock)));
}

// (7)
TEST_F(TryTacticTransformTest, BlockInlineStartEquality) {
  TryTacticTransform expected = TryTacticTransform(Tactics(
      TryTactic::kFlipBlock, TryTactic::kFlipInline, TryTactic::kFlipStart));
  EXPECT_EQ(expected, TryTacticTransform(Tactics(TryTactic::kFlipStart,
                                                 TryTactic::kFlipBlock,
                                                 TryTactic::kFlipInline)));
  EXPECT_EQ(expected, TryTacticTransform(Tactics(TryTactic::kFlipStart,
                                                 TryTactic::kFlipBlock,
                                                 TryTactic::kFlipInline)));
  EXPECT_EQ(expected, TryTacticTransform(Tactics(TryTactic::kFlipStart,
                                                 TryTactic::kFlipInline,
                                                 TryTactic::kFlipBlock)));
}

// Test Transform:

// (0)
TEST_F(TryTacticTransformTest, Transform_None) {
  EXPECT_EQ((LogicalSides{
                .inline_start = LogicalSide::kInlineStart,
                .inline_end = LogicalSide::kInlineEnd,
                .block_start = LogicalSide::kBlockStart,
                .block_end = LogicalSide::kBlockEnd,
            }),
            TransformLogicalSides(Tactics(TryTactic::kNone)));
}

// (1)
TEST_F(TryTacticTransformTest, Transform_Block) {
  EXPECT_EQ((LogicalSides{
                .inline_start = LogicalSide::kInlineStart,
                .inline_end = LogicalSide::kInlineEnd,
                .block_start = LogicalSide::kBlockEnd,
                .block_end = LogicalSide::kBlockStart,
            }),
            TransformLogicalSides(Tactics(TryTactic::kFlipBlock)));
}

// (2)
TEST_F(TryTacticTransformTest, Transform_Inline) {
  EXPECT_EQ((LogicalSides{
                .inline_start = LogicalSide::kInlineEnd,
                .inline_end = LogicalSide::kInlineStart,
                .block_start = LogicalSide::kBlockStart,
                .block_end = LogicalSide::kBlockEnd,
            }),
            TransformLogicalSides(Tactics(TryTactic::kFlipInline)));
}

// (3)
TEST_F(TryTacticTransformTest, Transform_Block_Inline) {
  EXPECT_EQ((LogicalSides{
                .inline_start = LogicalSide::kInlineEnd,
                .inline_end = LogicalSide::kInlineStart,
                .block_start = LogicalSide::kBlockEnd,
                .block_end = LogicalSide::kBlockStart,
            }),
            TransformLogicalSides(
                Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)));
}

// (4)
TEST_F(TryTacticTransformTest, Transform_Start) {
  EXPECT_EQ((LogicalSides{
                .inline_start = LogicalSide::kBlockStart,
                .inline_end = LogicalSide::kBlockEnd,
                .block_start = LogicalSide::kInlineStart,
                .block_end = LogicalSide::kInlineEnd,
            }),
            TransformLogicalSides(Tactics(TryTactic::kFlipStart)));
}

// (5)
TEST_F(TryTacticTransformTest, Transform_Block_Start) {
  EXPECT_EQ((LogicalSides{
                .inline_start = LogicalSide::kBlockStart,
                .inline_end = LogicalSide::kBlockEnd,
                .block_start = LogicalSide::kInlineEnd,
                .block_end = LogicalSide::kInlineStart,
            }),
            TransformLogicalSides(
                Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)));
}

// (6)
TEST_F(TryTacticTransformTest, Transform_Inline_Start) {
  EXPECT_EQ((LogicalSides{
                .inline_start = LogicalSide::kBlockEnd,
                .inline_end = LogicalSide::kBlockStart,
                .block_start = LogicalSide::kInlineStart,
                .block_end = LogicalSide::kInlineEnd,
            }),
            TransformLogicalSides(
                Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)));
}

// (7)
TEST_F(TryTacticTransformTest, Transform_Block_Inline_Start) {
  EXPECT_EQ((LogicalSides{
                .inline_start = LogicalSide::kBlockEnd,
                .inline_end = LogicalSide::kBlockStart,
                .block_start = LogicalSide::kInlineEnd,
                .block_end = LogicalSide::kInlineStart,
            }),
            TransformLogicalSides(Tactics(TryTactic::kFlipBlock,
                                          TryTactic::kFlipInline,
                                          TryTactic::kFlipStart)));
}

// Inverse

// (0)
TEST_F(TryTacticTransformTest, Inverse_None) {
  TryTacticTransform transform(Tactics(TryTactic::kNone));
  EXPECT_EQ(InitialLogicalSides(),
            transform.Inverse().Transform(
                transform.Transform(InitialLogicalSides())));
}

// (1)
TEST_F(TryTacticTransformTest, Inverse_Block) {
  TryTacticTransform transform(Tactics(TryTactic::kFlipBlock));
  EXPECT_EQ(InitialLogicalSides(),
            transform.Inverse().Transform(
                transform.Transform(InitialLogicalSides())));
}

// (2)
TEST_F(TryTacticTransformTest, Inverse_Inline) {
  TryTacticTransform transform(Tactics(TryTactic::kFlipInline));
  EXPECT_EQ(InitialLogicalSides(),
            transform.Inverse().Transform(
                transform.Transform(InitialLogicalSides())));
}

// (3)
TEST_F(TryTacticTransformTest, Inverse_Block_Inline) {
  TryTacticTransform transform(
      Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline));
  EXPECT_EQ(InitialLogicalSides(),
            transform.Inverse().Transform(
                transform.Transform(InitialLogicalSides())));
}

// (4)
TEST_F(TryTacticTransformTest, Inverse_Start) {
  TryTacticTransform transform(Tactics(TryTactic::kFlipStart));
  EXPECT_EQ(InitialLogicalSides(),
            transform.Inverse().Transform(
                transform.Transform(InitialLogicalSides())));
}

// (5)
TEST_F(TryTacticTransformTest, Inverse_Block_Start) {
  TryTacticTransform transform(
      Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart));
  EXPECT_EQ(InitialLogicalSides(),
            transform.Inverse().Transform(
                transform.Transform(InitialLogicalSides())));
}

// (6)
TEST_F(TryTacticTransformTest, Inverse_Inline_Start) {
  TryTacticTransform transform(
      Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart));
  EXPECT_EQ(InitialLogicalSides(),
            transform.Inverse().Transform(
                transform.Transform(InitialLogicalSides())));
}

// (7)
TEST_F(TryTacticTransformTest, Inverse_Block_Inline_Start) {
  TryTacticTransform transform(Tactics(
      TryTactic::kFlipBlock, TryTactic::kFlipInline, TryTactic::kFlipStart));
  EXPECT_EQ(InitialLogicalSides(),
            transform.Inverse().Transform(
                transform.Transform(InitialLogicalSides())));
}

// CacheIndex
TEST_F(TryTacticTransformTest, NoTacticsCacheIndex) {
  TryTacticTransform transform(kNoTryTactics);
  // TryValueFlips::FlipSet relies on the kNoTryTactics transform having
  // a CacheIndex of zero.
  EXPECT_EQ(0u, transform.CacheIndex());
}

}  // namespace blink
