// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"

#include <hb.h>
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

ShapeResultBloberizer::ShapeResultBloberizer(const Font& font,
                                             float device_scale_factor,
                                             Type type)
    : font_(font), device_scale_factor_(device_scale_factor), type_(type) {}

bool ShapeResultBloberizer::HasPendingVerticalOffsets() const {
  // We exclusively store either horizontal/x-only ofssets -- in which case
  // m_offsets.size == size, or vertical/xy offsets -- in which case
  // m_offsets.size == size * 2.
  DCHECK(pending_glyphs_.size() == pending_offsets_.size() ||
         pending_glyphs_.size() * 2 == pending_offsets_.size());
  return pending_glyphs_.size() != pending_offsets_.size();
}

void ShapeResultBloberizer::CommitPendingRun() {
  if (pending_glyphs_.IsEmpty())
    return;

  if (pending_canvas_rotation_ != builder_rotation_) {
    // The pending run rotation doesn't match the current blob; start a new
    // blob.
    CommitPendingBlob();
    builder_rotation_ = pending_canvas_rotation_;
  }

  SkFont run_font;
  pending_font_data_->PlatformData().SetupSkFont(&run_font,
                                                 device_scale_factor_, &font_);

  const auto run_size = pending_glyphs_.size();
  const auto& buffer = HasPendingVerticalOffsets()
                           ? builder_.allocRunPos(run_font, run_size)
                           : builder_.allocRunPosH(run_font, run_size, 0);

  std::copy(pending_glyphs_.begin(), pending_glyphs_.end(), buffer.glyphs);
  std::copy(pending_offsets_.begin(), pending_offsets_.end(), buffer.pos);

  builder_run_count_ += 1;
  pending_glyphs_.Shrink(0);
  pending_offsets_.Shrink(0);
}

void ShapeResultBloberizer::CommitPendingBlob() {
  if (!builder_run_count_)
    return;

  blobs_.emplace_back(builder_.make(), builder_rotation_);
  builder_run_count_ = 0;
}

const ShapeResultBloberizer::BlobBuffer& ShapeResultBloberizer::Blobs() {
  CommitPendingRun();
  CommitPendingBlob();
  DCHECK(pending_glyphs_.IsEmpty());
  DCHECK_EQ(builder_run_count_, 0u);

  return blobs_;
}

namespace {

inline bool IsSkipInkException(const ShapeResultBloberizer& bloberizer,
                               const StringView& text,
                               unsigned character_index) {
  // We want to skip descenders in general, but it is undesirable renderings for
  // CJK characters.
  return bloberizer.GetType() == ShapeResultBloberizer::Type::kTextIntercepts &&
         !Character::CanTextDecorationSkipInk(
             text.CodepointAt(character_index));
}

inline void AddEmphasisMark(ShapeResultBloberizer& bloberizer,
                            const GlyphData& emphasis_data,
                            CanvasRotationInVertical canvas_rotation,
                            FloatPoint glyph_center,
                            float mid_glyph_offset) {
  const SimpleFontData* emphasis_font_data = emphasis_data.font_data;
  DCHECK(emphasis_font_data);

  bool is_vertical =
      emphasis_font_data->PlatformData().IsVerticalAnyUpright() &&
      IsCanvasRotationInVerticalUpright(emphasis_data.canvas_rotation);

  if (!is_vertical) {
    bloberizer.Add(emphasis_data.glyph, emphasis_font_data,
                   CanvasRotationInVertical::kRegular,
                   mid_glyph_offset - glyph_center.X());
  } else {
    bloberizer.Add(
        emphasis_data.glyph, emphasis_font_data, emphasis_data.canvas_rotation,
        FloatPoint(-glyph_center.X(), mid_glyph_offset - glyph_center.Y()));
  }
}

class GlyphCallbackContext {
  STACK_ALLOCATED();

 public:
  ShapeResultBloberizer* bloberizer;
  const StringView& text;

 private:
  DISALLOW_COPY_AND_ASSIGN(GlyphCallbackContext);
};

void AddGlyphToBloberizer(void* context,
                          unsigned character_index,
                          Glyph glyph,
                          FloatSize glyph_offset,
                          float advance,
                          bool is_horizontal,
                          CanvasRotationInVertical rotation,
                          const SimpleFontData* font_data) {
  GlyphCallbackContext* parsed_context =
      static_cast<GlyphCallbackContext*>(context);
  ShapeResultBloberizer* bloberizer = parsed_context->bloberizer;
  const StringView& text = parsed_context->text;

  if (IsSkipInkException(*bloberizer, text, character_index))
    return;
  FloatPoint start_offset =
      is_horizontal ? FloatPoint(advance, 0) : FloatPoint(0, advance);
  bloberizer->Add(glyph, font_data, rotation, start_offset + glyph_offset);
}

void AddFastHorizontalGlyphToBloberizer(
    void* context,
    unsigned,
    Glyph glyph,
    FloatSize glyph_offset,
    float advance,
    bool is_horizontal,
    CanvasRotationInVertical canvas_rotation,
    const SimpleFontData* font_data) {
  ShapeResultBloberizer* bloberizer =
      static_cast<ShapeResultBloberizer*>(context);
  DCHECK(!glyph_offset.Height());
  DCHECK(is_horizontal);
  bloberizer->Add(glyph, font_data, canvas_rotation,
                  advance + glyph_offset.Width());
}

float FillGlyphsForResult(ShapeResultBloberizer* bloberizer,
                          const ShapeResult* result,
                          const StringView& text,
                          unsigned from,
                          unsigned to,
                          float initial_advance,
                          unsigned run_offset) {
  GlyphCallbackContext context = {bloberizer, text};
  return result->ForEachGlyph(initial_advance, from, to, run_offset,
                              AddGlyphToBloberizer,
                              static_cast<void*>(&context));
}

class ClusterCallbackContext {
  STACK_ALLOCATED();

 public:
  ShapeResultBloberizer* bloberizer;
  const StringView& text;
  const GlyphData& emphasis_data;
  FloatPoint glyph_center;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClusterCallbackContext);
};

void AddEmphasisMarkToBloberizer(void* context,
                                 unsigned character_index,
                                 float advance_so_far,
                                 unsigned graphemes_in_cluster,
                                 float cluster_advance,
                                 CanvasRotationInVertical canvas_rotation) {
  ClusterCallbackContext* parsed_context =
      static_cast<ClusterCallbackContext*>(context);
  ShapeResultBloberizer* bloberizer = parsed_context->bloberizer;
  const StringView& text = parsed_context->text;
  const GlyphData& emphasis_data = parsed_context->emphasis_data;
  FloatPoint glyph_center = parsed_context->glyph_center;

  if (text.Is8Bit()) {
    if (Character::CanReceiveTextEmphasis(text[character_index])) {
      AddEmphasisMark(*bloberizer, emphasis_data, canvas_rotation, glyph_center,
                      advance_so_far + cluster_advance / 2);
    }
  } else {
    float glyph_advance_x = cluster_advance / graphemes_in_cluster;
    for (unsigned j = 0; j < graphemes_in_cluster; ++j) {
      // Do not put emphasis marks on space, separator, and control
      // characters.
      if (Character::CanReceiveTextEmphasis(text[character_index])) {
        AddEmphasisMark(*bloberizer, emphasis_data, canvas_rotation,
                        glyph_center, advance_so_far + glyph_advance_x / 2);
      }
      advance_so_far += glyph_advance_x;
    }
  }
}

}  // namespace

float ShapeResultBloberizer::FillGlyphs(
    const TextRunPaintInfo& run_info,
    const ShapeResultBuffer& result_buffer) {
  if (CanUseFastPath(run_info.from, run_info.to, run_info.run.length(),
                     result_buffer.HasVerticalOffsets())) {
    return FillFastHorizontalGlyphs(result_buffer, run_info.run.Direction());
  }

  float advance = 0;
  auto results = result_buffer.results_;

  if (run_info.run.Rtl()) {
    unsigned word_offset = run_info.run.length();
    for (unsigned j = 0; j < results.size(); j++) {
      unsigned resolved_index = results.size() - 1 - j;
      const scoped_refptr<const ShapeResult>& word_result =
          results[resolved_index];
      word_offset -= word_result->NumCharacters();
      advance = FillGlyphsForResult(this, word_result.get(),
                                    run_info.run.ToStringView(), run_info.from,
                                    run_info.to, advance, word_offset);
    }
  } else {
    unsigned word_offset = 0;
    for (const auto& word_result : results) {
      advance = FillGlyphsForResult(this, word_result.get(),
                                    run_info.run.ToStringView(), run_info.from,
                                    run_info.to, advance, word_offset);
      word_offset += word_result->NumCharacters();
    }
  }

  return advance;
}

float ShapeResultBloberizer::FillGlyphs(const StringView& text,
                                        unsigned from,
                                        unsigned to,
                                        const ShapeResultView* result) {
  DCHECK(result);
  DCHECK(to <= text.length());
  float initial_advance = 0;
  if (CanUseFastPath(from, to, result)) {
    DCHECK(!result->HasVerticalOffsets());
    DCHECK_NE(GetType(), ShapeResultBloberizer::Type::kTextIntercepts);
    return result->ForEachGlyph(initial_advance,
                                &AddFastHorizontalGlyphToBloberizer,
                                static_cast<void*>(this));
  }

  float run_offset = 0;
  GlyphCallbackContext context = {this, text};
  return result->ForEachGlyph(initial_advance, from, to, run_offset,
                              AddGlyphToBloberizer,
                              static_cast<void*>(&context));
}

void ShapeResultBloberizer::FillTextEmphasisGlyphs(
    const TextRunPaintInfo& run_info,
    const GlyphData& emphasis,
    const ShapeResultBuffer& result_buffer) {
  FloatPoint glyph_center =
      emphasis.font_data->BoundsForGlyph(emphasis.glyph).Center();

  float advance = 0;
  auto results = result_buffer.results_;

  if (run_info.run.Rtl()) {
    unsigned word_offset = run_info.run.length();
    for (unsigned j = 0; j < results.size(); j++) {
      unsigned resolved_index = results.size() - 1 - j;
      const scoped_refptr<const ShapeResult>& word_result =
          results[resolved_index];
      word_offset -= word_result->NumCharacters();
      StringView text = run_info.run.ToStringView();
      ClusterCallbackContext context = {this, text, emphasis, glyph_center};
      advance = word_result->ForEachGraphemeClusters(
          text, advance, run_info.from, run_info.to, word_offset,
          AddEmphasisMarkToBloberizer, static_cast<void*>(&context));
    }
  } else {  // Left-to-right.
    unsigned word_offset = 0;
    for (const auto& word_result : results) {
      StringView text = run_info.run.ToStringView();
      ClusterCallbackContext context = {this, text, emphasis, glyph_center};
      advance = word_result->ForEachGraphemeClusters(
          text, advance, run_info.from, run_info.to, word_offset,
          AddEmphasisMarkToBloberizer, static_cast<void*>(&context));
      word_offset += word_result->NumCharacters();
    }
  }
}

void ShapeResultBloberizer::FillTextEmphasisGlyphs(
    const StringView& text,
    unsigned from,
    unsigned to,
    const GlyphData& emphasis,
    const ShapeResultView* result) {
  FloatPoint glyph_center =
      emphasis.font_data->BoundsForGlyph(emphasis.glyph).Center();
  ClusterCallbackContext context = {this, text, emphasis, glyph_center};
  float initial_advance = 0;
  unsigned index_offset = 0;
  result->ForEachGraphemeClusters(text, initial_advance, from, to, index_offset,
                                  AddEmphasisMarkToBloberizer,
                                  static_cast<void*>(&context));
}

bool ShapeResultBloberizer::CanUseFastPath(unsigned from,
                                           unsigned to,
                                           unsigned length,
                                           bool has_vertical_offsets) {
  return !from && to == length && !has_vertical_offsets &&
         GetType() != ShapeResultBloberizer::Type::kTextIntercepts;
}

bool ShapeResultBloberizer::CanUseFastPath(
    unsigned from,
    unsigned to,
    const ShapeResultView* shape_result) {
  return from <= shape_result->StartIndex() && to >= shape_result->EndIndex() &&
         !shape_result->HasVerticalOffsets() &&
         GetType() != ShapeResultBloberizer::Type::kTextIntercepts;
}

float ShapeResultBloberizer::FillFastHorizontalGlyphs(
    const ShapeResultBuffer& result_buffer,
    TextDirection text_direction) {
  DCHECK(!result_buffer.HasVerticalOffsets());
  DCHECK_NE(GetType(), ShapeResultBloberizer::Type::kTextIntercepts);

  float advance = 0;
  auto results = result_buffer.results_;

  for (unsigned i = 0; i < results.size(); ++i) {
    const auto& word_result =
        IsLtr(text_direction) ? results[i] : results[results.size() - 1 - i];
    advance = FillFastHorizontalGlyphs(word_result.get(), advance);
  }

  return advance;
}

float ShapeResultBloberizer::FillFastHorizontalGlyphs(const ShapeResult* result,
                                                      float initial_advance) {
  DCHECK(!result->HasVerticalOffsets());
  DCHECK_NE(GetType(), ShapeResultBloberizer::Type::kTextIntercepts);

  return result->ForEachGlyph(initial_advance,
                              &AddFastHorizontalGlyphToBloberizer,
                              static_cast<void*>(this));
}

}  // namespace blink
