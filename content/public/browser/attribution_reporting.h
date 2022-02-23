// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ATTRIBUTION_REPORTING_H_
#define CONTENT_PUBLIC_BROWSER_ATTRIBUTION_REPORTING_H_

#include "content/common/content_export.h"

namespace content {

enum class AttributionNoiseMode {
  // Various aspects of the API are subject to noise:
  // - Sources are subject to randomized response
  // - Reports within a reporting window are shuffled
  // - Pending reports are randomly delayed when the browser comes online
  kDefault,
  // None of the above applies.
  kNone,
};

enum class AttributionDelayMode {
  // Reports are sent in reporting windows some time after attribution is
  // triggered.
  kDefault,
  // Reports are sent immediately after attribution is triggered.
  kNone,
};

// Controls randomized response rates for the API: when a source is registered,
// these rates are used to determine whether any subsequent attributions for the
// source are handled truthfully, or whether the source is immediately
// attributed with zero or more fake reports and real attributions are dropped.
struct CONTENT_EXPORT AttributionRandomizedResponseRates {
  // The default rates used by the API, deliberately unspecified here.
  static const AttributionRandomizedResponseRates kDefault;

  // The rate for navigation sources. Must be in the range [0, 1].
  double navigation = 0;

  // The rate for event sources. Must be in the range [0, 1].
  double event = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ATTRIBUTION_REPORTING_H_
