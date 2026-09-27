// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_IME_CODE_POINT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_IME_CODE_POINT_UTILS_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// These helpers are used to convert the supplied lengths from code points
// into the UTF-16 code units.

// Returns the number of UTF-16 code units spanned by the
// `before_length_in_code_points` code points of `text` that precede
// `selection_start`, or std::nullopt if that range splits a surrogate pair.
CORE_EXPORT std::optional<int> CalculateBeforeDeletionLengthsInCodePoints(
    const String& text,
    int before_length_in_code_points,
    int selection_start);

// Returns the number of UTF-16 code units spanned by the
// `after_length_in_code_points` code points of `text` that follow
// `selection_end`, or std::nullopt if that range splits a surrogate pair.
CORE_EXPORT std::optional<int> CalculateAfterDeletionLengthsInCodePoints(
    const String& text,
    int after_length_in_code_points,
    int selection_end);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_IME_CODE_POINT_UTILS_H_
