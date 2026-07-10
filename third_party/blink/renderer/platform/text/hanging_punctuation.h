// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_HANGING_PUNCTUATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_HANGING_PUNCTUATION_H_

#include <unicode/uchar.h>

#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

enum class HangingPunctuation : unsigned {
  kNone = 0,
  kFirst = 1,
  kLast = 2,
  kAllowEnd = 4,
};

inline HangingPunctuation operator|(HangingPunctuation a,
                                    HangingPunctuation b) {
  return static_cast<HangingPunctuation>(static_cast<unsigned>(a) |
                                         static_cast<unsigned>(b));
}
inline HangingPunctuation& operator|=(HangingPunctuation& a,
                                      HangingPunctuation b) {
  return a = a | b;
}
inline HangingPunctuation operator^(HangingPunctuation a,
                                    HangingPunctuation b) {
  return static_cast<HangingPunctuation>(static_cast<unsigned>(a) ^
                                         static_cast<unsigned>(b));
}
inline HangingPunctuation& operator^=(HangingPunctuation& a,
                                      HangingPunctuation b) {
  return a = a ^ b;
}
inline HangingPunctuation operator&(HangingPunctuation a,
                                    HangingPunctuation b) {
  return static_cast<HangingPunctuation>(static_cast<unsigned>(a) &
                                         static_cast<unsigned>(b));
}
inline HangingPunctuation& operator&=(HangingPunctuation& a,
                                      HangingPunctuation b) {
  return a = a & b;
}
inline HangingPunctuation operator~(HangingPunctuation x) {
  return static_cast<HangingPunctuation>(~static_cast<unsigned>(x));
}

PLATFORM_EXPORT bool IsHangingPunctuation(UChar32 ch, HangingPunctuation mask);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_HANGING_PUNCTUATION_H_
