// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_CONFIG_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_CONFIG_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "third_party/perfetto/include/perfetto/tracing/core/chrome_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace base {
namespace trace_event {
class TraceConfig;
}  // namespace trace_event
}  // namespace base

namespace tracing {

perfetto::TraceConfig COMPONENT_EXPORT(TRACING_CPP) GetDefaultPerfettoConfig(
    const base::trace_event::TraceConfig& chrome_config,
    bool privacy_filtering_enabled = false,
    bool convert_to_legacy_json = false,
    perfetto::protos::gen::ChromeConfig::ClientPriority =
        perfetto::protos::gen::ChromeConfig::USER_INITIATED,
    const std::string& json_agent_label_filter = "");

// Creates a perfetto trace config with only the data sources included in
// |source_names| and enabled by |trace_config|. Passing empty set will add all
// data sources based on trace config. The list of possible names are listed in
// services/tracing/public/mojom/perfetto_service.mojom.
perfetto::TraceConfig COMPONENT_EXPORT(TRACING_CPP)
    GetPerfettoConfigWithDataSources(
        const base::trace_event::TraceConfig& chrome_config,
        const std::set<std::string>& source_names,
        bool privacy_filtering_enabled = false,
        bool convert_to_legacy_json = false,
        perfetto::protos::gen::ChromeConfig::ClientPriority =
            perfetto::protos::gen::ChromeConfig::USER_INITIATED,
        const std::string& json_agent_label_filter = "");

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_CONFIG_H_
