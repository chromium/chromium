// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_config.h"

#include <cstdint>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace tracing {

namespace {

perfetto::TraceConfig::DataSource* AddDataSourceConfig(
    perfetto::TraceConfig* perfetto_config,
    const char* name,
    const std::string& chrome_config_string,
    bool privacy_filtering_enabled) {
  auto* data_source = perfetto_config->add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name(name);
  source_config->set_target_buffer(0);
  auto* chrome_config = source_config->mutable_chrome_config();
  chrome_config->set_trace_config(chrome_config_string);
  chrome_config->set_privacy_filtering_enabled(privacy_filtering_enabled);
  return data_source;
}

}  // namespace

perfetto::TraceConfig GetDefaultPerfettoConfig(
    const base::trace_event::TraceConfig& chrome_config,
    bool privacy_filtering_enabled) {
  perfetto::TraceConfig perfetto_config;

  size_t size_limit = chrome_config.GetTraceBufferSizeInKb();
  if (size_limit == 0) {
    size_limit = 100 * 1024;
  }
  auto* buffer_config = perfetto_config.add_buffers();
  buffer_config->set_size_kb(size_limit);
  switch (chrome_config.GetTraceRecordMode()) {
    case base::trace_event::RECORD_UNTIL_FULL:
    case base::trace_event::RECORD_AS_MUCH_AS_POSSIBLE:
      buffer_config->set_fill_policy(
          perfetto::TraceConfig::BufferConfig::DISCARD);
      break;
    case base::trace_event::RECORD_CONTINUOUSLY:
      buffer_config->set_fill_policy(
          perfetto::TraceConfig::BufferConfig::RING_BUFFER);
      break;
    case base::trace_event::ECHO_TO_CONSOLE:
      NOTREACHED();
      break;
  }

  // Perfetto uses clock_gettime for its internal snapshotting, which gets
  // blocked by the sandboxed and isn't needed for Chrome regardless.
  auto* builtin_data_sources = perfetto_config.mutable_builtin_data_sources();
  builtin_data_sources->set_disable_trace_config(privacy_filtering_enabled);
  builtin_data_sources->set_disable_system_info(privacy_filtering_enabled);

  // Clear incremental state every 5 seconds, so that we lose at most the first
  // 5 seconds of the trace (if we wrap around perfetto's central buffer).
  perfetto_config.mutable_incremental_state_config()->set_clear_period_ms(5000);

  // We strip the process filter from the config string we send to Perfetto, so
  // perfetto doesn't reject it from a future TracingService::ChangeTraceConfig
  // call due to being an unsupported update. We also strip the trace buffer
  // size configuration to prevent chrome from rejecting an update to it after
  // startup tracing via TraceConfig::Merge (see trace_startup.cc). For
  // perfetto, the buffer size is configured via perfetto's buffer config and
  // only affects the perfetto service.
  base::trace_event::TraceConfig stripped_config(chrome_config);
  stripped_config.SetProcessFilterConfig(
      base::trace_event::TraceConfig::ProcessFilterConfig());
  stripped_config.SetTraceBufferSizeInKb(0);
  stripped_config.SetTraceBufferSizeInEvents(0);
  std::string chrome_config_string = stripped_config.ToString();

  // Capture actual trace events.
  auto* trace_event_data_source = AddDataSourceConfig(
      &perfetto_config, tracing::mojom::kTraceEventDataSourceName,
      chrome_config_string, privacy_filtering_enabled);
  for (auto& enabled_pid :
       chrome_config.process_filter_config().included_process_ids()) {
    *trace_event_data_source->add_producer_name_filter() = base::StrCat(
        {mojom::kPerfettoProducerNamePrefix,
         base::NumberToString(static_cast<uint32_t>(enabled_pid))});
  }

// Capture system trace events if supported and enabled. The datasources will
// only emit events if system tracing is enabled in |chrome_config|.
  if (!privacy_filtering_enabled) {
#if defined(OS_CHROMEOS) || (defined(IS_CHROMECAST) && defined(OS_LINUX))
    AddDataSourceConfig(&perfetto_config,
                        tracing::mojom::kSystemTraceDataSourceName,
                        chrome_config_string, privacy_filtering_enabled);
#endif

#if defined(OS_CHROMEOS)
    AddDataSourceConfig(&perfetto_config,
                        tracing::mojom::kArcTraceDataSourceName,
                        chrome_config_string, privacy_filtering_enabled);
#endif
  }

  // Also capture global metadata.
  AddDataSourceConfig(&perfetto_config, tracing::mojom::kMetaDataSourceName,
                      chrome_config_string, privacy_filtering_enabled);

  if (chrome_config.IsCategoryGroupEnabled(
          TRACE_DISABLED_BY_DEFAULT("cpu_profiler"))) {
    AddDataSourceConfig(&perfetto_config,
                        tracing::mojom::kSamplerProfilerSourceName,
                        chrome_config_string, privacy_filtering_enabled);
  }

  if (chrome_config.IsCategoryGroupEnabled(
          TRACE_DISABLED_BY_DEFAULT("java_heap_profiler"))) {
    AddDataSourceConfig(&perfetto_config,
                        tracing::mojom::kJavaHeapProfilerSourceName,
                        chrome_config_string, privacy_filtering_enabled);
  }

  return perfetto_config;
}

}  // namespace tracing
