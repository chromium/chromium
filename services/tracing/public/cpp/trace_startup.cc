// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_startup.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/system_producer.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/trace_event_args_allowlist.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

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

  TraceEventDataSource::GetInstance()->RegisterStartupHooks();

  // Initialize the Perfetto client library now that we're past the zygote's
  // fork point. This is important to ensure Perfetto's track registry gets a
  // unique uuid for generating track ids.
  PerfettoTracedProcess::Get()->SetupClientLibrary();

  // Ensure TraceLog is initialized first.
  // https://crbug.com/764357
  TraceLog::GetInstance();
  auto* startup_config = TraceStartupConfig::GetInstance();

  if (startup_config->IsEnabled()) {
    // Ensure that data sources are created and registered.
    TraceEventAgent::GetInstance();

    TraceConfig trace_config = startup_config->GetTraceConfig();

    PerfettoProducer* producer =
        PerfettoTracedProcess::Get()->producer_client();
    if (startup_config->GetSessionOwner() ==
        TraceStartupConfig::SessionOwner::kSystemTracing) {
      PerfettoTracedProcess::Get()->SetupSystemTracing();
      producer = PerfettoTracedProcess::Get()->system_producer();
    }

    bool privacy_filtering_enabled =
        startup_config->GetSessionOwner() ==
            TraceStartupConfig::SessionOwner::kBackgroundTracing ||
        command_line.HasSwitch(switches::kTraceStartupEnablePrivacyFiltering);

    if (!PerfettoTracedProcess::Get()->SetupStartupTracing(
            producer, trace_config, privacy_filtering_enabled)) {
      startup_config->SetDisabled();
    }
  }
}

bool EnableStartupTracingForProcess(
    const base::trace_event::TraceConfig& trace_config,
    bool privacy_filtering_enabled) {
  return PerfettoTracedProcess::Get()->SetupStartupTracing(
      PerfettoTracedProcess::Get()->producer_client(), trace_config,
      privacy_filtering_enabled);
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

  PerfettoTracedProcess::Get()->OnThreadPoolAvailable();

  if (ShouldSetupSystemTracing()) {
    // Ensure that data sources are created and registered.
    TraceEventAgent::GetInstance();
    // Connect to system service if available (currently a no-op except on
    // Posix). Has to happen on the producer's sequence, as all communication
    // with the system Perfetto service should occur on a single sequence.
    if (!PerfettoTracedProcess::Get()->system_producer())
      PerfettoTracedProcess::Get()->SetupSystemTracing();
    PerfettoTracedProcess::Get()
        ->GetTaskRunner()
        ->GetOrCreateTaskRunner()
        ->PostTask(FROM_HERE, base::BindOnce([]() {
                     PerfettoTracedProcess::Get()
                         ->system_producer()
                         ->ConnectToSystemService();
                   }));
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

  // (Posix)SystemProducer doesn't currently support startup tracing, so don't
  // attempt to enable startup tracing in child processes if system tracing is
  // active.
  if (PerfettoTracedProcess::Get()->system_producer() &&
      PerfettoTracedProcess::Get()->system_producer()->IsTracingActive()) {
    return;
  }

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
