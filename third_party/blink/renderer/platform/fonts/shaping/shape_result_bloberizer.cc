// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"

#include <hb.h>

#include <algorithm>

#include "base/logging.h"
#include "base/strings/to_string.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

namespace {

// A helper for FillGlyphsSlow().
inline const ShapeResult* GetShapeResult(const PlainTextItem& item) {
  return item.GetShapeResult();
}

}  // namespace

ShapeResultBloberizer::ShapeResultBloberizer(
    const FontDescription& font_description,
    Type type)
    : font_description_(font_description), type_(type) {}

bool ShapeResultBloberizer::HasPendingVerticalOffsets() const {
  // We exclusively store either horizontal/x-only offsets -- in which case
  // m_offsets.size == size, or vertical/xy offsets -- in which case
  // m_offsets.size == size * 2.
  DCHECK(pending_glyphs_.size() == pending_offsets_.size() ||
         pending_glyphs_.size() * 2 == pending_offsets_.size());
  return pending_glyphs_.size() != pending_offsets_.size();
}

void ShapeResultBloberizer::SetText(const StringView& text,
                                    unsigned from,
                                    unsigned to,
                                    base::span<const unsigned> cluster_starts) {
  if (current_text_.IsNull())
    CommitPendingRun();

  // Any outstanding 'current' state should have been moved to 'pending'.
  DCHECK(current_character_indexes_.empty());

  DVLOG(4) << "   SetText from: " << from << " to: " << to;

  // cluster_ends_ must be at least the size of the source run length,
  // but the run length may be negative (in which case no glyphs will be added).
  if (from < to) {
    DVLOG(4) << "   SetText text: "
             << StringView(text, from, to - from).ToString();
    cluster_ends_.resize(to - from);
    for (size_t i = 0; i < cluster_starts.size() - 1; ++i) {
      cluster_ends_[cluster_starts[i] - from] = cluster_starts[i + 1];
    }
  } else {
    cluster_ends_.Shrink(0);
  }

  DVLOG(4) << "   Cluster ends: " << base::ToString(base::span(cluster_ends_));

  cluster_ends_offset_ = from;
  current_text_ = text;
}

void ShapeResultBloberizer::CommitText() {
  if (current_character_indexes_.empty())
    return;

  unsigned from = current_character_indexes_[0];
  unsigned to = current_character_indexes_[0];
  for (unsigned character_index : current_character_indexes_) {
    unsigned character_index_end =
        cluster_ends_[character_index - cluster_ends_offset_];
    from = std::min(from, character_index);
    to = std::max(to, character_index_end);
  }

  DCHECK(!current_text_.IsNull());

  DVLOG(4) << "   CommitText from: " << from << " to: " << to;
  DVLOG(4) << "   CommitText glyphs: "
           << base::ToString(base::span(pending_glyphs_)
                                 .last(current_character_indexes_.size()));
  DVLOG(4) << "   CommitText cluster starts: "
           << base::ToString(base::span(current_character_indexes_));

  wtf_size_t pending_utf8_original_size = pending_utf8_.size();
  wtf_size_t pending_utf8_character_indexes_original_size =
      pending_utf8_character_indexes_.size();

  // Do the UTF-8 conversion here.
  // For each input code point track the location of output UTF-8 code point.

  unsigned current_text_length = current_text_.length();
  DCHECK_LE(to, current_text_length);

  unsigned size = to - from;
  Vector<uint32_t, 256> pending_utf8_character_index_from_character_index(size);
  if (current_text_.Is8Bit()) {
    const LChar* latin1 = current_text_.Span8().data();
    wtf_size_t utf8_size = pending_utf8_.size();
    for (unsigned i = from; i < to;) {
      pending_utf8_character_index_from_character_index[i - from] = utf8_size;

      LChar cp = UNSAFE_TODO(latin1[i++]);
      pending_utf8_.Grow(utf8_size + U8_LENGTH(cp));
      UNSAFE_TODO(U8_APPEND_UNSAFE(pending_utf8_.begin(), utf8_size, cp));
    }
  } else {
    const UChar* utf16 = current_text_.Span16().data();
    wtf_size_t utf8_size = pending_utf8_.size();
    for (unsigned i = from; i < to;) {
      pending_utf8_character_index_from_character_index[i - from] = utf8_size;

      UChar32 cp;
      UNSAFE_TODO(U16_NEXT_OR_FFFD(utf16, i, current_text_length, cp));
      pending_utf8_.Grow(utf8_size + U8_LENGTH(cp));
      UNSAFE_TODO(U8_APPEND_UNSAFE(pending_utf8_.begin(), utf8_size, cp));
    }
  }

  for (unsigned character_index : current_character_indexes_) {
    unsigned index = character_index - from;
    pending_utf8_character_indexes_.push_back(
        pending_utf8_character_index_from_character_index[index]);
  }

  current_character_indexes_.Shrink(0);

  DVLOG(4) << "  CommitText appended UTF-8: \""
           << std::string(
                  &pending_utf8_[pending_utf8_original_size],
                  UNSAFE_TODO(pending_utf8_.data() + pending_utf8_.size()))
           << "\"";
  DVLOG(4) << "  CommitText UTF-8 indexes: "
           << base::ToString(
                  base::span(pending_utf8_character_indexes_)
                      .subspan(pending_utf8_character_indexes_original_size));
}

void ShapeResultBloberizer::CommitPendingRun() {
  if (pending_glyphs_.empty())
    return;

  if (pending_canvas_rotation_ != builder_rotation_) {
    // The pending run rotation doesn't match the current blob; start a new
    // blob.
    CommitPendingBlob();
    builder_rotation_ = pending_canvas_rotation_;
  }

  if (!current_character_indexes_.empty()) [[unlikely]] {
    CommitText();
  }

  SkFont run_font =
      pending_font_data_->PlatformData().CreateSkFont(&font_description_);

  const auto run_size = pending_glyphs_.size();
  const auto text_size = pending_utf8_.size();
  const auto& buffer = [&]() {
    if (HasPendingVerticalOffsets()) {
      if (text_size)
        return builder_.allocRunTextPos(run_font, run_size, text_size);
      else
        return builder_.allocRunPos(run_font, run_size);
    } else {
      if (text_size)
        return builder_.allocRunTextPosH(run_font, run_size, 0, text_size);
      else
        return builder_.allocRunPosH(run_font, run_size, 0);
    }
  }();
  builder_run_count_ += 1;

  if (text_size) {
    DVLOG(4) << "  CommitPendingRun text: \""
             << std::string(pending_utf8_.begin(), pending_utf8_.end()) << "\"";
    DVLOG(4) << "  CommitPendingRun glyphs: "
             << base::ToString(base::span(pending_glyphs_));
    DVLOG(4) << "  CommitPendingRun indexes: "
             << base::ToString(base::span(pending_utf8_character_indexes_));
    DCHECK_EQ(pending_utf8_character_indexes_.size(), run_size);
    std::ranges::copy(pending_utf8_character_indexes_, buffer.clusters);
    std::ranges::copy(pending_utf8_, buffer.utf8text);

    pending_utf8_.Shrink(0);
    pending_utf8_character_indexes_.Shrink(0);
  }

  std::ranges::copy(pending_glyphs_, buffer.glyphs);
  std::ranges::copy(pending_offsets_, buffer.pos);
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
  DCHECK(pending_glyphs_.empty());
  DCHECK_EQ(builder_run_count_, 0u);

  return blobs_;
}

inline bool ShapeResultBloberizer::IsSkipInkException(
    const StringView& text,
    unsigned character_index) {
  // We want to skip descenders in general, but it is undesirable renderings for
  // CJK characters.
  return type_ == ShapeResultBloberizer::Type::kTextIntercepts &&
         !Character::CanTextDecorationSkipInk(
             text.CodepointAt(character_index));
}

inline void ShapeResultBloberizer::AddEmphasisMark(
    const GlyphData& emphasis_data,
    CanvasRotationInVertical canvas_rotation,
    gfx::PointF glyph_center,
    float mid_glyph_offset,
    float letter_spacing) {
  const SimpleFontData* emphasis_font_data = emphasis_data.font_data;
  DCHECK(emphasis_font_data);

  bool is_vertical =
      emphasis_font_data->PlatformData().IsVerticalAnyUpright() &&
      IsCanvasRotationInVerticalUpright(emphasis_data.canvas_rotation);

  if (!is_vertical) {
    if (RuntimeEnabledFeatures::TextEmphasisLetterSpacingEnabled()) {
      Add(emphasis_data.glyph, emphasis_font_data,
          CanvasRotationInVertical::kRegular,
          mid_glyph_offset - glyph_center.x() - letter_spacing / 2, 0);
    } else {
      Add(emphasis_data.glyph, emphasis_font_data,
          CanvasRotationInVertical::kRegular,
          mid_glyph_offset - glyph_center.x(), 0);
    }
  } else {
    Add(emphasis_data.glyph, emphasis_font_data, emphasis_data.canvas_rotation,
        gfx::Vector2dF(-glyph_center.x(), mid_glyph_offset - glyph_center.y()),
        0);
  }
}

namespace {
class GlyphCallbackContext {
  STACK_ALLOCATED();

 public:
  GlyphCallbackContext(ShapeResultBloberizer* bloberizer,
                       const StringView& text)
      : bloberizer(bloberizer), text(text) {}
  GlyphCallbackContext(const GlyphCallbackContext&) = delete;
  GlyphCallbackContext& operator=(const GlyphCallbackContext&) = delete;

  ShapeResultBloberizer* bloberizer;
  const StringView& text;
};
}  // namespace

/*static*/ void ShapeResultBloberizer::AddGlyphToBloberizer(
    void* context,
    unsigned character_index,
    Glyph glyph,
    gfx::Vector2dF glyph_offset,
    float advance,
    bool is_horizontal,
    CanvasRotationInVertical rotation,
    const SimpleFontData* font_data) {
  GlyphCallbackContext* parsed_context =
      static_cast<GlyphCallbackContext*>(context);
  ShapeResultBloberizer* bloberizer = parsed_context->bloberizer;
  const StringView& text = parsed_context->text;

  if (bloberizer->IsSkipInkException(text, character_index))
    return;
  gfx::Vector2dF start_offset =
      is_horizontal ? gfx::Vector2dF(advance, 0) : gfx::Vector2dF(0, advance);
  bloberizer->Add(glyph, font_data, rotation, start_offset + glyph_offset,
                  character_index);
}

/*static*/ void ShapeResultBloberizer::AddFastHorizontalGlyphToBloberizer(
    void* context,
    unsigned character_index,
    Glyph glyph,
    gfx::Vector2dF glyph_offset,
    float advance,
    bool is_horizontal,
    CanvasRotationInVertical canvas_rotation,
    const SimpleFontData* font_data) {
  ShapeResultBloberizer* bloberizer =
      static_cast<ShapeResultBloberizer*>(context);
  DCHECK(!glyph_offset.y());
  DCHECK(is_horizontal);
  bloberizer->Add(glyph, font_data, canvas_rotation, advance + glyph_offset.x(),
                  character_index);
}

float ShapeResultBloberizer::FillGlyphsForResult(const ShapeResult* result,
                                                 const StringView& text,
                                                 unsigned from,
                                                 unsigned to,
                                                 float initial_advance,
                                                 unsigned run_offset) {
  GlyphCallbackContext context = {this, text};
  return result->ForEachGlyph(initial_advance, from, to, run_offset,
                              AddGlyphToBloberizer,
                              static_cast<void*>(&context));
}

namespace {
class ClusterCallbackContext {
  STACK_ALLOCATED();

 public:
  ClusterCallbackContext(ShapeResultBloberizer* bloberizer,
                         const StringView& text,
                         const GlyphData& emphasis_data,
                         gfx::PointF glyph_center,
                         float letter_spacing)
      : bloberizer(bloberizer),
        text(text),
        emphasis_data(emphasis_data),
        glyph_center(std::move(glyph_center)),
        letter_spacing(letter_spacing) {}
  ClusterCallbackContext(const ClusterCallbackContext&) = delete;
  ClusterCallbackContext& operator=(const ClusterCallbackContext&) = delete;

  ShapeResultBloberizer* bloberizer;
  const StringView& text;
  const GlyphData& emphasis_data;
  gfx::PointF glyph_center;
  float letter_spacing;
};
}  // namespace

/*static*/ void ShapeResultBloberizer::AddEmphasisMarkToBloberizer(
    void* context,
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
  gfx::PointF glyph_center = parsed_context->glyph_center;

  if (text.Is8Bit()) {
    if (Character::CanReceiveTextEmphasis(text[character_index])) {
      bloberizer->AddEmphasisMark(emphasis_data, canvas_rotation, glyph_center,
                                  advance_so_far + cluster_advance / 2,
                                  parsed_context->letter_spacing);
    }
  } else {
    float glyph_advance_x = cluster_advance / graphemes_in_cluster;
    for (unsigned j = 0; j < graphemes_in_cluster; ++j) {
      // Do not put emphasis marks on space, separator, and control
      // characters.
      if (Character::CanReceiveTextEmphasis(
              text.CodepointAt(character_index))) {
        bloberizer->AddEmphasisMark(emphasis_data, canvas_rotation,
                                    glyph_center,
                                    advance_so_far + glyph_advance_x / 2,
                                    parsed_context->letter_spacing);
      }
      advance_so_far += glyph_advance_x;
    }
  }
}

namespace {
class ClusterStarts {
  STACK_ALLOCATED();

 public:
  ClusterStarts() = default;
  ClusterStarts(const ClusterStarts&) = delete;
  ClusterStarts& operator=(const ClusterStarts&) = delete;

  static void Accumulate(void* context,
                         unsigned character_index,
                         Glyph,
                         gfx::Vector2dF,
                         float,
                         bool,
                         CanvasRotationInVertical,
                         const SimpleFontData*) {
    ClusterStarts* self = static_cast<ClusterStarts*>(context);

    if (self->cluster_starts_.empty() ||
        self->last_seen_character_index_ != character_index) {
      self->cluster_starts_.push_back(character_index);
      self->last_seen_character_index_ = character_index;
    }
  }

  void Finish(unsigned from, unsigned to) {
    std::sort(cluster_starts_.begin(), cluster_starts_.end());
    DCHECK_EQ(std::ranges::adjacent_find(cluster_starts_),
              cluster_starts_.end());
    DVLOG(4) << "  Cluster starts: "
             << base::ToString(base::span(cluster_starts_));
    if (!cluster_starts_.empty()) {
      // 'from' may point inside a cluster; the least seen index may be larger.
      DCHECK_LE(from, *cluster_starts_.begin());
      DCHECK_LT(*(UNSAFE_TODO(cluster_starts_.end() - 1)), to);
    }
    cluster_starts_.push_back(to);
  }

  base::span<const unsigned> Data() { return cluster_starts_; }

 private:
  Vector<unsigned, 256> cluster_starts_;
  unsigned last_seen_character_index_ = 0;
};
}  // namespace

ShapeResultBloberizer::FillGlyphs::FillGlyphs(
    const FontDescription& font_description,
    const PlainTextNode& node,
    const Type type)
    : ShapeResultBloberizer(font_description, type) {
  DCHECK(!node.ContainsRtlItems());
  const unsigned to = node.TextContent().length();
  if (CanUseFastPath(0, to, to, node.HasVerticalOffsets())) {
    DVLOG(4) << "FillGlyphs fast path";
    DCHECK_NE(type_, ShapeResultBloberizer::Type::kTextIntercepts);
    DCHECK_NE(type_, ShapeResultBloberizer::Type::kEmitText);
    float advance = 0;
    for (const auto& item : node.ItemList()) {
      advance = FillFastHorizontalGlyphs(item.GetShapeResult(), advance);
    }
    advance_ = advance;
    return;
  }

  FillGlyphsSlow(node.TextContent(), node.BaseDirection(), node.ItemList(), 0,
                 to);
}

template <typename ShapeList>
void ShapeResultBloberizer::FillGlyphs::FillGlyphsSlow(StringView text,
                                                       TextDirection direction,
                                                       const ShapeList& list,
                                                       unsigned from,
                                                       unsigned to) {
  if (type_ == Type::kEmitText) [[unlikely]] {
    unsigned word_offset = 0;
    ClusterStarts cluster_starts;
    for (const auto& item : list) {
      const ShapeResult* word_result = GetShapeResult(item);
      word_result->ForEachGlyph(0, from, to, word_offset,
                                ClusterStarts::Accumulate,
                                static_cast<void*>(&cluster_starts));
      word_offset += word_result->NumCharacters();
    }
    cluster_starts.Finish(from, to);
    SetText(text, from, to, cluster_starts.Data());
  }

  float advance = 0;
  if (IsRtl(direction)) {
    unsigned word_offset = text.length();
    for (const auto& item : base::Reversed(list)) {
      const ShapeResult* word_result = GetShapeResult(item);
      unsigned word_characters = word_result->NumCharacters();
      word_offset -= word_characters;
      DVLOG(4) << " FillGlyphs RTL run from: " << from << " to: " << to
               << " offset: " << word_offset << " length: " << word_characters;
      advance = FillGlyphsForResult(word_result, text, from, to, advance,
                                    word_offset);
    }
  } else {
    unsigned word_offset = 0;
    for (const auto& item : list) {
      const ShapeResult* word_result = GetShapeResult(item);
      unsigned word_characters = word_result->NumCharacters();
      DVLOG(4) << " FillGlyphs LTR run from: " << from << " to: " << to
               << " offset: " << word_offset << " length: " << word_characters;
      advance = FillGlyphsForResult(word_result, text, from, to, advance,
                                    word_offset);
      word_offset += word_characters;
    }
  }

  if (type_ == Type::kEmitText) [[unlikely]] {
    CommitText();
  }

  advance_ = advance;
}

ShapeResultBloberizer::FillGlyphsNG::FillGlyphsNG(
    const FontDescription& font_description,
    const StringView& text,
    unsigned from,
    unsigned to,
    const ShapeResultView* result,
    const Type type)
    : ShapeResultBloberizer(font_description, type) {
  DCHECK(result);
  DCHECK(to <= text.length());
  float initial_advance = 0;
  if (CanUseFastPath(from, to, result)) {
    DVLOG(4) << "FillGlyphsNG fast path";
    DCHECK(!result->HasVerticalOffsets());
    DCHECK_NE(type_, ShapeResultBloberizer::Type::kTextIntercepts);
    DCHECK_NE(type_, ShapeResultBloberizer::Type::kEmitText);
    advance_ = result->ForEachGlyph(initial_advance,
                                    &AddFastHorizontalGlyphToBloberizer,
                                    static_cast<void*>(this));
    return;
  }

  DVLOG(4) << "FillGlyphsNG slow path";
  unsigned run_offset = 0;
  if (type_ == Type::kEmitText) [[unlikely]] {
    ClusterStarts cluster_starts;
    result->ForEachGlyph(initial_advance, from, to, run_offset,
                         ClusterStarts::Accumulate,
                         static_cast<void*>(&cluster_starts));
    cluster_starts.Finish(from, to);
    SetText(text, from, to, cluster_starts.Data());
  }

  GlyphCallbackContext context = {this, text};
  advance_ =
      result->ForEachGlyph(initial_advance, from, to, run_offset,
                           AddGlyphToBloberizer, static_cast<void*>(&context));

  if (type_ == Type::kEmitText) [[unlikely]] {
    CommitText();
  }
}

ShapeResultBloberizer::FillTextEmphasisGlyphsNG::FillTextEmphasisGlyphsNG(
    const FontDescription& font_description,
    const StringView& text,
    unsigned from,
    unsigned to,
    const ShapeResultView* result,
    const GlyphData& emphasis)
    : ShapeResultBloberizer(font_description, Type::kNormal) {
  gfx::PointF glyph_center =
      emphasis.font_data->BoundsForGlyph(emphasis.glyph).CenterPoint();
  ClusterCallbackContext context = {this, text, emphasis, glyph_center,
                                    font_description.LetterSpacing()};
  float initial_advance = 0;
  unsigned index_offset = 0;
  advance_ = result->ForEachGraphemeClusters(
      text, initial_advance, from, to, index_offset,
      AddEmphasisMarkToBloberizer, static_cast<void*>(&context));
}

bool ShapeResultBloberizer::CanUseFastPath(unsigned from,
                                           unsigned to,
                                           unsigned length,
                                           bool has_vertical_offsets) {
  return !from && to == length && !has_vertical_offsets &&
         type_ != ShapeResultBloberizer::Type::kTextIntercepts &&
         type_ != ShapeResultBloberizer::Type::kEmitText;
}

bool ShapeResultBloberizer::CanUseFastPath(
    unsigned from,
    unsigned to,
    const ShapeResultView* shape_result) {
  return from <= shape_result->StartIndex() && to >= shape_result->EndIndex() &&
         !shape_result->HasVerticalOffsets() &&
         type_ != ShapeResultBloberizer::Type::kTextIntercepts &&
         type_ != ShapeResultBloberizer::Type::kEmitText;
}

float ShapeResultBloberizer::FillFastHorizontalGlyphs(const ShapeResult* result,
                                                      float initial_advance) {
  DCHECK(!result->HasVerticalOffsets());
  DCHECK_NE(type_, ShapeResultBloberizer::Type::kTextIntercepts);

  return result->ForEachGlyph(initial_advance,
                              &AddFastHorizontalGlyphToBloberizer,
                              static_cast<void*>(this));
}

void DrawTextBlobs(const ShapeResultBloberizer::BlobBuffer& blobs,
                   cc::PaintCanvas& canvas,
                   const gfx::PointF& point,
                   const cc::PaintFlags& flags,
                   cc::NodeId node_id) {
  for (const auto& blob_info : blobs) {
    DCHECK(blob_info.blob);
    cc::PaintCanvasAutoRestore auto_restore(&canvas, false);
    switch (blob_info.rotation) {
      case CanvasRotationInVertical::kRegular:
        break;
      case CanvasRotationInVertical::kRotateCanvasUpright: {
        canvas.save();

        SkMatrix m;
        m.setSinCos(-1, 0, point.x(), point.y());
        canvas.concat(SkM44(m));
        break;
      }
      case CanvasRotationInVertical::kRotateCanvasUprightOblique: {
        canvas.save();

        SkMatrix m;
        m.setSinCos(-1, 0, point.x(), point.y());
        // TODO(yosin): We should use angle specified in CSS instead of
        // constant value -15deg.
        // Note: We draw glyph in right-top corner upper.
        // See CSS "transform: skew(0, -15deg)"
        SkMatrix skew_y;
        constexpr SkScalar kSkewY = -0.2679491924311227;  // tan(-15deg)
        skew_y.setSkew(0, kSkewY, point.x(), point.y());
        m.preConcat(skew_y);
        canvas.concat(SkM44(m));
        break;
      }
      case CanvasRotationInVertical::kOblique: {
        // TODO(yosin): We should use angle specified in CSS instead of
        // constant value 15deg.
        // Note: We draw glyph in right-top corner upper.
        // See CSS "transform: skew(0, -15deg)"
        canvas.save();
        SkMatrix skew_x;
        constexpr SkScalar kSkewX = 0.2679491924311227;  // tan(15deg)
        skew_x.setSkew(kSkewX, 0, point.x(), point.y());
        canvas.concat(SkM44(skew_x));
        break;
      }
    }
    if (node_id != cc::kInvalidNodeId) {
      canvas.drawTextBlob(blob_info.blob, point.x(), point.y(), node_id, flags);
    } else {
      canvas.drawTextBlob(blob_info.blob, point.x(), point.y(), flags);
    }
  }
}

}  // namespace blink
