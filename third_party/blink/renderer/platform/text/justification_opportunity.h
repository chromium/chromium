// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_JUSTIFICATION_OPPORTUNITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_JUSTIFICATION_OPPORTUNITY_H_

#include <utility>

#include "third_party/blink/renderer/platform/text/text_justify.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// Information carried between characters when calculating justification
// opportunities.
struct JustificationContext {
  // Type of the previously processed character.
  enum class Type : uint8_t {
    kNormal,
    kAtomicInline,
    kCursive,
  };
  Type previous_type = Type::kNormal;
  // Whether the previously processed character had the after-glyph opportunity.
  bool is_after_opportunity = true;

  // Returns a pair of flags;
  // - first: true if we should expand just before `ch`
  // - second: true if we should expand just after `ch`
  //
  // These functions don't take care of line edges, so they should be called
  // with is_after_opportunity=true for the first character of a line, and the
  // `second` result should be ignored for the last character of a line.
  //
  // CheckOpportunity8() is for a 8-bit string.
  // CheckOpportunity16() is for a 16-bit string.
  std::pair<bool, bool> CheckOpportunity8(TextJustify method, LChar ch);
  std::pair<bool, bool> CheckOpportunity16(TextJustify method, UChar32 ch);

  // Debug helpers.
  static StringView ToString(JustificationContext::Type type);
  String ToString() const;
};

// Returns the number of justification opportunities around `ch`.
//
// CountJustificationOpportunity8() is for a 8-bit string.
// CountJustificationOpportunity16() is for a 16-bit string.
inline wtf_size_t CountJustificationOpportunity8(
    TextJustify method,
    LChar ch,
    JustificationContext& context) {
  auto [before, after] = context.CheckOpportunity8(method, ch);
  return (before ? 1 : 0) + (after ? 1 : 0);
}
inline wtf_size_t CountJustificationOpportunity16(
    TextJustify method,
    UChar32 ch,
    JustificationContext& context) {
  auto [before, after] = context.CheckOpportunity16(method, ch);
  return (before ? 1 : 0) + (after ? 1 : 0);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_JUSTIFICATION_OPPORTUNITY_H_
