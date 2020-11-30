// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_BATTERY_SAVINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_BATTERY_SAVINGS_H_

namespace blink {

// These are constants for the various keywords allowed for the battery-savings
// meta element. For instance:
//
// <meta name="battery-savings" content="allow-reduced-framerate">
// These constants are bits which can be combined.
enum BatterySavings {
  kAllowReducedFrameRate = 1 << 0,
  kAllowReducedScriptSpeed = 1 << 1,
};

using BatterySavingsFlags = unsigned;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_BATTERY_SAVINGS_H_
