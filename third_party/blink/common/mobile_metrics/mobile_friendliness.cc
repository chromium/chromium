// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"

namespace blink {

bool MobileFriendliness::operator==(const MobileFriendliness& other) const {
  return viewport_device_width == other.viewport_device_width &&
         viewport_initial_scale_x10 == other.viewport_initial_scale_x10 &&
         viewport_hardcoded_width == other.viewport_hardcoded_width &&
         allow_user_zoom == other.allow_user_zoom &&
         small_text_ratio == other.small_text_ratio &&
         text_content_outside_viewport_percentage ==
             other.text_content_outside_viewport_percentage;
}

}  // namespace blink
