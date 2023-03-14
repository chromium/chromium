// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_WHITE_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_WHITE_SPACE_H_

#include <cstdint>

namespace blink {

// Semantic behaviors of the `white-space` property. All values of the
// `white-space` property can be expressed by combinations of these bits.
enum class WhiteSpaceBehavior : uint8_t {
  kPreserveSpacesAndTabs = 1,
  kPreserveBreaks = 2,
  kPreserveAllWhiteSpaces = kPreserveSpacesAndTabs | kPreserveBreaks,
  kNoWrapLine = 4,
  kBreakSpaces = 8,
  // Ensure `kWhiteSpaceBehaviorBits` has enough bits.
};

// Ensure this is in sync with `css_properties.json5`.
static constexpr int kWhiteSpaceBehaviorBits = 4;

constexpr WhiteSpaceBehavior operator|(WhiteSpaceBehavior a,
                                       WhiteSpaceBehavior b) {
  return static_cast<WhiteSpaceBehavior>(static_cast<unsigned>(a) |
                                         static_cast<unsigned>(b));
}

//
// The `white-space` property.
// https://w3c.github.io/csswg-drafts/css-text-4/#propdef-white-space
//
enum class EWhiteSpace : uint8_t {
  kNormal = 0,
  kNowrap = static_cast<uint8_t>(WhiteSpaceBehavior::kNoWrapLine),
  kPre = static_cast<uint8_t>(WhiteSpaceBehavior::kPreserveAllWhiteSpaces |
                              WhiteSpaceBehavior::kNoWrapLine),
  kPreLine = static_cast<uint8_t>(WhiteSpaceBehavior::kPreserveBreaks),
  kPreWrap = static_cast<uint8_t>(WhiteSpaceBehavior::kPreserveAllWhiteSpaces),
  kBreakSpaces =
      static_cast<uint8_t>(WhiteSpaceBehavior::kPreserveAllWhiteSpaces |
                           WhiteSpaceBehavior::kBreakSpaces),
};

// Ensure this is in sync with `css_properties.json5`.
static constexpr int kEWhiteSpaceBits = kWhiteSpaceBehaviorBits;

//
// Functions for semantic behaviors.
//
// Note that functions in `ComputedStyle` are preferred over these functions
// because the `white-space` property may become a shorthand in future. When
// that happens, these functions may be removed, or less performant than
// functions in `ComputedStyle`.
// https://w3c.github.io/csswg-drafts/css-text-4/#propdef-white-space
//
inline bool IsWhiteSpaceAny(EWhiteSpace value, WhiteSpaceBehavior flags) {
  return static_cast<uint8_t>(value) & static_cast<uint8_t>(flags);
}

//
// `white-space-collapse`: Collapsing/preserving white-spaces.
// https://w3c.github.io/csswg-drafts/css-text-4/#propdef-white-space-collapse
//
inline bool ShouldPreserveBreaks(EWhiteSpace value) {
  return IsWhiteSpaceAny(value, WhiteSpaceBehavior::kPreserveBreaks);
}
inline bool ShouldPreserveSpacesAndTabs(EWhiteSpace value) {
  return IsWhiteSpaceAny(value, WhiteSpaceBehavior::kPreserveSpacesAndTabs);
}
inline bool ShouldCollapseBreaks(EWhiteSpace value) {
  return !ShouldPreserveBreaks(value);
}
inline bool ShouldCollapseSpacesAndTabs(EWhiteSpace value) {
  return !ShouldPreserveSpacesAndTabs(value);
}
inline bool ShouldBreakSpaces(EWhiteSpace value) {
  return IsWhiteSpaceAny(value, WhiteSpaceBehavior::kBreakSpaces);
}

//
// `text-wrap`: Text Wrapping.
// https://w3c.github.io/csswg-drafts/css-text-4/#propdef-text-wrap
//
inline bool ShouldWrapLine(EWhiteSpace value) {
  return !IsWhiteSpaceAny(value, WhiteSpaceBehavior::kNoWrapLine);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_WHITE_SPACE_H_
