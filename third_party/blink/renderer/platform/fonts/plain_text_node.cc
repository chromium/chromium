// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
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
    : base_direction_(run.Direction()) {
  // TODO(crbug.com/389726691): Implement segmentation and shaping.
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

}  // namespace blink
