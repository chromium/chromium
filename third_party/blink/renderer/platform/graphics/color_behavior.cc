// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/color_behavior.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image_metrics.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"

namespace blink {

bool ColorBehavior::operator==(const ColorBehavior& other) const {
  if (type_ != other.type_)
    return false;
  return true;
}

bool ColorBehavior::operator!=(const ColorBehavior& other) const {
  return !(*this == other);
}

}  // namespace blink
