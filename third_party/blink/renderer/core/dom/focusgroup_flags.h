// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSGROUP_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSGROUP_FLAGS_H_

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Element;

enum FocusgroupFlags : int8_t {
  kNone = 0,
  kExtend = 1 << 0,
  kHorizontal = 1 << 1,
  kVertical = 1 << 2,
  kWrapHorizontally = 1 << 3,
  kWrapVertically = 1 << 4,
  kGrid = 1 << 5,
  kExplicitlyNone = 1 << 6,
};

inline constexpr FocusgroupFlags operator&(FocusgroupFlags a,
                                           FocusgroupFlags b) {
  return static_cast<FocusgroupFlags>(static_cast<int8_t>(a) &
                                      static_cast<int8_t>(b));
}

inline constexpr FocusgroupFlags operator|(FocusgroupFlags a,
                                           FocusgroupFlags b) {
  return static_cast<FocusgroupFlags>(static_cast<int8_t>(a) |
                                      static_cast<int8_t>(b));
}

inline FocusgroupFlags& operator|=(FocusgroupFlags& a, FocusgroupFlags b) {
  return a = a | b;
}

inline FocusgroupFlags& operator&=(FocusgroupFlags& a, FocusgroupFlags b) {
  return a = a & b;
}

inline constexpr FocusgroupFlags operator~(FocusgroupFlags flags) {
  return static_cast<FocusgroupFlags>(~static_cast<int8_t>(flags));
}

namespace focusgroup {
inline bool IsFocusgroup(FocusgroupFlags flags) {
  return flags != FocusgroupFlags::kNone &&
         !(flags & FocusgroupFlags::kExplicitlyNone);
}

// Implemented based on this explainer:
// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/Focusgroup/explainer.md
FocusgroupFlags ParseFocusgroup(const Element* element,
                                const AtomicString& input);

}  // namespace focusgroup

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSGROUP_FLAGS_H_