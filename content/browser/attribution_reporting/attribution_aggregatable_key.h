// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_KEY_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_KEY_H_

#include <stdint.h>

namespace content {

struct AttributionAggregatableKey {
  uint64_t high_bits;
  uint64_t low_bits;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_KEY_H_
