// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_JUSTIFICATION_OPPORTUNITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_JUSTIFICATION_OPPORTUNITY_H_

#include <utility>

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_justify.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// Information carried between characters when calculating justification
// opportunities.
class PLATFORM_EXPORT JustificationContext {
 public:
  JustificationContext() = default;
  bool IsAfterOpportunity() const { return is_after_opportunity_; }
  void SetAfterOpportunity(bool flag) { is_after_opportunity_ = flag; }

  // Returns a pair of flags;
  // - first: true if we should expand just before `ch`
  // - second: true if we should expand just after `ch`
  //
  // These functions don't take care of line edges, so they should be called
  // with is_after_opportunity_=true for the first character of a line, and the
  // `second` result should be ignored for the last character of a line.
  //
  // CheckOpportunity8() is for a 8-bit string.
  // CheckOpportunity16() is for a 16-bit string.
  std::pair<bool, bool> CheckOpportunity8(TextJustify method, LChar ch);
  std::pair<bool, bool> CheckOpportunity16(TextJustify method, UChar32 ch);

  // Returns the number of justification opportunities around `ch`.
  //
  // CountOpportunity8() is for a 8-bit string.
  // CountOpportunity16() is for a 16-bit string.
  inline wtf_size_t CountOpportunity8(TextJustify method, LChar ch) {
    auto [before, after] = CheckOpportunity8(method, ch);
    return (before ? 1 : 0) + (after ? 1 : 0);
  }
  inline wtf_size_t CountOpportunity16(TextJustify method, UChar32 ch) {
    auto [before, after] = CheckOpportunity16(method, ch);
    return (before ? 1 : 0) + (after ? 1 : 0);
  }

  // Returns the number of justification opportunities of `chars`.
  wtf_size_t CountOpportunities(TextJustify method,
                                base::span<const LChar> chars,
                                TextDirection);
  wtf_size_t CountOpportunities(TextJustify method,
                                base::span<const UChar> chars,
                                TextDirection);

  // Debug helper.
  String ToString() const;

 private:
  // Type of the previously processed character.
  enum class Type : uint8_t {
    kNormal,
    kAtomicInline,
    kCursive,
  };

  static StringView ToString(JustificationContext::Type type);
  template <typename CharType>
    requires IsStringCharType<CharType>
  std::pair<bool, bool> CheckOpportunity(TextJustify method, UChar32 ch);

  Type previous_type_ = Type::kNormal;
  // Whether the previously processed character had the after-glyph opportunity.
  bool is_after_opportunity_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_JUSTIFICATION_OPPORTUNITY_H_
