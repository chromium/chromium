// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/try_value_flips.h"

#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class TryValueFlipsTest : public PageTestBase {
 public:
  struct ExpectedFlips {
    CSSPropertyID inset_block_start = CSSPropertyID::kInsetBlockStart;
    CSSPropertyID inset_block_end = CSSPropertyID::kInsetBlockEnd;
    CSSPropertyID inset_inline_start = CSSPropertyID::kInsetInlineStart;
    CSSPropertyID inset_inline_end = CSSPropertyID::kInsetInlineEnd;
    CSSPropertyID block_size = CSSPropertyID::kBlockSize;
    CSSPropertyID inline_size = CSSPropertyID::kInlineSize;
    CSSPropertyID min_block_size = CSSPropertyID::kMinBlockSize;
    CSSPropertyID min_inline_size = CSSPropertyID::kMinInlineSize;
    CSSPropertyID max_block_size = CSSPropertyID::kMaxBlockSize;
    CSSPropertyID max_inline_size = CSSPropertyID::kMaxInlineSize;
  };

  // Creates a CSSPropertyValueSet that contains CSSFlipRevertValue
  // for each declarations in `flips` that actually represents a flip
  // (i.e. doesn't just flip to itself).
  const CSSPropertyValueSet* ExpectedFlipsSet(ExpectedFlips flips) {
    auto* set =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);

    auto add_if_flipped = [set](CSSPropertyID from, CSSPropertyID to) {
      if (from != to) {
        set->SetProperty(
            from, *MakeGarbageCollected<cssvalue::CSSFlipRevertValue>(to));
      }
    };

    add_if_flipped(CSSPropertyID::kInsetBlockStart, flips.inset_block_start);
    add_if_flipped(CSSPropertyID::kInsetBlockEnd, flips.inset_block_end);
    add_if_flipped(CSSPropertyID::kInsetInlineStart, flips.inset_inline_start);
    add_if_flipped(CSSPropertyID::kInsetInlineEnd, flips.inset_inline_end);
    add_if_flipped(CSSPropertyID::kBlockSize, flips.block_size);
    add_if_flipped(CSSPropertyID::kInlineSize, flips.inline_size);
    add_if_flipped(CSSPropertyID::kMinBlockSize, flips.min_block_size);
    add_if_flipped(CSSPropertyID::kMinInlineSize, flips.min_inline_size);
    add_if_flipped(CSSPropertyID::kMaxBlockSize, flips.max_block_size);
    add_if_flipped(CSSPropertyID::kMaxInlineSize, flips.max_inline_size);

    return set;
  }

  // Serializes the declarations of `set` into a vector. AsText is not used,
  // because it shorthandifies the declarations, which is not helpful
  // for debugging failing tests.
  Vector<String> DeclarationStrings(const CSSPropertyValueSet* set) {
    Vector<String> result;
    for (unsigned i = 0; i < set->PropertyCount(); ++i) {
      CSSPropertyValueSet::PropertyReference ref = set->PropertyAt(i);
      result.push_back(ref.Name().ToAtomicString() + ":" +
                       ref.Value().CssText());
    }
    return result;
  }

  Vector<String> ExpectedFlipsVector(ExpectedFlips flips) {
    return DeclarationStrings(ExpectedFlipsSet(flips));
  }

  Vector<String> ActualFlipsVector(const TryTacticList& tactic_list) {
    TryValueFlips flips;
    return DeclarationStrings(flips.FlipSet(tactic_list));
  }

  TryTacticList Tactics(TryTactic t0,
                        TryTactic t1 = TryTactic::kNone,
                        TryTactic t2 = TryTactic::kNone) {
    return TryTacticList{t0, t1, t2};
  }
};

TEST_F(TryValueFlipsTest, None) {
  TryValueFlips flips;
  EXPECT_FALSE(flips.FlipSet(Tactics(TryTactic::kNone)));
}

// Flips without kFlipStart:

TEST_F(TryValueFlipsTest, FlipBlock) {
  EXPECT_EQ(ExpectedFlipsVector(ExpectedFlips{
                .inset_block_start = CSSPropertyID::kInsetBlockEnd,
                .inset_block_end = CSSPropertyID::kInsetBlockStart,
            }),
            ActualFlipsVector(Tactics(TryTactic::kFlipBlock)));
}

TEST_F(TryValueFlipsTest, FlipInline) {
  EXPECT_EQ(ExpectedFlipsVector(ExpectedFlips{
                .inset_inline_start = CSSPropertyID::kInsetInlineEnd,
                .inset_inline_end = CSSPropertyID::kInsetInlineStart,
            }),
            ActualFlipsVector(Tactics(TryTactic::kFlipInline)));
}

TEST_F(TryValueFlipsTest, FlipBlockInline) {
  EXPECT_EQ(ExpectedFlipsVector(ExpectedFlips{
                .inset_block_start = CSSPropertyID::kInsetBlockEnd,
                .inset_block_end = CSSPropertyID::kInsetBlockStart,
                .inset_inline_start = CSSPropertyID::kInsetInlineEnd,
                .inset_inline_end = CSSPropertyID::kInsetInlineStart,
            }),
            ActualFlipsVector(
                Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)));
}

TEST_F(TryValueFlipsTest, FlipInlineBlock) {
  EXPECT_EQ(
      ActualFlipsVector(Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)),
      ActualFlipsVector(
          Tactics(TryTactic::kFlipInline, TryTactic::kFlipBlock)));
}

// Flips with kFlipStart:

TEST_F(TryValueFlipsTest, FlipStart) {
  EXPECT_EQ(
      ExpectedFlipsVector(ExpectedFlips{
          .inset_block_start = CSSPropertyID::kInsetInlineStart,
          .inset_block_end = CSSPropertyID::kInsetInlineEnd,
          .inset_inline_start = CSSPropertyID::kInsetBlockStart,
          .inset_inline_end = CSSPropertyID::kInsetBlockEnd,
          // Flipped sizing:
          .block_size = CSSPropertyID::kInlineSize,
          .inline_size = CSSPropertyID::kBlockSize,
          .min_block_size = CSSPropertyID::kMinInlineSize,
          .min_inline_size = CSSPropertyID::kMinBlockSize,
          .max_block_size = CSSPropertyID::kMaxInlineSize,
          .max_inline_size = CSSPropertyID::kMaxBlockSize,
      }),
      ActualFlipsVector(Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart,
                                TryTactic::kFlipInline)));
}

TEST_F(TryValueFlipsTest, FlipBlockStartInline) {
  EXPECT_EQ(
      ActualFlipsVector(Tactics(TryTactic::kFlipStart)),
      ActualFlipsVector(Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart,
                                TryTactic::kFlipInline)));
}

TEST_F(TryValueFlipsTest, FlipInlineStartBlock) {
  EXPECT_EQ(
      ActualFlipsVector(Tactics(TryTactic::kFlipStart)),
      ActualFlipsVector(Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart,
                                TryTactic::kFlipBlock)));
}

TEST_F(TryValueFlipsTest, FlipStartBlock) {
  EXPECT_EQ(
      ExpectedFlipsVector(ExpectedFlips{
          .inset_block_start = CSSPropertyID::kInsetInlineEnd,
          .inset_block_end = CSSPropertyID::kInsetInlineStart,
          .inset_inline_start = CSSPropertyID::kInsetBlockStart,
          .inset_inline_end = CSSPropertyID::kInsetBlockEnd,
          // Flipped sizing:
          .block_size = CSSPropertyID::kInlineSize,
          .inline_size = CSSPropertyID::kBlockSize,
          .min_block_size = CSSPropertyID::kMinInlineSize,
          .min_inline_size = CSSPropertyID::kMinBlockSize,
          .max_block_size = CSSPropertyID::kMaxInlineSize,
          .max_inline_size = CSSPropertyID::kMaxBlockSize,
      }),
      ActualFlipsVector(Tactics(TryTactic::kFlipStart, TryTactic::kFlipBlock)));
}

TEST_F(TryValueFlipsTest, FlipInlineStart) {
  EXPECT_EQ(
      ActualFlipsVector(Tactics(TryTactic::kFlipStart, TryTactic::kFlipBlock)),
      ActualFlipsVector(
          Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)));
}

TEST_F(TryValueFlipsTest, FlipStartInline) {
  EXPECT_EQ(ExpectedFlipsVector(ExpectedFlips{
                .inset_block_start = CSSPropertyID::kInsetInlineStart,
                .inset_block_end = CSSPropertyID::kInsetInlineEnd,
                .inset_inline_start = CSSPropertyID::kInsetBlockEnd,
                .inset_inline_end = CSSPropertyID::kInsetBlockStart,
                // Flipped sizing:
                .block_size = CSSPropertyID::kInlineSize,
                .inline_size = CSSPropertyID::kBlockSize,
                .min_block_size = CSSPropertyID::kMinInlineSize,
                .min_inline_size = CSSPropertyID::kMinBlockSize,
                .max_block_size = CSSPropertyID::kMaxInlineSize,
                .max_inline_size = CSSPropertyID::kMaxBlockSize,
            }),
            ActualFlipsVector(
                Tactics(TryTactic::kFlipStart, TryTactic::kFlipInline)));
}

TEST_F(TryValueFlipsTest, FlipBlockStart) {
  EXPECT_EQ(
      ActualFlipsVector(Tactics(TryTactic::kFlipStart, TryTactic::kFlipInline)),
      ActualFlipsVector(Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)));
}

TEST_F(TryValueFlipsTest, FlipStartBlockInline) {
  EXPECT_EQ(
      ExpectedFlipsVector(ExpectedFlips{
          .inset_block_start = CSSPropertyID::kInsetInlineEnd,
          .inset_block_end = CSSPropertyID::kInsetInlineStart,
          .inset_inline_start = CSSPropertyID::kInsetBlockEnd,
          .inset_inline_end = CSSPropertyID::kInsetBlockStart,
          // Flipped sizing:
          .block_size = CSSPropertyID::kInlineSize,
          .inline_size = CSSPropertyID::kBlockSize,
          .min_block_size = CSSPropertyID::kMinInlineSize,
          .min_inline_size = CSSPropertyID::kMinBlockSize,
          .max_block_size = CSSPropertyID::kMaxInlineSize,
          .max_inline_size = CSSPropertyID::kMaxBlockSize,
      }),
      ActualFlipsVector(Tactics(TryTactic::kFlipStart, TryTactic::kFlipBlock,
                                TryTactic::kFlipInline)));
}

TEST_F(TryValueFlipsTest, FlipStartInlineBlock) {
  EXPECT_EQ(
      ActualFlipsVector(Tactics(TryTactic::kFlipStart, TryTactic::kFlipBlock,
                                TryTactic::kFlipInline)),
      ActualFlipsVector(Tactics(TryTactic::kFlipStart, TryTactic::kFlipInline,
                                TryTactic::kFlipBlock)));
}

TEST_F(TryValueFlipsTest, FlipInlineBlockStart) {
  EXPECT_EQ(
      ActualFlipsVector(Tactics(TryTactic::kFlipStart, TryTactic::kFlipBlock,
                                TryTactic::kFlipInline)),
      ActualFlipsVector(Tactics(TryTactic::kFlipInline, TryTactic::kFlipBlock,
                                TryTactic::kFlipStart)));
}

TEST_F(TryValueFlipsTest, FlipBlockInlineStart) {
  EXPECT_EQ(
      ActualFlipsVector(Tactics(TryTactic::kFlipStart, TryTactic::kFlipBlock,
                                TryTactic::kFlipInline)),
      ActualFlipsVector(Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline,
                                TryTactic::kFlipStart)));
}

}  // namespace blink
