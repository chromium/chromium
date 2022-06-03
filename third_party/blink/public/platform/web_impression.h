// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_IMPRESSION_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_IMPRESSION_H_

#include <stdint.h>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

// Struct which contains all attributes declared by an impression anchor tag.
// This data is associated with a navigation created by clicking on an anchor
// tag which declares attributes for conversion measurement.
struct WebImpression {
  // Origin declared by the impression which is the intended final top-level
  // origin of the resulting navigation.
  WebSecurityOrigin conversion_destination;

  // Optional origin that will receive all conversion measurement reports
  // associated with this impression. Declared by the impression tag.
  absl::optional<WebSecurityOrigin> reporting_origin;

  // Data that will be sent in conversion reports to identify this impression.
  // Declared by the impression tag.
  uint64_t impression_data;

  // Optional expiry specifying the amount of time this impression can convert.
  // Declared by the impression tag.
  absl::optional<base::TimeDelta> expiry;

  // Priority for the attribution source. Declared by the impression tag.
  // This is 64 bits to allow timestamps to be used as a prioirty.
  int64_t priority;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_IMPRESSION_H_
