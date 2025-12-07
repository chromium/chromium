// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_STATE_MACHINES_STATE_MACHINE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_STATE_MACHINES_STATE_MACHINE_UTIL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink {

// Returns true if there is a grapheme boundary between prevCodePoint and
// nextCodePoint.
// DO NOT USE this function directly since this doesn't care about preceding
// regional indicator symbols. Use ForwardGraphemeBoundaryStateMachine or
// BackwardGraphemeBoundaryStateMachine instead.
CORE_EXPORT bool IsGraphemeBreak(UChar32 prev_code_point,
                                 UChar32 next_code_point);

// Returns `\p{Extended_Pictographic}` for UAX#29 [GB11].
// This is changed to `IsExtendedPictographic()` at Unicode 11.
// [GB11]: https://unicode.org/reports/tr29/tr29-33.html#GB11
inline bool IsExtendedPictographicGb11(UChar32 code_point) {
  // TODO(crbug.com/445495210): `IsExtendedPictographic()` alone should be
  // sufficient but it's not because the `BackspaceStateMachine` doesn't support
  // `Emoji_Modifier` as [`Extended`] (e.g., U+1F3FB).
  // [`Extended`]: https://unicode.org/reports/tr29/tr29-33.html#Extend0
  return RuntimeEnabledFeatures::EditEmojiUnicode11Enabled()
             ? Character::IsExtendedPictographic(code_point) ||
                   Character::IsEmoji(code_point)
             : Character::IsEmoji(code_point);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_STATE_MACHINES_STATE_MACHINE_UTIL_H_
