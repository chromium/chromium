// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_COMMON_DATA_SOURCES_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_COMMON_DATA_SOURCES_H_

#include "base/component_export.h"

namespace tracing {

// Registers the Perfetto data sources that are common to all platforms (e.g.
// TrackEvent, HistogramSamples, SystemMetrics, etc).
void COMPONENT_EXPORT(TRACING_CPP)
    RegisterCommonPerfettoDataSources(bool enable_consumer);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_COMMON_DATA_SOURCES_H_
