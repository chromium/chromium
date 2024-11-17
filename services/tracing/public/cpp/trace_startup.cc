// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_startup.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/memory/shared_memory_switch.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "components/tracing/common/etw_export_win.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/system_producer.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/trace_event_args_allowlist.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/mach_port_rendezvous.h"
#endif

namespace tracing {
namespace {

#if BUILDFLAG(IS_APPLE)
constexpr base::MachPortsForRendezvous::key_type kTraceConfigRendezvousKey =
    'trcc';
#endif

constexpr uint32_t kStartupTracingTimeoutMs = 30 * 1000;  // 30 sec

using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;

}  // namespace

bool g_tracing_initialized_after_threadpool_and_featurelist = false;

bool IsTracingInitialized() {
  return g_tracing_initialized_after_threadpool_and_featurelist;
}

void EnableStartupTracingIfNeeded() {
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

    auto perfetto_config = startup_config.GetPerfettoConfig();

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
    const perfetto::TraceConfig& perfetto_config) {
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

base::ReadOnlySharedMemoryRegion CreateTracingConfigSharedMemory() {
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();
  const auto& startup_config = TraceStartupConfig::GetInstance();
  perfetto::TraceConfig trace_config;
  if (startup_config.IsEnabled()) {
    trace_config = startup_config.GetPerfettoConfig();
  } else if (trace_log->IsEnabled()) {
    bool has_relevant_config = false;
    for (const auto& session : trace_log->GetTrackEventSessions()) {
      if (session.backend_type == perfetto::kCustomBackend &&
          !session.config.has_interceptor_config()) {
        *trace_config.add_data_sources()->mutable_config() = session.config;
        has_relevant_config = true;
        break;
      }
    }
    if (!has_relevant_config) {
      return base::ReadOnlySharedMemoryRegion();
    }
  } else {
    return base::ReadOnlySharedMemoryRegion();
  }

  std::vector<uint8_t> serialized_config = trace_config.SerializeAsArray();

  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(serialized_config.size());
  if (!shm.IsValid()) {
    return base::ReadOnlySharedMemoryRegion();
  }
  base::span(shm.mapping).copy_from(serialized_config);
  return std::move(shm.region);
}

void COMPONENT_EXPORT(TRACING_CPP) AddTraceConfigToLaunchParameters(
    base::ReadOnlySharedMemoryRegion read_only_memory_region,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    base::GlobalDescriptors::Key descriptor_key,
    base::ScopedFD& out_descriptor_to_share,
#endif
    base::CommandLine* command_line,
    base::LaunchOptions* launch_options) {
  base::shared_memory::AddToLaunchParameters(switches::kTraceConfigHandle,
                                             std::move(read_only_memory_region),
#if BUILDFLAG(IS_APPLE)
                                             kTraceConfigRendezvousKey,
#elif BUILDFLAG(IS_POSIX)
                                             descriptor_key,
                                             out_descriptor_to_share,
#endif
                                             command_line, launch_options);
}

}  // namespace tracing
