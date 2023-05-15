// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_REPORTING_RUNTIME_FEATURES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_REPORTING_RUNTIME_FEATURES_H_

#include "base/component_export.h"

namespace network {

// This corresponds to network::mojom::AttributionReportingRuntimeFeatures.
// See the comments there.
struct COMPONENT_EXPORT(NETWORK_CPP_ATTRIBUTION)
    AttributionReportingRuntimeFeatures {
  bool cross_app_web_enabled = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_REPORTING_RUNTIME_FEATURES_H_
