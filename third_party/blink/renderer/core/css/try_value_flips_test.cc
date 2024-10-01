// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/try_value_flips.h"

#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/try_tactic_transform.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"

namespace blink {

constexpr TryTacticList Tactics(TryTactic t0,
                                TryTactic t1 = TryTactic::kNone,
                                TryTactic t2 = TryTactic::kNone) {
  return TryTacticList{t0, t1, t2};
}

class TryValueFlipsTest : public PageTestBase {
 public:
  struct ExpectedFlips {
    CSSPropertyID inset_block_start = CSSPropertyID::kInsetBlockStart;
    CSSPropertyID inset_block_end = CSSPropertyID::kInsetBlockEnd;
    CSSPropertyID inset_inline_start = CSSPropertyID::kInsetInlineStart;
    CSSPropertyID inset_inline_end = CSSPropertyID::kInsetInlineEnd;
    CSSPropertyID margin_block_start = CSSPropertyID::kMarginBlockStart;
    CSSPropertyID margin_block_end = CSSPropertyID::kMarginBlockEnd;
    CSSPropertyID margin_inline_start = CSSPropertyID::kMarginInlineStart;
    CSSPropertyID margin_inline_end = CSSPropertyID::kMarginInlineEnd;
    CSSPropertyID align_self = CSSPropertyID::kAlignSelf;
    CSSPropertyID justify_self = CSSPropertyID::kJustifySelf;
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

    auto add = [set](CSSPropertyID from, CSSPropertyID to) {
      set->SetProperty(from,
                       *MakeGarbageCollected<cssvalue::CSSFlipRevertValue>(
                           to, TryTacticTransform()));
    };

    auto add_if_flipped = [&add](CSSPropertyID from, CSSPropertyID to) {
      if (from != to) {
        add(from, to);
      }
    };

    add_if_flipped(CSSPropertyID::kInsetBlockStart, flips.inset_block_start);
    add_if_flipped(CSSPropertyID::kInsetBlockEnd, flips.inset_block_end);
    add_if_flipped(CSSPropertyID::kInsetInlineStart, flips.inset_inline_start);
    add_if_flipped(CSSPropertyID::kInsetInlineEnd, flips.inset_inline_end);
    add_if_flipped(CSSPropertyID::kMarginBlockStart, flips.margin_block_start);
    add_if_flipped(CSSPropertyID::kMarginBlockEnd, flips.margin_block_end);
    add_if_flipped(CSSPropertyID::kMarginInlineStart,
                   flips.margin_inline_start);
    add_if_flipped(CSSPropertyID::kMarginInlineEnd, flips.margin_inline_end);
    add(CSSPropertyID::kAlignSelf, flips.align_self);
    add(CSSPropertyID::kJustifySelf, flips.justify_self);
    add(CSSPropertyID::kPositionArea, CSSPropertyID::kPositionArea);
    if (RuntimeEnabledFeatures::CSSInsetAreaPropertyEnabled()) {
      add(CSSPropertyID::kInsetArea, CSSPropertyID::kInsetArea);
    }
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
                .margin_block_start = CSSPropertyID::kMarginBlockEnd,
                .margin_block_end = CSSPropertyID::kMarginBlockStart,
            }),
            ActualFlipsVector(Tactics(TryTactic::kFlipBlock)));
}

TEST_F(TryValueFlipsTest, FlipInline) {
  EXPECT_EQ(ExpectedFlipsVector(ExpectedFlips{
                .inset_inline_start = CSSPropertyID::kInsetInlineEnd,
                .inset_inline_end = CSSPropertyID::kInsetInlineStart,
                .margin_inline_start = CSSPropertyID::kMarginInlineEnd,
                .margin_inline_end = CSSPropertyID::kMarginInlineStart,
            }),
            ActualFlipsVector(Tactics(TryTactic::kFlipInline)));
}

TEST_F(TryValueFlipsTest, FlipBlockInline) {
  EXPECT_EQ(ExpectedFlipsVector(ExpectedFlips{
                .inset_block_start = CSSPropertyID::kInsetBlockEnd,
                .inset_block_end = CSSPropertyID::kInsetBlockStart,
                .inset_inline_start = CSSPropertyID::kInsetInlineEnd,
                .inset_inline_end = CSSPropertyID::kInsetInlineStart,
                .margin_block_start = CSSPropertyID::kMarginBlockEnd,
                .margin_block_end = CSSPropertyID::kMarginBlockStart,
                .margin_inline_start = CSSPropertyID::kMarginInlineEnd,
                .margin_inline_end = CSSPropertyID::kMarginInlineStart,
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
          .margin_block_start = CSSPropertyID::kMarginInlineStart,
          .margin_block_end = CSSPropertyID::kMarginInlineEnd,
          .margin_inline_start = CSSPropertyID::kMarginBlockStart,
          .margin_inline_end = CSSPropertyID::kMarginBlockEnd,
          // Flipped alignment:
          .align_self = CSSPropertyID::kJustifySelf,
          .justify_self = CSSPropertyID::kAlignSelf,
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
          .margin_block_start = CSSPropertyID::kMarginInlineEnd,
          .margin_block_end = CSSPropertyID::kMarginInlineStart,
          .margin_inline_start = CSSPropertyID::kMarginBlockStart,
          .margin_inline_end = CSSPropertyID::kMarginBlockEnd,
          // Flipped alignment:
          .align_self = CSSPropertyID::kJustifySelf,
          .justify_self = CSSPropertyID::kAlignSelf,
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
                .margin_block_start = CSSPropertyID::kMarginInlineStart,
                .margin_block_end = CSSPropertyID::kMarginInlineEnd,
                .margin_inline_start = CSSPropertyID::kMarginBlockEnd,
                .margin_inline_end = CSSPropertyID::kMarginBlockStart,
                // Flipped alignment:
                .align_self = CSSPropertyID::kJustifySelf,
                .justify_self = CSSPropertyID::kAlignSelf,
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
          .margin_block_start = CSSPropertyID::kMarginInlineEnd,
          .margin_block_end = CSSPropertyID::kMarginInlineStart,
          .margin_inline_start = CSSPropertyID::kMarginBlockEnd,
          .margin_inline_end = CSSPropertyID::kMarginBlockStart,
          // Flipped alignment:
          .align_self = CSSPropertyID::kJustifySelf,
          .justify_self = CSSPropertyID::kAlignSelf,
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

namespace {

struct Declaration {
  STACK_ALLOCATED();

 public:
  CSSPropertyID property_id;
  const CSSValue* value;
};

Declaration ParseDeclaration(String string) {
  const CSSPropertyValueSet* set =
      css_test_helpers::ParseDeclarationBlock(string);
  CHECK(set);
  CHECK_EQ(1u, set->PropertyCount());
  CSSPropertyValueSet::PropertyReference ref = set->PropertyAt(0);
  return Declaration{.property_id = ref.Name().Id(), .value = &ref.Value()};
}

}  // namespace

struct FlipValueTestData {
  const char* input;
  const char* expected;
  TryTacticList tactic;
  WritingDirectionMode writing_direction =
      WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr);
};

FlipValueTestData flip_value_test_data[] = {
    // clang-format off

    // Possible transforms (from try_tactic_transforms.h):
    //
    // block                  (1)
    // inline                 (2)
    // block inline           (3)
    // start                  (4)
    // block start            (5)
    // inline start           (6)
    // block inline start     (7)

    // Physical anchor():

    // (1)
    {
      .input = "left:anchor(right)",
      .expected = "left:anchor(right)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "right:anchor(left)",
      .expected = "right:anchor(left)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "top:anchor(bottom)",
      .expected = "bottom:anchor(top)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "bottom:anchor(top)",
      .expected = "top:anchor(bottom)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },

    // (2)
    {
      .input = "left:anchor(right)",
      .expected = "right:anchor(left)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "right:anchor(left)",
      .expected = "left:anchor(right)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "top:anchor(bottom)",
      .expected = "top:anchor(bottom)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "bottom:anchor(top)",
      .expected = "bottom:anchor(top)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },

    // (3)
    {
      .input = "left:anchor(right)",
      .expected = "right:anchor(left)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },
    {
      .input = "right:anchor(left)",
      .expected = "left:anchor(right)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },
    {
      .input = "top:anchor(bottom)",
      .expected = "bottom:anchor(top)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },
    {
      .input = "bottom:anchor(top)",
      .expected = "top:anchor(bottom)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },

    // (4)
    {
      .input = "left:anchor(right)",
      .expected = "top:anchor(bottom)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },
    {
      .input = "right:anchor(left)",
      .expected = "bottom:anchor(top)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },
    {
      .input = "top:anchor(bottom)",
      .expected = "left:anchor(right)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },
    {
      .input = "bottom:anchor(top)",
      .expected = "right:anchor(left)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // (5)
    {
      .input = "left:anchor(right)",
      .expected = "top:anchor(bottom)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)
    },
    {
      .input = "right:anchor(left)",
      .expected = "bottom:anchor(top)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)
    },
    {
      .input = "top:anchor(bottom)",
      .expected = "right:anchor(left)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)
    },
    {
      .input = "bottom:anchor(top)",
      .expected = "left:anchor(right)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)
    },

    // (6)
    {
      .input = "left:anchor(right)",
      .expected = "bottom:anchor(top)",
      .tactic = Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)
    },
    {
      .input = "right:anchor(left)",
      .expected = "top:anchor(bottom)",
      .tactic = Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)
    },
    {
      .input = "top:anchor(bottom)",
      .expected = "left:anchor(right)",
      .tactic = Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)
    },
    {
      .input = "bottom:anchor(top)",
      .expected = "right:anchor(left)",
      .tactic = Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)
    },

    // (7)
    {
      .input = "left:anchor(right)",
      .expected = "bottom:anchor(top)",
      .tactic = Tactics(TryTactic::kFlipBlock,
                        TryTactic::kFlipInline,
                        TryTactic::kFlipStart)
    },
    {
      .input = "right:anchor(left)",
      .expected = "top:anchor(bottom)",
      .tactic = Tactics(TryTactic::kFlipBlock,
                        TryTactic::kFlipInline,
                        TryTactic::kFlipStart)
    },
    {
      .input = "top:anchor(bottom)",
      .expected = "right:anchor(left)",
      .tactic = Tactics(TryTactic::kFlipBlock,
                        TryTactic::kFlipInline,
                        TryTactic::kFlipStart)
    },
    {
      .input = "bottom:anchor(top)",
      .expected = "left:anchor(right)",
      .tactic = Tactics(TryTactic::kFlipBlock,
                        TryTactic::kFlipInline,
                        TryTactic::kFlipStart)
    },

    // Logical anchor():

    // (1)
    {
      .input = "left:anchor(end)",
      .expected = "left:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "right:anchor(start)",
      .expected = "right:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "top:anchor(end)",
      .expected = "bottom:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "bottom:anchor(start)",
      .expected = "top:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },

    // (2)
    {
      .input = "left:anchor(end)",
      .expected = "right:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "right:anchor(start)",
      .expected = "left:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "top:anchor(end)",
      .expected = "top:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "bottom:anchor(start)",
      .expected = "bottom:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },

    // (3)
    {
      .input = "left:anchor(end)",
      .expected = "right:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },
    {
      .input = "right:anchor(start)",
      .expected = "left:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },
    {
      .input = "top:anchor(end)",
      .expected = "bottom:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },
    {
      .input = "bottom:anchor(start)",
      .expected = "top:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },

    // (4)
    {
      .input = "left:anchor(end)",
      .expected = "top:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },
    {
      .input = "right:anchor(start)",
      .expected = "bottom:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },
    {
      .input = "top:anchor(end)",
      .expected = "left:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },
    {
      .input = "bottom:anchor(start)",
      .expected = "right:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // (5)
    {
      .input = "left:anchor(end)",
      .expected = "top:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)
    },
    {
      .input = "right:anchor(start)",
      .expected = "bottom:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)
    },
    {
      .input = "top:anchor(end)",
      .expected = "right:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)
    },
    {
      .input = "bottom:anchor(start)",
      .expected = "left:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart)
    },

    // (6)
    {
      .input = "left:anchor(end)",
      .expected = "bottom:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)
    },
    {
      .input = "right:anchor(start)",
      .expected = "top:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)
    },
    {
      .input = "top:anchor(end)",
      .expected = "left:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)
    },
    {
      .input = "bottom:anchor(start)",
      .expected = "right:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipInline, TryTactic::kFlipStart)
    },

    // (7)
    {
      .input = "left:anchor(end)",
      .expected = "bottom:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipBlock,
                        TryTactic::kFlipInline,
                        TryTactic::kFlipStart)
    },
    {
      .input = "right:anchor(start)",
      .expected = "top:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipBlock,
                        TryTactic::kFlipInline,
                        TryTactic::kFlipStart)
    },
    {
      .input = "top:anchor(end)",
      .expected = "right:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipBlock,
                        TryTactic::kFlipInline,
                        TryTactic::kFlipStart)
    },
    {
      .input = "bottom:anchor(start)",
      .expected = "left:anchor(end)",
      .tactic = Tactics(TryTactic::kFlipBlock,
                        TryTactic::kFlipInline,
                        TryTactic::kFlipStart)
    },

    // Physical anchor-size()

    // (1)
    {
      .input = "width:anchor-size(width)",
      .expected = "width:anchor-size(width)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },

    // (2)
    {
      .input = "width:anchor-size(width)",
      .expected = "width:anchor-size(width)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },

    // (3)
    {
      .input = "width:anchor-size(width)",
      .expected = "width:anchor-size(width)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },

    // (4)
    {
      .input = "width:anchor-size(width)",
      .expected = "height:anchor-size(height)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // (5)
    {
      .input = "width:anchor-size(width)",
      .expected = "height:anchor-size(height)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // (6)
    {
      .input = "width:anchor-size(width)",
      .expected = "height:anchor-size(height)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // (7)
    {
      .input = "width:anchor-size(width)",
      .expected = "height:anchor-size(height)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // Logical anchor-size():

    // (1)
    {
      .input = "width:anchor-size(inline)",
      .expected = "width:anchor-size(inline)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },

    // (2)
    {
      .input = "width:anchor-size(inline)",
      .expected = "width:anchor-size(inline)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },

    // (3)
    {
      .input = "width:anchor-size(inline)",
      .expected = "width:anchor-size(inline)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipInline)
    },

    // (4)
    {
      .input = "width:anchor-size(inline)",
      .expected = "height:anchor-size(block)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // (5)
    {
      .input = "width:anchor-size(inline)",
      .expected = "height:anchor-size(block)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // (6)
    {
      .input = "width:anchor-size(inline)",
      .expected = "height:anchor-size(block)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // (7)
    {
      .input = "width:anchor-size(inline)",
      .expected = "height:anchor-size(block)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },

    // calc() expressions, etc:

    {
      .input = "left:calc(anchor(left) + 10px)",
      .expected = "right:calc(anchor(right) + 10px)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "left:calc(min(anchor(left), anchor(right), 50px) + 10px)",
      .expected = "right:calc(min(anchor(right), anchor(left), 50px) + 10px)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "left:calc(anchor(left, anchor(right)) + 10px)",
      .expected = "right:calc(anchor(right, anchor(left)) + 10px)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },

    // Writing modes:

    {
      .input = "left:anchor(left)",
      .expected = "right:anchor(right)",
      .tactic = Tactics(TryTactic::kFlipInline),
      .writing_direction = WritingDirectionMode(
        WritingMode::kHorizontalTb, TextDirection::kRtl)
    },
    {
      .input = "right:anchor(right)",
      .expected = "left:anchor(left)",
      .tactic = Tactics(TryTactic::kFlipInline),
      .writing_direction = WritingDirectionMode(
        WritingMode::kHorizontalTb, TextDirection::kRtl)
    },
    {
      .input = "left:anchor(left)",
      .expected = "right:anchor(right)",
      .tactic = Tactics(TryTactic::kFlipBlock),
      .writing_direction = WritingDirectionMode(
        WritingMode::kVerticalLr, TextDirection::kLtr)
    },
    {
      .input = "left:anchor(left)",
      .expected = "top:anchor(top)",
      .tactic = Tactics(TryTactic::kFlipBlock, TryTactic::kFlipStart),
      .writing_direction = WritingDirectionMode(
        WritingMode::kVerticalRl, TextDirection::kLtr)
    },

    // clang-format on
};

class FlipValueTest : public PageTestBase,
                      public testing::WithParamInterface<FlipValueTestData> {};

INSTANTIATE_TEST_SUITE_P(TryValueFlipsTest,
                         FlipValueTest,
                         testing::ValuesIn(flip_value_test_data));

TEST_P(FlipValueTest, All) {
  FlipValueTestData param = GetParam();
  Declaration input = ParseDeclaration(String(param.input));
  Declaration expected = ParseDeclaration(String(param.expected));
  TryTacticTransform transform = TryTacticTransform(param.tactic);
  const CSSValue* actual_value = TryValueFlips::FlipValue(
      input.property_id, input.value, transform, param.writing_direction);
  ASSERT_TRUE(actual_value);
  EXPECT_EQ(expected.value->CssText(), actual_value->CssText());
}

struct NoFlipValueTestData {
  const char* input;
  TryTacticList tactic;
  WritingDirectionMode writing_direction =
      WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr);
};

// These cases should cause TryValueFlips::FlipValue to return
// the incoming CSSValue instance.
NoFlipValueTestData no_flip_value_test_data[] = {
    // clang-format off

    {
      .input = "left:10px",
      .tactic = Tactics(TryTactic::kNone)
    },
    {
      .input = "left:calc(10px + 20px)",
      .tactic = Tactics(TryTactic::kNone)
    },
    {
      .input = "left:min(10px, 20px)",
      .tactic = Tactics(TryTactic::kNone)
    },
    {
      .input = "left:anchor(left)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "left:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "top:anchor(start)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "left:anchor(self-start)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "top:anchor(self-start)",
      .tactic = Tactics(TryTactic::kFlipInline)
    },
    {
      .input = "left:calc(anchor(left) + 10px)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "left:calc(anchor(left) + 10px)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "left:calc(anchor(start) + 10px)",
      .tactic = Tactics(TryTactic::kFlipStart)
    },
    {
      .input = "width:anchor-size(width)",
      .tactic = Tactics(TryTactic::kNone)
    },
    {
      .input = "width:anchor-size(width)",
      .tactic = Tactics(TryTactic::kFlipBlock)
    },
    {
      .input = "width:calc(anchor-size(width) + anchor-size(height))",
      .tactic = Tactics(TryTactic::kFlipInline)
    },

    // clang-format on
};

class NoFlipValueTest
    : public PageTestBase,
      public testing::WithParamInterface<NoFlipValueTestData> {};

INSTANTIATE_TEST_SUITE_P(TryValueFlipsTest,
                         NoFlipValueTest,
                         testing::ValuesIn(no_flip_value_test_data));

TEST_P(NoFlipValueTest, All) {
  NoFlipValueTestData param = GetParam();
  Declaration input = ParseDeclaration(String(param.input));
  TryTacticTransform transform = TryTacticTransform(param.tactic);
  const CSSValue* actual_value = TryValueFlips::FlipValue(
      input.property_id, input.value, transform, param.writing_direction);
  ASSERT_TRUE(actual_value);
  SCOPED_TRACE(testing::Message() << "Actual: " << actual_value->CssText());
  SCOPED_TRACE(testing::Message() << "Expected: " << input.value->CssText());
  EXPECT_EQ(input.value, actual_value);
}

}  // namespace blink
