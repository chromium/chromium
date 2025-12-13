// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"

#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/frame_shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"

namespace blink {

namespace {

template <typename CharType>
std::pair<String, bool> NormalizeSpacesAndMaybeBidiInternal(
    StringView text,
    base::span<const CharType> chars,
    bool is_canvas) {
  // This function accesses the input via a raw pointer for better performance.
  const CharType* source = chars.data();
  const size_t length = chars.size();
  std::optional<StringBuffer<UChar>> buffer;
  size_t result_length = 0;
  bool maybe_bidi = false;
  bool error = false;
  for (size_t i = 0; i < length;) {
    const size_t last_index = i;
    UChar32 character;
    if constexpr (sizeof(CharType) == 1) {
      // SAFETY: `i` is less than chars.size().
      character = UNSAFE_BUFFERS(source[i++]);
    } else {
      // SAFETY: `i` is less than chars.size().
      UNSAFE_BUFFERS(U16_NEXT(source, i, length, character));
    }

    UChar32 normalized = character;
    // Don't normalize tabs as they are not treated as spaces for word-end.
    if (is_canvas && Character::IsNormalizedCanvasSpaceCharacter(character)) {
      normalized = uchar::kSpace;
    } else if (Character::TreatAsSpace(character) &&
               character != uchar::kNoBreakSpace) {
      normalized = uchar::kSpace;
    } else if (Character::TreatAsZeroWidthSpaceInComplexScriptLegacy(
                   character)) {
      // Replace only ZWS-like characters in BMP because we'd like to avoid
      // changing the string length.
      DCHECK_LT(character, 0x10000);
      normalized = uchar::kZeroWidthSpace;
    }

    if (!maybe_bidi && Character::MaybeBidiRtl(character)) {
      maybe_bidi = true;
    }

    if (character != normalized && !buffer) {
      buffer.emplace(length);
      base::span<const CharType> prefix = chars.first(last_index);
      std::copy(prefix.begin(), prefix.end(), buffer->Span().data());
      result_length = last_index;
    }
    if (buffer) {
      U16_APPEND(buffer->Span(), result_length, length, normalized, error);
      DCHECK(!error);
    }
  }
  if (buffer) {
    DCHECK_EQ(result_length, length);
    return {String::Adopt(*buffer), maybe_bidi};
  }
  return {text.ToString(), maybe_bidi};
}

template <bool split_by_zws>
bool IsWordDelimiter(UChar ch) {
  // As of 2025 March, Google Docs always wraps text with BiDi control
  // characters, and they are replaced with ZWS for HarfBuzzShaper.
  // Assuming ZWS as a word delimiter improves hit rate of a shape cache.
  return ch == uchar::kSpace || ch == uchar::kTab ||
         (split_by_zws && ch == uchar::kZeroWidthSpace);
}

unsigned NextWordEndIndex(StringView text, unsigned start_index) {
  const unsigned length = text.length();
  if (start_index >= length) {
    return 0;
  }

  if (start_index + 1u == length || IsWordDelimiter<true>(text[start_index])) {
    return start_index + 1;
  }

  // 8Bit words end at IsWordDelimiter().
  if (text.Is8Bit()) {
    for (unsigned i = start_index + 1;; ++i) {
      if (i == length || IsWordDelimiter<false>(text[i])) {
        return i;
      }
    }
  }

  // Non-CJK/Emoji words end at IsWordDelimiter() or CJK/Emoji characters.
  unsigned end = start_index;
  UChar32 ch = text.CodePointAtAndNext(end);
  if (!Character::IsCJKIdeographOrSymbol(ch)) {
    for (unsigned next_end = end; end < length; end = next_end) {
      ch = text.CodePointAtAndNext(next_end);
      if (IsWordDelimiter<true>(ch) ||
          Character::IsCJKIdeographOrSymbolBase(ch)) {
        return end;
      }
    }
    return length;
  }

  // For CJK/Emoji words, delimit every character because these scripts do
  // not delimit words by spaces, and delimiting only at IsWordDelimiter()
  // worsen the cache efficiency.
  bool has_any_script = !Character::IsCommonOrInheritedScript(ch);
  for (unsigned next_end = end; end < length; end = next_end) {
    ch = text.CodePointAtAndNext(next_end);
    // Modifier check in order not to split Emoji sequences.
    if (U_GET_GC_MASK(ch) & (U_GC_M_MASK | U_GC_LM_MASK | U_GC_SK_MASK) ||
        ch == uchar::kZeroWidthJoiner || Character::IsEmojiComponent(ch) ||
        Character::IsExtendedPictographic(ch)) {
      continue;
    }
    // Avoid delimiting COMMON/INHERITED alone, which makes harder to
    // identify the script.
    if (Character::IsCJKIdeographOrSymbol(ch)) {
      if (Character::IsCommonOrInheritedScript(ch)) {
        continue;
      }
      if (!has_any_script) {
        has_any_script = true;
        continue;
      }
    }
    return end;
  }
  return length;
}

}  // namespace

struct CharacterRangeContext {
  const StringView& text;
  const bool is_rtl;
  int from;
  int to;
  float current_x;
  unsigned total_num_characters = 0;
  std::optional<float> from_x;
  std::optional<float> to_x;
  float min_y = 0;
  float max_y = 0;

  void ComputeRangeIn(const ShapeResult& result, const gfx::RectF& ink_bounds);
};

void CharacterRangeContext::ComputeRangeIn(const ShapeResult& result,
                                           const gfx::RectF& ink_bounds) {
  result.EnsureGraphemes(
      StringView(text, total_num_characters, result.NumCharacters()));
  if (is_rtl) {
    // Convert logical offsets to visual offsets, because results are in
    // logical order while runs are in visual order.
    if (!from_x && from >= 0 &&
        static_cast<unsigned>(from) < result.NumCharacters()) {
      from = result.NumCharacters() - from - 1;
    }
    if (!to_x && to >= 0 &&
        static_cast<unsigned>(to) < result.NumCharacters()) {
      to = result.NumCharacters() - to - 1;
    }
    current_x -= result.Width();
  }
  for (const auto& run : result.RunsOrParts()) {
    if (!run) {
      continue;
    }
    DCHECK_EQ(is_rtl, run->IsRtl());
    int num_characters = run->NumCharacters();
    if (!from_x && from >= 0 && from < num_characters) {
      from_x = run->XPositionForVisualOffset(from, AdjustMidCluster::kToStart) +
               current_x;
    } else {
      from -= num_characters;
    }

    if (!to_x && to >= 0 && to < num_characters) {
      to_x = run->XPositionForVisualOffset(to, AdjustMidCluster::kToEnd) +
             current_x;
    } else {
      to -= num_characters;
    }

    if (from_x || to_x) {
      min_y = std::min(min_y, ink_bounds.y());
      max_y = std::max(max_y, ink_bounds.bottom());
    }

    if (from_x && to_x) {
      break;
    }
    current_x += run->Width();
  }
  if (is_rtl) {
    current_x -= result.Width();
  }
  total_num_characters += result.NumCharacters();
}

// ================================================================

void PlainTextItem::Trace(Visitor* visitor) const {
  visitor->Trace(shape_result_);
  visitor->Trace(shape_result_view_);
}

const ShapeResultView* PlainTextItem::EnsureView() const {
  if (shape_result_ && !shape_result_view_) {
    shape_result_view_ = ShapeResultView::Create(shape_result_);
  }
  return shape_result_view_;
}

// ================================================================

PlainTextNode::PlainTextNode(const TextRun& run,
                             bool normalize_space,
                             const Font& font,
                             bool supports_bidi,
                             FrameShapeCache* cache)
    : normalize_space_(normalize_space), base_direction_(run.Direction()) {
  if (supports_bidi && run.DirectionalOverride()) [[unlikely]] {
    // If directional override, create a new string with Unicode directional
    // override characters.
    const String text_with_override =
        BidiParagraph::StringWithDirectionalOverride(run.ToStringView(),
                                                     run.Direction());
    TextRun run_with_override(text_with_override, run.Direction(),
                              /* directional_override */ false);
    SegmentText(run_with_override, /* bidi_overridden */ true, font,
                supports_bidi);
  } else {
    SegmentText(run, /* bidi_overridden */ false, font, supports_bidi);
  }
  Shape(font, cache);
}

void PlainTextNode::Trace(Visitor* visitor) const {
  visitor->Trace(item_list_);
}

std::pair<String, bool> PlainTextNode::NormalizeSpacesAndMaybeBidi(
    StringView text,
    bool normalize_canvas_space) {
  return text.Is8Bit() ? NormalizeSpacesAndMaybeBidiInternal(
                             text, text.Span8(), normalize_canvas_space)
                       : NormalizeSpacesAndMaybeBidiInternal(
                             text, text.Span16(), normalize_canvas_space);
}

void PlainTextNode::SegmentText(const TextRun& run,
                                bool bidi_overridden,
                                const Font& font,
                                bool supports_bidi) {
  auto [text, maybe_bidi] =
      NormalizeSpacesAndMaybeBidi(run.ToStringView(), normalize_space_);
  text_content_ = text;
  bool is_bidi_enabled = supports_bidi && (maybe_bidi || run.Rtl());

  if (is_bidi_enabled) {
    // We should apply BiDi reorder to the original text in the TextRun, not
    // a normalized one because the normalization removes bidi control
    // characters.
    String original_text = run.ToStringView().ToString();
    original_text.Ensure16Bit();
    BidiParagraph bidi;
    if (bidi_overridden) {
      // See BidiParagraph::StringWithDirectionalOverride().
      DCHECK(original_text[0] == uchar::kLeftToRightOverride ||
             original_text[0] == uchar::kRightToLeftOverride)
          << original_text;
      DCHECK(original_text[original_text.length() - 1] ==
             uchar::kPopDirectionalFormatting)
          << original_text;
      text_content_ = text_content_.Substring(1, text_content_.length() - 2);
    }
    if (bidi.SetParagraph(original_text, run.Direction())) {
      base_direction_ = bidi.BaseDirection();
      if (bidi.IsUnidirectional() && IsLtr(bidi.BaseDirection())) {
        is_bidi_enabled = false;
      } else {
        BidiParagraph::Runs bidi_runs;
        bidi.GetVisualRuns(original_text, &bidi_runs);
        item_list_.reserve(bidi_runs.size());
        for (const BidiParagraph::Run& bidi_run : bidi_runs) {
          if (IsRtl(bidi_run.Direction())) {
            contains_rtl_items_ = true;
          }
          if (!bidi_overridden) {
            SegmentWord(bidi_run.start, bidi_run.Length(), bidi_run.Direction(),
                        font);
          } else {
            // `bidi_run.start` and `bidi_run.end` are offsets in
            // `original_text`, which is longer than `text_content_` by two
            // characters.  SegmentWord() handles `text_content_`, and we need
            // to adjust offsets.
            const unsigned run_length = bidi_run.Length();
            // The bidi_run contains both of the leading and trailing BiDi
            // override controls.
            if (bidi_run.start == 0 && bidi_run.end == original_text.length()) {
              if (run_length > 2) {
                SegmentWord(0, run_length - 2, bidi_run.Direction(), font);
              }
            }
            // The bidi_run starts with the leading BiDi override control.
            else if (bidi_run.start == 0 && run_length > 0) {
              if (run_length > 1) {
                SegmentWord(0, run_length - 1, bidi_run.Direction(), font);
              }
            }
            // The bidi_run ends with the trailing BiDi override control.
            else if (bidi_run.end == original_text.length() && run_length > 0) {
              if (run_length > 1) {
                SegmentWord(bidi_run.start - 1, run_length - 1,
                            bidi_run.Direction(), font);
              }
            }
            // Otherwise.
            else {
              SegmentWord(bidi_run.start - 1, bidi_run.Length(),
                          bidi_run.Direction(), font);
            }
          }
        }
      }
    }
  } else if (font.GetFontDescription().Orientation() !=
             FontOrientation::kHorizontal) {
    // HarfBuzzShaper doesn't work well with a 8bit text + non-horizontal
    // FontOrientation.
    text_content_.Ensure16Bit();
  }

  if (item_list_.empty()) {
    SegmentWord(0, text_content_.length(), base_direction_, font);
  }
}

// LayoutNG does not split text into words during shaping, but PlainTextNode
// splits it for Google Docs.
//
// * Performance: Google Docs calls CanvasRenderingContext2D.measureText() with
//   a word, and fillText() with multiple words. We need a word-granularity
//   ShapeResult cache to maintain performance.
//
// * Rendering behavior: Google Docs relies on the rendering of
//   `fillText("a b", 0, 0)` being the same as `fillText("a", 0, 0)` and
//   `fillText("b", measureText("a") + measureText(" "), 0)`. So we should not
//   support kerning/ligature including spaces.
void PlainTextNode::SegmentWord(wtf_size_t start_offset,
                                wtf_size_t run_length,
                                TextDirection direction,
                                const Font& font) {
  if (!font.CanShapeWordByWord()) {
    item_list_.push_back(
        PlainTextItem(start_offset, run_length, direction, text_content_));
    return;
  }

  const wtf_size_t insertion_index = item_list_.size();
  StringView text_content(text_content_, start_offset, run_length);
  for (wtf_size_t index = 0; index < run_length;) {
    wtf_size_t new_index = NextWordEndIndex(text_content, index);
    PlainTextItem item(start_offset + index, new_index - index, direction,
                       text_content_);
    if (IsLtr(direction)) {
      item_list_.push_back(std::move(item));
    } else {
      item_list_.insert(insertion_index, std::move(item));
    }
    index = new_index;
  }
}

void PlainTextNode::Shape(const Font& font, FrameShapeCache* cache) {
  ShapeResultSpacing spacing(text_content_);
  spacing.SetSpacing(font.GetFontDescription(), normalize_space_);
  for (auto& item : item_list_) {
    FrameShapeCache::ShapeEntry* entry = nullptr;
    if (cache) {
      entry = cache->FindOrCreateShapeEntry(item.text_, item.Direction());
      if (entry && entry->shape_result) {
        item.shape_result_ = entry->shape_result;
        item.ink_bounds_ = entry->ink_bounds;
        has_vertical_offsets_ |= item.shape_result_->HasVerticalOffsets();
        continue;
      }
    }

    HarfBuzzShaper shaper(item.text_);
    ShapeResult* shape_result = shaper.Shape(&font, item.Direction());
    DCHECK(shape_result);
    gfx::RectF ink_bounds;
    if (!spacing.HasSpacing()) [[likely]] {
      ink_bounds = shape_result->ComputeInkBounds();
    } else {
      shape_result->ApplySpacing(spacing, item.StartOffset());
      ink_bounds = shape_result->ComputeInkBounds();
      DCHECK_GE(ink_bounds.width(), 0);

      if (shape_result->Width() >= 0) {
        // Return bounds as is because glyph bounding box is in logical space.
      } else {
        // Negative word-spacing and/or letter-spacing may cause some glyphs
        // to overflow the left boundary and result negative measured width.
        // Adjust glyph bounds accordingly to cover the overflow.
        // The negative width should be clamped to 0 in CSS box model, but
        // it's up to caller's responsibility.
        float left = std::min(shape_result->Width(), ink_bounds.width());
        if (left < ink_bounds.x()) {
          // The right edge should be the width of the first character in
          // most cases, but computing it requires re-measuring bounding box
          // of each glyph. Leave it unchanged, which gives an excessive
          // right edge but assures it covers all glyphs.
          ink_bounds.Outset(gfx::OutsetsF().set_left(ink_bounds.x() - left));
        }
      }
    }
    item.shape_result_ = shape_result;
    item.ink_bounds_ = ink_bounds;
    has_vertical_offsets_ |= item.shape_result_->HasVerticalOffsets();
    if (cache) {
      cache->RegisterShapeEntry(item, entry);
    }
  }
}

float PlainTextNode::AccumulateInlineSize(gfx::RectF* glyph_bounds) const {
  const bool is_rtl = IsRtl(base_direction_);
  float inline_size = 0;
  for (const auto& item : item_list_) {
    const ShapeResult* shape_result = item.GetShapeResult();
    if (!shape_result) {
      continue;
    }
    // For every shape_result we need to accumulate its width to adjust the
    // glyph_bounds. When the word_result is in RTL we accumulate in the
    // opposite direction (negative).
    if (is_rtl) {
      inline_size -= shape_result->Width();
    }
    if (glyph_bounds) {
      gfx::RectF adjusted_bounds = item.ink_bounds_;
      // Translate glyph bounds to the current glyph position which
      // is the total width before this glyph.
      adjusted_bounds.set_x(adjusted_bounds.x() + inline_size);
      glyph_bounds->Union(adjusted_bounds);
    }
    if (!is_rtl) {
      inline_size += shape_result->Width();
    }
  }

  // Finally, convert width back to positive if run is RTL.
  if (is_rtl) {
    inline_size = -inline_size;
    if (glyph_bounds) {
      glyph_bounds->set_x(glyph_bounds->x() + inline_size);
    }
  }

  return inline_size;
}

CharacterRange PlainTextNode::ComputeCharacterRange(
    unsigned absolute_from,
    unsigned absolute_to) const {
  const bool is_rtl = IsRtl(base_direction_);
  const float total_width = AccumulateInlineSize(nullptr);

  // The absolute_from and absolute_to arguments represent the start/end offset
  // for the entire run, from/to are continuously updated to be relative to
  // the current word (ShapeResult instance).
  int from = absolute_from;
  int to = absolute_to;

  CharacterRangeContext context{text_content_, is_rtl, from, to,
                                is_rtl ? total_width : 0};
  for (const auto& item : item_list_) {
    const ShapeResult* result = item.GetShapeResult();
    if (!result) {
      continue;
    }
    context.ComputeRangeIn(*result, item.InkBounds());
  }

  // The position in question might be just after the text.
  if (!context.from_x && absolute_from == context.total_num_characters) {
    context.from_x = is_rtl ? 0 : total_width;
  }
  if (!context.to_x && absolute_to == context.total_num_characters) {
    context.to_x = is_rtl ? 0 : total_width;
  }
  if (!context.from_x) {
    context.from_x = 0;
  }
  if (!context.to_x) {
    context.to_x = is_rtl ? 0 : total_width;
  }

  if (*context.from_x < *context.to_x) {
    return CharacterRange(*context.from_x, *context.to_x, -context.min_y,
                          context.max_y);
  }
  return CharacterRange(*context.to_x, *context.from_x, -context.min_y,
                        context.max_y);
}

}  // namespace blink
