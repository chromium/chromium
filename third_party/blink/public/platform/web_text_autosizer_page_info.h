// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXT_AUTOSIZER_PAGE_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXT_AUTOSIZER_PAGE_INFO_H_

namespace blink {

struct WebTextAutosizerPageInfo {
  WebTextAutosizerPageInfo() = default;

  int main_frame_width;  // LocalFrame width in density-independent pixels.
  int main_frame_layout_width;  // Layout width in CSS pixels.
  float device_scale_adjustment;
};

inline bool operator==(const WebTextAutosizerPageInfo& lhs,
                       const WebTextAutosizerPageInfo& rhs) {
  return lhs.main_frame_width == rhs.main_frame_width &&
         lhs.main_frame_layout_width == rhs.main_frame_layout_width &&
         lhs.device_scale_adjustment == rhs.device_scale_adjustment;
}

inline bool operator!=(const WebTextAutosizerPageInfo& lhs,
                       const WebTextAutosizerPageInfo& rhs) {
  return lhs.main_frame_width != rhs.main_frame_width ||
         lhs.main_frame_layout_width != rhs.main_frame_layout_width ||
         lhs.device_scale_adjustment != rhs.device_scale_adjustment;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXT_AUTOSIZER_PAGE_INFO_H_
