// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_startup.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/memory/shared_memory_switch.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/trace_session_observer.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

#if BUILDFLAG(IS_WIN)
#include "components/tracing/common/etw_export_win.h"
#endif

namespace tracing {
namespace {

using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;

class StartupTrackEventConfigObserver
    : public perfetto::TrackEventSessionObserver {
 public:
  static StartupTrackEventConfigObserver& GetInstance() {
    static base::NoDestructor<StartupTrackEventConfigObserver> instance;
    return *instance;
  }

  StartupTrackEventConfigObserver() {
    base::TrackEvent::AddSessionObserver(this);
  }

  // perfetto::TrackEventSessionObserver implementation.
  void OnSetup(const perfetto::DataSourceBase::SetupArgs& args) override {
    if (args.backend_type != perfetto::kCustomBackend ||
        args.config->has_interceptor_config()) {
      return;
    }
    base::AutoLock lock(lock_);
    track_event_sessions_.emplace(args.internal_instance_index,
                                  TrackEventSession(false, *args.config));
  }

  void OnStart(const perfetto::DataSourceBase::StartArgs& args) override {
    base::AutoLock lock(lock_);
    auto it = track_event_sessions_.find(args.internal_instance_index);
    if (it != track_event_sessions_.end()) {
      it->second.started = true;
    }
  }

  void OnStop(const perfetto::DataSourceBase::StopArgs& args) override {
    base::AutoLock lock(lock_);
    track_event_sessions_.erase(args.internal_instance_index);
  }

  std::optional<perfetto::DataSourceConfig> GetTrackEventConfig() {
    base::AutoLock lock(lock_);
    for (const auto& [session_id, session] : track_event_sessions_) {
      if (session.started) {
        return session.config;
      }
    }
    return std::nullopt;
  }

 private:
  ~StartupTrackEventConfigObserver() override {
    base::TrackEvent::RemoveSessionObserver(this);
  }

  struct TrackEventSession {
    bool started;
    perfetto::DataSourceConfig config;
  };
  base::Lock lock_;
  base::flat_map<uint32_t, TrackEventSession> track_event_sessions_
      GUARDED_BY(lock_);
};

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
  base::TimeTicks init_start = base::TimeTicks::Now();

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
  StartupTrackEventConfigObserver::GetInstance();
  base::trace_event::TraceSessionObserverList::Initialize();

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
  base::UmaHistogramTimes("Tracing.Init.InitTracing",
                          base::TimeTicks::Now() - init_start);
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
  const auto& startup_config = TraceStartupConfig::GetInstance();
  perfetto::TraceConfig trace_config;
  if (startup_config.IsEnabled()) {
    trace_config = startup_config.GetPerfettoConfig();
  } else if (auto maybe_config = StartupTrackEventConfigObserver::GetInstance()
                                     .GetTrackEventConfig();
             maybe_config.has_value()) {
    *trace_config.add_data_sources()->mutable_config() = *maybe_config;
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
  auto shm = base::UnsafeSharedMemoryRegion::Create(
      features::kPerfettoSharedMemorySizeBytes.Get());
  if (!shm.IsValid()) {
    return base::UnsafeSharedMemoryRegion();
  }
  return shm;
}

}  // namespace tracing
