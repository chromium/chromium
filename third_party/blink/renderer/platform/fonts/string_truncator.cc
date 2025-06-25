/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/string_truncator.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

#define STRING_BUFFER_SIZE 2048

typedef base::span<const UChar> TruncationFunction(const String&,
                                                   unsigned keep_count,
                                                   base::span<UChar> buffer);

static inline int TextBreakAtOrPreceding(const CharacterBreakIterator& it,
                                         int offset) {
  if (it.IsBreak(offset))
    return offset;

  int result = it.Preceding(offset);
  return result == kTextBreakDone ? 0 : result;
}

static inline int BoundedTextBreakFollowing(const CharacterBreakIterator& it,
                                            int offset,
                                            int length) {
  int result = it.Following(offset);
  return result == kTextBreakDone ? length : result;
}

static base::span<const UChar> CenterTruncateToBuffer(
    const String& string,
    unsigned keep_count,
    base::span<UChar> buffer) {
  DCHECK_LT(keep_count, string.length());
  DCHECK(keep_count < STRING_BUFFER_SIZE);

  unsigned omit_start = (keep_count + 1) / 2;
  CharacterBreakIterator it(string);
  unsigned omit_end = BoundedTextBreakFollowing(
      it, omit_start + (string.length() - keep_count) - 1, string.length());
  omit_start = TextBreakAtOrPreceding(it, omit_start);

  unsigned truncated_length = omit_start + 1 + (string.length() - omit_end);
  DCHECK_LE(truncated_length, string.length());

  string.CopyTo(buffer.first(omit_start), 0);
  buffer[omit_start] = uchar::kHorizontalEllipsis;
  string.CopyTo(buffer.subspan(omit_start + 1, string.length() - omit_end),
                omit_end);

  return buffer.first(truncated_length);
}

static base::span<const UChar> RightTruncateToBuffer(const String& string,
                                                     unsigned keep_count,
                                                     base::span<UChar> buffer) {
  DCHECK_LT(keep_count, string.length());
  DCHECK(keep_count < STRING_BUFFER_SIZE);

  CharacterBreakIterator it(string);
  unsigned keep_length = TextBreakAtOrPreceding(it, keep_count);
  unsigned truncated_length = keep_length + 1;

  string.CopyTo(buffer.first(keep_length), 0);
  buffer[keep_length] = uchar::kHorizontalEllipsis;

  return buffer.first(truncated_length);
}

static float StringWidth(const Font& renderer,
                         base::span<const UChar> characters) {
  return PlainTextPainter::Shared().ComputeInlineSize(TextRun(characters),
                                                      renderer);
}

static String TruncateString(const String& string,
                             float max_width,
                             const Font& font,
                             TruncationFunction truncate_to_buffer) {
  if (string.empty())
    return string;

  DCHECK_GE(max_width, 0);

  const float current_ellipsis_width =
      StringWidth(font, base::span_from_ref(uchar::kHorizontalEllipsis));

  UChar string_buffer[STRING_BUFFER_SIZE];
  base::span<const UChar> truncated_string;
  unsigned keep_count;

  if (string.length() > STRING_BUFFER_SIZE) {
    keep_count = STRING_BUFFER_SIZE - 1;  // need 1 character for the ellipsis
    truncated_string =
        CenterTruncateToBuffer(string, keep_count, string_buffer);
  } else {
    keep_count = string.length();
    auto string_buffer_piece = base::span(string_buffer).first(keep_count);
    string.CopyTo(string_buffer_piece, 0);
    truncated_string = string_buffer_piece;
  }

  float width = StringWidth(font, truncated_string);
  if (width <= max_width)
    return string;

  unsigned keep_count_for_largest_known_to_fit = 0;
  float width_for_largest_known_to_fit = current_ellipsis_width;

  unsigned keep_count_for_smallest_known_to_not_fit = keep_count;
  float width_for_smallest_known_to_not_fit = width;

  if (current_ellipsis_width >= max_width) {
    keep_count_for_largest_known_to_fit = 1;
    keep_count_for_smallest_known_to_not_fit = 2;
  }

  while (keep_count_for_largest_known_to_fit + 1 <
         keep_count_for_smallest_known_to_not_fit) {
    DCHECK_LE(width_for_largest_known_to_fit, max_width);
    DCHECK_GT(width_for_smallest_known_to_not_fit, max_width);

    float ratio =
        (keep_count_for_smallest_known_to_not_fit -
         keep_count_for_largest_known_to_fit) /
        (width_for_smallest_known_to_not_fit - width_for_largest_known_to_fit);
    keep_count = static_cast<unsigned>(max_width * ratio);

    if (keep_count <= keep_count_for_largest_known_to_fit) {
      keep_count = keep_count_for_largest_known_to_fit + 1;
    } else if (keep_count >= keep_count_for_smallest_known_to_not_fit) {
      keep_count = keep_count_for_smallest_known_to_not_fit - 1;
    }

    DCHECK_LT(keep_count, string.length());
    DCHECK_GT(keep_count, 0u);
    DCHECK_LT(keep_count, keep_count_for_smallest_known_to_not_fit);
    DCHECK_GT(keep_count, keep_count_for_largest_known_to_fit);

    truncated_string = truncate_to_buffer(string, keep_count,
      base::span(string_buffer));

    width = StringWidth(font, truncated_string);
    if (width <= max_width) {
      keep_count_for_largest_known_to_fit = keep_count;
      width_for_largest_known_to_fit = width;
    } else {
      keep_count_for_smallest_known_to_not_fit = keep_count;
      width_for_smallest_known_to_not_fit = width;
    }
  }

  if (!keep_count_for_largest_known_to_fit)
    keep_count_for_largest_known_to_fit = 1;

  if (keep_count != keep_count_for_largest_known_to_fit) {
    keep_count = keep_count_for_largest_known_to_fit;
    truncated_string = truncate_to_buffer(string, keep_count,
      base::span(string_buffer));
  }

  return String(truncated_string);
}

String StringTruncator::CenterTruncate(const String& string,
                                       float max_width,
                                       const Font& font) {
  return TruncateString(string, max_width, font, CenterTruncateToBuffer);
}

String StringTruncator::RightTruncate(const String& string,
                                      float max_width,
                                      const Font& font) {
  return TruncateString(string, max_width, font, RightTruncateToBuffer);
}

}  // namespace blink
