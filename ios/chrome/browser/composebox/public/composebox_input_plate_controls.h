// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_INPUT_PLATE_CONTROLS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_INPUT_PLATE_CONTROLS_H_

#import <type_traits>

// The available controls in the input plate.
enum class ComposeboxInputPlateControls : unsigned int {
  kNone = 0,
  kPlus = 1 << 0,
  kAIM = 1 << 1,
  kCreateImage = 1 << 2,
  kLens = 1 << 3,
  kVoice = 1 << 4,
  kSend = 1 << 5,
  kLeadingImage = 1 << 6,
};

constexpr ComposeboxInputPlateControls operator|(
    ComposeboxInputPlateControls lhs,
    ComposeboxInputPlateControls rhs) {
  using T = std::underlying_type_t<ComposeboxInputPlateControls>;
  return static_cast<ComposeboxInputPlateControls>(static_cast<T>(lhs) |
                                                   static_cast<T>(rhs));
}

constexpr ComposeboxInputPlateControls operator&(
    ComposeboxInputPlateControls lhs,
    ComposeboxInputPlateControls rhs) {
  using T = std::underlying_type_t<ComposeboxInputPlateControls>;
  return static_cast<ComposeboxInputPlateControls>(static_cast<T>(lhs) &
                                                   static_cast<T>(rhs));
}

constexpr bool operator!(ComposeboxInputPlateControls flags) {
  return flags == ComposeboxInputPlateControls::kNone;
}

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_INPUT_PLATE_CONTROLS_H_
