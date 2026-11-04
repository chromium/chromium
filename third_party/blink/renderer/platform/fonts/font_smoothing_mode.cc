// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_smoothing_mode.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String ToString(FontSmoothingMode mode) {
  switch (mode) {
    case FontSmoothingMode::kAuto:
      return "Auto";
    case FontSmoothingMode::kNone:
      return "None";
    case FontSmoothingMode::kAntialiased:
      return "Antialiased";
    case FontSmoothingMode::kSubpixelAntialiased:
      return "SubpixelAntialiased";
  }
  return "Unknown";
}

}  // namespace blink
