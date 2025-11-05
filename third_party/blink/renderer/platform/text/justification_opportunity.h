// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_JUSTIFICATION_OPPORTUNITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_JUSTIFICATION_OPPORTUNITY_H_

#include <utility>

#include "third_party/blink/renderer/platform/text/text_justify.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// Returns a pair of flags;
// - first: true if we should expand just before `ch`
// - second: true if we should expand just after `ch`
//
// These functions don't take care of line edges, so they should be called
// with is_after_opportunity=true for the first character of a line, and the
// `second` result should be ignored for the last character of a line.
//
// CheckJustificationOpportunity8() is for a 8-bit string.
// CheckJustificationOpportunity16() is for a 16-bit string.
std::pair<bool, bool> CheckJustificationOpportunity8(
    TextJustify method,
    LChar ch,
    bool& is_after_opportunity);
std::pair<bool, bool> CheckJustificationOpportunity16(
    TextJustify method,
    UChar32 ch,
    bool& is_after_opportunity);

// Returns the number of justification opportunities around `ch`.
//
// CountJustificationOpportunity8() is for a 8-bit string.
// CountJustificationOpportunity16() is for a 16-bit string.
inline wtf_size_t CountJustificationOpportunity8(TextJustify method,
                                                 LChar ch,
                                                 bool& is_after_opportunity) {
  auto [before, after] =
      CheckJustificationOpportunity8(method, ch, is_after_opportunity);
  return (before ? 1 : 0) + (after ? 1 : 0);
}
inline wtf_size_t CountJustificationOpportunity16(TextJustify method,
                                                  UChar32 ch,
                                                  bool& is_after_opportunity) {
  auto [before, after] =
      CheckJustificationOpportunity16(method, ch, is_after_opportunity);
  return (before ? 1 : 0) + (after ? 1 : 0);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_JUSTIFICATION_OPPORTUNITY_H_
