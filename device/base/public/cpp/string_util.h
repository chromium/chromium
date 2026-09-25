// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_PUBLIC_CPP_STRING_UTIL_H_
#define DEVICE_BASE_PUBLIC_CPP_STRING_UTIL_H_

#include <string>
#include <string_view>

#include "device/base/device_base_export.h"

namespace device {

// Returns true if the string contains any Unicode Graphic characters as defined
// by http://www.unicode.org/reports/tr18/#graph
DEVICE_BASE_EXPORT bool HasGraphicCharacter(std::string_view s);

// Formats an arbitrary string for display in UI surfaces:
// - Collapses consecutive whitespace to a single space, stripping
// leading/trailing.
// - Truncates to max 64 code units at safe code point / surrogate boundary,
//   appending an ellipsis (U+2026) if truncated.
// - Balances directional isolates (UAX #9).
// - Wraps in First Strong Isolate (U+2068) and Pop Directional Isolate
// (U+2069).
DEVICE_BASE_EXPORT std::u16string ContainStringForDisplay(
    std::u16string_view str);

}  // namespace device

#endif  // DEVICE_BASE_PUBLIC_CPP_STRING_UTIL_H_
