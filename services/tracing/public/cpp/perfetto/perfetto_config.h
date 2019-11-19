// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_CONFIG_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_CONFIG_H_

#include "base/component_export.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace base {
namespace trace_event {
class TraceConfig;
}  // namespace trace_event
}  // namespace base

namespace tracing {

perfetto::TraceConfig COMPONENT_EXPORT(TRACING_CPP) GetDefaultPerfettoConfig(
    const base::trace_event::TraceConfig& chrome_config,
    bool privacy_filtering_enabled = false);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_CONFIG_H_
