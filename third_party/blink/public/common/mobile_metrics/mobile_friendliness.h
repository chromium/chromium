// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MOBILE_METRICS_MOBILE_FRIENDLINESS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MOBILE_METRICS_MOBILE_FRIENDLINESS_H_

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// This structure contains extracted mobile friendliness metrics from the page.
// Used for UKM logging.
struct BLINK_COMMON_EXPORT MobileFriendliness {
  MobileFriendliness() = default;
  MobileFriendliness(const MobileFriendliness&) = default;

  bool operator==(const MobileFriendliness& other) const;
  bool operator!=(const MobileFriendliness& other) const {
    return !(*this == other);
  }

  // Whether <meta name="viewport" content="width=device-width"> is specified or
  // not.
  bool viewport_device_width = false;

  // The value specified in meta tag like <meta name="viewport"
  // content="initial-scale=1.0">.
  double viewport_initial_scale = 1.0;

  // The value specified in meta tag like <meta name="viewport"
  // content="width=500">.
  int viewport_hardcoded_width = 0;

  // Whether the page allows user to zoom in/out.
  // It is specified like <meta name="viewport" content="user-scalable=no">.
  bool allow_user_zoom = true;

  // Percentage of small font size text area in all text area.
  int small_text_ratio = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MOBILE_METRICS_MOBILE_FRIENDLINESS_H_
