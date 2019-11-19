// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_startup.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/trace_event_args_whitelist.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace tracing {
namespace {
using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;
}  // namespace

bool g_tracing_initialized_after_threadpool_and_featurelist = false;

bool IsTracingInitialized() {
  return g_tracing_initialized_after_threadpool_and_featurelist;
}

void EnableStartupTracingIfNeeded() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // TODO(oysteine): Support startup tracing to a perfetto protobuf trace. This
  // should also enable TraceLog and call
  // TraceEventDataSource::SetupStartupTracing().
  if (command_line.HasSwitch(switches::kPerfettoOutputFile))
    return;

  // Ensure TraceLog is initialized first.
  // https://crbug.com/764357
  auto* trace_log = TraceLog::GetInstance();
  auto* startup_config = TraceStartupConfig::GetInstance();

  if (startup_config->IsEnabled()) {
    TraceConfig trace_config = startup_config->GetTraceConfig();

    // Make sure we only record whitelisted arguments even during early startup
    // tracing if the config specifies argument filtering.
    if (trace_config.IsArgumentFilterEnabled() &&
        base::trace_event::TraceLog::GetInstance()
            ->GetArgumentFilterPredicate()
            .is_null()) {
      base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
          base::BindRepeating(&IsTraceEventArgsWhitelisted));
      base::trace_event::TraceLog::GetInstance()->SetMetadataFilterPredicate(
          base::BindRepeating(&IsMetadataWhitelisted));
    }

    // Perfetto backend configures buffer sizes when tracing is started in the
    // service (see perfetto_config.cc). Zero them out here to avoid DCHECKs
    // in TraceConfig::Merge.
    trace_config.SetTraceBufferSizeInKb(0);
    trace_config.SetTraceBufferSizeInEvents(0);

    if (trace_config.IsCategoryGroupEnabled(
            TRACE_DISABLED_BY_DEFAULT("cpu_profiler"))) {
      TracingSamplerProfiler::SetupStartupTracing();
    }
    TraceEventDataSource::GetInstance()->SetupStartupTracing(
        startup_config->GetSessionOwner() ==
            TraceStartupConfig::SessionOwner::kBackgroundTracing ||
        command_line.HasSwitch(switches::kTraceStartupEnablePrivacyFiltering));

    uint8_t modes = TraceLog::RECORDING_MODE;
    if (!trace_config.event_filters().empty())
      modes |= TraceLog::FILTERING_MODE;
    trace_log->SetEnabled(trace_config, modes);
  } else if (command_line.HasSwitch(switches::kTraceToConsole)) {
    // TODO(eseckler): Remove ability to trace to the console, perfetto doesn't
    // support this and noone seems to use it.
    TraceConfig trace_config = GetConfigForTraceToConsole();
    LOG(ERROR) << "Start " << switches::kTraceToConsole
               << " with CategoryFilter '"
               << trace_config.ToCategoryFilterString() << "'.";
    TraceEventDataSource::GetInstance()->SetupStartupTracing(
        /*privacy_filtering_enabled=*/false);
    trace_log->SetEnabled(trace_config, TraceLog::RECORDING_MODE);
  }
}

void InitTracingPostThreadPoolStartAndFeatureList() {
  if (g_tracing_initialized_after_threadpool_and_featurelist)
    return;
  g_tracing_initialized_after_threadpool_and_featurelist = true;
  // TODO(nuskos): We should switch these to DCHECK once we're reasonably
  // confident we've ensured this is called properly in all processes. Probably
  // after M78 release has been cut (since we'll verify in the rollout of M78).
  CHECK(base::ThreadPoolInstance::Get());
  CHECK(base::FeatureList::GetInstance());
  // Below are the things tracing must do once per process.
  TraceEventDataSource::GetInstance()->OnTaskSchedulerAvailable();
  if (ShouldSetupSystemTracing()) {
    // We have to ensure that we register all the data sources we care about.
    TraceEventAgent::GetInstance();
    // To ensure System tracing connects we have to initialize the process wide
    // state. This Get() call ensures that the constructor has run.
    PerfettoTracedProcess::Get();
  }
}

void PropagateTracingFlagsToChildProcessCmdLine(base::CommandLine* cmd_line) {
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();

  if (!trace_log->IsEnabled())
    return;

  // It's possible that tracing is enabled only for atrace, in which case the
  // TraceEventDataSource isn't registered. In that case, there's no reason to
  // enable startup tracing in the child process (and we wouldn't know the
  // correct value for privacy_filtering_enabled below).
  if (!TraceEventDataSource::GetInstance()->IsEnabled())
    return;

  // The child process startup may race with a concurrent disabling of the
  // tracing session by the tracing service. To avoid being stuck in startup
  // tracing mode forever, the child process will discard the startup session
  // after a timeout (|startup_tracing_timer_| in TraceEventDataSource).
  //
  // Note that we disregard the config's process filter, since it's possible
  // that the trace consumer will update the config to include the process
  // shortly. Otherwise, the startup tracing timeout in the child will
  // eventually disable tracing for the process.

  const auto trace_config = trace_log->GetCurrentTraceConfig();

  // We can't currently propagate event filter options, memory dump configs, or
  // trace buffer sizes via command line flags (they only support categories,
  // trace options, record mode). If event filters are set, we bail out here to
  // avoid recording events that we shouldn't in the child process. Even if
  // memory dump config is set, it's OK to propagate the remaining config,
  // because the child won't record events it shouldn't without it and will
  // adopt the memory dump config once it connects to the tracing service.
  // Buffer sizes configure the tracing service's central buffer, so also don't
  // affect local tracing.
  //
  // TODO(eseckler): Support propagating the full config via command line flags
  // somehow (--trace-config?). This will also need some rethinking to support
  // multiple concurrent tracing sessions in the future.
  if (!trace_config.event_filters().empty())
    return;

  // Make sure that the startup session uses privacy filtering mode if it's
  // enabled for the browser's session.
  if (TraceEventDataSource::GetInstance()->privacy_filtering_enabled())
    cmd_line->AppendSwitch(switches::kTraceStartupEnablePrivacyFiltering);

  cmd_line->AppendSwitchASCII(switches::kTraceStartup,
                              trace_config.ToCategoryFilterString());
  // The argument filtering setting is passed via trace options as part of
  // --trace-startup-record-mode.
  cmd_line->AppendSwitchASCII(switches::kTraceStartupRecordMode,
                              trace_config.ToTraceOptionsString());
}

}  // namespace tracing
