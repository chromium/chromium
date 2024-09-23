/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"

#include <hb.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include <algorithm>
#include <hb-cplusplus.hh>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_iterator.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/font_variant_emoji.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_caps_support.h"
#include "third_party/blink/renderer/platform/fonts/shaping/case_mapping_harfbuzz_buffer_filler.h"
#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"
#include "third_party/blink/renderer/platform/fonts/shaping/han_kerning.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "third_party/blink/renderer/platform/fonts/small_caps_iterator.h"
#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

namespace {

constexpr hb_feature_t CreateFeature(char c1,
                                     char c2,
                                     char c3,
                                     char c4,
                                     uint32_t value = 0) {
  return {HB_TAG(c1, c2, c3, c4), value, 0 /* start */,
          static_cast<unsigned>(-1) /* end */};
}

#if EXPENSIVE_DCHECKS_ARE_ON()
// Check if the ShapeResult has the specified range.
// |text| and |font| are only for logging.
void CheckShapeResultRange(const ShapeResult* result,
                           unsigned start,
                           unsigned end,
                           const String& text,
                           const Font* font) {
  if (!result) {
    return;
  }
  DCHECK_LE(start, end);
  unsigned length = end - start;
  if (length == result->NumCharacters() &&
      (!length ||
       (start == result->StartIndex() && end == result->EndIndex()))) {
    return;
  }

  // Log font-family/size as specified.
  StringBuilder log;
  log.Append("Font='");
  const FontDescription& font_description = font->GetFontDescription();
  log.Append(font_description.Family().ToString());
  log.AppendFormat("', %f", font_description.ComputedSize());

  // Log the primary font with its family name in the font file.
  const SimpleFontData* font_data = font->PrimaryFont();
  if (font_data) {
    const SkTypeface* typeface = font_data->PlatformData().Typeface();
    SkString family_name;
    typeface->getFamilyName(&family_name);
    log.Append(", primary=");
    log.Append(family_name.c_str());
  }

  // Log the text to shape.
  log.AppendFormat(": %u-%u -> %u-%u:", start, end, result->StartIndex(),
                   result->EndIndex());
  for (unsigned i = start; i < end; ++i) {
    log.AppendFormat(" %02X", text[i]);
  }

  log.Append(", result=");
  result->ToString(&log);

  NOTREACHED_IN_MIGRATION() << log.ToString();
}
#endif

struct TrackEmoji {
  bool is_start;
  unsigned tracked_cluster_index;
  bool cluster_broken;

  unsigned num_broken_clusters;
  unsigned num_clusters;
};

// The algorithm is relying on the following assumption: If an emoji is shaped
// correctly it will present as only one glyph. This definitely holds for
// NotoColorEmoji. So if one sequence (which HarfBuzz groups as a cluster)
// presents as multiple glyphs, it means an emoji is rendered as sequence that
// the font did not understand and did not shape into only one glyph. If it
// renders as only one glyph but that glyph is .notdef/Tofu, it also means it's
// broken.  Due to the way flags work (pairs of regional indicators), broken
// flags cannot be correctly identified with this method - as each regional
// indicator will display as one emoji with Noto Color Emoji.
void IdentifyBrokenEmoji(void* context,
                         unsigned character_index,
                         Glyph glyph,
                         gfx::Vector2dF,
                         float,
                         bool,
                         CanvasRotationInVertical,
                         const SimpleFontData*) {
  DCHECK(context);
  TrackEmoji* track_emoji = reinterpret_cast<TrackEmoji*>(context);

  if (character_index != track_emoji->tracked_cluster_index ||
      track_emoji->is_start) {
    // We have reached the next cluster and can decide for the previous cluster
    // whether it was broken or not.
    track_emoji->num_clusters++;
    track_emoji->is_start = false;
    track_emoji->tracked_cluster_index = character_index;
    if (track_emoji->cluster_broken) {
      track_emoji->num_broken_clusters++;
    }
    track_emoji->cluster_broken = glyph == 0;
  } else {
    // We have reached an additional glyph for the same cluster, which means the
    // sequence was not identified by the font and is showing as multiple
    // glyphs.
    track_emoji->cluster_broken = true;
  }
}

struct EmojiCorrectness {
  unsigned num_clusters = 0;
  unsigned num_broken_clusters = 0;
};

EmojiCorrectness ComputeBrokenEmojiPercentage(ShapeResult* shape_result,
                                              unsigned start_index,
                                              unsigned end_index) {
  TrackEmoji track_emoji = {true, 0, false, 0, 0};
  shape_result->ForEachGlyph(0.f, start_index, end_index, 0 /* index_offset */,
                             IdentifyBrokenEmoji, &track_emoji);
  track_emoji.num_broken_clusters += track_emoji.cluster_broken ? 1 : 0;
  return {track_emoji.num_clusters, track_emoji.num_broken_clusters};
}

FontFallbackPriority ApplyFontVariantEmojiOnFallbackPriority(
    FontFallbackPriority curr_font_fallback_priority,
    FontVariantEmoji font_variant_emoji) {
  // font-variant-emoji property should not override emoji variation selectors,
  // see https://www.w3.org/TR/css-fonts-4/#font-variant-emoji-prop.
  if (RuntimeEnabledFeatures::FontVariantEmojiEnabled() &&
      !HasVSFallbackPriority(curr_font_fallback_priority)) {
    if (font_variant_emoji == kEmojiVariantEmoji) {
      return FontFallbackPriority::kEmojiEmoji;
    }
    if (font_variant_emoji == kTextVariantEmoji) {
      return FontFallbackPriority::kText;
    }
  }
  return curr_font_fallback_priority;
}

}  // namespace

enum ReshapeQueueItemAction {
  kReshapeQueueNextFont,
  kReshapeQueueRange,
  kReshapeQueueReset
};

struct ReshapeQueueItem {
  DISALLOW_NEW();
  ReshapeQueueItemAction action_;
  unsigned start_index_;
  unsigned num_characters_;
  ReshapeQueueItem(ReshapeQueueItemAction action, unsigned start, unsigned num)
      : action_(action), start_index_(start), num_characters_(num) {}
};

//
// Represents a context while shaping a range.
//
// Input-only data and objects whose pointers don't change are marked as
// `const`.
//
struct RangeContext {
  STACK_ALLOCATED();

 public:
  RangeContext(const Font* font,
               TextDirection direction,
               unsigned start,
               unsigned end,
               ShapeOptions options = ShapeOptions())
      : font(font),
        text_direction(direction),
        start(start),
        end(end),
        buffer(hb_buffer_create()),
        options(options) {
    DCHECK_GE(end, start);
    font_features.Initialize(font->GetFontDescription());
  }

  const Font* const font;
  const TextDirection text_direction;
  const unsigned start;
  const unsigned end;
  const hb::unique_ptr<hb_buffer_t> buffer;
  FontFeatures font_features;
  Deque<ReshapeQueueItem> reshape_queue;
  const ShapeOptions options;

  hb_direction_t HarfBuzzDirection(CanvasRotationInVertical canvas_rotation) {
    FontOrientation orientation = font->GetFontDescription().Orientation();
    hb_direction_t direction =
        IsVerticalAnyUpright(orientation) &&
                IsCanvasRotationInVerticalUpright(canvas_rotation)
            ? HB_DIRECTION_TTB
            : HB_DIRECTION_LTR;
    return text_direction == TextDirection::kRtl
               ? HB_DIRECTION_REVERSE(direction)
               : direction;
  }
};

struct BufferSlice {
  unsigned start_character_index;
  unsigned num_characters;
  unsigned start_glyph_index;
  unsigned num_glyphs;
};

namespace {

// A port of hb_icu_script_to_script because harfbuzz on CrOS is built
// without hb-icu. See http://crbug.com/356929
static inline hb_script_t ICUScriptToHBScript(UScriptCode script) {
  if (script == USCRIPT_INVALID_CODE) [[unlikely]] {
    return HB_SCRIPT_INVALID;
  }

  return hb_script_from_string(uscript_getShortName(script), -1);
}

inline float HarfBuzzPositionToFloat(hb_position_t value) {
  return static_cast<float>(value) / (1 << 16);
}

void RoundHarfBuzzPosition(hb_position_t* value) {
  if ((*value) & 0xFFFF) {
    // There is a non-zero fractional part in the 16.16 value.
    *value = static_cast<hb_position_t>(
                 round(static_cast<float>(*value) / (1 << 16)))
             << 16;
  }
}

void RoundHarfBuzzBufferPositions(hb_buffer_t* buffer) {
  unsigned int len;
  hb_glyph_position_t* glyph_positions =
      hb_buffer_get_glyph_positions(buffer, &len);
  for (unsigned int i = 0; i < len; i++) {
    hb_glyph_position_t* pos = &glyph_positions[i];
    RoundHarfBuzzPosition(&pos->x_offset);
    RoundHarfBuzzPosition(&pos->y_offset);
    RoundHarfBuzzPosition(&pos->x_advance);
    RoundHarfBuzzPosition(&pos->y_advance);
  }
}

inline bool ShapeRange(hb_buffer_t* buffer,
                       const FontFeatures& font_features,
                       const SimpleFontData* current_font,
                       const UnicodeRangeSet* current_font_range_set,
                       UScriptCode current_run_script,
                       hb_direction_t direction,
                       hb_language_t language,
                       float specified_size) {
  const FontPlatformData* platform_data = &(current_font->PlatformData());
  HarfBuzzFace* face = platform_data->GetHarfBuzzFace();
  if (!face) {
    DLOG(ERROR) << "Could not create HarfBuzzFace from FontPlatformData.";
    return false;
  }

  FontFeatures variant_features;
  if (!platform_data->ResolvedFeatures().empty()) {
    const ResolvedFontFeatures& resolved_features =
        platform_data->ResolvedFeatures();
    for (const std::pair<uint32_t, uint32_t>& feature : resolved_features) {
      variant_features.Append({feature.first, feature.second, 0 /* start */,
                               static_cast<unsigned>(-1) /* end */});
    }
  }

  bool needs_feature_merge = variant_features.size();
  if (needs_feature_merge) {
    for (wtf_size_t i = 0; i < font_features.size(); ++i) {
      variant_features.Append(font_features.data()[i]);
    }
  }
  const FontFeatures& argument_features =
      needs_feature_merge ? variant_features : font_features;

  hb_buffer_set_language(buffer, language);
  hb_buffer_set_script(buffer, ICUScriptToHBScript(current_run_script));
  hb_buffer_set_direction(buffer, direction);

  hb_font_t* hb_font =
      face->GetScaledFont(current_font_range_set,
                          HB_DIRECTION_IS_VERTICAL(direction)
                              ? HarfBuzzFace::kPrepareForVerticalLayout
                              : HarfBuzzFace::kNoVerticalLayout,
                          specified_size);
  hb_shape(hb_font, buffer, argument_features.data(), argument_features.size());
  if (!face->ShouldSubpixelPosition()) {
    RoundHarfBuzzBufferPositions(buffer);
  }

  return true;
}

BufferSlice ComputeSlice(RangeContext* range_data,
                         const ReshapeQueueItem& current_queue_item,
                         const hb_glyph_info_t* glyph_info,
                         unsigned num_glyphs,
                         unsigned old_glyph_index,
                         unsigned new_glyph_index) {
  // Compute the range indices of consecutive shaped or .notdef glyphs.
  // Cluster information for RTL runs becomes reversed, e.g. glyph 0
  // has cluster index 5 in a run of 6 characters.
  BufferSlice result;
  result.start_glyph_index = old_glyph_index;
  result.num_glyphs = new_glyph_index - old_glyph_index;

  if (HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(range_data->buffer))) {
    result.start_character_index = glyph_info[old_glyph_index].cluster;
    if (new_glyph_index == num_glyphs) {
      // Clamp the end offsets of the queue item to the offsets representing
      // the shaping window.
      unsigned shape_end =
          std::min(range_data->end, current_queue_item.start_index_ +
                                        current_queue_item.num_characters_);
      result.num_characters = shape_end - result.start_character_index;
    } else {
      result.num_characters =
          glyph_info[new_glyph_index].cluster - result.start_character_index;
    }
  } else {
    // Direction Backwards
    result.start_character_index = glyph_info[new_glyph_index - 1].cluster;
    if (old_glyph_index == 0) {
      // Clamp the end offsets of the queue item to the offsets representing
      // the shaping window.
      unsigned shape_end =
          std::min(range_data->end, current_queue_item.start_index_ +
                                        current_queue_item.num_characters_);
      result.num_characters = shape_end - result.start_character_index;
    } else {
      result.num_characters = glyph_info[old_glyph_index - 1].cluster -
                              glyph_info[new_glyph_index - 1].cluster;
    }
  }

  return result;
}

bool IsLastFontToShape(HarfBuzzShaper::FallbackFontStage fallback_stage) {
  return fallback_stage == HarfBuzzShaper::kLast ||
         fallback_stage == HarfBuzzShaper::kLastIgnoreVS;
}

bool StageNeedsQueueReset(HarfBuzzShaper::FallbackFontStage fallback_stage) {
  return fallback_stage == HarfBuzzShaper::kLastWithVS;
}

HarfBuzzShaper::FallbackFontStage ChangeStageToLast(
    HarfBuzzShaper::FallbackFontStage fallback_stage) {
  switch (fallback_stage) {
    case HarfBuzzShaper::kIntermediate:
      return HarfBuzzShaper::kLast;
    case HarfBuzzShaper::kIntermediateWithVS:
      DCHECK(RuntimeEnabledFeatures::FontVariationSequencesEnabled());
      return HarfBuzzShaper::kLastWithVS;
    case HarfBuzzShaper::kIntermediateIgnoreVS:
      DCHECK(RuntimeEnabledFeatures::FontVariationSequencesEnabled());
      return HarfBuzzShaper::kLastIgnoreVS;
    default:
      return fallback_stage;
  }
}

HarfBuzzShaper::FallbackFontStage ChangeStageToVS(
    HarfBuzzShaper::FallbackFontStage fallback_stage) {
  DCHECK(RuntimeEnabledFeatures::FontVariationSequencesEnabled());
  switch (fallback_stage) {
    case HarfBuzzShaper::kIntermediate:
      return HarfBuzzShaper::kIntermediateWithVS;
    case HarfBuzzShaper::kLast:
      return HarfBuzzShaper::kLastWithVS;
    case HarfBuzzShaper::kIntermediateWithVS:
    case HarfBuzzShaper::kLastWithVS:
      return fallback_stage;
    default:
      // We should not call this function on the second fallback pass.
      NOTREACHED_IN_MIGRATION();
  }
  return fallback_stage;
}

void QueueCharacters(RangeContext* range_data,
                     const SimpleFontData* current_font,
                     bool& font_cycle_queued,
                     const BufferSlice& slice,
                     HarfBuzzShaper::FallbackFontStage font_stage) {
  if (!font_cycle_queued) {
    if (StageNeedsQueueReset(font_stage)) {
      DCHECK(RuntimeEnabledFeatures::FontVariationSequencesEnabled());
      range_data->reshape_queue.push_back(
          ReshapeQueueItem(kReshapeQueueReset, 0, 0));
    } else {
      range_data->reshape_queue.push_back(
          ReshapeQueueItem(kReshapeQueueNextFont, 0, 0));
    }
    font_cycle_queued = true;
  }

  DCHECK(slice.num_characters);
  range_data->reshape_queue.push_back(ReshapeQueueItem(
      kReshapeQueueRange, slice.start_character_index, slice.num_characters));
}

CanvasRotationInVertical CanvasRotationForRun(
    FontOrientation font_orientation,
    OrientationIterator::RenderOrientation render_orientation,
    const FontDescription& font_description) {
  if (font_orientation == FontOrientation::kVerticalUpright) {
    return font_description.IsSyntheticOblique()
               ? CanvasRotationInVertical::kRotateCanvasUprightOblique
               : CanvasRotationInVertical::kRotateCanvasUpright;
  }

  if (font_orientation == FontOrientation::kVerticalMixed) {
    if (render_orientation == OrientationIterator::kOrientationKeep) {
      return font_description.IsSyntheticOblique()
                 ? CanvasRotationInVertical::kRotateCanvasUprightOblique
                 : CanvasRotationInVertical::kRotateCanvasUpright;
    }
    return font_description.IsSyntheticOblique()
               ? CanvasRotationInVertical::kOblique
               : CanvasRotationInVertical::kRegular;
  }

  return CanvasRotationInVertical::kRegular;
}

}  // namespace

inline void HarfBuzzShaper::CheckTextLen(unsigned start,
                                         unsigned length) const {
  CHECK_LE(start, text_.length());
  CHECK_LE(length, text_.length() - start);
}

inline void HarfBuzzShaper::CheckTextEnd(unsigned start, unsigned end) const {
  CHECK_LE(start, end);
  CHECK_LE(start, text_.length());
  CHECK_LE(end, text_.length());
}

void HarfBuzzShaper::CommitGlyphs(RangeContext* range_data,
                                  const SimpleFontData* current_font,
                                  UScriptCode current_run_script,
                                  CanvasRotationInVertical canvas_rotation,
                                  FallbackFontStage fallback_stage,
                                  const BufferSlice& slice,
                                  ShapeResult* shape_result) const {
  hb_direction_t direction = range_data->HarfBuzzDirection(canvas_rotation);
  hb_script_t script = ICUScriptToHBScript(current_run_script);
  // Here we need to specify glyph positions.
  BufferSlice next_slice;
  unsigned run_start_index = slice.start_character_index;
  for (const BufferSlice* current_slice = &slice;;) {
    auto* run = MakeGarbageCollected<ShapeResult::RunInfo>(
        current_font, direction, canvas_rotation, script, run_start_index,
        current_slice->num_glyphs, current_slice->num_characters);
    unsigned next_start_glyph;
    shape_result->InsertRun(run, current_slice->start_glyph_index,
                            current_slice->num_glyphs, &next_start_glyph,
                            range_data->buffer);
    DCHECK_GE(current_slice->start_glyph_index + current_slice->num_glyphs,
              next_start_glyph);
    unsigned next_num_glyphs =
        current_slice->num_glyphs -
        (next_start_glyph - current_slice->start_glyph_index);
    if (!next_num_glyphs) {
      break;
    }

    // If the slice exceeds the limit a RunInfo can store, create another
    // RunInfo for the rest of the slice.
    DCHECK_GT(current_slice->num_characters, run->num_characters_);
    next_slice = {current_slice->start_character_index + run->num_characters_,
                  current_slice->num_characters - run->num_characters_,
                  next_start_glyph, next_num_glyphs};
    current_slice = &next_slice;

    // The |InsertRun| has truncated the right end. In LTR, advance the
    // |run_start_index| because the end characters are truncated. In RTL, keep
    // the same |run_start_index| because the start characters are truncated.
    if (HB_DIRECTION_IS_FORWARD(direction)) {
      run_start_index = next_slice.start_character_index;
    }
  }
  if (IsLastFontToShape(fallback_stage)) {
    range_data->font->ReportNotDefGlyph();
  }
}

void HarfBuzzShaper::ExtractShapeResults(
    RangeContext* range_data,
    bool& font_cycle_queued,
    const ReshapeQueueItem& current_queue_item,
    const SimpleFontData* current_font,
    UScriptCode current_run_script,
    CanvasRotationInVertical canvas_rotation,
    FallbackFontStage& fallback_stage,
    ShapeResult* shape_result) const {
  enum ClusterResult { kShaped, kNotDef, kUnknown };
  ClusterResult current_cluster_result = kUnknown;
  ClusterResult previous_cluster_result = kUnknown;
  unsigned previous_cluster = 0;
  unsigned current_cluster = 0;

  // Find first notdef glyph in buffer.
  unsigned num_glyphs = hb_buffer_get_length(range_data->buffer);
  hb_glyph_info_t* glyph_info =
      hb_buffer_get_glyph_infos(range_data->buffer, nullptr);

  unsigned last_change_glyph_index = 0;
  unsigned previous_cluster_start_glyph_index = 0;

  if (!num_glyphs) {
    return;
  }

  const Glyph space_glyph = current_font->SpaceGlyph();
  for (unsigned glyph_index = 0; glyph_index < num_glyphs; ++glyph_index) {
    // We proceed by full clusters and determine a shaping result - either
    // kShaped or kNotDef for each cluster.
    const hb_glyph_info_t& glyph = glyph_info[glyph_index];
    previous_cluster = current_cluster;
    current_cluster = glyph.cluster;
    const hb_codepoint_t glyph_id = glyph.codepoint;
    ClusterResult glyph_result;
    if (glyph_id == 0) {
      // Glyph 0 must be assigned to a .notdef glyph.
      // https://docs.microsoft.com/en-us/typography/opentype/spec/recom#glyph-0-the-notdef-glyph
      glyph_result = kNotDef;
    } else if (glyph_id == space_glyph && !IsLastFontToShape(fallback_stage) &&
               text_[current_cluster] == kIdeographicSpaceCharacter) {
      // HarfBuzz synthesizes U+3000 IDEOGRAPHIC SPACE using the space glyph.
      // This is not desired for run-splitting, applying features, and for
      // computing `line-height`. crbug.com/1193282
      // We revisit when HarfBuzz decides how to solve this more generally.
      // https://github.com/harfbuzz/harfbuzz/issues/2889
      glyph_result = kNotDef;
    } else if (glyph_id == kUnmatchedVSGlyphId) {
      fallback_stage = ChangeStageToVS(fallback_stage);
      glyph_result = kNotDef;
    } else {
      glyph_result = kShaped;
    }

    if (current_cluster != previous_cluster) {
      // We are transitioning to a new cluster (whose shaping result state we
      // have not looked at yet). This means the cluster we just looked at is
      // completely analysed and we can determine whether it was fully shaped
      // and whether that means a state change to the cluster before that one.
      if ((previous_cluster_result != current_cluster_result) &&
          previous_cluster_result != kUnknown) {
        BufferSlice slice = ComputeSlice(
            range_data, current_queue_item, glyph_info, num_glyphs,
            last_change_glyph_index, previous_cluster_start_glyph_index);
        // If the most recent cluster is shaped and there is a state change,
        // it means the previous ones were unshaped, so we queue them, unless
        // we're using the last resort font.
        if (current_cluster_result == kShaped &&
            !IsLastFontToShape(fallback_stage)) {
          QueueCharacters(range_data, current_font, font_cycle_queued, slice,
                          fallback_stage);
        } else {
          // If the most recent cluster is unshaped and there is a state
          // change, it means the previous one(s) were shaped, so we commit
          // the glyphs. We also commit when we've reached the last resort
          // font.
          CommitGlyphs(range_data, current_font, current_run_script,
                       canvas_rotation, fallback_stage, slice, shape_result);
        }
        last_change_glyph_index = previous_cluster_start_glyph_index;
      }

      // No state change happened, continue.
      previous_cluster_result = current_cluster_result;
      previous_cluster_start_glyph_index = glyph_index;
      // Reset current cluster result.
      current_cluster_result = glyph_result;
    } else {
      // Update and merge current cluster result.
      current_cluster_result =
          glyph_result == kShaped && (current_cluster_result == kShaped ||
                                      current_cluster_result == kUnknown)
              ? kShaped
              : kNotDef;
    }
  }

  // End of the run.
  if (current_cluster_result != previous_cluster_result &&
      previous_cluster_result != kUnknown &&
      !IsLastFontToShape(fallback_stage)) {
    // The last cluster in the run still had shaping status different from
    // the cluster(s) before it, we need to submit one shaped and one
    // unshaped segment.
    if (current_cluster_result == kShaped) {
      BufferSlice slice = ComputeSlice(
          range_data, current_queue_item, glyph_info, num_glyphs,
          last_change_glyph_index, previous_cluster_start_glyph_index);
      QueueCharacters(range_data, current_font, font_cycle_queued, slice,
                      fallback_stage);
      slice =
          ComputeSlice(range_data, current_queue_item, glyph_info, num_glyphs,
                       previous_cluster_start_glyph_index, num_glyphs);
      CommitGlyphs(range_data, current_font, current_run_script,
                   canvas_rotation, fallback_stage, slice, shape_result);
    } else {
      BufferSlice slice = ComputeSlice(
          range_data, current_queue_item, glyph_info, num_glyphs,
          last_change_glyph_index, previous_cluster_start_glyph_index);
      CommitGlyphs(range_data, current_font, current_run_script,
                   canvas_rotation, fallback_stage, slice, shape_result);
      slice =
          ComputeSlice(range_data, current_queue_item, glyph_info, num_glyphs,
                       previous_cluster_start_glyph_index, num_glyphs);
      QueueCharacters(range_data, current_font, font_cycle_queued, slice,
                      fallback_stage);
    }
  } else {
    // There hasn't been a state change for the last cluster, so we can just
    // either commit or queue what we have up until here.
    BufferSlice slice =
        ComputeSlice(range_data, current_queue_item, glyph_info, num_glyphs,
                     last_change_glyph_index, num_glyphs);
    if (current_cluster_result == kNotDef &&
        !IsLastFontToShape(fallback_stage)) {
      QueueCharacters(range_data, current_font, font_cycle_queued, slice,
                      fallback_stage);
    } else {
      CommitGlyphs(range_data, current_font, current_run_script,
                   canvas_rotation, fallback_stage, slice, shape_result);
    }
  }
}

bool HarfBuzzShaper::CollectFallbackHintChars(
    const Deque<ReshapeQueueItem>& reshape_queue,
    bool needs_hint_list,
    HintCharList& hint) const {
  if (reshape_queue.empty()) {
    return false;
  }

  // Clear without releasing the capacity to avoid reallocations.
  hint.resize(0);

  size_t num_chars_added = 0;
  for (auto it = reshape_queue.begin(); it != reshape_queue.end(); ++it) {
    if (it->action_ == kReshapeQueueNextFont) {
      break;
    }

    CheckTextLen(it->start_index_, it->num_characters_);
    if (text_.Is8Bit()) {
      for (unsigned i = 0; i < it->num_characters_; i++) {
        const UChar hint_char = text_[it->start_index_ + i];
        hint.push_back(hint_char);
        num_chars_added++;
        // Determine if we can take a shortcut and not fill the hint list
        // further: We can do that if we do not need a hint list, and we have
        // managed to find a character with a definite script since
        // FontFallbackIterator needs a character with a determined script to
        // perform meaningful system fallback.
        if (!needs_hint_list && Character::HasDefiniteScript(hint_char)) {
          return true;
        }
      }
      continue;
    }

    // !text_.Is8Bit()...
    UChar32 hint_char;
    UTF16TextIterator iterator(text_.Characters16() + it->start_index_,
                               it->num_characters_);
    while (iterator.Consume(hint_char)) {
      hint.push_back(hint_char);
      num_chars_added++;
      // Determine if we can take a shortcut and not fill the hint list
      // further: We can do that if we do not need a hint list, and we have
      // managed to find a character with a definite script since
      // FontFallbackIterator needs a character with a determined script to
      // perform meaningful system fallback.
      if (!needs_hint_list && Character::HasDefiniteScript(hint_char)) {
        return true;
      }
      iterator.Advance();
    }
  }
  return num_chars_added > 0;
}

namespace {

void SplitUntilNextCaseChange(
    const String& text,
    Deque<blink::ReshapeQueueItem>* queue,
    blink::ReshapeQueueItem& current_queue_item,
    SmallCapsIterator::SmallCapsBehavior& small_caps_behavior) {
  // TODO(layout-dev): Add support for latin-1 to SmallCapsIterator.
  const UChar* normalized_buffer;
  std::optional<String> utf16_text;
  if (text.Is8Bit()) {
    utf16_text.emplace(text);
    utf16_text->Ensure16Bit();
    normalized_buffer = utf16_text->Characters16();
  } else {
    normalized_buffer = text.Characters16();
  }

  unsigned num_characters_until_case_change = 0;
  SmallCapsIterator small_caps_iterator(
      normalized_buffer + current_queue_item.start_index_,
      current_queue_item.num_characters_);
  small_caps_iterator.Consume(&num_characters_until_case_change,
                              &small_caps_behavior);
  if (num_characters_until_case_change > 0 &&
      num_characters_until_case_change < current_queue_item.num_characters_) {
    queue->push_front(blink::ReshapeQueueItem(
        blink::ReshapeQueueItemAction::kReshapeQueueRange,
        current_queue_item.start_index_ + num_characters_until_case_change,
        current_queue_item.num_characters_ - num_characters_until_case_change));
    current_queue_item.num_characters_ = num_characters_until_case_change;
  }
}

class CapsFeatureSettingsScopedOverlay final {
  STACK_ALLOCATED();

 public:
  CapsFeatureSettingsScopedOverlay(FontFeatures*,
                                   FontDescription::FontVariantCaps);
  CapsFeatureSettingsScopedOverlay() = delete;
  ~CapsFeatureSettingsScopedOverlay();

 private:
  void OverlayCapsFeatures(FontDescription::FontVariantCaps);
  void PrependCounting(const hb_feature_t&);
  FontFeatures* features_;
  wtf_size_t count_features_;
};

CapsFeatureSettingsScopedOverlay::CapsFeatureSettingsScopedOverlay(
    FontFeatures* features,
    FontDescription::FontVariantCaps variant_caps)
    : features_(features), count_features_(0) {
  OverlayCapsFeatures(variant_caps);
}

void CapsFeatureSettingsScopedOverlay::OverlayCapsFeatures(
    FontDescription::FontVariantCaps variant_caps) {
  static constexpr hb_feature_t smcp = CreateFeature('s', 'm', 'c', 'p', 1);
  static constexpr hb_feature_t pcap = CreateFeature('p', 'c', 'a', 'p', 1);
  static constexpr hb_feature_t c2sc = CreateFeature('c', '2', 's', 'c', 1);
  static constexpr hb_feature_t c2pc = CreateFeature('c', '2', 'p', 'c', 1);
  static constexpr hb_feature_t unic = CreateFeature('u', 'n', 'i', 'c', 1);
  static constexpr hb_feature_t titl = CreateFeature('t', 'i', 't', 'l', 1);
  if (variant_caps == FontDescription::kSmallCaps ||
      variant_caps == FontDescription::kAllSmallCaps) {
    PrependCounting(smcp);
    if (variant_caps == FontDescription::kAllSmallCaps) {
      PrependCounting(c2sc);
    }
  }
  if (variant_caps == FontDescription::kPetiteCaps ||
      variant_caps == FontDescription::kAllPetiteCaps) {
    PrependCounting(pcap);
    if (variant_caps == FontDescription::kAllPetiteCaps) {
      PrependCounting(c2pc);
    }
  }
  if (variant_caps == FontDescription::kUnicase) {
    PrependCounting(unic);
  }
  if (variant_caps == FontDescription::kTitlingCaps) {
    PrependCounting(titl);
  }
}

void CapsFeatureSettingsScopedOverlay::PrependCounting(
    const hb_feature_t& feature) {
  features_->Insert(feature);
  count_features_++;
}

CapsFeatureSettingsScopedOverlay::~CapsFeatureSettingsScopedOverlay() {
  features_->EraseAt(0, count_features_);
}

}  // namespace

void HarfBuzzShaper::ShapeSegment(
    RangeContext* range_data,
    const RunSegmenter::RunSegmenterRange& segment,
    ShapeResult* result) const {
  DCHECK(result);
  DCHECK(range_data->buffer);
  const Font* font = range_data->font;
  const FontDescription& font_description = font->GetFontDescription();
  const LayoutLocale& locale = font_description.LocaleOrDefault();
  const hb_language_t language = locale.HarfbuzzLanguage();
  bool needs_caps_handling =
      font_description.VariantCaps() != FontDescription::kCapsNormal;
  OpenTypeCapsSupport caps_support;

  FontFallbackIterator fallback_iterator(
      font->CreateFontFallbackIterator(ApplyFontVariantEmojiOnFallbackPriority(
          segment.font_fallback_priority, font_description.VariantEmoji())));

  range_data->reshape_queue.push_back(
      ReshapeQueueItem(kReshapeQueueNextFont, 0, 0));
  range_data->reshape_queue.push_back(ReshapeQueueItem(
      kReshapeQueueRange, segment.start, segment.end - segment.start));

  bool font_cycle_queued = false;
  HintCharList fallback_chars_hint;
  // Reserve sufficient capacity to avoid multiple reallocations, only when a
  // full hint list is needed.
  if (fallback_iterator.NeedsHintList()) {
    fallback_chars_hint.ReserveInitialCapacity(range_data->end -
                                               range_data->start);
  }
  FontDataForRangeSet* current_font_data_for_range_set = nullptr;
  FallbackFontStage fallback_stage = kIntermediate;
  // Variation selector mode should be always set to default at the
  // beginning of the segment shaping run.
  DCHECK(HarfBuzzFace::GetVariationSelectorMode() ==
         kUseSpecifiedVariationSelector);
  if (RuntimeEnabledFeatures::FontVariantEmojiEnabled() &&
      font_description.VariantEmoji() != kNormalVariantEmoji) {
    HarfBuzzFace::SetVariationSelectorMode(
        GetVariationSelectorModeFromFontVariantEmoji(
            font_description.VariantEmoji()));
  }
  while (!range_data->reshape_queue.empty()) {
    ReshapeQueueItem current_queue_item = range_data->reshape_queue.TakeFirst();

    if (current_queue_item.action_ != kReshapeQueueRange) {
      if (current_queue_item.action_ == kReshapeQueueReset) {
        // We reached last font in the list, some of the variation sequences
        // are not shaped yet and there is a fonts in the list that has glyphs
        // for the base codepoint of unshaped variation sequences, so we need to
        // restart the fallback queue and set the variation selector mode to
        // `kIgnoreVariationSelector`.
        DCHECK(RuntimeEnabledFeatures::FontVariationSequencesEnabled());
        DCHECK_EQ(fallback_stage, kLastWithVS);
        fallback_iterator.Reset();
        fallback_stage = kIntermediateIgnoreVS;
        HarfBuzzFace::SetVariationSelectorMode(kIgnoreVariationSelector);
      }

      if (!CollectFallbackHintChars(range_data->reshape_queue,
                                    fallback_iterator.NeedsHintList(),
                                    fallback_chars_hint)) {
        // Give up shaping since we cannot retrieve a font fallback
        // font without a hintlist.
        range_data->reshape_queue.clear();
        break;
      }

      current_font_data_for_range_set =
          fallback_iterator.Next(fallback_chars_hint);
      if (!current_font_data_for_range_set->FontData()) {
        DCHECK(range_data->reshape_queue.empty());
        break;
      }
      font_cycle_queued = false;
      continue;
    }

    if (!fallback_iterator.HasNext()) {
      fallback_stage = ChangeStageToLast(fallback_stage);
    }

    const SimpleFontData* font_data =
        current_font_data_for_range_set->FontData();
    SmallCapsIterator::SmallCapsBehavior small_caps_behavior =
        SmallCapsIterator::kSmallCapsSameCase;
    if (needs_caps_handling) {
      caps_support =
          OpenTypeCapsSupport(font_data->PlatformData().GetHarfBuzzFace(),
                              font_description.VariantCaps(),
                              font_description.GetFontSynthesisSmallCaps(),
                              ICUScriptToHBScript(segment.script));
      if (caps_support.NeedsRunCaseSplitting()) {
        SplitUntilNextCaseChange(text_, &range_data->reshape_queue,
                                 current_queue_item, small_caps_behavior);
        // Skip queue items generated by SplitUntilNextCaseChange that do not
        // contribute to the shape result if the range_data restricts shaping to
        // a substring.
        if (range_data->start >= current_queue_item.start_index_ +
                                     current_queue_item.num_characters_ ||
            range_data->end <= current_queue_item.start_index_) {
          continue;
        }
      }
    }

    DCHECK(current_queue_item.num_characters_);
    const SimpleFontData* adjusted_font = font_data;

    // Clamp the start and end offsets of the queue item to the offsets
    // representing the shaping window.
    const unsigned shape_start =
        std::max(range_data->start, current_queue_item.start_index_);
    const unsigned shape_end =
        std::min(range_data->end, current_queue_item.start_index_ +
                                      current_queue_item.num_characters_);
    DCHECK_GT(shape_end, shape_start);
    CheckTextEnd(shape_start, shape_end);

    CaseMapIntend case_map_intend = CaseMapIntend::kKeepSameCase;
    if (needs_caps_handling) {
      case_map_intend = caps_support.NeedsCaseChange(small_caps_behavior);
      if (caps_support.NeedsSyntheticFont(small_caps_behavior)) {
        adjusted_font = font_data->SmallCapsFontData(font_description);
      }
    }

    CaseMappingHarfBuzzBufferFiller(
        case_map_intend, font_description.LocaleOrDefault(), range_data->buffer,
        text_, shape_start, shape_end - shape_start);

    CanvasRotationInVertical canvas_rotation =
        CanvasRotationForRun(adjusted_font->PlatformData().Orientation(),
                             segment.render_orientation, font_description);

    CapsFeatureSettingsScopedOverlay caps_overlay(
        &range_data->font_features,
        caps_support.FontFeatureToUse(small_caps_behavior));
    hb_direction_t direction = range_data->HarfBuzzDirection(canvas_rotation);
    HanKerning han_kerning(
        text_, shape_start, shape_end, *adjusted_font, font_description,
        {.is_horizontal = HB_DIRECTION_IS_HORIZONTAL(direction),
         .is_line_start = range_data->options.is_line_start &&
                          range_data->start == shape_start,
         .apply_start = range_data->options.han_kerning_start &&
                        range_data->start == shape_start,
         .apply_end = range_data->options.han_kerning_end &&
                      range_data->end == shape_end},
        &range_data->font_features);

    if (!ShapeRange(range_data->buffer, range_data->font_features,
                    adjusted_font, current_font_data_for_range_set->Ranges(),
                    segment.script, direction, language,
                    font_description.SpecifiedSize())) {
      DLOG(ERROR) << "Shaping range failed.";
    }

    ExtractShapeResults(range_data, font_cycle_queued, current_queue_item,
                        adjusted_font, segment.script, canvas_rotation,
                        fallback_stage, result);

    if (!han_kerning.UnsafeToBreakBefore().empty()) [[unlikely]] {
      result->AddUnsafeToBreak(han_kerning.UnsafeToBreakBefore());
    }

    hb_buffer_reset(range_data->buffer);
  }

  // Ignore variation selectors flag should be only changed when the
  // FontVariationSequences runtime flag is enabled.
  DCHECK(
      RuntimeEnabledFeatures::FontVariationSequencesEnabled() ||
      !ShouldIgnoreVariationSelector(HarfBuzzFace::GetVariationSelectorMode()));

  if (RuntimeEnabledFeatures::FontVariationSequencesEnabled()) {
    // Set variation selector mode to the default state.
    HarfBuzzFace::SetVariationSelectorMode(kUseSpecifiedVariationSelector);
  }

  if (IsEmojiPresentationEmoji(segment.font_fallback_priority)) {
    EmojiCorrectness emoji_correctness =
        ComputeBrokenEmojiPercentage(result, segment.start, segment.end);
    if (emoji_metrics_reporter_for_testing_) {
      emoji_metrics_reporter_for_testing_.Run(
          emoji_correctness.num_clusters,
          emoji_correctness.num_broken_clusters);
    } else {
      range_data->font->ReportEmojiSegmentGlyphCoverage(
          emoji_correctness.num_clusters,
          emoji_correctness.num_broken_clusters);
    }
  }
}

ShapeResult* HarfBuzzShaper::Shape(const Font* font,
                                   TextDirection direction,
                                   unsigned start,
                                   unsigned end) const {
  DCHECK_GE(end, start);
  DCHECK_LE(end, text_.length());

  const unsigned length = end - start;
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(font, start, length, direction);
  RangeContext range_data(font, direction, start, end);
  if (text_.Is8Bit()) {
    // 8-bit text is guaranteed to horizontal latin-1.
    RunSegmenter::RunSegmenterRange segment_range = {
        start, end, USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText};
    ShapeSegment(&range_data, segment_range, result);

  } else {
    // Run segmentation needs to operate on the entire string, regardless of the
    // shaping window (defined by the start and end parameters).
    DCHECK(!text_.Is8Bit());
    RunSegmenter run_segmenter(text_.Characters16(), text_.length(),
                               font->GetFontDescription().Orientation());
    RunSegmenter::RunSegmenterRange segment_range;
    while (run_segmenter.Consume(&segment_range)) {
      // Only shape segments overlapping with the range indicated by start and
      // end. Not only those strictly within.
      if (start < segment_range.end && end > segment_range.start) {
        ShapeSegment(&range_data, segment_range, result);
      }

      // Break if beyond the requested range. Because RunSegmenter is
      // incremental, further ranges are not needed. This also allows reusing
      // the segmenter state for next incremental calls.
      if (segment_range.end >= end) {
        break;
      }
    }
  }

#if EXPENSIVE_DCHECKS_ARE_ON()
  CheckShapeResultRange(result, start, end, text_, font);
#endif
  return result;
}

ShapeResult* HarfBuzzShaper::Shape(
    const Font* font,
    TextDirection direction,
    unsigned start,
    unsigned end,
    const Vector<RunSegmenter::RunSegmenterRange>& ranges,
    ShapeOptions options) const {
  DCHECK_GE(end, start);
  DCHECK_LE(end, text_.length());
  DCHECK_GT(ranges.size(), 0u);
  DCHECK_EQ(start, ranges[0].start);
  DCHECK_EQ(end, ranges[ranges.size() - 1].end);

  const unsigned length = end - start;
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(font, start, length, direction);
  RangeContext range_data(font, direction, start, end, options);
  for (const RunSegmenter::RunSegmenterRange& segmented_range : ranges) {
    DCHECK_GE(segmented_range.end, segmented_range.start);
    DCHECK_GE(segmented_range.start, start);
    DCHECK_LE(segmented_range.end, end);
    ShapeSegment(&range_data, segmented_range, result);
  }

#if EXPENSIVE_DCHECKS_ARE_ON()
  CheckShapeResultRange(result, start, end, text_, font);
#endif
  return result;
}

ShapeResult* HarfBuzzShaper::Shape(
    const Font* font,
    TextDirection direction,
    unsigned start,
    unsigned end,
    const RunSegmenter::RunSegmenterRange pre_segmented,
    ShapeOptions options) const {
  DCHECK_GE(end, start);
  DCHECK_LE(end, text_.length());
  DCHECK_GE(start, pre_segmented.start);
  DCHECK_LE(end, pre_segmented.end);

  const unsigned length = end - start;
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(font, start, length, direction);
  RangeContext range_data(font, direction, start, end, options);
  ShapeSegment(&range_data, pre_segmented, result);

#if EXPENSIVE_DCHECKS_ARE_ON()
  CheckShapeResultRange(result, start, end, text_, font);
#endif
  return result;
}

ShapeResult* HarfBuzzShaper::Shape(const Font* font,
                                   TextDirection direction) const {
  return Shape(font, direction, 0, text_.length());
}

void HarfBuzzShaper::GetGlyphData(const SimpleFontData& font_data,
                                  const LayoutLocale& locale,
                                  UScriptCode script,
                                  bool is_horizontal,
                                  GlyphDataList& glyphs) {
  hb::unique_ptr<hb_buffer_t> hb_buffer(hb_buffer_create());
  hb_buffer_set_language(hb_buffer, locale.HarfbuzzLanguage());
  hb_buffer_set_script(hb_buffer, ICUScriptToHBScript(script));
  hb_buffer_set_direction(hb_buffer,
                          is_horizontal ? HB_DIRECTION_LTR : HB_DIRECTION_TTB);
  CHECK(!text_.Is8Bit());
  static_assert(sizeof(uint16_t) == sizeof(UChar));
  hb_buffer_add_utf16(hb_buffer,
                      reinterpret_cast<const uint16_t*>(text_.Characters16()),
                      text_.length(), 0, text_.length());

  const FontPlatformData& platform_data = font_data.PlatformData();
  HarfBuzzFace* const hb_face = platform_data.GetHarfBuzzFace();
  DCHECK(hb_face);
  hb_font_t* const hb_font = hb_face->GetScaledFont(
      nullptr,
      is_horizontal ? HarfBuzzFace::kNoVerticalLayout
                    : HarfBuzzFace::kPrepareForVerticalLayout,
      platform_data.size());
  DCHECK(hb_font);
  hb_shape(hb_font, hb_buffer, nullptr, 0);

  // Create `GlyphDataList` from `hb_buffer`.
  unsigned num_glyphs;
  hb_glyph_info_t* glyph_info =
      hb_buffer_get_glyph_infos(hb_buffer, &num_glyphs);
  hb_glyph_position_t* glyph_position =
      hb_buffer_get_glyph_positions(hb_buffer, nullptr);
  glyphs.reserve(num_glyphs);
  for (; num_glyphs; --num_glyphs, ++glyph_info, ++glyph_position) {
    glyphs.push_back(GlyphData{
        .cluster = glyph_info->cluster,
        .glyph = static_cast<Glyph>(glyph_info->codepoint),
        .advance = {HarfBuzzPositionToFloat(glyph_position->x_advance),
                    -HarfBuzzPositionToFloat(glyph_position->y_advance)},
        .offset = {HarfBuzzPositionToFloat(glyph_position->x_offset),
                   -HarfBuzzPositionToFloat(glyph_position->y_offset)}});
  }
}

}  // namespace blink
