// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_COMMON_MOBILE_METRICS_MOBILE_FRIENDLINESS_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_COMMON_MOBILE_METRICS_MOBILE_FRIENDLINESS_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/public/mojom/mobile_metrics/mobile_friendliness.mojom-shared.h"

namespace mojo {

template <>
class StructTraits<blink::mojom::MobileFriendlinessDataView,
                   blink::MobileFriendliness> {
 public:
  static bool viewport_device_width(const blink::MobileFriendliness& mf) {
    return mf.viewport_device_width;
  }
  static double viewport_initial_scale_x10(
      const blink::MobileFriendliness& mf) {
    return mf.viewport_initial_scale_x10;
  }
  static int viewport_hardcoded_width(const blink::MobileFriendliness& mf) {
    return mf.viewport_hardcoded_width;
  }
  static bool allow_user_zoom(const blink::MobileFriendliness& mf) {
    return mf.allow_user_zoom;
  }
  static int small_text_ratio(const blink::MobileFriendliness& mf) {
    return mf.small_text_ratio;
  }
  static int text_content_outside_viewport_percentage(
      const blink::MobileFriendliness& mf) {
    return mf.text_content_outside_viewport_percentage;
  }
  static int bad_tap_targets_ratio(const blink::MobileFriendliness& mf) {
    return mf.bad_tap_targets_ratio;
  }
  static bool Read(blink::mojom::MobileFriendlinessDataView data,
                   blink::MobileFriendliness* mf);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_COMMON_MOBILE_METRICS_MOBILE_FRIENDLINESS_MOJOM_TRAITS_H_
