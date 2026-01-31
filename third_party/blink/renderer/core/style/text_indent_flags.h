// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_INDENT_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_INDENT_FLAGS_H_

#include <type_traits>

namespace blink {

enum class TextIndentFlags : uint8_t {
  kDefault = 0,
  kEachLine = 1 << 0,
  kHanging = 1 << 1,

  // When adding new values, ensure that the `TextIndentFlags` in
  // `computed_style_extra_fields.json5` has enough capacity.
};

inline constexpr TextIndentFlags operator|(TextIndentFlags a,
                                           TextIndentFlags b) {
  using T = std::underlying_type_t<TextIndentFlags>;
  return static_cast<TextIndentFlags>(static_cast<T>(a) | static_cast<T>(b));
}

inline constexpr TextIndentFlags operator&(TextIndentFlags a,
                                           TextIndentFlags b) {
  using T = std::underlying_type_t<TextIndentFlags>;
  return static_cast<TextIndentFlags>(static_cast<T>(a) & static_cast<T>(b));
}

inline constexpr TextIndentFlags& operator|=(TextIndentFlags& a,
                                             TextIndentFlags b) {
  a = a | b;
  return a;
}

inline constexpr bool operator!(TextIndentFlags a) {
  using T = std::underlying_type_t<TextIndentFlags>;
  return !static_cast<T>(a);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_INDENT_FLAGS_H_
