// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"

namespace blink {

bool MobileFriendliness::operator==(const MobileFriendliness& other) const {
  return viewport_device_width == other.viewport_device_width &&
         viewport_initial_scale == other.viewport_initial_scale &&
         viewport_hardcoded_width == other.viewport_hardcoded_width &&
         allow_user_zoom == other.allow_user_zoom;
}

}  // namespace blink
