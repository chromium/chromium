// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCH_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCH_FLAGS_H_

#include <cstdint>

namespace blink {

// During rule-matching, we collect some information about what the match
// result depended on. This is useful for for e.g. targeted invalidation when
// hover-state (etc) changes.
//
// If you add any new flags here, see if you need to update
// FlagsCauseInvalidation().
enum class MatchFlag {
  // The following flags are set when the given pseudo-class is encountered
  // in a right-most compound selector:

  // :-webkit-drag
  kAffectedByDrag = 1 << 0,
  // :focus-within
  kAffectedByFocusWithin = 1 << 1,
  // :hover
  kAffectedByHover = 1 << 2,
  // :active
  kAffectedByActive = 1 << 3,
  // @starting-style
  kAffectedByStartingStyle = 1 << 4,
  // Unconditionally matched :hover in rightmost compound to precompute hover
  // styles for the compositor.
  kPrecomputedHoverMatch = 1 << 5,
  // Set when matching :hover outside the rightmost compound or in nested
  // complex selectors. Will cancel out a kPrecomputedHoverMatch.
  kCancelPrecomputedHoverMatch = 1 << 6,
  // Matched :hover, not in subject.
  kAffectedByNonSubjectHoverSelector = 1 << 7,
};

using MatchFlags = uint8_t;

inline constexpr MatchFlags kPrecomputedHoverMask =
    static_cast<MatchFlags>(MatchFlag::kPrecomputedHoverMatch) |
    static_cast<MatchFlags>(MatchFlag::kCancelPrecomputedHoverMatch);

// #define DUMP_PRECOMPUTED_HOVER

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCH_FLAGS_H_
