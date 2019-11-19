// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_optical_sizing.h"

namespace blink {

String ToString(OpticalSizing font_optical_sizing) {
  switch (font_optical_sizing) {
    case OpticalSizing::kAutoOpticalSizing:
      return "Auto";
    case OpticalSizing::kNoneOpticalSizing:
      return "None";
  }
  return "Unknown";
}

}  // namespace blink
