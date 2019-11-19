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

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"

#include <hb.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_iterator.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_caps_support.h"
#include "third_party/blink/renderer/platform/fonts/shaping/case_mapping_harfbuzz_buffer_filler.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "third_party/blink/renderer/platform/fonts/small_caps_iterator.h"
#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

namespace {

#if DCHECK_IS_ON()
// Check if the ShapeResult has the specified range.
// |text| and |font| are only for logging.
void CheckShapeResultRange(const ShapeResult* result,
                           unsigned start,
                           unsigned end,
                           const String& text,
                           const Font* font) {
  DCHECK_LE(start, end);
  unsigned length = end - start;
  if (length == result->NumCharacters() &&
      (!length || (start == result->StartIndex() && end == result->EndIndex())))
    return;

  // Log font-family/size as specified.
  StringBuilder log;
  log.Append("Font='");
  const FontDescription& font_description = font->GetFontDescription();
  for (const FontFamily* family = &font_description.Family();;) {
    log.Append(family->Family());
    family = family->Next();
    if (!family)
      break;
    log.Append(", ");
  }
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
  for (unsigned i = start; i < end; ++i)
    log.AppendFormat(" %02X", text[i]);

  log.Append(", result=");
  result->ToString(&log);

  NOTREACHED() << log.ToString();
}
#endif

}  // namespace

enum ReshapeQueueItemAction { kReshapeQueueNextFont, kReshapeQueueRange };

struct ReshapeQueueItem {
  DISALLOW_NEW();
  ReshapeQueueItemAction action_;
  unsigned start_index_;
  unsigned num_characters_;
  ReshapeQueueItem(ReshapeQueueItemAction action, unsigned start, unsigned num)
      : action_(action), start_index_(start), num_characters_(num) {}
};

template <typename T>
class HarfBuzzScopedPtr {
  STACK_ALLOCATED();

 public:
  typedef void (*DestroyFunction)(T*);

  HarfBuzzScopedPtr(T* ptr, DestroyFunction destroy)
      : ptr_(ptr), destroy_(destroy) {
    DCHECK(destroy_);
  }
  ~HarfBuzzScopedPtr() {
    if (ptr_)
      (*destroy_)(ptr_);
  }

  T* Get() { return ptr_; }
  void Set(T* ptr) { ptr_ = ptr; }

 private:
  T* ptr_;
  DestroyFunction destroy_;

  DISALLOW_COPY_AND_ASSIGN(HarfBuzzScopedPtr);
};

using FeaturesVector = Vector<hb_feature_t, 6>;
struct RangeData {
  hb_buffer_t* buffer;
  const Font* font;
  TextDirection text_direction;
  unsigned start;
  unsigned end;
  FeaturesVector font_features;
  Deque<ReshapeQueueItem> reshape_queue;

  hb_direction_t HarfBuzzDirection(CanvasRotationInVertical canvas_rotation) {
    FontOrientation orientation = font->GetFontDescription().Orientation();
    hb_direction_t direction =
        IsVerticalAnyUpright(orientation) &&
                (canvas_rotation ==
                 CanvasRotationInVertical::kRotateCanvasUpright)
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
  if (UNLIKELY(script == USCRIPT_INVALID_CODE))
    return HB_SCRIPT_INVALID;

  return hb_script_from_string(uscript_getShortName(script), -1);
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
                       hb_feature_t* font_features,
                       unsigned font_features_size,
                       const SimpleFontData* current_font,
                       scoped_refptr<UnicodeRangeSet> current_font_range_set,
                       UScriptCode current_run_script,
                       hb_direction_t direction,
                       hb_language_t language) {
  const FontPlatformData* platform_data = &(current_font->PlatformData());
  HarfBuzzFace* face = platform_data->GetHarfBuzzFace();
  if (!face) {
    DLOG(ERROR) << "Could not create HarfBuzzFace from FontPlatformData.";
    return false;
  }

  hb_buffer_set_language(buffer, language);
  hb_buffer_set_script(buffer, ICUScriptToHBScript(current_run_script));
  hb_buffer_set_direction(buffer, direction);

  hb_font_t* hb_font =
      face->GetScaledFont(std::move(current_font_range_set),
                          HB_DIRECTION_IS_VERTICAL(direction)
                              ? HarfBuzzFace::PrepareForVerticalLayout
                              : HarfBuzzFace::NoVerticalLayout);
  hb_shape(hb_font, buffer, font_features, font_features_size);
  if (!face->ShouldSubpixelPosition())
    RoundHarfBuzzBufferPositions(buffer);

  return true;
}

BufferSlice ComputeSlice(RangeData* range_data,
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

void QueueCharacters(RangeData* range_data,
                     const SimpleFontData* current_font,
                     bool& font_cycle_queued,
                     const BufferSlice& slice) {
  if (!font_cycle_queued) {
    range_data->reshape_queue.push_back(
        ReshapeQueueItem(kReshapeQueueNextFont, 0, 0));
    font_cycle_queued = true;
  }

  DCHECK(slice.num_characters);
  range_data->reshape_queue.push_back(ReshapeQueueItem(
      kReshapeQueueRange, slice.start_character_index, slice.num_characters));
}

CanvasRotationInVertical CanvasRotationForRun(
    FontOrientation font_orientation,
    OrientationIterator::RenderOrientation render_orientation) {
  if (font_orientation == FontOrientation::kVerticalUpright ||
      (font_orientation == FontOrientation::kVerticalMixed &&
       render_orientation == OrientationIterator::kOrientationKeep))
    return CanvasRotationInVertical::kRotateCanvasUpright;
  return CanvasRotationInVertical::kRegular;
}

}  // namespace

void HarfBuzzShaper::CommitGlyphs(RangeData* range_data,
                                  const SimpleFontData* current_font,
                                  UScriptCode current_run_script,
                                  CanvasRotationInVertical canvas_rotation,
                                  bool is_last_resort,
                                  const BufferSlice& slice,
                                  ShapeResult* shape_result) const {
  hb_direction_t direction = range_data->HarfBuzzDirection(canvas_rotation);
  hb_script_t script = ICUScriptToHBScript(current_run_script);
  // Here we need to specify glyph positions.
  BufferSlice next_slice;
  for (const BufferSlice* current_slice = &slice;;) {
    auto run = ShapeResult::RunInfo::Create(
        current_font, direction, canvas_rotation, script,
        current_slice->start_character_index, current_slice->num_glyphs,
        current_slice->num_characters);
    shape_result->InsertRun(run, current_slice->start_glyph_index,
                            current_slice->num_glyphs, range_data->buffer);
    unsigned num_glyphs_inserted = run->NumGlyphs();
    if (num_glyphs_inserted == current_slice->num_glyphs)
      break;
    // If the slice exceeds the limit a RunInfo can store, create another
    // RunInfo for the rest of the slice.
    DCHECK_GT(current_slice->num_characters, run->num_characters_);
    DCHECK_GT(current_slice->num_glyphs, num_glyphs_inserted);
    next_slice = {current_slice->start_character_index + run->num_characters_,
                  current_slice->num_characters - run->num_characters_,
                  current_slice->start_glyph_index + num_glyphs_inserted,
                  current_slice->num_glyphs - num_glyphs_inserted};
    current_slice = &next_slice;
  }
  if (is_last_resort)
    range_data->font->ReportNotDefGlyph();
}

void HarfBuzzShaper::ExtractShapeResults(
    RangeData* range_data,
    bool& font_cycle_queued,
    const ReshapeQueueItem& current_queue_item,
    const SimpleFontData* current_font,
    UScriptCode current_run_script,
    CanvasRotationInVertical canvas_rotation,
    bool is_last_resort,
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

  if (!num_glyphs)
    return;

  for (unsigned glyph_index = 0; glyph_index < num_glyphs; ++glyph_index) {
    // We proceed by full clusters and determine a shaping result - either
    // kShaped or kNotDef for each cluster.
    ClusterResult glyph_result =
        glyph_info[glyph_index].codepoint == 0 ? kNotDef : kShaped;
    previous_cluster = current_cluster;
    current_cluster = glyph_info[glyph_index].cluster;

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
        if (current_cluster_result == kShaped && !is_last_resort) {
          QueueCharacters(range_data, current_font, font_cycle_queued, slice);
        } else {
          // If the most recent cluster is unshaped and there is a state
          // change, it means the previous one(s) were shaped, so we commit
          // the glyphs. We also commit when we've reached the last resort
          // font.
          CommitGlyphs(range_data, current_font, current_run_script,
                       canvas_rotation, is_last_resort, slice, shape_result);
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
      previous_cluster_result != kUnknown && !is_last_resort) {
    // The last cluster in the run still had shaping status different from
    // the cluster(s) before it, we need to submit one shaped and one
    // unshaped segment.
    if (current_cluster_result == kShaped) {
      BufferSlice slice = ComputeSlice(
          range_data, current_queue_item, glyph_info, num_glyphs,
          last_change_glyph_index, previous_cluster_start_glyph_index);
      QueueCharacters(range_data, current_font, font_cycle_queued, slice);
      slice =
          ComputeSlice(range_data, current_queue_item, glyph_info, num_glyphs,
                       previous_cluster_start_glyph_index, num_glyphs);
      CommitGlyphs(range_data, current_font, current_run_script,
                   canvas_rotation, is_last_resort, slice, shape_result);
    } else {
      BufferSlice slice = ComputeSlice(
          range_data, current_queue_item, glyph_info, num_glyphs,
          last_change_glyph_index, previous_cluster_start_glyph_index);
      CommitGlyphs(range_data, current_font, current_run_script,
                   canvas_rotation, is_last_resort, slice, shape_result);
      slice =
          ComputeSlice(range_data, current_queue_item, glyph_info, num_glyphs,
                       previous_cluster_start_glyph_index, num_glyphs);
      QueueCharacters(range_data, current_font, font_cycle_queued, slice);
    }
  } else {
    // There hasn't been a state change for the last cluster, so we can just
    // either commit or queue what we have up until here.
    BufferSlice slice =
        ComputeSlice(range_data, current_queue_item, glyph_info, num_glyphs,
                     last_change_glyph_index, num_glyphs);
    if (current_cluster_result == kNotDef && !is_last_resort) {
      QueueCharacters(range_data, current_font, font_cycle_queued, slice);
    } else {
      CommitGlyphs(range_data, current_font, current_run_script,
                   canvas_rotation, is_last_resort, slice, shape_result);
    }
  }
}

bool HarfBuzzShaper::CollectFallbackHintChars(
    const Deque<ReshapeQueueItem>& reshape_queue,
    bool needs_hint_list,
    Vector<UChar32>& hint) const {
  if (reshape_queue.empty())
    return false;

  // Clear without releasing the capacity to avoid reallocations.
  hint.resize(0);

  size_t num_chars_added = 0;
  for (auto it = reshape_queue.begin(); it != reshape_queue.end(); ++it) {
    if (it->action_ == kReshapeQueueNextFont)
      break;

    CHECK_LE((it->start_index_ + it->num_characters_), text_.length());
    if (text_.Is8Bit()) {
      for (unsigned i = 0; i < it->num_characters_; i++) {
        hint.push_back(text_[it->start_index_ + i]);
        num_chars_added++;
        // Determine if we can take a shortcut and not fill the hint list
        // further: We can do that if we do not need a hint list, and we have
        // managed to find a character with a definite script since
        // FontFallbackIterator needs a character with a determined script to
        // perform meaningful system fallback.
        if (!needs_hint_list &&
            Character::HasDefiniteScript(text_[it->start_index_ + i]))
          return true;
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
      if (!needs_hint_list && Character::HasDefiniteScript(hint_char))
        return true;
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
  base::Optional<String> utf16_text;
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

constexpr hb_feature_t CreateFeature(hb_tag_t tag, uint32_t value = 0) {
  return {tag, value, 0 /* start */, static_cast<unsigned>(-1) /* end */};
}

// TODO(kojii): crbug.com/762493 This list is getting long enough to extract out
// of HarfBuzzShaper.cpp.
void SetFontFeatures(const Font* font, FeaturesVector* features) {
  const FontDescription& description = font->GetFontDescription();

  constexpr hb_feature_t no_kern = CreateFeature(HB_TAG('k', 'e', 'r', 'n'));
  constexpr hb_feature_t no_vkrn = CreateFeature(HB_TAG('v', 'k', 'r', 'n'));
  switch (description.GetKerning()) {
    case FontDescription::kNormalKerning:
      // kern/vkrn are enabled by default in HarfBuzz
      break;
    case FontDescription::kNoneKerning:
      features->push_back(description.IsVerticalAnyUpright() ? no_vkrn
                                                             : no_kern);
      break;
    case FontDescription::kAutoKerning:
      break;
  }

  {
    bool default_is_off = description.TextRendering() == blink::kOptimizeSpeed;
    bool letter_spacing = description.LetterSpacing() != 0;
    constexpr auto normal = FontDescription::kNormalLigaturesState;
    constexpr auto enabled = FontDescription::kEnabledLigaturesState;
    constexpr auto disabled = FontDescription::kDisabledLigaturesState;

    // clig and liga are on by default in HarfBuzz
    constexpr hb_feature_t no_clig = CreateFeature(HB_TAG('c', 'l', 'i', 'g'));
    constexpr hb_feature_t no_liga = CreateFeature(HB_TAG('l', 'i', 'g', 'a'));
    auto common = description.CommonLigaturesState();
    if (letter_spacing ||
        (common == disabled || (common == normal && default_is_off))) {
      features->push_back(no_liga);
      features->push_back(no_clig);
    }
    // dlig is off by default in HarfBuzz
    constexpr hb_feature_t dlig = CreateFeature(HB_TAG('d', 'l', 'i', 'g'), 1);
    auto discretionary = description.DiscretionaryLigaturesState();
    if (!letter_spacing && discretionary == enabled) {
      features->push_back(dlig);
    }
    // hlig is off by default in HarfBuzz
    constexpr hb_feature_t hlig = CreateFeature(HB_TAG('h', 'l', 'i', 'g'), 1);
    auto historical = description.HistoricalLigaturesState();
    if (!letter_spacing && historical == enabled) {
      features->push_back(hlig);
    }
    // calt is on by default in HarfBuzz
    constexpr hb_feature_t no_calt = CreateFeature(HB_TAG('c', 'a', 'l', 't'));
    auto contextual = description.ContextualLigaturesState();
    if (letter_spacing ||
        (contextual == disabled || (contextual == normal && default_is_off))) {
      features->push_back(no_calt);
    }
  }

  static constexpr hb_feature_t hwid =
      CreateFeature(HB_TAG('h', 'w', 'i', 'd'), 1);
  static constexpr hb_feature_t twid =
      CreateFeature(HB_TAG('t', 'w', 'i', 'd'), 1);
  static constexpr hb_feature_t qwid =
      CreateFeature(HB_TAG('q', 'w', 'i', 'd'), 1);
  switch (description.WidthVariant()) {
    case kHalfWidth:
      features->push_back(hwid);
      break;
    case kThirdWidth:
      features->push_back(twid);
      break;
    case kQuarterWidth:
      features->push_back(qwid);
      break;
    case kRegularWidth:
      break;
  }

  // font-variant-east-asian:
  const FontVariantEastAsian east_asian = description.VariantEastAsian();
  if (UNLIKELY(!east_asian.IsAllNormal())) {
    static constexpr hb_feature_t jp78 =
        CreateFeature(HB_TAG('j', 'p', '7', '8'), 1);
    static constexpr hb_feature_t jp83 =
        CreateFeature(HB_TAG('j', 'p', '8', '3'), 1);
    static constexpr hb_feature_t jp90 =
        CreateFeature(HB_TAG('j', 'p', '9', '0'), 1);
    static constexpr hb_feature_t jp04 =
        CreateFeature(HB_TAG('j', 'p', '0', '4'), 1);
    static constexpr hb_feature_t smpl =
        CreateFeature(HB_TAG('s', 'm', 'p', 'l'), 1);
    static constexpr hb_feature_t trad =
        CreateFeature(HB_TAG('t', 'r', 'a', 'd'), 1);
    switch (east_asian.Form()) {
      case FontVariantEastAsian::kNormalForm:
        break;
      case FontVariantEastAsian::kJis78:
        features->push_back(jp78);
        break;
      case FontVariantEastAsian::kJis83:
        features->push_back(jp83);
        break;
      case FontVariantEastAsian::kJis90:
        features->push_back(jp90);
        break;
      case FontVariantEastAsian::kJis04:
        features->push_back(jp04);
        break;
      case FontVariantEastAsian::kSimplified:
        features->push_back(smpl);
        break;
      case FontVariantEastAsian::kTraditional:
        features->push_back(trad);
        break;
      default:
        NOTREACHED();
    }
    static constexpr hb_feature_t fwid =
        CreateFeature(HB_TAG('f', 'w', 'i', 'd'), 1);
    static constexpr hb_feature_t pwid =
        CreateFeature(HB_TAG('p', 'w', 'i', 'd'), 1);
    switch (east_asian.Width()) {
      case FontVariantEastAsian::kNormalWidth:
        break;
      case FontVariantEastAsian::kFullWidth:
        features->push_back(fwid);
        break;
      case FontVariantEastAsian::kProportionalWidth:
        features->push_back(pwid);
        break;
      default:
        NOTREACHED();
    }
    static constexpr hb_feature_t ruby =
        CreateFeature(HB_TAG('r', 'u', 'b', 'y'), 1);
    if (east_asian.Ruby())
      features->push_back(ruby);
  }

  // font-variant-numeric:
  static constexpr hb_feature_t lnum =
      CreateFeature(HB_TAG('l', 'n', 'u', 'm'), 1);
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kLiningNums)
    features->push_back(lnum);

  static constexpr hb_feature_t onum =
      CreateFeature(HB_TAG('o', 'n', 'u', 'm'), 1);
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kOldstyleNums)
    features->push_back(onum);

  static constexpr hb_feature_t pnum =
      CreateFeature(HB_TAG('p', 'n', 'u', 'm'), 1);
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kProportionalNums)
    features->push_back(pnum);
  static constexpr hb_feature_t tnum =
      CreateFeature(HB_TAG('t', 'n', 'u', 'm'), 1);
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kTabularNums)
    features->push_back(tnum);

  static constexpr hb_feature_t afrc =
      CreateFeature(HB_TAG('a', 'f', 'r', 'c'), 1);
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kStackedFractions)
    features->push_back(afrc);
  static constexpr hb_feature_t frac =
      CreateFeature(HB_TAG('f', 'r', 'a', 'c'), 1);
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kDiagonalFractions)
    features->push_back(frac);

  static constexpr hb_feature_t ordn =
      CreateFeature(HB_TAG('o', 'r', 'd', 'n'), 1);
  if (description.VariantNumeric().OrdinalValue() ==
      FontVariantNumeric::kOrdinalOn)
    features->push_back(ordn);

  static constexpr hb_feature_t zero =
      CreateFeature(HB_TAG('z', 'e', 'r', 'o'), 1);
  if (description.VariantNumeric().SlashedZeroValue() ==
      FontVariantNumeric::kSlashedZeroOn)
    features->push_back(zero);

  FontFeatureSettings* settings = description.FeatureSettings();
  if (!settings)
    return;

  // TODO(drott): crbug.com/450619 Implement feature resolution instead of
  // just appending the font-feature-settings.
  unsigned num_features = settings->size();
  for (unsigned i = 0; i < num_features; ++i) {
    hb_feature_t feature;
    const AtomicString& tag = settings->at(i).Tag();
    feature.tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
    feature.value = settings->at(i).Value();
    feature.start = 0;
    feature.end = static_cast<unsigned>(-1);
    features->push_back(feature);
  }
}

inline RangeData CreateRangeData(const Font* font,
                                 TextDirection direction,
                                 hb_buffer_t* buffer) {
  RangeData range_data;
  range_data.buffer = buffer;
  range_data.font = font;
  range_data.text_direction = direction;
  SetFontFeatures(font, &range_data.font_features);
  return range_data;
}

class CapsFeatureSettingsScopedOverlay final {
  STACK_ALLOCATED();

 public:
  CapsFeatureSettingsScopedOverlay(FeaturesVector*,
                                   FontDescription::FontVariantCaps);
  CapsFeatureSettingsScopedOverlay() = delete;
  ~CapsFeatureSettingsScopedOverlay();

 private:
  void OverlayCapsFeatures(FontDescription::FontVariantCaps);
  void PrependCounting(const hb_feature_t&);
  FeaturesVector* features_;
  wtf_size_t count_features_;
};

CapsFeatureSettingsScopedOverlay::CapsFeatureSettingsScopedOverlay(
    FeaturesVector* features,
    FontDescription::FontVariantCaps variant_caps)
    : features_(features), count_features_(0) {
  OverlayCapsFeatures(variant_caps);
}

void CapsFeatureSettingsScopedOverlay::OverlayCapsFeatures(
    FontDescription::FontVariantCaps variant_caps) {
  static constexpr hb_feature_t smcp =
      CreateFeature(HB_TAG('s', 'm', 'c', 'p'), 1);
  static constexpr hb_feature_t pcap =
      CreateFeature(HB_TAG('p', 'c', 'a', 'p'), 1);
  static constexpr hb_feature_t c2sc =
      CreateFeature(HB_TAG('c', '2', 's', 'c'), 1);
  static constexpr hb_feature_t c2pc =
      CreateFeature(HB_TAG('c', '2', 'p', 'c'), 1);
  static constexpr hb_feature_t unic =
      CreateFeature(HB_TAG('u', 'n', 'i', 'c'), 1);
  static constexpr hb_feature_t titl =
      CreateFeature(HB_TAG('t', 'i', 't', 'l'), 1);
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
  features_->push_front(feature);
  count_features_++;
}

CapsFeatureSettingsScopedOverlay::~CapsFeatureSettingsScopedOverlay() {
  features_->EraseAt(0, count_features_);
}

}  // namespace

void HarfBuzzShaper::ShapeSegment(
    RangeData* range_data,
    const RunSegmenter::RunSegmenterRange& segment,
    ShapeResult* result) const {
  DCHECK(result);
  DCHECK(range_data->buffer);
  const Font* font = range_data->font;
  const FontDescription& font_description = font->GetFontDescription();
  const hb_language_t language =
      font_description.LocaleOrDefault().HarfbuzzLanguage();
  bool needs_caps_handling =
      font_description.VariantCaps() != FontDescription::kCapsNormal;
  OpenTypeCapsSupport caps_support;

  scoped_refptr<FontFallbackIterator> fallback_iterator =
      font->CreateFontFallbackIterator(segment.font_fallback_priority);

  range_data->reshape_queue.push_back(
      ReshapeQueueItem(kReshapeQueueNextFont, 0, 0));
  range_data->reshape_queue.push_back(ReshapeQueueItem(
      kReshapeQueueRange, segment.start, segment.end - segment.start));

  bool font_cycle_queued = false;
  Vector<UChar32> fallback_chars_hint;
  // Reserve sufficient capacity to avoid multiple reallocations, only when a
  // full hint list is needed.
  if (fallback_iterator->NeedsHintList()) {
    fallback_chars_hint.ReserveInitialCapacity(range_data->end -
                                               range_data->start);
  }
  scoped_refptr<FontDataForRangeSet> current_font_data_for_range_set;
  while (!range_data->reshape_queue.empty()) {
    ReshapeQueueItem current_queue_item = range_data->reshape_queue.TakeFirst();

    if (current_queue_item.action_ == kReshapeQueueNextFont) {
      if (!CollectFallbackHintChars(range_data->reshape_queue,
                                    fallback_iterator->NeedsHintList(),
                                    fallback_chars_hint)) {
        // Give up shaping since we cannot retrieve a font fallback
        // font without a hintlist.
        range_data->reshape_queue.clear();
        break;
      }

      current_font_data_for_range_set =
          fallback_iterator->Next(fallback_chars_hint);
      if (!current_font_data_for_range_set->FontData()) {
        DCHECK(range_data->reshape_queue.empty());
        break;
      }
      font_cycle_queued = false;
      continue;
    }

    const SimpleFontData* font_data =
        current_font_data_for_range_set->FontData();
    SmallCapsIterator::SmallCapsBehavior small_caps_behavior =
        SmallCapsIterator::kSmallCapsSameCase;
    if (needs_caps_handling) {
      caps_support = OpenTypeCapsSupport(
          font_data->PlatformData().GetHarfBuzzFace(),
          font_description.VariantCaps(), ICUScriptToHBScript(segment.script));
      if (caps_support.NeedsRunCaseSplitting()) {
        SplitUntilNextCaseChange(text_, &range_data->reshape_queue,
                                 current_queue_item, small_caps_behavior);
        // Skip queue items generated by SplitUntilNextCaseChange that do not
        // contribute to the shape result if the range_data restricts shaping to
        // a substring.
        if (range_data->start >= current_queue_item.start_index_ +
                                     current_queue_item.num_characters_ ||
            range_data->end <= current_queue_item.start_index_)
          continue;
      }
    }

    DCHECK(current_queue_item.num_characters_);
    const SimpleFontData* adjusted_font = font_data;

    // Clamp the start and end offsets of the queue item to the offsets
    // representing the shaping window.
    unsigned shape_start =
        std::max(range_data->start, current_queue_item.start_index_);
    unsigned shape_end =
        std::min(range_data->end, current_queue_item.start_index_ +
                                      current_queue_item.num_characters_);
    DCHECK_GT(shape_end, shape_start);

    CaseMapIntend case_map_intend = CaseMapIntend::kKeepSameCase;
    if (needs_caps_handling) {
      case_map_intend = caps_support.NeedsCaseChange(small_caps_behavior);
      if (caps_support.NeedsSyntheticFont(small_caps_behavior))
        adjusted_font = font_data->SmallCapsFontData(font_description).get();
    }

    CaseMappingHarfBuzzBufferFiller(
        case_map_intend, font_description.LocaleOrDefault(), range_data->buffer,
        text_, shape_start, shape_end - shape_start);

    CanvasRotationInVertical canvas_rotation =
        CanvasRotationForRun(adjusted_font->PlatformData().Orientation(),
                             segment.render_orientation);

    CapsFeatureSettingsScopedOverlay caps_overlay(
        &range_data->font_features,
        caps_support.FontFeatureToUse(small_caps_behavior));
    hb_direction_t direction = range_data->HarfBuzzDirection(canvas_rotation);

    if (!ShapeRange(range_data->buffer,
                    range_data->font_features.IsEmpty()
                        ? nullptr
                        : range_data->font_features.data(),
                    range_data->font_features.size(), adjusted_font,
                    current_font_data_for_range_set->Ranges(), segment.script,
                    direction, language))
      DLOG(ERROR) << "Shaping range failed.";

    ExtractShapeResults(range_data, font_cycle_queued, current_queue_item,
                        adjusted_font, segment.script, canvas_rotation,
                        !fallback_iterator->HasNext(), result);

    hb_buffer_reset(range_data->buffer);
  }
}

scoped_refptr<ShapeResult> HarfBuzzShaper::Shape(const Font* font,
                                                 TextDirection direction,
                                                 unsigned start,
                                                 unsigned end) const {
  DCHECK_GE(end, start);
  DCHECK_LE(end, text_.length());

  unsigned length = end - start;
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(font, start, length, direction);

  HarfBuzzScopedPtr<hb_buffer_t> buffer(hb_buffer_create(), hb_buffer_destroy);
  RangeData range_data = CreateRangeData(font, direction, buffer.Get());
  range_data.start = start;
  range_data.end = end;

  if (text_.Is8Bit()) {
    // 8-bit text is guaranteed to horizontal latin-1.
    RunSegmenter::RunSegmenterRange segment_range = {
        start, end, USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText};
    ShapeSegment(&range_data, segment_range, result.get());

  } else {
    // Run segmentation needs to operate on the entire string, regardless of the
    // shaping window (defined by the start and end parameters).
    DCHECK(!text_.Is8Bit());
    RunSegmenter run_segmenter(text_.Characters16(), text_.length(),
                               font->GetFontDescription().Orientation());
    RunSegmenter::RunSegmenterRange segment_range = RunSegmenter::NullRange();
    while (run_segmenter.Consume(&segment_range)) {
      // Only shape segments overlapping with the range indicated by start and
      // end. Not only those strictly within.
      if (start < segment_range.end && end > segment_range.start)
        ShapeSegment(&range_data, segment_range, result.get());

      // Break if beyond the requested range. Because RunSegmenter is
      // incremental, further ranges are not needed. This also allows reusing
      // the segmenter state for next incremental calls.
      if (segment_range.end >= end)
        break;
    }
  }

#if DCHECK_IS_ON()
  if (result)
    CheckShapeResultRange(result.get(), start, end, text_, font);
#endif

  return result;
}

scoped_refptr<ShapeResult> HarfBuzzShaper::Shape(
    const Font* font,
    TextDirection direction,
    unsigned start,
    unsigned end,
    const Vector<RunSegmenter::RunSegmenterRange>& ranges) const {
  DCHECK_GE(end, start);
  DCHECK_LE(end, text_.length());
  DCHECK_GT(ranges.size(), 0u);
  DCHECK_EQ(start, ranges[0].start);
  DCHECK_EQ(end, ranges[ranges.size() - 1].end);

  unsigned length = end - start;
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(font, start, length, direction);

  HarfBuzzScopedPtr<hb_buffer_t> buffer(hb_buffer_create(), hb_buffer_destroy);
  RangeData range_data = CreateRangeData(font, direction, buffer.Get());

  for (const RunSegmenter::RunSegmenterRange& segmented_range : ranges) {
    DCHECK_GE(segmented_range.end, segmented_range.start);
    DCHECK_GE(segmented_range.start, start);
    DCHECK_LE(segmented_range.end, end);

    range_data.start = segmented_range.start;
    range_data.end = segmented_range.end;
    ShapeSegment(&range_data, segmented_range, result.get());
  }

#if DCHECK_IS_ON()
  if (result)
    CheckShapeResultRange(result.get(), start, end, text_, font);
#endif

  return result;
}

scoped_refptr<ShapeResult> HarfBuzzShaper::Shape(
    const Font* font,
    TextDirection direction,
    unsigned start,
    unsigned end,
    const RunSegmenter::RunSegmenterRange pre_segmented) const {
  DCHECK_GE(end, start);
  DCHECK_LE(end, text_.length());
  DCHECK_GE(start, pre_segmented.start);
  DCHECK_LE(end, pre_segmented.end);

  unsigned length = end - start;
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(font, start, length, direction);

  HarfBuzzScopedPtr<hb_buffer_t> buffer(hb_buffer_create(), hb_buffer_destroy);
  RangeData range_data = CreateRangeData(font, direction, buffer.Get());
  range_data.start = start;
  range_data.end = end;

  ShapeSegment(&range_data, pre_segmented, result.get());

#if DCHECK_IS_ON()
  if (result)
    CheckShapeResultRange(result.get(), start, end, text_, font);
#endif

  return result;
}

scoped_refptr<ShapeResult> HarfBuzzShaper::Shape(
    const Font* font,
    TextDirection direction) const {
  return Shape(font, direction, 0, text_.length());
}

}  // namespace blink
