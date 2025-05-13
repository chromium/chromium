// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "services/tracing/public/cpp/perfetto/perfetto_config.h"

#include <cstdint>
#include <string>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/trace_time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/histogram_samples.gen.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"

namespace tracing {

namespace {

// Clear incremental state every 0.5 seconds, so that we lose at most the
// first 0.5 seconds of the trace (if we wrap around Perfetto's central
// buffer).
// This value strikes balance between minimizing interned data overhead, and
// reducing the risk of data loss in ring buffer mode.
constexpr int kDefaultIncrementalStateClearPeriodMs = 500;

perfetto::TraceConfig::DataSource* AddDataSourceConfig(
    perfetto::TraceConfig* perfetto_config,
    const char* name,
    const std::string& chrome_config_string,
    bool privacy_filtering_enabled,
    bool convert_to_legacy_json,
    perfetto::protos::gen::ChromeConfig::ClientPriority client_priority,
    const std::string& json_agent_label_filter,
    bool enable_package_name_filter) {
  auto* data_source = perfetto_config->add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name(name);
  source_config->set_target_buffer(0);

  auto* chrome_config = source_config->mutable_chrome_config();
  if (!chrome_config_string.empty()) {
    chrome_config->set_trace_config(chrome_config_string);
  }
  chrome_config->set_privacy_filtering_enabled(privacy_filtering_enabled);
  chrome_config->set_convert_to_legacy_json(convert_to_legacy_json);
  chrome_config->set_client_priority(client_priority);
  chrome_config->set_event_package_name_filter_enabled(
      enable_package_name_filter);
  if (!json_agent_label_filter.empty())
    chrome_config->set_json_agent_label_filter(json_agent_label_filter);

  return data_source;
}

void AddDataSourceConfigs(
    perfetto::TraceConfig* perfetto_config,
    const base::trace_event::TraceConfig::ProcessFilterConfig& process_filters,
    const base::trace_event::TraceConfig& stripped_config,
    bool systrace_enabled,
    bool privacy_filtering_enabled,
    bool convert_to_legacy_json,
    perfetto::protos::gen::ChromeConfig::ClientPriority client_priority,
    const std::string& json_agent_label_filter,
    bool enable_package_name_filter) {
  const std::string chrome_config_string = stripped_config.ToString();

  if (stripped_config.IsCategoryGroupEnabled(
          base::trace_event::MemoryDumpManager::kTraceCategory)) {
    AddDataSourceConfig(
        perfetto_config, tracing::mojom::kMemoryInstrumentationDataSourceName,
        chrome_config_string, privacy_filtering_enabled, convert_to_legacy_json,
        client_priority, json_agent_label_filter, enable_package_name_filter);
    AddDataSourceConfig(
        perfetto_config, tracing::mojom::kNativeHeapProfilerSourceName,
        chrome_config_string, privacy_filtering_enabled, convert_to_legacy_json,
        client_priority, json_agent_label_filter, enable_package_name_filter);
  }

  auto* trace_event_data_source = AddDataSourceConfig(
      perfetto_config, tracing::mojom::kTraceEventDataSourceName,
      chrome_config_string, privacy_filtering_enabled, convert_to_legacy_json,
      client_priority, json_agent_label_filter, enable_package_name_filter);
  auto* trace_event_source_config = trace_event_data_source->mutable_config();
  trace_event_source_config->set_name("track_event");
  trace_event_source_config->set_track_event_config_raw(
      stripped_config.ToPerfettoTrackEventConfigRaw(privacy_filtering_enabled));
  for (auto& enabled_pid : process_filters.included_process_ids()) {
    *trace_event_data_source->add_producer_name_filter() = base::StrCat(
        {mojom::kPerfettoProducerNamePrefix,
         base::NumberToString(static_cast<uint32_t>(enabled_pid))});
  }

  // Capture system trace events if supported and enabled. The datasources will
  // only emit events if system tracing is enabled in |chrome_config|.
  if (!privacy_filtering_enabled && systrace_enabled) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_CASTOS)
    AddDataSourceConfig(
        perfetto_config, tracing::mojom::kSystemTraceDataSourceName,
        chrome_config_string, privacy_filtering_enabled, convert_to_legacy_json,
        client_priority, json_agent_label_filter, enable_package_name_filter);
#endif

#if BUILDFLAG(IS_CHROMEOS)
    AddDataSourceConfig(
        perfetto_config, tracing::mojom::kArcTraceDataSourceName,
        chrome_config_string, privacy_filtering_enabled, convert_to_legacy_json,
        client_priority, json_agent_label_filter, enable_package_name_filter);
#endif
  }

  // Also capture global metadata.
  AddDataSourceConfig(perfetto_config, tracing::mojom::kMetaDataSourceName,
                      chrome_config_string, privacy_filtering_enabled,
                      convert_to_legacy_json, client_priority,
                      json_agent_label_filter, enable_package_name_filter);

  if (stripped_config.IsCategoryGroupEnabled(
          TRACE_DISABLED_BY_DEFAULT("histogram_samples"))) {
    auto* data_source = AddDataSourceConfig(
        perfetto_config, tracing::mojom::kHistogramSampleSourceName,
        /*chrome_config_string=*/"", privacy_filtering_enabled,
        convert_to_legacy_json, client_priority, json_agent_label_filter,
        enable_package_name_filter);

    perfetto::protos::gen::ChromiumHistogramSamplesConfig histogram_config;
    histogram_config.set_filter_histogram_names(privacy_filtering_enabled);
    for (const auto& histogram_name : stripped_config.histogram_names()) {
      perfetto::protos::gen::ChromiumHistogramSamplesConfig::HistogramSample
          sample;
      sample.set_histogram_name(histogram_name);
      *histogram_config.add_histograms() = sample;
    }
    data_source->mutable_config()->set_chromium_histogram_samples_raw(
        histogram_config.SerializeAsString());
  }

  if (stripped_config.IsCategoryGroupEnabled(
          TRACE_DISABLED_BY_DEFAULT("cpu_profiler"))) {
    AddDataSourceConfig(perfetto_config,
                        tracing::mojom::kSamplerProfilerSourceName,
                        /*chrome_config_string=*/"", privacy_filtering_enabled,
                        convert_to_legacy_json, client_priority,
                        json_agent_label_filter, enable_package_name_filter);
  }

  if (stripped_config.IsCategoryGroupEnabled(
          TRACE_DISABLED_BY_DEFAULT("system_metrics"))) {
    AddDataSourceConfig(perfetto_config,
                        tracing::mojom::kSystemMetricsSourceName,
                        /*chrome_config_string=*/"", privacy_filtering_enabled,
                        convert_to_legacy_json, client_priority,
                        json_agent_label_filter, enable_package_name_filter);
  }

  if (stripped_config.IsCategoryGroupEnabled(
          TRACE_DISABLED_BY_DEFAULT("java-heap-profiler"))) {
    AddDataSourceConfig(
        perfetto_config, tracing::mojom::kJavaHeapProfilerSourceName,
        chrome_config_string, privacy_filtering_enabled, convert_to_legacy_json,
        client_priority, json_agent_label_filter, enable_package_name_filter);
  }
}

size_t GetDefaultTraceBufferSize() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string switch_value = command_line->GetSwitchValueASCII(
      switches::kDefaultTraceBufferSizeLimitInKb);
  size_t switch_kilobytes;
  if (!switch_value.empty() &&
      base::StringToSizeT(switch_value, &switch_kilobytes)) {
    return switch_kilobytes;
  } else {
    // TODO(eseckler): Reduce the default buffer size after benchmarks set
    // what they require. Should also invest some time to reduce the overhead
    // of begin/end pairs further.
    return 200 * 1024;
  }
}

void AdaptBuiltinDataSourcesConfig(
    perfetto::TraceConfig::BuiltinDataSource* config,
    bool privacy_filtering_enabled) {
  // Chrome uses CLOCK_MONOTONIC as its trace clock on Posix. To avoid that
  // trace processor converts Chrome's event timestamps into CLOCK_BOOTTIME
  // during import, we set the trace clock here (the service will emit it into
  // the trace's ClockSnapshots). See also crbug.com/1060400, where the
  // conversion to BOOTTIME caused CrOS and chromecast system data source data
  // to be misaligned.
  config->set_primary_trace_clock(
      static_cast<perfetto::protos::gen::BuiltinClock>(
          base::tracing::kTraceClockId));

  // Chrome emits system / trace config metadata itself.
  config->set_disable_trace_config(privacy_filtering_enabled);
  config->set_disable_system_info(privacy_filtering_enabled);
  config->set_disable_service_events(privacy_filtering_enabled);
}

void AdaptTrackEventConfig(perfetto::protos::gen::TrackEventConfig* config,
                           bool privacy_filtering_enabled) {
  if (!config->has_enable_thread_time_sampling()) {
    config->set_enable_thread_time_sampling(true);
  }
  if (!config->has_timestamp_unit_multiplier()) {
    config->set_timestamp_unit_multiplier(1000);
  }
  if (privacy_filtering_enabled) {
    config->set_filter_dynamic_event_names(true);
    config->set_filter_debug_annotations(true);
  }
}

void AdaptDataSourceConfig(
    perfetto::DataSourceConfig* config,
    bool privacy_filtering_enabled,
    bool enable_package_name_filter,
    perfetto::protos::gen::ChromeConfig::ClientPriority client_priority,
    bool enable_system_backend) {
  if (!config->has_target_buffer()) {
    config->set_target_buffer(0);
  }

  // Adapt data source config if
  // 1. the scenario uses the default custom backend, or
  // 2. the scenario uses the system backend. Only Chrome data source should be
  // adapted. Other data source names are ignored.
  if (!enable_system_backend || (config->name() == "track_event" ||
                                 config->name().starts_with("org.chromium."))) {
    auto* chrome_config = config->mutable_chrome_config();
    chrome_config->set_privacy_filtering_enabled(privacy_filtering_enabled);
    // There are no use case for legacy json, since this is used to adapt
    // background tracing configs.
    chrome_config->set_convert_to_legacy_json(false);
    chrome_config->set_client_priority(client_priority);
    chrome_config->set_event_package_name_filter_enabled(
        enable_package_name_filter);
  }

  if (config->name() == tracing::mojom::kHistogramSampleSourceName) {
    perfetto::protos::gen::ChromiumHistogramSamplesConfig histogram_config;
    if (!config->chromium_histogram_samples_raw().empty() &&
        !histogram_config.ParseFromString(
            config->chromium_histogram_samples_raw())) {
      DLOG(ERROR) << "Failed to parse chromium_histogram_samples";
      return;
    }
    histogram_config.set_filter_histogram_names(privacy_filtering_enabled);
    config->set_chromium_histogram_samples_raw(
        histogram_config.SerializeAsString());
  }

  if (!config->track_event_config_raw().empty()) {
    config->set_name("track_event");
    perfetto::protos::gen::TrackEventConfig track_event_config;
    track_event_config.ParseFromString(config->track_event_config_raw());
    AdaptTrackEventConfig(&track_event_config, privacy_filtering_enabled);
    config->set_track_event_config_raw(track_event_config.SerializeAsString());
  }
}

}  // namespace

perfetto::TraceConfig GetDefaultPerfettoConfig(
    const base::trace_event::TraceConfig& chrome_config,
    bool privacy_filtering_enabled,
    bool convert_to_legacy_json,
    perfetto::protos::gen::ChromeConfig::ClientPriority client_priority,
    const std::string& json_agent_label_filter) {
  perfetto::TraceConfig perfetto_config;

  size_t size_limit = chrome_config.GetTraceBufferSizeInKb();
  if (size_limit == 0) {
    // If trace config did not provide trace buffer size, we will use default
    size_limit = GetDefaultTraceBufferSize();
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
  }

  AdaptBuiltinDataSourcesConfig(perfetto_config.mutable_builtin_data_sources(),
                                privacy_filtering_enabled);

  perfetto_config.mutable_incremental_state_config()->set_clear_period_ms(
      kDefaultIncrementalStateClearPeriodMs);

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
                       stripped_config, chrome_config.IsSystraceEnabled(),
                       privacy_filtering_enabled, convert_to_legacy_json,
                       client_priority, json_agent_label_filter,
                       chrome_config.IsEventPackageNameFilterEnabled());

  return perfetto_config;
}

bool AdaptPerfettoConfigForChrome(
    perfetto::TraceConfig* perfetto_config,
    bool privacy_filtering_enabled,
    bool enable_package_name_filter,
    perfetto::protos::gen::ChromeConfig::ClientPriority client_priority,
    bool enable_system_backend) {
  if (perfetto_config->buffers_size() < 1) {
    auto* buffer_config = perfetto_config->add_buffers();
    buffer_config->set_size_kb(GetDefaultTraceBufferSize());
    buffer_config->set_fill_policy(
        perfetto::TraceConfig::BufferConfig::RING_BUFFER);
  }

  AdaptBuiltinDataSourcesConfig(perfetto_config->mutable_builtin_data_sources(),
                                privacy_filtering_enabled);
  if (!perfetto_config->has_incremental_state_config()) {
    perfetto_config->mutable_incremental_state_config()->set_clear_period_ms(
        kDefaultIncrementalStateClearPeriodMs);
  }

  for (auto& data_source_config : *perfetto_config->mutable_data_sources()) {
    AdaptDataSourceConfig(data_source_config.mutable_config(),
                          privacy_filtering_enabled, enable_package_name_filter,
                          client_priority, enable_system_backend);
  }
  return true;
}

}  // namespace tracing
