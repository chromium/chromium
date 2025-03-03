// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
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
      normalized = kSpaceCharacter;
    } else if (Character::TreatAsSpace(character) &&
               character != kNoBreakSpaceCharacter) {
      normalized = kSpaceCharacter;
    } else if (Character::TreatAsZeroWidthSpaceInComplexScriptLegacy(
                   character)) {
      // Replace only ZWS-like characters in BMP because we'd like to avoid
      // changing the string length.
      DCHECK_LT(character, 0x10000);
      normalized = kZeroWidthSpaceCharacter;
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
      // SAFETY: buffer->Characters()'s size is `length`, and `result_length`
      // is less than `length`.
      UNSAFE_BUFFERS(U16_APPEND(buffer->Characters(), result_length, length,
                                normalized, error));
      DCHECK(!error);
    }
  }
  if (buffer) {
    DCHECK_EQ(result_length, length);
    return {String::Adopt(*buffer), maybe_bidi};
  }
  return {text.ToString(), maybe_bidi};
}

}  // namespace

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
                             bool bidi_overridden,
                             const Font& font,
                             bool supports_bidi)
    : normalize_space_(normalize_space), base_direction_(run.Direction()) {
  SegmentText(run, bidi_overridden, font, supports_bidi);
  // TODO(crbug.com/389726691): Implement shaping.
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
      // TODO(crbug.com/389726691): Remove leading/trailing BiDi control
      // characters from text_content_.
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
          SegmentWord(bidi_run.start, bidi_run.Length(), bidi_run.Direction(),
                      font);
          // TODO(crbug.com/389726691): Adjust offset values if bidi_overridden.
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

void PlainTextNode::SegmentWord(wtf_size_t start_offset,
                                wtf_size_t run_length,
                                TextDirection direction,
                                const Font& font) {
  item_list_.push_back(
      PlainTextItem(start_offset, run_length, direction, text_content_));

  // TODO(crbug.com/389726691): Implement word segmentation.
}

}  // namespace blink
