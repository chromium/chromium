// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_DECORATION_SKIP_SPACES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_DECORATION_SKIP_SPACES_H_

namespace blink {

enum class TextDecorationSkipSpaces : unsigned {
  kNone = 0,
  kStart = 1,
  kEnd = 2,
  kAll = 4,
  kStartEnd = kStart | kEnd,
};

inline TextDecorationSkipSpaces operator|(TextDecorationSkipSpaces a,
                                          TextDecorationSkipSpaces b) {
  return static_cast<TextDecorationSkipSpaces>(static_cast<unsigned>(a) |
                                               static_cast<unsigned>(b));
}
inline TextDecorationSkipSpaces& operator|=(TextDecorationSkipSpaces& a,
                                            TextDecorationSkipSpaces b) {
  return a = a | b;
}
inline TextDecorationSkipSpaces operator^(TextDecorationSkipSpaces a,
                                          TextDecorationSkipSpaces b) {
  return static_cast<TextDecorationSkipSpaces>(static_cast<unsigned>(a) ^
                                               static_cast<unsigned>(b));
}
inline TextDecorationSkipSpaces& operator^=(TextDecorationSkipSpaces& a,
                                            TextDecorationSkipSpaces b) {
  return a = a ^ b;
}
inline TextDecorationSkipSpaces operator&(TextDecorationSkipSpaces a,
                                          TextDecorationSkipSpaces b) {
  return static_cast<TextDecorationSkipSpaces>(static_cast<unsigned>(a) &
                                               static_cast<unsigned>(b));
}
inline TextDecorationSkipSpaces& operator&=(TextDecorationSkipSpaces& a,
                                            TextDecorationSkipSpaces b) {
  return a = a & b;
}
inline TextDecorationSkipSpaces operator~(TextDecorationSkipSpaces x) {
  return static_cast<TextDecorationSkipSpaces>(~static_cast<unsigned>(x));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_DECORATION_SKIP_SPACES_H_
