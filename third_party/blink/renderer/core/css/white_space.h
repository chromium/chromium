// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_WHITE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_WHITE_SPACE_H_

#include <cstdint>

namespace blink {

//
// This file contains definitions of the `white-space` shorthand property and
// its longhands.
// https://w3c.github.io/csswg-drafts/css-text-4/#propdef-white-space
//

//
// The `white-space-collapse` property.
// https://w3c.github.io/csswg-drafts/css-text-4/#white-space-collapsing
//
enum class WhiteSpaceCollapse : uint8_t {
  kCollapse = 0,
  kPreserve = 1,
  // `KPreserve` is a bit-flag, but bit 2 is shared by two different behaviors
  // below to save memory. Use functions below instead of direct comparisons.
  kPreserveBreaks = 2,
  kBreakSpaces = kPreserve | 2,
  // Ensure `kWhiteSpaceCollapseBits` can hold all values.
};

// Ensure this is in sync with `css_properties.json5`.
static constexpr int kWhiteSpaceCollapseBits = 2;
static constexpr uint8_t kWhiteSpaceCollapseMask =
    (1 << kWhiteSpaceCollapseBits) - 1;

inline bool IsWhiteSpaceCollapseAny(WhiteSpaceCollapse value,
                                    WhiteSpaceCollapse flags) {
  return static_cast<uint8_t>(value) & static_cast<uint8_t>(flags);
}

// Whether to collapse or preserve all whitespaces: spaces (U+0020), tabs
// (U+0009), and segment breaks.
// https://w3c.github.io/csswg-drafts/css-text-4/#white-space
inline bool ShouldPreserveWhiteSpaces(WhiteSpaceCollapse collapse) {
  return IsWhiteSpaceCollapseAny(collapse, WhiteSpaceCollapse::kPreserve);
}
inline bool ShouldCollapseWhiteSpaces(WhiteSpaceCollapse collapse) {
  return !ShouldPreserveWhiteSpaces(collapse);
}
// Whether to collapse or preserve segment breaks.
// https://w3c.github.io/csswg-drafts/css-text-4/#segment-break
inline bool ShouldPreserveBreaks(WhiteSpaceCollapse collapse) {
  return collapse != WhiteSpaceCollapse::kCollapse;
}
inline bool ShouldCollapseBreaks(WhiteSpaceCollapse collapse) {
  return !ShouldPreserveBreaks(collapse);
}
inline bool ShouldBreakSpaces(WhiteSpaceCollapse collapse) {
  return collapse == WhiteSpaceCollapse::kBreakSpaces;
}

//
// The `text-wrap` property.
// https://w3c.github.io/csswg-drafts/css-text-4/#propdef-text-wrap
//
enum class TextWrap : uint8_t {
  kWrap = 0,
  kNoWrap = 1,
  kBalance = 2,
  kPretty = 3,
  // Ensure `kTextWrapBits` can hold all values.
};

// Ensure this is in sync with `css_properties.json5`.
static constexpr int kTextWrapBits = 2;

inline bool ShouldWrapLine(TextWrap wrap) {
  return wrap != TextWrap::kNoWrap;
}

//
// The `white-space` property.
// https://w3c.github.io/csswg-drafts/css-text-4/#propdef-white-space
//
// `EWhiteSpace` is represented by bit-flags of combinations of all possible
// longhand values. Thus `ToWhiteSpace()` may return values that are not defined
// as the `EWhiteSpace` value. `IsValidWhiteSpace()` can check if a value is one
// of pre-defined keywords.
//
constexpr uint8_t ToWhiteSpaceValue(WhiteSpaceCollapse collapse,
                                    TextWrap wrap) {
  return static_cast<uint8_t>(collapse) |
         (static_cast<uint8_t>(wrap) << kWhiteSpaceCollapseBits);
}

enum class EWhiteSpace : uint8_t {
  kNormal = ToWhiteSpaceValue(WhiteSpaceCollapse::kCollapse, TextWrap::kWrap),
  kNowrap = ToWhiteSpaceValue(WhiteSpaceCollapse::kCollapse, TextWrap::kNoWrap),
  kPre = ToWhiteSpaceValue(WhiteSpaceCollapse::kPreserve, TextWrap::kNoWrap),
  kPreLine =
      ToWhiteSpaceValue(WhiteSpaceCollapse::kPreserveBreaks, TextWrap::kWrap),
  kPreWrap = ToWhiteSpaceValue(WhiteSpaceCollapse::kPreserve, TextWrap::kWrap),
  kBreakSpaces =
      ToWhiteSpaceValue(WhiteSpaceCollapse::kBreakSpaces, TextWrap::kWrap),
};

static_assert(kWhiteSpaceCollapseBits + kTextWrapBits <=
              sizeof(EWhiteSpace) * 8);

// Convert longhands of `white-space` to `EWhiteSpace`. The return value may not
// be one of the defined enum values. Please see the comment above.
inline EWhiteSpace ToWhiteSpace(WhiteSpaceCollapse collapse, TextWrap wrap) {
  return static_cast<EWhiteSpace>(ToWhiteSpaceValue(collapse, wrap));
}

inline bool IsValidWhiteSpace(EWhiteSpace whitespace) {
  return whitespace == EWhiteSpace::kNormal ||
         whitespace == EWhiteSpace::kNowrap ||
         whitespace == EWhiteSpace::kPre ||
         whitespace == EWhiteSpace::kPreLine ||
         whitespace == EWhiteSpace::kPreWrap ||
         whitespace == EWhiteSpace::kBreakSpaces;
}

// Convert `EWhiteSpace` to longhands.
inline WhiteSpaceCollapse ToWhiteSpaceCollapse(EWhiteSpace whitespace) {
  return static_cast<WhiteSpaceCollapse>(static_cast<uint8_t>(whitespace) &
                                         kWhiteSpaceCollapseMask);
}
inline TextWrap ToTextWrap(EWhiteSpace whitespace) {
  return static_cast<TextWrap>(static_cast<uint8_t>(whitespace) >>
                               kWhiteSpaceCollapseBits);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_WHITE_SPACE_H_
