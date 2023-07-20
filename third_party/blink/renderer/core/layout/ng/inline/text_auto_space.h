// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_TEXT_AUTO_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_TEXT_AUTO_SPACE_H_

#include <unicode/umachine.h>
#include <ostream>

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class CORE_EXPORT TextAutoSpace {
 public:
  enum CharType { kOther, kIdeograph, kLetterOrNumeral };

  // Returns the `CharType` according to:
  // https://drafts.csswg.org/css-text-4/#text-spacing-classes
  static CharType GetType(UChar32 ch);
};

CORE_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                     TextAutoSpace::CharType);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_TEXT_AUTO_SPACE_H_
