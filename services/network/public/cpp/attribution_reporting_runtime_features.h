// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_REPORTING_RUNTIME_FEATURES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_REPORTING_RUNTIME_FEATURES_H_

#include "base/containers/enum_set.h"

namespace network {

enum class AttributionReportingRuntimeFeature {
  kCrossAppWeb,
  kMinValue = kCrossAppWeb,
  kMaxValue = kCrossAppWeb,
};

using AttributionReportingRuntimeFeatures =
    base::EnumSet<AttributionReportingRuntimeFeature,
                  AttributionReportingRuntimeFeature::kMinValue,
                  AttributionReportingRuntimeFeature::kMaxValue>;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_REPORTING_RUNTIME_FEATURES_H_
