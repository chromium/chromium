// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_width_variant.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String ToString(FontWidthVariant variant) {
  switch (variant) {
    case FontWidthVariant::kRegular:
      return "Regular";
    case FontWidthVariant::kHalf:
      return "Half";
    case FontWidthVariant::kThird:
      return "Third";
    case FontWidthVariant::kQuarter:
      return "Quarter";
  }
  return "Unknown";
}

}  // namespace blink
