/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/text_run_constructor.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/text/bidi_text_run.h"

namespace blink {

template <typename CharacterType>
static inline TextRun ConstructTextRunInternal(const Font& font,
                                               const CharacterType* characters,
                                               int length,
                                               const ComputedStyle& style,
                                               TextDirection direction) {
  TextRun::ExpansionBehavior expansion =
      TextRun::kAllowTrailingExpansion | TextRun::kForbidLeadingExpansion;
  bool directional_override = style.RtlOrdering() == EOrder::kVisual;
  TextRun run(characters, length, 0, 0, expansion, direction,
              directional_override);
  return run;
}

template <typename CharacterType>
static inline TextRun ConstructTextRunInternal(const Font& font,
                                               const CharacterType* characters,
                                               int length,
                                               const ComputedStyle& style,
                                               TextDirection direction,
                                               TextRunFlags flags) {
  TextDirection text_direction = direction;
  bool directional_override = style.RtlOrdering() == EOrder::kVisual;
  if (flags != kDefaultTextRunFlags) {
    if (flags & kRespectDirection)
      text_direction = style.Direction();
    if (flags & kRespectDirectionOverride)
      directional_override |= IsOverride(style.GetUnicodeBidi());
  }

  TextRun::ExpansionBehavior expansion =
      TextRun::kAllowTrailingExpansion | TextRun::kForbidLeadingExpansion;
  TextRun run(characters, length, 0, 0, expansion, text_direction,
              directional_override);
  return run;
}

TextRun ConstructTextRun(const Font& font,
                         const LChar* characters,
                         int length,
                         const ComputedStyle& style,
                         TextDirection direction) {
  return ConstructTextRunInternal(font, characters, length, style, direction);
}

TextRun ConstructTextRun(const Font& font,
                         const UChar* characters,
                         int length,
                         const ComputedStyle& style,
                         TextDirection direction) {
  return ConstructTextRunInternal(font, characters, length, style, direction);
}

TextRun ConstructTextRun(const Font& font,
                         const LayoutText* text,
                         unsigned offset,
                         unsigned length,
                         const ComputedStyle& style,
                         TextDirection direction) {
  DCHECK_LE(offset + length, text->TextLength());
  if (text->HasEmptyText())
    return ConstructTextRunInternal(font, static_cast<const LChar*>(nullptr), 0,
                                    style, direction);
  if (text->Is8Bit())
    return ConstructTextRunInternal(font, text->Characters8() + offset, length,
                                    style, direction);
  return ConstructTextRunInternal(font, text->Characters16() + offset, length,
                                  style, direction);
}

TextRun ConstructTextRun(const Font& font,
                         const String& string,
                         const ComputedStyle& style,
                         TextDirection direction,
                         TextRunFlags flags) {
  unsigned length = string.length();
  if (!length)
    return ConstructTextRunInternal(font, static_cast<const LChar*>(nullptr),
                                    length, style, direction, flags);
  if (string.Is8Bit())
    return ConstructTextRunInternal(font, string.Characters8(), length, style,
                                    direction, flags);
  return ConstructTextRunInternal(font, string.Characters16(), length, style,
                                  direction, flags);
}

TextRun ConstructTextRun(const Font& font,
                         const String& string,
                         const ComputedStyle& style,
                         TextRunFlags flags) {
  return ConstructTextRun(font, string, style,
                          string.IsEmpty() || string.Is8Bit()
                              ? TextDirection::kLtr
                              : DetermineDirectionality(string),
                          flags);
}

TextRun ConstructTextRun(const Font& font,
                         const LineLayoutText text,
                         unsigned offset,
                         unsigned length,
                         const ComputedStyle& style) {
  SECURITY_DCHECK(offset + length <= text.TextLength());
  if (text.HasEmptyText()) {
    return ConstructTextRunInternal(font, static_cast<const LChar*>(nullptr), 0,
                                    style, TextDirection::kLtr);
  }
  if (text.Is8Bit()) {
    return ConstructTextRunInternal(font, text.Characters8() + offset, length,
                                    style, TextDirection::kLtr);
  }

  TextRun run = ConstructTextRunInternal(font, text.Characters16() + offset,
                                         length, style, TextDirection::kLtr);
  run.SetDirection(DirectionForRun(run));
  return run;
}

}  // namespace blink
