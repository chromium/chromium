// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_startup.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/system_producer.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/trace_event_args_allowlist.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

#include "components/tracing/common/etw_export_win.h"

namespace tracing {
namespace {

constexpr char kJsonFormat[] = "json";
constexpr uint32_t kStartupTracingTimeoutMs = 30 * 1000;  // 30 sec

using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;

bool CanBePropagatedViaCommandLine(
    const base::trace_event::TraceConfig& trace_config) {
  base::trace_event::TraceConfig reconstructed_config(
      trace_config.ToCategoryFilterString(),
      trace_config.ToTraceOptionsString());
  return reconstructed_config.ToString() == trace_config.ToString();
}

std::string CategoryFilterStringFromTrackEventConfig(
    const perfetto::protos::gen::TrackEventConfig& te_cfg) {
  std::string filter;
  for (const auto& cat : te_cfg.disabled_categories()) {
    if (!filter.empty()) {
      filter += ",";
    }
    filter += "-" + cat;
  }
  for (const auto& cat : te_cfg.enabled_categories()) {
    if (!filter.empty()) {
      filter += ",";
    }
    filter += cat;
  }
  return filter;
}

}  // namespace

bool g_tracing_initialized_after_threadpool_and_featurelist = false;

bool IsTracingInitialized() {
  return g_tracing_initialized_after_threadpool_and_featurelist;
}

void EnableStartupTracingIfNeeded() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  RegisterTracedValueProtoWriter();
  TraceEventDataSource::GetInstance()->RegisterStartupHooks();

  // Create the PerfettoTracedProcess.
  PerfettoTracedProcess::Get();

  // Initialize the client library's TrackRegistry to support trace points
  // during startup tracing. We don't setup the client library completely here
  // yet, because we don't have field trials loaded yet (which influence which
  // backends we enable).
  // TODO(eseckler): Make it possible to initialize client lib backends after
  // setting up the client library?
  perfetto::internal::TrackRegistry::InitializeInstance();

  // Ensure TraceLog is initialized first.
  // https://crbug.com/764357
  TraceLog::GetInstance();
  auto& startup_config = TraceStartupConfig::GetInstance();

  if (startup_config.IsEnabled()) {
    // Ensure that data sources are created and registered.
    TraceEventAgent::GetInstance();

    TraceConfig trace_config = startup_config.GetTraceConfig();

    bool privacy_filtering_enabled =
        startup_config.GetSessionOwner() ==
            TraceStartupConfig::SessionOwner::kBackgroundTracing ||
        command_line.HasSwitch(switches::kTraceStartupEnablePrivacyFiltering);

    bool convert_to_legacy_json = startup_config.GetOutputFormat() ==
                                  TraceStartupConfig::OutputFormat::kLegacyJSON;

    perfetto::TraceConfig perfetto_config = tracing::GetDefaultPerfettoConfig(
        trace_config, privacy_filtering_enabled, convert_to_legacy_json);
    int duration_in_seconds =
        tracing::TraceStartupConfig::GetInstance().GetStartupDuration();
    if (duration_in_seconds > 0)
      perfetto_config.set_duration_ms(duration_in_seconds * 1000);
    perfetto::Tracing::SetupStartupTracingOpts opts;
    opts.timeout_ms = kStartupTracingTimeoutMs;
    // TODO(khokhlov): Support startup tracing with the system backend in the
    // SDK build.
    opts.backend = perfetto::kCustomBackend;
    // TODO(khokhlov): After client library is moved onto a separate thread
    // and it's possible to start startup tracing early, replace this call with
    // perfetto::Tracing::SetupStartupTracing(perfetto_config, args).
    PerfettoTracedProcess::Get()->RequestStartupTracing(perfetto_config, opts);
  }
}

bool EnableStartupTracingForProcess(
    const base::trace_event::TraceConfig& trace_config,
    bool privacy_filtering_enabled) {
  perfetto::TraceConfig perfetto_config =
      tracing::GetDefaultPerfettoConfig(trace_config, privacy_filtering_enabled,
                                        /*convert_to_legacy_json=*/false);
  perfetto::Tracing::SetupStartupTracingOpts opts;
  opts.timeout_ms = kStartupTracingTimeoutMs;
  opts.backend = perfetto::kCustomBackend;
  // TODO(khokhlov): After client library is moved onto a separate thread
  // and it's possible to start startup tracing early, replace this call with
  // perfetto::Tracing::SetupStartupTracing(perfetto_config, args).
  PerfettoTracedProcess::Get()->RequestStartupTracing(perfetto_config, opts);
  return true;
}

void InitTracingPostThreadPoolStartAndFeatureList(bool enable_consumer) {
  if (g_tracing_initialized_after_threadpool_and_featurelist)
    return;
  g_tracing_initialized_after_threadpool_and_featurelist = true;
  DCHECK(base::ThreadPoolInstance::Get());
  DCHECK(base::FeatureList::GetInstance());

  PerfettoTracedProcess::Get()->OnThreadPoolAvailable(enable_consumer);
#if BUILDFLAG(IS_WIN)
  tracing::EnableETWExport();
#endif  // BUILDFLAG(IS_WIN)
}

void PropagateTracingFlagsToChildProcessCmdLine(base::CommandLine* cmd_line) {
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();

  base::trace_event::TraceConfig trace_config;
  bool privacy_filtering_enabled = false;
  bool convert_to_legacy_json = false;

  // TODO(khokhlov): Figure out if we are using custom or system backend and
  // propagate this info to the child process (after startup tracing w/system
  // backend is supported in the SDK build).
  const auto& startup_config = TraceStartupConfig::GetInstance();
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (startup_config.IsEnabled()) {
    trace_config = startup_config.GetTraceConfig();
    privacy_filtering_enabled =
        startup_config.GetSessionOwner() ==
            TraceStartupConfig::SessionOwner::kBackgroundTracing ||
        command_line.HasSwitch(switches::kTraceStartupEnablePrivacyFiltering);
    convert_to_legacy_json = startup_config.GetOutputFormat() ==
                             TraceStartupConfig::OutputFormat::kLegacyJSON;
  } else if (trace_log->IsEnabled()) {
    perfetto::DataSourceConfig data_source_config =
        trace_log->GetCurrentTrackEventDataSourceConfig();
    if (data_source_config.has_interceptor_config()) {
      return;
    }
    const auto chrome_config = data_source_config.chrome_config();
    if (chrome_config.trace_config().size() > 0) {
      // If the chrome_config part of the data source config is set, propagate
      // it as is.
      trace_config =
          base::trace_event::TraceConfig(chrome_config.trace_config());
      privacy_filtering_enabled = chrome_config.privacy_filtering_enabled();
      convert_to_legacy_json = chrome_config.convert_to_legacy_json();
    } else {
      // If chrome_config is not set, reconstruct category filter based on
      // the track_event config to propagate the correct categories.
      // See  perfetto::DataSourceBase::CanAdoptStartupSession for why
      // category list must match exactly.
      perfetto::protos::gen::TrackEventConfig te_cfg;
      te_cfg.ParseFromString(data_source_config.track_event_config_raw());
      trace_config = base::trace_event::TraceConfig(
          CategoryFilterStringFromTrackEventConfig(te_cfg), "");
      privacy_filtering_enabled = te_cfg.filter_debug_annotations() ||
                                  te_cfg.filter_dynamic_event_names();
      convert_to_legacy_json = false;
    }
  } else {
    return;
  }

  // We can't currently propagate event filter options, histogram names, memory
  // dump configs, or trace buffer sizes via command line flags (they only
  // support categories, trace options, record mode). If event filters or
  // histogram names are set, we bail out here to avoid recording events that we
  // shouldn't in the child process. Even if memory dump config is set, it's OK
  // to propagate the remaining config, because the child won't record events it
  // shouldn't without it and will adopt the memory dump config once it connects
  // to the tracing service. Buffer sizes configure the tracing service's
  // central buffer, so also don't affect local tracing.
  //
  // TODO(eseckler): Support propagating the full config via command line flags
  // somehow (--trace-config?). This will also need some rethinking to support
  // multiple concurrent tracing sessions in the future.
  if (!trace_config.event_filters().empty())
    return;
  if (!trace_config.histogram_names().empty())
    return;

  // In SDK build, any difference between startup config and the config
  // supplied to the tracing service will prevent the service from adopting
  // the startup session. So if the config contains any field that can't be
  // propagated via command line, we bail out here.
  if (!CanBePropagatedViaCommandLine(trace_config))
    return;

  // Make sure that the startup session uses privacy filtering mode if it's
  // enabled for the browser's session.
  if (privacy_filtering_enabled)
    cmd_line->AppendSwitch(switches::kTraceStartupEnablePrivacyFiltering);
  if (convert_to_legacy_json)
    cmd_line->AppendSwitchASCII(switches::kTraceStartupFormat, kJsonFormat);

  cmd_line->AppendSwitchASCII(switches::kTraceStartup,
                              trace_config.ToCategoryFilterString());
  // The argument filtering setting is passed via trace options as part of
  // --trace-startup-record-mode.
  cmd_line->AppendSwitchASCII(switches::kTraceStartupRecordMode,
                              trace_config.ToTraceOptionsString());
}

}  // namespace tracing
