// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_startup.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/memory/shared_memory_switch.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "services/tracing/public/cpp/trace_event_args_allowlist.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

#if BUILDFLAG(IS_WIN)
#include "components/tracing/common/etw_export_win.h"
#endif

#if BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_IOS_TVOS)
#include "base/apple/mach_port_rendezvous.h"
#endif

namespace tracing {
namespace {

#if BUILDFLAG(IS_APPLE)
using base::shared_memory::SharedMemoryMachPortRendezvousKey;
constexpr SharedMemoryMachPortRendezvousKey kTraceConfigRendezvousKey = 'trcc';
constexpr SharedMemoryMachPortRendezvousKey kTraceBufferRendezvousKey = 'trbc';
#endif

using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;

}  // namespace

bool g_tracing_initialized = false;

bool IsTracingInitialized() {
  return g_tracing_initialized;
}

void InitTracing(
    bool enable_consumer,
    bool will_trace_thread_restart,
    bool enable_system_backend,
    base::RepeatingCallback<bool()> allow_system_tracing_consumer) {
  DCHECK(!g_tracing_initialized);
  g_tracing_initialized = true;

  std::optional<uint64_t> maybe_process_track_uuid;
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTraceProcessTrackUuid)) {
    uint64_t process_track_uuid;
    if (base::StringToUint64(
            command_line->GetSwitchValueASCII(switches::kTraceProcessTrackUuid),
            &process_track_uuid)) {
      maybe_process_track_uuid = process_track_uuid;
    }
  }

  // Create the PerfettoTracedProcess.
  auto& traced_process =
      PerfettoTracedProcess::MaybeCreateInstance(will_trace_thread_restart);
  if (allow_system_tracing_consumer) {
    traced_process.SetAllowSystemTracingConsumerCallback(
        std::move(allow_system_tracing_consumer));
  }
  traced_process.SetupClientLibrary(enable_consumer, enable_system_backend,
                                    maybe_process_track_uuid);

  RegisterTracedValueProtoWriter();

  // Ensure TraceLog is initialized first.
  // https://crbug.com/764357
  TraceLog::GetInstance();

#if BUILDFLAG(IS_WIN)
  tracing::EnableETWExport();
#endif  // BUILDFLAG(IS_WIN)

  auto& startup_config = TraceStartupConfig::GetInstance();

  if (startup_config.IsEnabled()) {
    auto perfetto_config = startup_config.GetPerfettoConfig();

    perfetto::Tracing::SetupStartupTracingOpts opts;
    opts.timeout_ms = kStartupTracingTimeoutMs;
    // TODO(khokhlov): Support startup tracing with the system backend in the
    // SDK build.
    opts.backend = perfetto::kCustomBackend;

    perfetto::Tracing::SetupStartupTracingBlocking(perfetto_config, opts);
  }
}

void InitTracingPostFeatureList(
    bool enable_consumer,
    bool will_trace_thread_restart,
    base::RepeatingCallback<bool()> allow_system_tracing_consumer) {
  DCHECK(base::FeatureList::GetInstance());
  InitTracing(enable_consumer, will_trace_thread_restart,
              ShouldSetupSystemTracing(),
              std::move(allow_system_tracing_consumer));
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

base::UnsafeSharedMemoryRegion CreateTracingOutputSharedMemory() {
#if DCHECK_IS_ON()
  // This should not be called if tracing config shm was not created beforehand.
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();
  const auto& startup_config = TraceStartupConfig::GetInstance();
  DCHECK(startup_config.IsEnabled() || trace_log->IsEnabled());

  if (!startup_config.IsEnabled()) {
    const auto sessions = trace_log->GetTrackEventSessions();
    bool has_relevant_config =
        std::any_of(sessions.begin(), sessions.end(), [](const auto& session) {
          return session.backend_type == perfetto::kCustomBackend &&
                 !session.config.has_interceptor_config();
        });
    DCHECK(has_relevant_config);
  }
#endif  // DCHECK_IS_ON()

  auto shm = base::UnsafeSharedMemoryRegion::Create(
      features::kPerfettoSharedMemorySizeBytes.Get());
  if (!shm.IsValid()) {
    return base::UnsafeSharedMemoryRegion();
  }
  return shm;
}

void COMPONENT_EXPORT(TRACING_CPP) AddTraceConfigToLaunchParameters(
    const base::ReadOnlySharedMemoryRegion& read_only_memory_region,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    base::GlobalDescriptors::Key descriptor_key,
    base::ScopedFD& out_descriptor_to_share,
#endif
    base::CommandLine* command_line,
    base::LaunchOptions* launch_options) {
  base::shared_memory::AddToLaunchParameters(switches::kTraceConfigHandle,
                                             read_only_memory_region,
#if BUILDFLAG(IS_APPLE)
                                             kTraceConfigRendezvousKey,
#elif BUILDFLAG(IS_POSIX)
                                             descriptor_key,
                                             out_descriptor_to_share,
#endif
                                             command_line, launch_options);
}

void COMPONENT_EXPORT(TRACING_CPP) AddTraceOutputToLaunchParameters(
    const base::UnsafeSharedMemoryRegion& unsafe_memory_region,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    base::GlobalDescriptors::Key descriptor_key,
    base::ScopedFD& out_descriptor_to_share,
#endif
    base::CommandLine* command_line,
    base::LaunchOptions* launch_options) {
  base::shared_memory::AddToLaunchParameters(switches::kTraceBufferHandle,
                                             unsafe_memory_region,
#if BUILDFLAG(IS_APPLE)
                                             kTraceBufferRendezvousKey,
#elif BUILDFLAG(IS_POSIX)
                                             descriptor_key,
                                             out_descriptor_to_share,
#endif
                                             command_line, launch_options);
}

}  // namespace tracing
