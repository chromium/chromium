// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"

#include <hb.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

ShapeResultRun* CreateTestShapeResultRun(wtf_size_t num_glyphs,
                                         wtf_size_t num_characters,
                                         const SimpleFontData* font = nullptr) {
  return MakeGarbageCollected<ShapeResultRun>(
      font, hb_direction_t::HB_DIRECTION_LTR,
      CanvasRotationInVertical::kRegular, hb_script_t::HB_SCRIPT_LATIN,
      /*start_index*/ 0, num_glyphs, num_characters);
}

}  // namespace

class ShapeResultRunTest : public testing::Test {
 protected:
  ShapeResultRun* CreateConstantAdvanceRun(
      wtf_size_t num_glyphs,
      wtf_size_t num_characters,
      const SimpleFontData* font = nullptr) {
    ShapeResultRun* run =
        CreateTestShapeResultRun(num_glyphs, num_characters, font);
    for (wtf_size_t i = 0; i < num_glyphs; ++i) {
      HarfBuzzRunGlyphData& glyph = run->glyph_data_.MutableGlyphAt(i);
      glyph.glyph = 42 + i;
      glyph.character_index = i;
      glyph.SetSafeToBreakBefore(SafeToBreak::kSafe);
      glyph.SetAdvance(10.0f);
    }
    run->width_ = num_glyphs * 10.0f;
    return run;
  }

  ShapeResultRun* CreateCompactRun(wtf_size_t num_glyphs,
                                   wtf_size_t num_characters) {
    ShapeResultRun* run = CreateConstantAdvanceRun(num_glyphs, num_characters);
    EXPECT_TRUE(run->glyph_data_.TryMakeCompact());
    return run;
  }
};
TEST_F(ShapeResultRunTest, GlyphDataCopyConstructor) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);
  auto* graphemes = MakeGarbageCollected<GCedHeapVector<wtf_size_t>>(2);
  run->glyph_data_.SetGraphemes(graphemes);

  ShapeResultRun* run2 = MakeGarbageCollected<ShapeResultRun>(*run);
  EXPECT_FALSE(run2->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(graphemes, run2->glyph_data_.Graphemes());

  run->glyph_data_.SetOffsetAt(0, GlyphOffset(1, 1));
  ShapeResultRun* run3 = MakeGarbageCollected<ShapeResultRun>(*run);
  ASSERT_TRUE(run3->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(1, 1), run3->glyph_data_.Offsets()[0]);
  EXPECT_NE(run->glyph_data_.Offsets().data(),
            run3->glyph_data_.Offsets().data());
  EXPECT_EQ(graphemes, run3->glyph_data_.Graphemes());

  run->glyph_data_.SetOffsetAt(0, GlyphOffset(2, 2));
  EXPECT_EQ(GlyphOffset(1, 1), run3->glyph_data_.Offsets()[0]);
}

TEST_F(ShapeResultRunTest, LargeNumCharacters) {
  constexpr unsigned kNumCharacters = 131072u;  // Requires more than 17 bits.
  // Only two glyphs are allocated; the character count is stored as metadata.
  ShapeResultRun* run =
      CreateTestShapeResultRun(/*num_glyphs*/ 2, kNumCharacters);
  EXPECT_EQ(kNumCharacters, run->NumCharacters());
}

TEST_F(ShapeResultRunTest, GlyphDataCopyFromRange) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  ShapeResultRun* run2 = CreateTestShapeResultRun(2, 2);
  run2->glyph_data_.CopyFromRange(GlyphDataRange{*run});
  EXPECT_FALSE(run2->glyph_data_.HasNonZeroOffsets());

  run->glyph_data_.SetOffsetAt(0, GlyphOffset(1, 1));
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());

  ShapeResultRun* run3 = CreateTestShapeResultRun(2, 2);
  run3->glyph_data_.CopyFromRange(GlyphDataRange{*run});
  ASSERT_TRUE(run3->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(1, 1), run3->glyph_data_.Offsets()[0]);
}

TEST_F(ShapeResultRunTest, GlyphDataReverse) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  run->glyph_data_.Reverse();
  EXPECT_FALSE(run->glyph_data_.HasNonZeroOffsets());

  run->glyph_data_.SetOffsetAt(0, GlyphOffset(1, 1));
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  run->glyph_data_.Reverse();
  EXPECT_EQ(GlyphOffset(), run->glyph_data_.Offsets()[0]);
  EXPECT_EQ(GlyphOffset(1, 1), run->glyph_data_.Offsets()[1]);
}

TEST_F(ShapeResultRunTest, GlyphDataAddOffsetHeightAt) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  run->glyph_data_.AddOffsetHeightAt(1, 1.5f);
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(0, 1.5f), run->glyph_data_.Offsets()[1]);

  run->glyph_data_.AddOffsetHeightAt(1, 2.0f);
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(0, 3.5f), run->glyph_data_.Offsets()[1]);
}

TEST_F(ShapeResultRunTest, GlyphDataAddOffsetWidthAt) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  run->glyph_data_.AddOffsetWidthAt(1, 1.5f);
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(1.5f, 0), run->glyph_data_.Offsets()[1]);

  run->glyph_data_.AddOffsetWidthAt(1, 2.0f);
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(3.5f, 0), run->glyph_data_.Offsets()[1]);
}

TEST_F(ShapeResultRunTest, GlyphDataSetAt) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  run->glyph_data_.SetOffsetAt(0, GlyphOffset());
  // Setting a zero offset should not allocate storage if it wasn't already
  // allocated.
  EXPECT_FALSE(run->glyph_data_.HasNonZeroOffsets());

  run->glyph_data_.SetOffsetAt(1, GlyphOffset(1, 1));
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(1, 1), run->glyph_data_.Offsets()[1]);
}

TEST_F(ShapeResultRunTest, GlyphDataShrink) {
  // Case 1: Shrink when no offsets are allocated.
  ShapeResultRun* run_no_offsets = CreateTestShapeResultRun(3, 3);
  EXPECT_FALSE(run_no_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(3u, run_no_offsets->glyph_data_.size());

  run_no_offsets->glyph_data_.Shrink(2);  // Shrink from 3 to 2
  // Offsets should remain unallocated.
  EXPECT_FALSE(run_no_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(2u, run_no_offsets->glyph_data_.size());  // Data should shrink.

  run_no_offsets->glyph_data_.Shrink(1);  // Shrink from 2 to 1
  EXPECT_FALSE(run_no_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(1u, run_no_offsets->glyph_data_.size());

  // Case 2: Shrink when offsets are allocated.
  ShapeResultRun* run_with_offsets = CreateTestShapeResultRun(3, 3);
  run_with_offsets->glyph_data_.SetOffsetAt(0, GlyphOffset(1, 0));
  run_with_offsets->glyph_data_.SetOffsetAt(1, GlyphOffset(2, 0));
  run_with_offsets->glyph_data_.SetOffsetAt(2, GlyphOffset(3, 0));
  ASSERT_TRUE(run_with_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(3u, run_with_offsets->glyph_data_.size());
  ASSERT_EQ(3u, run_with_offsets->glyph_data_.Offsets().size());

  // Shrink to a smaller size.
  run_with_offsets->glyph_data_.Shrink(2);
  ASSERT_TRUE(run_with_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(2u, run_with_offsets->glyph_data_.size());
  ASSERT_EQ(2u, run_with_offsets->glyph_data_.Offsets().size());
  EXPECT_EQ(GlyphOffset(1, 0), run_with_offsets->glyph_data_.Offsets()[0]);
  EXPECT_EQ(GlyphOffset(2, 0), run_with_offsets->glyph_data_.Offsets()[1]);

  // Shrink further.
  run_with_offsets->glyph_data_.Shrink(1);
  ASSERT_TRUE(run_with_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(1u, run_with_offsets->glyph_data_.size());
  ASSERT_EQ(1u, run_with_offsets->glyph_data_.Offsets().size());
  EXPECT_EQ(GlyphOffset(1, 0), run_with_offsets->glyph_data_.Offsets()[0]);

  // Case 3: Shrink to the same size (no-op for vector sizes).
  ShapeResultRun* run_shrink_same_size = CreateTestShapeResultRun(2, 2);
  run_shrink_same_size->glyph_data_.SetOffsetAt(0, GlyphOffset(5, 0));
  ASSERT_TRUE(run_shrink_same_size->glyph_data_.HasNonZeroOffsets());
  run_shrink_same_size->glyph_data_.Shrink(2);  // Shrink to current size.
  ASSERT_TRUE(run_shrink_same_size->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(2u, run_shrink_same_size->glyph_data_.size());
  ASSERT_EQ(2u, run_shrink_same_size->glyph_data_.Offsets().size());
  EXPECT_EQ(GlyphOffset(5, 0), run_shrink_same_size->glyph_data_.Offsets()[0]);
}

#if DCHECK_IS_ON()
TEST_F(ShapeResultRunTest, CompactEqualityDoesNotMaterialize) {
  ShapeResultRun* runs[] = {CreateCompactRun(8, 8), CreateCompactRun(8, 8)};

  EXPECT_TRUE(*runs[0] == *runs[1]);
  EXPECT_TRUE(runs[0]->glyph_data_.IsCompact());
  EXPECT_TRUE(runs[1]->glyph_data_.IsCompact());
}
#endif  // DCHECK_IS_ON()

TEST_F(ShapeResultRunTest, CompactReadersDoNotMaterialize) {
  ShapeResultRun* full = CreateConstantAdvanceRun(8, 8);
  ShapeResultRun* compact = CreateCompactRun(8, 8);
  ASSERT_FALSE(full->glyph_data_.IsCompact());
  ASSERT_TRUE(compact->glyph_data_.IsCompact());

  const GlyphDataRange::Reader full_reader(*full);
  const GlyphDataRange range(*compact);
  const GlyphDataRange::Reader readers[] = {GlyphDataRange::Reader(*compact),
                                            GlyphDataRange::Reader(range)};
  ASSERT_EQ(8u, full_reader.size());
  for (const auto& reader : readers) {
    ASSERT_EQ(full_reader.size(), reader.size());
    unsigned i = 0;
    for (const auto glyph : reader) {
      ASSERT_LT(i, full_reader.size());
      EXPECT_EQ(full_reader[i], reader[i]);
      EXPECT_EQ(full_reader[i], glyph);
      ++i;
    }
    EXPECT_EQ(full_reader.size(), i);

    auto it = reader.end();
    for (unsigned j = reader.size(); j > 0; --j) {
      EXPECT_EQ(full_reader[j - 1], *--it);
    }
    EXPECT_EQ(reader.begin(), it);
    EXPECT_TRUE(compact->glyph_data_.IsCompact());
  }
}

TEST_F(ShapeResultRunTest, CompactEmptyGlyphRangeKeepsRun) {
  ShapeResultRun* run = CreateCompactRun(8, 8);

  GlyphDataRange range = run->FindGlyphDataRange(8, 10);
  EXPECT_TRUE(range.IsEmpty());
  EXPECT_EQ(range.GetRun(), run);
  EXPECT_TRUE(range.IsCompactSource());

  EXPECT_EQ(70.0f, run->XPositionForOffset(8, AdjustMidCluster::kToStart));
  EXPECT_EQ(80.0f, run->XPositionForOffset(8, AdjustMidCluster::kToEnd));
  EXPECT_TRUE(run->glyph_data_.IsCompact());
}

TEST_F(ShapeResultRunTest, CompactRejectsOversizedInputBeforeReading) {
  ShapeResultRun* run = CreateTestShapeResultRun(0, 0);
  bool read_glyph = false;

  EXPECT_FALSE(run->glyph_data_.TryMakeCompactFrom(
      HarfBuzzRunGlyphData::kMaxGlyphs + 1, [&](unsigned) {
        read_glyph = true;
        return ShapeResultRun::GlyphDataCollection::CompactableGlyph{};
      }));
  EXPECT_FALSE(read_glyph);
  EXPECT_FALSE(run->glyph_data_.IsCompact());
}

TEST_F(ShapeResultRunTest, CompactTrailingCharactersMatchFullStorage) {
  ShapeResultRun* full_run = CreateConstantAdvanceRun(8, 10);
  ShapeResultRun* compact_run = MakeGarbageCollected<ShapeResultRun>(*full_run);
  ASSERT_TRUE(compact_run->glyph_data_.TryMakeCompact());

  for (int offset : {-13, 0, 13}) {
    for (int from = 0; from <= 10; ++from) {
      for (int to = from; to <= 11; ++to) {
        int expected_from = offset + from;
        int expected_to = offset + to;
        full_run->ExpandRangeToIncludePartialGlyphs(offset, &expected_from,
                                                    &expected_to);
        int actual_from = offset + from;
        int actual_to = offset + to;
        compact_run->ExpandRangeToIncludePartialGlyphs(offset, &actual_from,
                                                       &actual_to);
        EXPECT_EQ(expected_from, actual_from);
        EXPECT_EQ(expected_to, actual_to);
      }
    }
  }

  for (unsigned offset = 0; offset <= 10; ++offset) {
    EXPECT_EQ(full_run->NextSafeToBreakOffset(offset),
              compact_run->NextSafeToBreakOffset(offset));
    EXPECT_EQ(full_run->PreviousSafeToBreakOffset(offset),
              compact_run->PreviousSafeToBreakOffset(offset));
    for (AdjustMidCluster adjust :
         {AdjustMidCluster::kToStart, AdjustMidCluster::kToEnd}) {
      EXPECT_EQ(full_run->XPositionForOffset(offset, adjust),
                compact_run->XPositionForOffset(offset, adjust));
    }
  }
  EXPECT_TRUE(compact_run->glyph_data_.IsCompact());
}

TEST_F(ShapeResultRunTest, CompactReaderMatchesGlyphAtForSubRange) {
  ShapeResultRun* full_run = CreateConstantAdvanceRun(8, 8);
  ShapeResultRun* compact_run = MakeGarbageCollected<ShapeResultRun>(*full_run);
  ASSERT_TRUE(compact_run->glyph_data_.TryMakeCompact());

  for (ShapeResultRun* run : {full_run, compact_run}) {
    const GlyphDataRange range = run->FindGlyphDataRange(3, 7);
    ASSERT_EQ(4u, range.size());
    const GlyphDataRange::Reader reader(range);
    ASSERT_EQ(range.size(), reader.size());
    for (unsigned i = 0; i < range.size(); ++i) {
      const HarfBuzzRunGlyphData expected = range.GlyphAtForTest(i);
      const HarfBuzzRunGlyphData actual = reader[i];
      EXPECT_EQ(expected.glyph, actual.glyph);
      EXPECT_EQ(expected.character_index, actual.character_index);
      EXPECT_EQ(expected.advance, actual.advance);
    }
  }
  EXPECT_TRUE(compact_run->glyph_data_.IsCompact());
}

TEST_F(ShapeResultRunTest, RangeSurvivesRepresentationChanges) {
  ShapeResultRun* run = CreateConstantAdvanceRun(12, 12);
  const GlyphDataRange range = run->FindGlyphDataRange(2, 10);

  auto ExpectRange = [&] {
    ASSERT_EQ(8u, range.size());
    const GlyphDataRange::Reader reader(range);
    for (unsigned i = 0; i < reader.size(); ++i) {
      const HarfBuzzRunGlyphData glyph = reader[i];
      EXPECT_EQ(44u + i, glyph.glyph);
      EXPECT_EQ(2u + i, glyph.character_index);
      EXPECT_EQ(TextRunLayoutUnit::FromFloatRound(10.0f), glyph.advance);
    }
  };

  ExpectRange();
  ASSERT_TRUE(run->glyph_data_.TryMakeCompact());
  ExpectRange();

  run->glyph_data_.MutableGlyphs();
  ExpectRange();

  ASSERT_TRUE(run->glyph_data_.TryMakeCompact());
  run->glyph_data_.SetOffsetAt(4, GlyphOffset(1, 2));
  ASSERT_TRUE(range.HasOffsets());
  EXPECT_EQ(GlyphOffset(1, 2), range.Offsets()[2]);
  ExpectRange();

  run->glyph_data_.MutableGlyphAt(4);
  EXPECT_FALSE(range.IsCompactSource());
  EXPECT_EQ(GlyphOffset(1, 2), range.Offsets()[2]);
  ExpectRange();
}

TEST_F(ShapeResultRunTest, NestedCompactRangesMatchFullStorage) {
  constexpr unsigned kNumGlyphs = 12;
  ShapeResultRun* full_run = CreateConstantAdvanceRun(kNumGlyphs, kNumGlyphs);
  ShapeResultRun* compact_run = MakeGarbageCollected<ShapeResultRun>(*full_run);
  ASSERT_TRUE(compact_run->glyph_data_.TryMakeCompact());

  for (unsigned outer_start = 0; outer_start <= kNumGlyphs; ++outer_start) {
    for (unsigned outer_end = outer_start; outer_end <= kNumGlyphs;
         ++outer_end) {
      const GlyphDataRange full_outer =
          full_run->FindGlyphDataRange(outer_start, outer_end);
      const GlyphDataRange compact_outer =
          compact_run->FindGlyphDataRange(outer_start, outer_end);
      ASSERT_EQ(full_outer.size(), compact_outer.size());

      for (unsigned inner_start = outer_start; inner_start <= outer_end;
           ++inner_start) {
        for (unsigned inner_end = inner_start; inner_end <= outer_end;
             ++inner_end) {
          const GlyphDataRange full_inner =
              full_outer.FindGlyphDataRange(false, inner_start, inner_end);
          const GlyphDataRange compact_inner =
              compact_outer.FindGlyphDataRange(false, inner_start, inner_end);
          ASSERT_EQ(full_inner.size(), compact_inner.size());
          for (unsigned i = 0; i < full_inner.size(); ++i) {
            EXPECT_EQ(full_inner.GlyphAtForTest(i),
                      compact_inner.GlyphAtForTest(i));
          }
        }
      }
    }
  }
}

TEST_F(ShapeResultRunTest, CompactCopyMaterializesIndependently) {
  ShapeResultRun* run = CreateCompactRun(8, 8);
  run->glyph_data_.SetOffsetAt(3, GlyphOffset(1, 2));
  auto* graphemes = MakeGarbageCollected<GCedHeapVector<unsigned>>(8);
  run->glyph_data_.SetGraphemes(graphemes);
  ASSERT_TRUE(run->glyph_data_.IsCompact());

  ShapeResultRun* copy = MakeGarbageCollected<ShapeResultRun>(*run);
  ASSERT_TRUE(copy->glyph_data_.IsCompact());
  EXPECT_EQ(run->glyph_data_.CompactData(), copy->glyph_data_.CompactData());
  EXPECT_EQ(graphemes, copy->glyph_data_.Graphemes());
  ASSERT_TRUE(copy->glyph_data_.HasNonZeroOffsets());
  EXPECT_NE(run->glyph_data_.Offsets().data(),
            copy->glyph_data_.Offsets().data());
  EXPECT_EQ(GlyphOffset(1, 2), copy->glyph_data_.Offsets()[3]);

  HarfBuzzRunGlyphData& copied_glyph = copy->glyph_data_.MutableGlyphAt(3);
  EXPECT_EQ(copied_glyph.glyph, 45u);
  EXPECT_EQ(copied_glyph.character_index, 3u);
  EXPECT_EQ(copied_glyph.advance, TextRunLayoutUnit::FromFloatRound(10.0f));
  EXPECT_FALSE(copy->glyph_data_.IsCompact());
  EXPECT_TRUE(run->glyph_data_.IsCompact());

  copied_glyph.glyph = 99;
  copied_glyph.SetAdvance(12.0f);
  copy->glyph_data_.SetOffsetAt(3, GlyphOffset(3, 4));
  EXPECT_EQ(99u, copy->glyph_data_.GlyphAt(3).glyph);
  EXPECT_EQ(TextRunLayoutUnit::FromFloatRound(12.0f),
            copy->glyph_data_.GlyphAt(3).advance);
  EXPECT_EQ(GlyphOffset(3, 4), copy->glyph_data_.Offsets()[3]);
  EXPECT_EQ(45u, run->glyph_data_.GlyphAt(3).glyph);
  EXPECT_EQ(TextRunLayoutUnit::FromFloatRound(10.0f),
            run->glyph_data_.GlyphAt(3).advance);
  EXPECT_EQ(GlyphOffset(1, 2), run->glyph_data_.Offsets()[3]);
  EXPECT_EQ(graphemes, run->glyph_data_.Graphemes());
  EXPECT_TRUE(run->glyph_data_.IsCompact());
}

TEST_F(ShapeResultRunTest, CompactZeroOffsetDoesNotMaterialize) {
  ShapeResultRun* run = CreateCompactRun(8, 8);

  run->glyph_data_.SetOffsetAt(3, GlyphOffset());

  EXPECT_TRUE(run->glyph_data_.IsCompact());
  EXPECT_FALSE(run->glyph_data_.HasNonZeroOffsets());
}

}  // namespace blink
