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
#include "build/chromecast_buildflags.h"
#include "services/tracing/public/cpp/perfetto/trace_time.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace tracing {

namespace {

perfetto::TraceConfig::DataSource* AddDataSourceConfig(
    perfetto::TraceConfig* perfetto_config,
    const char* name,
    const std::string& chrome_config_string,
    bool privacy_filtering_enabled,
    bool convert_to_legacy_json,
    perfetto::protos::gen::ChromeConfig::ClientPriority client_priority) {
  auto* data_source = perfetto_config->add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name(name);
  source_config->set_target_buffer(0);
  auto* chrome_config = source_config->mutable_chrome_config();
  chrome_config->set_trace_config(chrome_config_string);
  chrome_config->set_privacy_filtering_enabled(privacy_filtering_enabled);
  chrome_config->set_convert_to_legacy_json(convert_to_legacy_json);
  chrome_config->set_client_priority(client_priority);
  return data_source;
}

void AddDataSourceConfigs(
    perfetto::TraceConfig* perfetto_config,
    const base::trace_event::TraceConfig::ProcessFilterConfig& process_filters,
    const base::trace_event::TraceConfig& stripped_config,
    const std::set<std::string>& source_names,
    bool privacy_filtering_enabled,
    bool convert_to_legacy_json,
    perfetto::protos::gen::ChromeConfig::ClientPriority client_priority) {
  const std::string chrome_config_string = stripped_config.ToString();

  // Capture actual trace events.
  if (source_names.empty() ||
      source_names.count(tracing::mojom::kTraceEventDataSourceName) == 1) {
    auto* trace_event_data_source = AddDataSourceConfig(
        perfetto_config, tracing::mojom::kTraceEventDataSourceName,
        chrome_config_string, privacy_filtering_enabled, convert_to_legacy_json,
        client_priority);
    for (auto& enabled_pid : process_filters.included_process_ids()) {
      *trace_event_data_source->add_producer_name_filter() = base::StrCat(
          {mojom::kPerfettoProducerNamePrefix,
           base::NumberToString(static_cast<uint32_t>(enabled_pid))});
    }
  }

  // Capture system trace events if supported and enabled. The datasources will
  // only emit events if system tracing is enabled in |chrome_config|.
  if (!privacy_filtering_enabled) {
#if defined(OS_CHROMEOS) || (BUILDFLAG(IS_CHROMECAST) && defined(OS_LINUX))
    if (source_names.empty() ||
        source_names.count(tracing::mojom::kSystemTraceDataSourceName) == 1) {
      AddDataSourceConfig(perfetto_config,
                          tracing::mojom::kSystemTraceDataSourceName,
                          chrome_config_string, privacy_filtering_enabled,
                          convert_to_legacy_json, client_priority);
    }
#endif

#if defined(OS_CHROMEOS)
    if (source_names.empty() ||
        source_names.count(tracing::mojom::kArcTraceDataSourceName) == 1) {
      AddDataSourceConfig(perfetto_config,
                          tracing::mojom::kArcTraceDataSourceName,
                          chrome_config_string, privacy_filtering_enabled,
                          convert_to_legacy_json, client_priority);
    }
#endif
  }

  // Also capture global metadata.
  if (source_names.empty() ||
      source_names.count(tracing::mojom::kMetaDataSourceName) == 1) {
    AddDataSourceConfig(perfetto_config, tracing::mojom::kMetaDataSourceName,
                        chrome_config_string, privacy_filtering_enabled,
                        convert_to_legacy_json, client_priority);
  }

  if (stripped_config.IsCategoryGroupEnabled(
          TRACE_DISABLED_BY_DEFAULT("cpu_profiler"))) {
    DCHECK_EQ(
        1u, source_names.empty() ||
                source_names.count(tracing::mojom::kSamplerProfilerSourceName));
    AddDataSourceConfig(perfetto_config,
                        tracing::mojom::kSamplerProfilerSourceName,
                        chrome_config_string, privacy_filtering_enabled,
                        convert_to_legacy_json, client_priority);
  }

  if (stripped_config.IsCategoryGroupEnabled(
          TRACE_DISABLED_BY_DEFAULT("java-heap-profiler"))) {
    DCHECK_EQ(1u, source_names.empty() ||
                      source_names.count(
                          tracing::mojom::kJavaHeapProfilerSourceName));
    AddDataSourceConfig(perfetto_config,
                        tracing::mojom::kJavaHeapProfilerSourceName,
                        chrome_config_string, privacy_filtering_enabled,
                        convert_to_legacy_json, client_priority);
  }

  if (source_names.empty() ||
      source_names.count(tracing::mojom::kReachedCodeProfilerSourceName) == 1) {
    AddDataSourceConfig(perfetto_config,
                        tracing::mojom::kReachedCodeProfilerSourceName,
                        chrome_config_string, privacy_filtering_enabled,
                        convert_to_legacy_json, client_priority);
  }
}

}  // namespace

perfetto::TraceConfig GetDefaultPerfettoConfig(
    const base::trace_event::TraceConfig& chrome_config,
    bool privacy_filtering_enabled,
    bool convert_to_legacy_json,
    perfetto::protos::gen::ChromeConfig::ClientPriority client_priority) {
  return GetPerfettoConfigWithDataSources(
      chrome_config, {}, privacy_filtering_enabled, convert_to_legacy_json,
      client_priority);
}

perfetto::TraceConfig COMPONENT_EXPORT(TRACING_CPP)
    GetPerfettoConfigWithDataSources(
        const base::trace_event::TraceConfig& chrome_config,
        const std::set<std::string>& source_names,
        bool privacy_filtering_enabled,
        bool convert_to_legacy_json,
        perfetto::protos::gen::ChromeConfig::ClientPriority client_priority) {
  perfetto::TraceConfig perfetto_config;

  size_t size_limit = chrome_config.GetTraceBufferSizeInKb();
  if (size_limit == 0) {
    // TODO(eseckler): Reduce the default buffer size after benchmarks set what
    // they require. Should also invest some time to reduce the overhead of
    // begin/end pairs further.
    size_limit = 200 * 1024;
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

  auto* builtin_data_sources = perfetto_config.mutable_builtin_data_sources();

  // Chrome uses CLOCK_MONOTONIC as its trace clock on Posix. To avoid that
  // trace processor converts Chrome's event timestamps into CLOCK_BOOTTIME
  // during import, we set the trace clock here (the service will emit it into
  // the trace's ClockSnapshots). See also crbug.com/1060400, where the
  // conversion to BOOTTIME caused CrOS and chromecast system data source data
  // to be misaligned.
  builtin_data_sources->set_primary_trace_clock(
      static_cast<perfetto::protos::gen::BuiltinClock>(kTraceClockId));

  // Chrome emits system / trace config metadata itself.
  builtin_data_sources->set_disable_trace_config(privacy_filtering_enabled);
  builtin_data_sources->set_disable_system_info(privacy_filtering_enabled);
  builtin_data_sources->set_disable_service_events(privacy_filtering_enabled);

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

  AddDataSourceConfigs(&perfetto_config, chrome_config.process_filter_config(),
                       stripped_config, source_names, privacy_filtering_enabled,
                       convert_to_legacy_json, client_priority);

  return perfetto_config;
}

}  // namespace tracing
