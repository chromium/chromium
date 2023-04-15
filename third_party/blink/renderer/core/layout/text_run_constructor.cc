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

#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"

namespace blink {

template <typename CharacterType>
static inline TextRun ConstructTextRunInternal(const CharacterType* characters,
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

TextRun ConstructTextRun(const String& string,
                         const ComputedStyle& style,
                         TextRunFlags flags) {
  TextDirection direction =
      string.empty() || string.Is8Bit()
          ? TextDirection::kLtr
          : BidiParagraph::BaseDirectionForStringOrLtr(string);
  unsigned length = string.length();
  if (!length) {
    return ConstructTextRunInternal(static_cast<const LChar*>(nullptr), length,
                                    style, direction, flags);
  }
  if (string.Is8Bit())
    return ConstructTextRunInternal(string.Characters8(), length, style,
                                    direction, flags);
  return ConstructTextRunInternal(string.Characters16(), length, style,
                                  direction, flags);
}

}  // namespace blink
