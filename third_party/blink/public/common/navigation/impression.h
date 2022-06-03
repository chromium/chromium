// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_IMPRESSION_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_IMPRESSION_H_

#include <stdint.h>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "url/origin.h"

namespace blink {

// An impression represents a click on an anchor tag that has special Conversion
// Measurement attributes declared. When the anchor is clicked, an impression is
// generated from these attributes and associated with the resulting navigation.
// When an action is performed on the linked site at a later date, the
// impression information is used to provide context about the initial
// navigation that resulted in that action.
//
// Used for IPC transport of WebImpression. WebImpression cannot be used
// directly as it contains non-header-only blink types.
struct BLINK_COMMON_EXPORT Impression {
  Impression();
  Impression(const Impression& other);
  Impression& operator=(const Impression& other);
  ~Impression();

  // Intended committed top-level origin of the resulting navigation. Must match
  // the committed navigation's origin to be a valid impression. Declared by
  // the impression tag.
  url::Origin conversion_destination;

  // Optional origin that will receive all conversion measurement reports
  // associated with this impression. Declared by the impression tag.
  absl::optional<url::Origin> reporting_origin;

  // Data that will be sent in conversion reports to identify this impression.
  // Declared by the impression tag.
  uint64_t impression_data = 0UL;

  // Optional expiry specifying the amount of time this impression can convert.
  // Declared by the impression tag.
  absl::optional<base::TimeDelta> expiry;

  // Priority for the attribution source. Declared by the impression tag.
  int64_t priority = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_IMPRESSION_H_
