// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_METRICS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_METRICS_H_

#include "base/component_export.h"
#include "base/metrics/histogram_functions.h"

namespace network {

inline constexpr char kConnectionAllowlistTypeHistogram[] =
    "NetworkService.ConnectionAllowlist.Type";

// Corresponds to the ConnectionAllowlistType enum in
// tools/metrics/histograms/metadata/network/enums.xml.
enum class ConnectionAllowlistType {
  kEnforced = 0,
  kReportOnly = 1,
  kMaxValue = kReportOnly,
};

inline void LogConnectionAllowlistTypeHistogram(ConnectionAllowlistType type) {
  base::UmaHistogramEnumeration(kConnectionAllowlistTypeHistogram, type);
}

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_METRICS_H_
