// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_HOST_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_HOST_UTILS_H_

#include "content/browser/attribution_reporting/attribution_source_type.h"

namespace base {
class Time;
}  // namespace base

namespace blink {
struct Impression;
}  // namespace blink

namespace url {
class Origin;
}  // namespace url

namespace content {

class AttributionManager;

namespace attribution_host_utils {

// Performs required checks on an incoming impression's data (trustworthy
// origins, etc), and if verified, generates a `StorableSource` and persists
// it.
void VerifyAndStoreImpression(AttributionSourceType source_type,
                              const url::Origin& impression_origin,
                              const blink::Impression& impression,
                              AttributionManager& attribution_manager,
                              base::Time impression_time);

}  // namespace attribution_host_utils

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_HOST_UTILS_H_
