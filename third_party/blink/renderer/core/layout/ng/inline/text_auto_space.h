// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_TEXT_AUTO_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_TEXT_AUTO_SPACE_H_

#include <unicode/umachine.h>
#include <ostream>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_segment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_items_data.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

struct NGInlineItemsData;

class CORE_EXPORT TextAutoSpace {
  STACK_ALLOCATED();

 public:
  explicit TextAutoSpace(const NGInlineItemsData& data);

  // True if this may apply auto-spacing. If this is false, it's safe to skip
  // calling `Apply()`.
  bool MayApply() const { return !ranges_.empty(); }

  // Apply auto-spacing as per CSS Text:
  // https://drafts.csswg.org/css-text-4/#propdef-text-autospace
  //
  // The `data` must be the same instance as the one given to the constructor.
  //
  // If `offsets_out` is not null, the offsets of auto-space points are added to
  // it without applying auto-spacing. This is for tseting-purpose.
  void Apply(NGInlineItemsData& data,
             Vector<wtf_size_t>* offsets_out = nullptr);
  void ApplyIfNeeded(NGInlineItemsData& data,
                     Vector<wtf_size_t>* offsets_out = nullptr) {
    if (UNLIKELY(MayApply())) {
      Apply(data, offsets_out);
    }
  }

  enum CharType { kOther, kIdeograph, kLetterOrNumeral };

  // Returns the `CharType` according to:
  // https://drafts.csswg.org/css-text-4/#text-spacing-classes
  static CharType GetType(UChar32 ch);

  // `GetType` and advance the `offset` by one character (grapheme cluster.)
  static CharType GetTypeAndNext(const String& text, wtf_size_t& offset);
  // `GetType` of the character before `offset`.
  static CharType GetPrevType(const String& text, wtf_size_t offset);

  // `CharType::kIdeograph` is `USCRIPT_HAN`, except characters in this range
  // may be other scripts.
  constexpr static UChar32 kNonHanIdeographMin = 0x3041;
  constexpr static UChar32 kNonHanIdeographMax = 0x31FF;

 private:
  void Initialize(const NGInlineItemsData& data);

  NGInlineItemSegments::RunSegmenterRanges ranges_;
};

inline TextAutoSpace::TextAutoSpace(const NGInlineItemsData& data) {
  if (!RuntimeEnabledFeatures::CSSTextAutoSpaceEnabled()) {
    return;
  }

  if (data.text_content.Is8Bit()) {
    return;  // 8-bits never be `kIdeograph`. See `TextAutoSpaceTest`.
  }

  Initialize(data);
}

CORE_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                     TextAutoSpace::CharType);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_TEXT_AUTO_SPACE_H_
