// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"

namespace blink {

void LCPRectInfo::OutputToTraceValue(TracedValue& value) const {
  value.SetInteger("frame_x", frame_rect_info_.x());
  value.SetInteger("frame_y", frame_rect_info_.y());
  value.SetInteger("frame_width", frame_rect_info_.width());
  value.SetInteger("frame_height", frame_rect_info_.height());
  value.SetInteger("root_x", root_rect_info_.x());
  value.SetInteger("root_y", root_rect_info_.y());
  value.SetInteger("root_width", root_rect_info_.width());
  value.SetInteger("root_height", root_rect_info_.height());
}

}  // namespace blink
