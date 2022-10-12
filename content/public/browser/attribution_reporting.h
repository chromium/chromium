// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ATTRIBUTION_REPORTING_H_
#define CONTENT_PUBLIC_BROWSER_ATTRIBUTION_REPORTING_H_

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

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ATTRIBUTION_REPORTING_H_
