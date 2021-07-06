// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MOBILE_METRICS_MOBILE_FRIENDLINESS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MOBILE_METRICS_MOBILE_FRIENDLINESS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/mobile_metrics/mobile_friendliness.mojom-shared.h"

namespace blink {

// This structure contains extracted mobile friendliness metrics from the page.
// Used for UKM logging.
struct BLINK_COMMON_EXPORT MobileFriendliness {
  MobileFriendliness() = default;
  MobileFriendliness(const MobileFriendliness&) = default;
  MobileFriendliness& operator=(const MobileFriendliness&) = default;

  bool operator==(const MobileFriendliness& other) const;
  bool operator!=(const MobileFriendliness& other) const {
    return !(*this == other);
  }

  // Whether <meta name="viewport" content="width=device-width"> is specified or
  // not.
  mojom::ViewportStatus viewport_device_width = mojom::ViewportStatus::kUnknown;

  // The value specified in meta tag like <meta name="viewport"
  // content="initial-scale=1.0">.
  // Default -1 means "Unknown" and should not be sent as UKM.
  int viewport_initial_scale_x10 = -1;

  // The value specified in meta tag like <meta name="viewport"
  // content="width=500">.
  // Default -1 means "Unknown" and should not be sent as UKM.
  int viewport_hardcoded_width = -1;

  // Whether the page allows user to zoom in/out.
  // It is specified like <meta name="viewport" content="user-scalable=no">.
  mojom::ViewportStatus allow_user_zoom = mojom::ViewportStatus::kUnknown;

  // Percentage of small font size text area in all text area.
  // Default -1 means "Unknown" and should not be sent as UKM.
  int small_text_ratio = -1;

  // Percentage of pixels of text and images horizontally outside the viewport,
  // relative to the frame width.
  // Default -1 means "Unknown" and should not be sent as UKM.
  int text_content_outside_viewport_percentage = -1;

  // Percentage of tap targets whose center position is within another tap
  // target (expanded by a margin).  The detail of the algorithm is
  // go/bad-tap-target-ukm
  // Default -1 means "Unknown" and should not be sent as UKM.
  // If evaluation time budget exceeded, this will be -2.
  int bad_tap_targets_ratio = -1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MOBILE_METRICS_MOBILE_FRIENDLINESS_H_
