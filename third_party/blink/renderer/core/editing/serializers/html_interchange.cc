/*
 * Copyright (C) 2004, 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/serializers/html_interchange.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String ConvertHtmlTextToInterchangeFormat(
    const String& in,
    const Text& node,
    IsAtSelectionStart is_at_selection_start,
    IsAtSelectionEnd is_at_selection_end) {
  // Assume all the text comes from node.
  if (node.GetLayoutObject() &&
      node.GetLayoutObject()->StyleRef().ShouldPreserveBreaks()) {
    return in;
  }

  const char kConvertedSpaceString[] = "<span>\xA0</span>";
  static_assert((static_cast<LChar>('\xA0') == uchar::kNoBreakSpace),
                "\\xA0 should be non-breaking space");

  StringBuilder s;

  wtf_size_t i = 0;
  wtf_size_t consumed = 0;
  while (i < in.length()) {
    consumed = 1;
    if (IsCollapsibleWhitespace(in[i])) {
      // count number of adjoining spaces
      wtf_size_t j = i + 1;
      while (j < in.length() && IsCollapsibleWhitespace(in[j]))
        j++;
      wtf_size_t count = j - i;
      consumed = count;
      while (count) {
        wtf_size_t add = count % 3;
        switch (add) {
          case 0:
            s.Append(kConvertedSpaceString);
            s.Append(' ');
            s.Append(kConvertedSpaceString);
            add = 3;
            break;
          case 1:
            if (RuntimeEnabledFeatures::
                    NoNbspForInterElementSpaceOnCopyEnabled()) {
              // A lone space is only trimmed at the selection's edges. Convert
              // it to a non-breaking space there; an interior space (e.g.
              // between two inline elements) stays an ordinary breaking space.
              if ((i == 0 && is_at_selection_start.value()) ||
                  (i + 1 == in.length() && is_at_selection_end.value())) {
                s.Append(kConvertedSpaceString);
              } else {
                s.Append(' ');
              }
            } else {
              // Legacy behavior: a lone space at either end of the string is
              // always converted to a non-breaking space.
              if (i == 0 || i + 1 == in.length()) {
                s.Append(kConvertedSpaceString);
              } else {
                s.Append(' ');
              }
            }
            break;
          case 2:
            // A run of two or more collapsible spaces would collapse back to a
            // single space when re-parsed, so at least one must always become a
            // non-breaking space to survive the round trip, regardless of the
            // selection edges. The selection-edge check therefore only applies
            // to the lone-space case above.
            if (i == 0) {
              // at start of string
              s.Append(kConvertedSpaceString);
              s.Append(' ');
            } else if (i + 2 == in.length()) {
              // at end of string
              s.Append(kConvertedSpaceString);
              s.Append(kConvertedSpaceString);
            } else {
              s.Append(kConvertedSpaceString);
              s.Append(' ');
            }
            break;
        }
        count -= add;
      }
    } else {
      s.Append(in[i]);
    }
    i += consumed;
  }

  return s.ToString();
}

}  // namespace blink
