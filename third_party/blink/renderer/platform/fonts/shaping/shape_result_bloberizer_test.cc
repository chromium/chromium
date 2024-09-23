// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"

#include <memory>
#include <optional>

#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_vertical_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

// Creating minimal test SimpleFontData objects,
// the font won't have any glyphs, but that's okay.
static SimpleFontData* CreateTestSimpleFontData(bool force_rotation = false) {
  return MakeGarbageCollected<SimpleFontData>(
      MakeGarbageCollected<FontPlatformData>(
          skia::DefaultTypeface(), std::string(), 10, false, false,
          TextRenderingMode::kAutoTextRendering, ResolvedFontFeatures{},
          force_rotation ? FontOrientation::kVerticalUpright
                         : FontOrientation::kHorizontal),
      nullptr);
}

class ShapeResultBloberizerTest : public FontTestBase {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(12.0);
    font_description.SetLocale(LayoutLocale::Get(AtomicString("en")));
    ASSERT_EQ(USCRIPT_LATIN, font_description.GetScript());
    font_description.SetGenericFamily(FontDescription::kStandardFamily);

    cache = MakeGarbageCollected<ShapeCache>();
  }

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;

  Persistent<ShapeCache> cache;
};

struct ExpectedRun {
  int glyph_count;
  std::string utf8;
  // Currently RTL is output in reverse of logical order, but this is not
  // a requirement. This really just expects montonicity.
  enum ClusterDirection { kAscending, kDescending } cluster_direction;
};
using ExpectedBlob = std::vector<ExpectedRun>;

struct ExpectedRange {
  unsigned from;
  unsigned to;
  unsigned length() { return to - from; }
};

void CheckBlobBuffer(const ShapeResultBloberizer::BlobBuffer& blob_buffer,
                     const std::vector<ExpectedBlob>& expected_blobs) {
  EXPECT_EQ(blob_buffer.size(), expected_blobs.size());
  auto blob_info_iter = blob_buffer.begin();
  auto&& expected_blob_iter = expected_blobs.begin();
  for (; blob_info_iter != blob_buffer.end() &&
         expected_blob_iter != expected_blobs.end();
       ++blob_info_iter, ++expected_blob_iter) {
    size_t blob_index = expected_blob_iter - expected_blobs.begin();
    const ExpectedBlob& expected_blob = *expected_blob_iter;
    SkTextBlob::Iter::Run run;
    size_t actual_run_count = 0;
    for (SkTextBlob::Iter it(*blob_info_iter->blob.get()); it.next(&run);) {
      ++actual_run_count;
    }
    EXPECT_EQ(actual_run_count, expected_blob.size()) << "Blob: " << blob_index;
    auto&& expected_run_iter = expected_blob.begin();
    SkTextBlob::Iter it(*blob_info_iter->blob.get());
    for (; it.next(&run) && expected_run_iter != expected_blob.end();
         ++expected_run_iter) {
      size_t run_index = expected_run_iter - expected_blob.begin();
      const ExpectedRun& expected_run = *expected_run_iter;
      EXPECT_EQ(expected_run.glyph_count, run.fGlyphCount)
          << "Blob: " << blob_index << " Run: " << run_index;

      int actual_size = run.fUtf8Size_forTest;
      int expected_size = expected_run.utf8.size();
      EXPECT_EQ(actual_size, expected_size)
          << "Blob: " << blob_index << " Run: " << run_index;
      for (int i = 0; i < actual_size && i < expected_size; ++i) {
        EXPECT_EQ(run.fUtf8_forTest[i], expected_run.utf8[i])
            << "Blob: " << blob_index << " Run: " << run_index << " i: " << i;
      }

      auto utf8_index_previous = run.fClusterIndex_forTest[0];
      for (int i = 0; i < run.fGlyphCount; ++i) {
        EXPECT_LE(0ul, run.fClusterIndex_forTest[i]);
        EXPECT_LT((int)run.fClusterIndex_forTest[i], run.fUtf8Size_forTest);
        auto expected_direction = expected_run.cluster_direction;
        if (expected_direction == ExpectedRun::ClusterDirection::kAscending) {
          EXPECT_LE(utf8_index_previous, run.fClusterIndex_forTest[i]);
        } else {
          EXPECT_GE(utf8_index_previous, run.fClusterIndex_forTest[i]);
        }
        utf8_index_previous = run.fClusterIndex_forTest[i];
      }
    }
  }
}

}  // anonymous namespace

TEST_F(ShapeResultBloberizerTest, StartsEmpty) {
  Font font;
  ShapeResultBloberizer bloberizer(font.GetFontDescription(),
                                   ShapeResultBloberizer::Type::kNormal);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            nullptr);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer).size(),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer).size(),
            0ul);
  EXPECT_FALSE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  EXPECT_TRUE(bloberizer.Blobs().empty());
}

TEST_F(ShapeResultBloberizerTest, StoresGlyphsOffsets) {
  Font font;
  ShapeResultBloberizer bloberizer(font.GetFontDescription(),
                                   ShapeResultBloberizer::Type::kNormal);

  SimpleFontData* font1 = CreateTestSimpleFontData();
  SimpleFontData* font2 = CreateTestSimpleFontData();

  // 2 pending glyphs
  ShapeResultBloberizerTestInfo::Add(bloberizer, 42, font1,
                                     CanvasRotationInVertical::kRegular, 10, 0);
  ShapeResultBloberizerTestInfo::Add(bloberizer, 43, font1,
                                     CanvasRotationInVertical::kRegular, 15, 1);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font1);
  EXPECT_FALSE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 2ul);
    EXPECT_EQ(42, glyphs[0]);
    EXPECT_EQ(43, glyphs[1]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 2ul);
    EXPECT_EQ(10, offsets[0]);
    EXPECT_EQ(15, offsets[1]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  // one more glyph, different font => pending run flush
  ShapeResultBloberizerTestInfo::Add(bloberizer, 44, font2,
                                     CanvasRotationInVertical::kRegular, 12, 0);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font2);
  EXPECT_FALSE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 1ul);
    EXPECT_EQ(44, glyphs[0]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 1ul);
    EXPECT_EQ(12, offsets[0]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            1ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  // flush everything (1 blob w/ 2 runs)
  EXPECT_EQ(bloberizer.Blobs().size(), 1ul);
}

TEST_F(ShapeResultBloberizerTest, StoresGlyphsVerticalOffsets) {
  Font font;
  ShapeResultBloberizer bloberizer(font.GetFontDescription(),
                                   ShapeResultBloberizer::Type::kNormal);

  SimpleFontData* font1 = CreateTestSimpleFontData();
  SimpleFontData* font2 = CreateTestSimpleFontData();

  // 2 pending glyphs
  ShapeResultBloberizerTestInfo::Add(bloberizer, 42, font1,
                                     CanvasRotationInVertical::kRegular,
                                     gfx::Vector2dF(10, 0), 0);
  ShapeResultBloberizerTestInfo::Add(bloberizer, 43, font1,
                                     CanvasRotationInVertical::kRegular,
                                     gfx::Vector2dF(15, 0), 1);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font1);
  EXPECT_TRUE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 2ul);
    EXPECT_EQ(42, glyphs[0]);
    EXPECT_EQ(43, glyphs[1]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 4ul);
    EXPECT_EQ(10, offsets[0]);
    EXPECT_EQ(0, offsets[1]);
    EXPECT_EQ(15, offsets[2]);
    EXPECT_EQ(0, offsets[3]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  // one more glyph, different font => pending run flush
  ShapeResultBloberizerTestInfo::Add(bloberizer, 44, font2,
                                     CanvasRotationInVertical::kRegular,
                                     gfx::Vector2dF(12, 2), 2);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font2);
  EXPECT_TRUE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 1ul);
    EXPECT_EQ(44, glyphs[0]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 2ul);
    EXPECT_EQ(12, offsets[0]);
    EXPECT_EQ(2, offsets[1]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            1ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  // flush everything (1 blob w/ 2 runs)
  EXPECT_EQ(bloberizer.Blobs().size(), 1ul);
}

TEST_F(ShapeResultBloberizerTest, MixedBlobRotation) {
  Font font;
  ShapeResultBloberizer bloberizer(font.GetFontDescription(),
                                   ShapeResultBloberizer::Type::kNormal);

  SimpleFontData* test_font = CreateTestSimpleFontData();

  struct {
    CanvasRotationInVertical canvas_rotation;
    size_t expected_pending_glyphs;
    size_t expected_pending_runs;
    size_t expected_committed_blobs;
  } append_ops[] = {
      // append 2 horizontal glyphs -> these go into the pending glyph buffer
      {CanvasRotationInVertical::kRegular, 1u, 0u, 0u},
      {CanvasRotationInVertical::kRegular, 2u, 0u, 0u},

      // append 3 vertical rotated glyphs -> push the prev pending (horizontal)
      // glyphs into a new run in the current (horizontal) blob
      {CanvasRotationInVertical::kRotateCanvasUpright, 1u, 1u, 0u},
      {CanvasRotationInVertical::kRotateCanvasUpright, 2u, 1u, 0u},
      {CanvasRotationInVertical::kRotateCanvasUpright, 3u, 1u, 0u},

      // append 2 more horizontal glyphs -> flush the current (horizontal) blob,
      // push prev (vertical) pending glyphs into new vertical blob run
      {CanvasRotationInVertical::kRegular, 1u, 1u, 1u},
      {CanvasRotationInVertical::kRegular, 2u, 1u, 1u},

      // append 1 more vertical glyph -> flush current (vertical) blob, push
      // prev (horizontal) pending glyphs into a new horizontal blob run
      {CanvasRotationInVertical::kRotateCanvasUpright, 1u, 1u, 2u},
  };

  for (const auto& op : append_ops) {
    ShapeResultBloberizerTestInfo::Add(bloberizer, 42, test_font,
                                       op.canvas_rotation, gfx::Vector2dF(), 0);
    EXPECT_EQ(
        op.expected_pending_glyphs,
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer).size());
    EXPECT_EQ(op.canvas_rotation,
              ShapeResultBloberizerTestInfo::PendingBlobRotation(bloberizer));
    EXPECT_EQ(op.expected_pending_runs,
              ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer));
    EXPECT_EQ(op.expected_committed_blobs,
              ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer));
  }

  // flush everything -> 4 blobs total
  EXPECT_EQ(4u, bloberizer.Blobs().size());
}

// Tests that filling a glyph buffer for a specific range returns the same
// results when shaping word by word as when shaping the full run in one go.
TEST_F(ShapeResultBloberizerTest, CommonAccentLeftToRightFillGlyphBuffer) {
  // "/. ." with an accent mark over the first dot.
  const UChar kStr[] = {0x2F, 0x301, 0x2E, 0x20, 0x2E};
  TextRun text_run(kStr, base::make_span(kStr).size());
  TextRunPaintInfo run_info(text_run);
  run_info.to = 3;

  Font font(font_description);
  CachingWordShaper word_shaper(font);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  ShapeResultBloberizer::FillGlyphs bloberizer(
      font.GetFontDescription(), run_info, buffer,
      ShapeResultBloberizer::Type::kEmitText);

  Font reference_font(font_description);
  reference_font.SetCanShapeWordByWordForTesting(false);

  CachingWordShaper reference_word_shaper(reference_font);
  ShapeResultBuffer reference_buffer;
  reference_word_shaper.FillResultBuffer(run_info, &reference_buffer);
  ShapeResultBloberizer::FillGlyphs reference_bloberizer(
      reference_font.GetFontDescription(), run_info, reference_buffer,
      ShapeResultBloberizer::Type::kEmitText);

  const auto& glyphs =
      ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
  ASSERT_EQ(glyphs.size(), 3ul);
  const auto reference_glyphs =
      ShapeResultBloberizerTestInfo::PendingRunGlyphs(reference_bloberizer);
  ASSERT_EQ(reference_glyphs.size(), 3ul);

  EXPECT_EQ(reference_glyphs[0], glyphs[0]);
  EXPECT_EQ(reference_glyphs[1], glyphs[1]);
  EXPECT_EQ(reference_glyphs[2], glyphs[2]);

  CheckBlobBuffer(
      bloberizer.Blobs(),
      {{
          {3,
           text_run.ToStringView()
               .ToString()
               .Substring(run_info.from, run_info.to - run_info.from)
               .Utf8(),
           ExpectedRun::ClusterDirection::kAscending},
      }});
}

// Tests that filling a glyph buffer for a specific range returns the same
// results when shaping word by word as when shaping the full run in one go.
TEST_F(ShapeResultBloberizerTest, CommonAccentRightToLeftFillGlyphBuffer) {
  // "[] []" with an accent mark over the last square bracket.
  const UChar kStr[] = {0x5B, 0x5D, 0x20, 0x5B, 0x301, 0x5D};
  TextRun text_run(kStr, base::make_span(kStr).size());
  text_run.SetDirection(TextDirection::kRtl);
  TextRunPaintInfo run_info(text_run);
  run_info.from = 1;

  Font font(font_description);
  CachingWordShaper word_shaper(font);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  ShapeResultBloberizer::FillGlyphs bloberizer(
      font.GetFontDescription(), run_info, buffer,
      ShapeResultBloberizer::Type::kEmitText);

  Font reference_font(font_description);
  reference_font.SetCanShapeWordByWordForTesting(false);

  CachingWordShaper reference_word_shaper(reference_font);
  ShapeResultBuffer reference_buffer;
  reference_word_shaper.FillResultBuffer(run_info, &reference_buffer);
  ShapeResultBloberizer::FillGlyphs reference_bloberizer(
      reference_font.GetFontDescription(), run_info, reference_buffer,
      ShapeResultBloberizer::Type::kEmitText);

  const auto& glyphs =
      ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
  ASSERT_EQ(5u, glyphs.size());
  const auto reference_glyphs =
      ShapeResultBloberizerTestInfo::PendingRunGlyphs(reference_bloberizer);
  ASSERT_EQ(5u, reference_glyphs.size());

  EXPECT_EQ(reference_glyphs[0], glyphs[0]);
  EXPECT_EQ(reference_glyphs[1], glyphs[1]);
  EXPECT_EQ(reference_glyphs[2], glyphs[2]);
  EXPECT_EQ(reference_glyphs[3], glyphs[3]);
  EXPECT_EQ(reference_glyphs[4], glyphs[4]);
}

TEST_F(ShapeResultBloberizerTest, CommonAccentRightToLeftFillGlyphBufferNG) {
  // "[] []" with an accent mark over the last square bracket.
  const UChar kStr[] = {0x5B, 0x5D, 0x20, 0x5B, 0x301, 0x5D};
  String string(kStr, base::make_span(kStr).size());

  Font font(font_description);
  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, TextDirection::kRtl);

  ShapeResultView* result_view = ShapeResultView::Create(result);
  TextFragmentPaintInfo text_info{StringView(string), 1, string.length(),
                                  result_view};
  ShapeResultBloberizer::FillGlyphsNG bloberizer_ng(
      font.GetFontDescription(), text_info.text, text_info.from, text_info.to,
      text_info.shape_result, ShapeResultBloberizer::Type::kEmitText);

  CheckBlobBuffer(
      bloberizer_ng.Blobs(),
      {{
          {5,
           string.Substring(text_info.from, text_info.to - text_info.from)
               .Utf8(),
           ExpectedRun::ClusterDirection::kDescending},
      }});
}

TEST_F(ShapeResultBloberizerTest, FourByteUtf8CodepointsNG) {
  // Codepoints which encode to four UTF-8 code units.
  const UChar kStr[] = {0xD841, 0xDF31, 0xD841, 0xDF79};
  String string(kStr, base::make_span(kStr).size());

  Font font(font_description);
  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, TextDirection::kLtr);

  ShapeResultView* result_view = ShapeResultView::Create(result);
  TextFragmentPaintInfo text_info{StringView(string), 0, string.length(),
                                  result_view};
  ShapeResultBloberizer::FillGlyphsNG bloberizer_ng(
      font.GetFontDescription(), text_info.text, text_info.from, text_info.to,
      text_info.shape_result, ShapeResultBloberizer::Type::kEmitText);

  CheckBlobBuffer(
      bloberizer_ng.Blobs(),
      {{
          {2,
           string.Substring(text_info.from, text_info.to - text_info.from)
               .Utf8(),
           ExpectedRun::ClusterDirection::kAscending},
      }});
}

TEST_F(ShapeResultBloberizerTest, OffsetIntoTrailingSurrogateNG) {
  // Codepoints which encode to four UTF-8 code units.
  const UChar kStr[] = {0xD841, 0xDF31, 0xD841, 0xDF79};
  String string(kStr, base::make_span(kStr).size());

  Font font(font_description);
  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, TextDirection::kLtr);

  ShapeResultView* result_view = ShapeResultView::Create(result);
  // Start at offset 1 into text at trailing surrogate.
  TextFragmentPaintInfo text_info{StringView(string), 1, string.length(),
                                  result_view};
  ShapeResultBloberizer::FillGlyphsNG bloberizer_ng(
      font.GetFontDescription(), text_info.text, text_info.from, text_info.to,
      text_info.shape_result, ShapeResultBloberizer::Type::kEmitText);

  // Do not expect the trailing surrogate to be in any output.
  const auto& glyphs =
      ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer_ng);
  EXPECT_EQ(1u, glyphs.size());

  CheckBlobBuffer(
      bloberizer_ng.Blobs(),
      {{
          {1,
           string
               .Substring(text_info.from + 1, text_info.to - text_info.from - 1)
               .Utf8(),
           ExpectedRun::ClusterDirection::kAscending},
      }});
}

TEST_F(ShapeResultBloberizerTest, LatinMultRunNG) {
  TextDirection direction = TextDirection::kLtr;
  String string = "Testing ShapeResultIterator::CopyRange";

  ExpectedRange range_a{0, 5};
  ExpectedRange range_b{5, 7};
  ExpectedRange range_c{7, 32};
  ExpectedRange range_d{32, 38};
  HarfBuzzShaper shaper_a(string.Substring(range_a.from, range_a.to));
  HarfBuzzShaper shaper_b(string.Substring(range_b.from, range_b.to));
  HarfBuzzShaper shaper_c(string.Substring(range_c.from, range_c.to));
  HarfBuzzShaper shaper_d(string.Substring(range_d.from, range_d.to));

  Font font(font_description);

  FontDescription font2_description(font_description);
  font2_description.SetComputedSize(20);
  Font font2(font2_description);

  // Combine four separate results into a single one to ensure we have a result
  // with multiple runs. Interleave fonts to ensure run changes.
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(&font, 0, 0, direction);
  shaper_a.Shape(&font, direction)->CopyRange(0u, range_a.length(), result);
  shaper_b.Shape(&font2, direction)->CopyRange(0u, range_b.length(), result);
  shaper_c.Shape(&font, direction)->CopyRange(0u, range_c.length(), result);
  shaper_d.Shape(&font2, direction)->CopyRange(0u, range_d.length(), result);

  ShapeResultView* result_view = ShapeResultView::Create(result);
  TextFragmentPaintInfo text_info{StringView(string), 1, string.length(),
                                  result_view};
  ShapeResultBloberizer::FillGlyphsNG bloberizer_ng(
      font.GetFontDescription(), text_info.text, text_info.from, text_info.to,
      text_info.shape_result, ShapeResultBloberizer::Type::kEmitText);

  CheckBlobBuffer(
      bloberizer_ng.Blobs(),
      {{
          // "Testi"
          {static_cast<int>(range_a.length() - 1),
           string.Substring(range_a.from + 1, range_a.length() - 1).Utf8(),
           ExpectedRun::ClusterDirection::kAscending},
          // "ng"
          {static_cast<int>(range_b.length()),
           string.Substring(range_b.from, range_b.length()).Utf8(),
           ExpectedRun::ClusterDirection::kAscending},
          // " ShapeResultIterator::Cop"
          {static_cast<int>(range_c.length()),
           string.Substring(range_c.from, range_c.length()).Utf8(),
           ExpectedRun::ClusterDirection::kAscending},
          // "yRange"
          {static_cast<int>(range_d.length()),
           string.Substring(range_d.from, range_d.length()).Utf8(),
           ExpectedRun::ClusterDirection::kAscending},
      }});
}

TEST_F(ShapeResultBloberizerTest, SupplementaryMultiRunNG) {
  TextDirection direction = TextDirection::kLtr;
  // 𠜎𠜱𠝹𠱓𠱸𠲖𠳏𠳕
  const UChar kStrSupp[] = {0xD841, 0xDF0E, 0xD841, 0xDF31, 0xD841, 0xDF79,
                            0xD843, 0xDC53, 0xD843, 0xDC78, 0xD843, 0xDC96,
                            0xD843, 0xDCCF, 0xD843, 0xDCD5};
  String string(kStrSupp, base::make_span(kStrSupp).size());

  ExpectedRange range_a{0, 6};
  ExpectedRange range_b{6, 12};
  ExpectedRange range_c{12, 16};
  HarfBuzzShaper shaper_a(string.Substring(range_a.from, range_a.to));
  HarfBuzzShaper shaper_b(string.Substring(range_b.from, range_b.to));
  HarfBuzzShaper shaper_c(string.Substring(range_c.from, range_c.to));

  Font font = blink::test::CreateTestFont(
      AtomicString("NotoSansCJK"),
      blink::test::BlinkRootDir() +
          "/web_tests/third_party/NotoSansCJK/NotoSansCJKjp-Regular-subset.otf",
      12);
  Font font2 = blink::test::CreateTestFont(
      AtomicString("NotoSansCJK"),
      blink::test::BlinkRootDir() +
          "/web_tests/third_party/NotoSansCJK/NotoSansCJKjp-Regular-subset.otf",
      20);

  // Combine four separate results into a single one to ensure we have a result
  // with multiple runs. Interleave fonts to ensure run changes.
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(&font, 0, 0, direction);
  shaper_a.Shape(&font, direction)->CopyRange(0u, range_a.length(), result);
  shaper_b.Shape(&font2, direction)->CopyRange(0u, range_b.length(), result);
  shaper_c.Shape(&font, direction)->CopyRange(0u, range_c.length(), result);

  ShapeResultView* result_view = ShapeResultView::Create(result);
  TextFragmentPaintInfo text_info{StringView(string), 0, string.length(),
                                  result_view};
  ShapeResultBloberizer::FillGlyphsNG bloberizer_ng(
      font.GetFontDescription(), text_info.text, text_info.from, text_info.to,
      text_info.shape_result, ShapeResultBloberizer::Type::kEmitText);

  CheckBlobBuffer(bloberizer_ng.Blobs(),
                  {{
                      // "𠜎𠜱𠝹"
                      {static_cast<int>(range_a.length() / 2),
                       string.Substring(range_a.from, range_a.length()).Utf8(),
                       ExpectedRun::ClusterDirection::kAscending},
                      // "𠱓𠱸𠲖"
                      {static_cast<int>(range_b.length() / 2),
                       string.Substring(range_b.from, range_b.length()).Utf8(),
                       ExpectedRun::ClusterDirection::kAscending},
                      // "𠳏𠳕"
                      {static_cast<int>(range_c.length() / 2),
                       string.Substring(range_c.from, range_c.length()).Utf8(),
                       ExpectedRun::ClusterDirection::kAscending},
                  }});
}

// Tests that runs with zero glyphs (the ZWJ non-printable character in this
// case) are handled correctly. This test passes if it does not cause a crash.
TEST_F(ShapeResultBloberizerTest, SubRunWithZeroGlyphs) {
  // "Foo &zwnj; bar"
  const UChar kStr[] = {0x46, 0x6F, 0x6F, 0x20, 0x200C, 0x20, 0x62, 0x61, 0x71};
  TextRun text_run(kStr, base::make_span(kStr).size());

  Font font(font_description);
  CachingWordShaper shaper(font);
  gfx::RectF glyph_bounds;
  ASSERT_GT(shaper.Width(text_run, &glyph_bounds), 0);

  TextRunPaintInfo run_info(text_run);
  run_info.to = 8;

  CachingWordShaper word_shaper(font);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  ShapeResultBloberizer::FillGlyphs bloberizer(
      font.GetFontDescription(), run_info, buffer,
      ShapeResultBloberizer::Type::kEmitText);

  shaper.GetCharacterRange(text_run, 0, 8);
}

}  // namespace blink
