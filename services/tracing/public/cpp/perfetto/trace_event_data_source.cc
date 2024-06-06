// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/stack_allocated.h"
#include "base/no_destructor.h"
#include "base/process/current_process.h"
#include "base/sequence_checker.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/tracing/trace_time.h"
#include "base/tracing/tracing_tls.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/custom_event_recorder.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/system_producer.h"
#include "services/tracing/public/cpp/perfetto/trace_string_lookup.h"
#include "services/tracing/public/cpp/perfetto/track_event_thread_local_event_sink.h"
#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#include "services/tracing/public/cpp/trace_event_args_allowlist.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/message.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pbzero.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

using TraceLog = base::trace_event::TraceLog;
using TraceEvent = base::trace_event::TraceEvent;
using TraceConfig = base::trace_event::TraceConfig;
using TracePacketHandle = perfetto::TraceWriter::TracePacketHandle;
using TraceRecordMode = base::trace_event::TraceRecordMode;
using perfetto::protos::pbzero::ChromeMetadataPacket;
using perfetto::protos::pbzero::ChromeProcessDescriptor;
using perfetto::protos::pbzero::ProcessDescriptor;
using perfetto::protos::pbzero::TrackDescriptor;

namespace tracing {
namespace {

TraceEventMetadataSource* g_trace_event_metadata_source_for_testing = nullptr;

static_assert(
    sizeof(TraceEventDataSource::SessionFlags) <= sizeof(uint64_t),
    "SessionFlags should remain small to ensure lock-free atomic operations");

// Helper class used to ensure no tasks are posted while
// TraceEventDataSource::lock_ is held.
class SCOPED_LOCKABLE AutoLockWithDeferredTaskPosting {
  STACK_ALLOCATED();

 public:
  explicit AutoLockWithDeferredTaskPosting(base::Lock& lock)
      EXCLUSIVE_LOCK_FUNCTION(lock)
      : autolock_(lock) {}

  ~AutoLockWithDeferredTaskPosting() UNLOCK_FUNCTION() = default;

 private:
  // The ordering is important: |defer_task_posting_| must be destroyed
  // after |autolock_| to ensure the lock is not held when any deferred
  // tasks are posted..
  base::ScopedDeferTaskPosting defer_task_posting_;
  base::AutoLock autolock_;
};

}  // namespace

using perfetto::protos::pbzero::ChromeEventBundle;
using ChromeEventBundleHandle = protozero::MessageHandle<ChromeEventBundle>;

// static
TraceEventMetadataSource* TraceEventMetadataSource::GetInstance() {
  static base::NoDestructor<TraceEventMetadataSource> instance;
  return instance.get();
}

TraceEventMetadataSource::TraceEventMetadataSource()
    : DataSourceBase(mojom::kMetaDataSourceName),
      origin_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  g_trace_event_metadata_source_for_testing = this;
  PerfettoTracedProcess::Get()->AddDataSource(this);
  AddGeneratorFunction(base::BindRepeating(
      &TraceEventMetadataSource::WriteMetadataPacket, base::Unretained(this)));
  AddGeneratorFunction(base::BindRepeating(
      &TraceEventMetadataSource::GenerateTraceConfigMetadataDict,
      base::Unretained(this)));

  perfetto::DataSourceDescriptor dsd;
  dsd.set_name(mojom::kMetaDataSourceName);
  DataSourceProxy::Register(dsd, this);
}

TraceEventMetadataSource::~TraceEventMetadataSource() = default;

void TraceEventMetadataSource::AddGeneratorFunction(
    JsonMetadataGeneratorFunction generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(lock_);
    json_generator_functions_.push_back(generator);
  }
  // An EventBundle is created when nullptr is passed.
  GenerateJsonMetadataFromGenerator(generator, nullptr);
}

void TraceEventMetadataSource::AddGeneratorFunction(
    MetadataGeneratorFunction generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(lock_);
    generator_functions_.push_back(generator);
  }
  GenerateMetadataFromGenerator(generator);
}

void TraceEventMetadataSource::AddGeneratorFunction(
    PacketGeneratorFunction generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(lock_);
    packet_generator_functions_.push_back(generator);
  }
  GenerateMetadataPacket(generator);
}

void TraceEventMetadataSource::WriteMetadataPacket(
    perfetto::protos::pbzero::ChromeMetadataPacket* metadata_proto,
    bool privacy_filtering_enabled) {
#if BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)
  // Version code is only set for official builds on Android.
  const char* version_code_str =
      base::android::BuildInfo::GetInstance()->package_version_code();
  if (version_code_str) {
    int version_code = 0;
    bool res = base::StringToInt(version_code_str, &version_code);
    DCHECK(res);
    metadata_proto->set_chrome_version_code(version_code);
  }
#endif  // BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)

  AutoLockWithDeferredTaskPosting lock(lock_);

  if (parsed_chrome_config_) {
    metadata_proto->set_enabled_categories(
        parsed_chrome_config_->ToCategoryFilterString());
  }
}

std::optional<base::Value::Dict>
TraceEventMetadataSource::GenerateTraceConfigMetadataDict() {
  AutoLockWithDeferredTaskPosting lock(lock_);
  if (chrome_config_.empty()) {
    return std::nullopt;
  }

  base::Value::Dict metadata;
  // If argument filtering is enabled, we need to check if the trace config is
  // allowlisted before emitting it.
  // TODO(eseckler): Figure out a way to solve this without calling directly
  // into IsMetadataAllowlisted().
  if (!parsed_chrome_config_->IsArgumentFilterEnabled() ||
      IsMetadataAllowlisted("trace-config")) {
    metadata.Set("trace-config", chrome_config_);
  } else {
    metadata.Set("trace-config", "__stripped__");
  }

  chrome_config_ = std::string();
  return metadata;
}

void TraceEventMetadataSource::GenerateMetadataFromGenerator(
    const TraceEventMetadataSource::MetadataGeneratorFunction& generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (!emit_metadata_at_start_)
      return;
  }
  DataSourceProxy::Trace([&](DataSourceProxy::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
    packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
    auto* chrome_metadata = packet->set_chrome_metadata();
    generator.Run(chrome_metadata, privacy_filtering_enabled_);
  });
}

void TraceEventMetadataSource::GenerateMetadataPacket(
    const TraceEventMetadataSource::PacketGeneratorFunction& generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (!emit_metadata_at_start_)
      return;
  }
  DataSourceProxy::Trace([&](DataSourceProxy::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
    packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
    generator.Run(packet.get(), privacy_filtering_enabled_);
  });
}

void TraceEventMetadataSource::GenerateJsonMetadataFromGenerator(
    const TraceEventMetadataSource::JsonMetadataGeneratorFunction& generator,
    ChromeEventBundle* event_bundle) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

  auto write_to_bundle = [&generator](ChromeEventBundle* bundle) {
    std::optional<base::Value::Dict> metadata_dict = generator.Run();
    if (!metadata_dict)
      return;
    for (auto it : *metadata_dict) {
      auto* new_metadata = bundle->add_metadata();
      new_metadata->set_name(it.first.c_str());

      if (it.second.is_int()) {
        new_metadata->set_int_value(it.second.GetInt());
      } else if (it.second.is_bool()) {
        new_metadata->set_bool_value(it.second.GetBool());
      } else if (it.second.is_string()) {
        new_metadata->set_string_value(it.second.GetString().c_str());
      } else {
        std::string json_value;
        base::JSONWriter::Write(it.second, &json_value);
        new_metadata->set_json_value(json_value.c_str());
      }
    }
  };

  if (event_bundle) {
    write_to_bundle(event_bundle);
    return;
  }
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (!emit_metadata_at_start_)
      return;
  }
  DataSourceProxy::Trace([&](DataSourceProxy::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
    packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
    write_to_bundle(packet->set_chrome_events());
  });
}

void TraceEventMetadataSource::GenerateMetadata(
    std::unique_ptr<
        std::vector<TraceEventMetadataSource::JsonMetadataGeneratorFunction>>
        json_generators,
    std::unique_ptr<
        std::vector<TraceEventMetadataSource::MetadataGeneratorFunction>>
        proto_generators,
    std::unique_ptr<
        std::vector<TraceEventMetadataSource::PacketGeneratorFunction>>
        packet_generators) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

  bool privacy_filtering_enabled;
  base::trace_event::TraceRecordMode record_mode;
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    privacy_filtering_enabled = privacy_filtering_enabled_;
    record_mode = parsed_chrome_config_->GetTraceRecordMode();
  }

  DataSourceProxy::Trace([&](DataSourceProxy::TraceContext ctx) {
    for (auto& generator : *packet_generators) {
      auto packet = ctx.NewTracePacket();
      packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
      packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
      generator.Run(packet.get(), privacy_filtering_enabled);
    }

    {
      auto trace_packet = ctx.NewTracePacket();
      trace_packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
      trace_packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
      auto* chrome_metadata = trace_packet->set_chrome_metadata();
      for (auto& generator : *proto_generators) {
        generator.Run(chrome_metadata, privacy_filtering_enabled);
      }
    }

    if (!privacy_filtering_enabled) {
      auto trace_packet = ctx.NewTracePacket();
      trace_packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
      trace_packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
      ChromeEventBundle* event_bundle = trace_packet->set_chrome_events();
      for (auto& generator : *json_generators) {
        GenerateJsonMetadataFromGenerator(generator, event_bundle);
      }
    }

    // When not using ring buffer mode (but instead record-until-full / discard
    // mode), force flush the packets since the default flush happens at end of
    // the trace, and the trace writer's chunk could then be discarded.
    if (record_mode != base::trace_event::RECORD_CONTINUOUSLY)
      ctx.Flush();
  });
}

void TraceEventMetadataSource::StartTracingImpl(
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  auto json_generators =
      std::make_unique<std::vector<JsonMetadataGeneratorFunction>>();
  auto proto_generators =
      std::make_unique<std::vector<MetadataGeneratorFunction>>();
  auto packet_generators =
      std::make_unique<std::vector<PacketGeneratorFunction>>();
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    privacy_filtering_enabled_ =
        data_source_config.chrome_config().privacy_filtering_enabled();
    chrome_config_ = data_source_config.chrome_config().trace_config();
    parsed_chrome_config_ = std::make_unique<TraceConfig>(chrome_config_);
    switch (parsed_chrome_config_->GetTraceRecordMode()) {
      case TraceRecordMode::RECORD_UNTIL_FULL:
      case TraceRecordMode::RECORD_AS_MUCH_AS_POSSIBLE: {
        emit_metadata_at_start_ = true;
        *json_generators = json_generator_functions_;
        *proto_generators = generator_functions_;
        *packet_generators = packet_generator_functions_;
        break;
      }
      case TraceRecordMode::RECORD_CONTINUOUSLY:
      case TraceRecordMode::ECHO_TO_CONSOLE:
        emit_metadata_at_start_ = false;
        return;
    }
  }
  // |emit_metadata_at_start_| is true if we are in discard packets mode, write
  // metadata at the beginning of the trace to make it less likely to be
  // dropped.
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TraceEventMetadataSource::GenerateMetadata,
                     base::Unretained(this), std::move(json_generators),
                     std::move(proto_generators),
                     std::move(packet_generators)));
}

void TraceEventMetadataSource::StopTracingImpl(
    base::OnceClosure stop_complete_callback) {
  base::OnceClosure maybe_generate_task = base::DoNothing();
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (!emit_metadata_at_start_) {
      // Write metadata at the end of tracing if not emitted at start (in ring
      // buffer mode), to make it less likely that it is overwritten by other
      // trace data in perfetto's ring buffer.
      auto json_generators =
          std::make_unique<std::vector<JsonMetadataGeneratorFunction>>();
      *json_generators = json_generator_functions_;
      auto proto_generators =
          std::make_unique<std::vector<MetadataGeneratorFunction>>();
      *proto_generators = generator_functions_;
      auto packet_generators =
          std::make_unique<std::vector<PacketGeneratorFunction>>();
      *packet_generators = packet_generator_functions_;
      maybe_generate_task = base::BindOnce(
          &TraceEventMetadataSource::GenerateMetadata, base::Unretained(this),
          std::move(json_generators), std::move(proto_generators),
          std::move(packet_generators));
    }
  }
  // Even when not generating metadata, make sure the metadata generate task
  // posted at the start is finished, by posting task on origin task runner.
  origin_task_runner_->PostTaskAndReply(
      FROM_HERE, std::move(maybe_generate_task),
      base::BindOnce(
          [](TraceEventMetadataSource* ds,
             base::OnceClosure stop_complete_callback) {
            {
              AutoLockWithDeferredTaskPosting lock(ds->lock_);
              DataSourceProxy::Trace(
                  [&](DataSourceProxy::TraceContext ctx) { ctx.Flush(); });
              ds->chrome_config_ = std::string();
              ds->parsed_chrome_config_.reset();
              ds->emit_metadata_at_start_ = false;
            }
            std::move(stop_complete_callback).Run();
          },
          base::Unretained(this), std::move(stop_complete_callback)));
}

void TraceEventMetadataSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  origin_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                        std::move(flush_complete_callback));
}

base::SequencedTaskRunner* TraceEventMetadataSource::GetTaskRunner() {
  // Much like the data source, the task runner is long-lived, so returning the
  // raw pointer here is safe.
  base::AutoLock lock(lock_);
  return origin_task_runner_.get();
}

void TraceEventMetadataSource::ResetForTesting() {
  if (!g_trace_event_metadata_source_for_testing)
    return;
  g_trace_event_metadata_source_for_testing->~TraceEventMetadataSource();
  new (g_trace_event_metadata_source_for_testing) TraceEventMetadataSource;
}

namespace {

base::ThreadLocalStorage::Slot* ThreadLocalEventSinkSlot() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot>
      thread_local_event_sink_tls([](void* event_sink) {
        const base::AutoReset<bool> resetter(
            base::tracing::GetThreadIsInTraceEvent(), true, false);
        delete static_cast<TrackEventThreadLocalEventSink*>(event_sink);
      });

  return thread_local_event_sink_tls.get();
}

TraceEventDataSource* g_trace_event_data_source_for_testing = nullptr;

}  // namespace

// static
TraceEventDataSource* TraceEventDataSource::GetInstance() {
  static base::NoDestructor<TraceEventDataSource> instance;
  return instance.get();
}

// static
void TraceEventDataSource::ResetForTesting() {
  if (!g_trace_event_data_source_for_testing)
    return;
  g_trace_event_data_source_for_testing->~TraceEventDataSource();
  new (g_trace_event_data_source_for_testing) TraceEventDataSource;
}

TraceEventDataSource::TraceEventDataSource()
    : DataSourceBase(mojom::kTraceEventDataSourceName),
      disable_interning_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPerfettoDisableInterning)) {
  // Use an approximate creation time as this is not available as TimeTicks in
  // all platforms.
  process_creation_time_ticks_ = TRACE_TIME_TICKS_NOW();

  DCHECK(session_flags_.is_lock_free())
      << "SessionFlags are not atomic! We rely on efficient lock-free look-up "
         "of the session flags when emitting a trace event.";
  g_trace_event_data_source_for_testing = this;
}

TraceEventDataSource::~TraceEventDataSource() = default;

void TraceEventDataSource::RegisterStartupHooks() {
}

void TraceEventDataSource::RegisterWithTraceLog() {
  TraceLog::GetInstance()->SetAddTraceEventOverrides(
      &TraceEventDataSource::OnAddLegacyTraceEvent,
      &TraceEventDataSource::FlushCurrentThread,
      &TraceEventDataSource::OnUpdateDuration);

  base::AutoLock l(lock_);
  is_enabled_ = true;
}

void TraceEventDataSource::OnStopTracingDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

  // WARNING: This function might never be called at the end of a tracing
  // session. See comment in StartTracing() for more information.

  // Unregister overrides.
  TraceLog::GetInstance()->SetAddTraceEventOverrides(nullptr, nullptr, nullptr);

  base::OnceClosure task;
  {
    base::AutoLock l(lock_);
    is_enabled_ = false;

    // Check for any start or stop tracing pending task.
    task = std::move(flush_complete_task_);
    flushing_trace_log_ = false;

    IncrementSessionIdOrClearStartupFlagWhileLocked();
  }
  if (stop_complete_callback_) {
    std::move(stop_complete_callback_).Run();
  }
  if (task) {
    std::move(task).Run();
  }
}

// static
TrackEventThreadLocalEventSink* TraceEventDataSource::GetOrPrepareEventSink(bool create_if_needed) {
  // Avoid re-entrancy, which can happen during PostTasks (the taskqueue can
  // emit trace events). We discard the events in this case, as any PostTasking
  // to deal with these events later would break the event ordering that the
  // JSON traces rely on to merge 'A'/'B' events, as well as having to deal with
  // updating duration of 'X' events which haven't been added yet.
  if (*base::tracing::GetThreadIsInTraceEvent()) {
    return nullptr;
  }

  const base::AutoReset<bool> resetter(base::tracing::GetThreadIsInTraceEvent(),
                                       true);

  auto* thread_local_event_sink = static_cast<TrackEventThreadLocalEventSink*>(
      ThreadLocalEventSinkSlot()->Get());

  // Make sure the sink was reset since the last tracing session. Normally, it
  // is reset on Flush after the session is disabled. However, it may not have
  // been reset if the current thread doesn't support flushing. In that case, we
  // need to check here that it writes to the right buffer.
  //
  // Because we want to avoid locking for each event, we access |session_flags_|
  // racily. It's OK if we don't see it change to the session immediately. In
  // that case, the first few trace events may get lost, but we will eventually
  // notice that we are writing to the wrong buffer once the change to
  // |session_flags_| has propagated, and reset the sink. Note we will still
  // acquire the |lock_| to safely recreate the sink in
  // CreateThreadLocalEventSink().
  if (thread_local_event_sink) {
    SessionFlags new_session_flags =
        GetInstance()->session_flags_.load(std::memory_order_relaxed);
    if (new_session_flags.session_id != thread_local_event_sink->session_id()) {
      delete thread_local_event_sink;
      thread_local_event_sink = nullptr;
      ThreadLocalEventSinkSlot()->Set(nullptr);
    }
  }

  if (!thread_local_event_sink && create_if_needed) {
    thread_local_event_sink = GetInstance()->CreateThreadLocalEventSink();
    ThreadLocalEventSinkSlot()->Set(thread_local_event_sink);
  }

  return thread_local_event_sink;
}

bool TraceEventDataSource::IsEnabled() {
  base::AutoLock l(lock_);
  return is_enabled_;
}

void TraceEventDataSource::SetupStartupTracing(
    PerfettoProducer* producer,
    const base::trace_event::TraceConfig& trace_config,
    bool privacy_filtering_enabled) {
  NOTREACHED_IN_MIGRATION() << "This is not expected to run in SDK build.";

  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    // Do not enable startup tracing if trace log is being flushed. The
    // previous tracing session has not ended yet.
    if (flushing_trace_log_) {
      return;
    }
    // No need to do anything if startup tracing has already been set,
    // or we know Perfetto has already been setup.
    if (IsStartupTracingActive() || producer_) {
      DCHECK(!privacy_filtering_enabled || privacy_filtering_enabled_);
      return;
    }

    producer_ = producer;
    privacy_filtering_enabled_ = privacy_filtering_enabled;
    record_mode_ = trace_config.GetTraceRecordMode();

    SetStartupTracingFlagsWhileLocked();

    DCHECK(!trace_writer_);
    trace_writer_ = CreateTraceWriterLocked();
  }
  EmitRecurringUpdates();

  RegisterWithTraceLog();
  CustomEventRecorder::GetInstance()->OnStartupTracingStarted(
      trace_config, privacy_filtering_enabled);

}

void TraceEventDataSource::AbortStartupTracing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
  std::unique_ptr<perfetto::TraceWriter> trace_writer;
  PerfettoProducer* producer;
  uint32_t session_id;
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (!IsStartupTracingActive()) {
      return;
    }

    producer = producer_;

    // Prevent recreation of ThreadLocalEventSinks after flush.
    producer_ = nullptr;
    target_buffer_ = 0;
    flushing_trace_log_ = true;
    trace_writer = std::move(trace_writer_);

    // Increment the session id to make sure that any subsequent tracing session
    // uses fresh trace writers.
    session_id = IncrementSessionIdOrClearStartupFlagWhileLocked();
  }
  if (trace_writer) {
    ReturnTraceWriter(std::move(trace_writer));
  }
  producer->AbortStartupTracingForReservation(session_id);
  auto* trace_log = base::trace_event::TraceLog::GetInstance();
  trace_log->SetDisabled();
  trace_log->Flush(base::BindRepeating(&TraceEventDataSource::OnFlushFinished,
                                       base::Unretained(this)),
                   /*use_worker_thread=*/false);
}

uint32_t
TraceEventDataSource::IncrementSessionIdOrClearStartupFlagWhileLocked() {
  // Protected by |lock_| for CreateThreadLocalEventSink() and
  // SetStartupTracingFlagsWhileLocked().
  lock_.AssertAcquired();
  SessionFlags flags = session_flags_.load(std::memory_order_relaxed);
  if (flags.is_startup_tracing) {
    // Don't increment the session ID if startup tracing was active for this
    // session. This way, the sinks that were created while startup tracing for
    // the current session won't be cleared away (resetting such sinks could
    // otherwise cause data buffered in their potentially still unbound
    // StartupTraceWriters to be lost).
    flags.is_startup_tracing = false;
  } else {
    flags.session_id++;
  }
  session_flags_.store(flags, std::memory_order_relaxed);
  return flags.session_id;
}

void TraceEventDataSource::SetStartupTracingFlagsWhileLocked() {
  // Protected by |lock_| for CreateThreadLocalEventSink() and
  // IncrementSessionIdOrClearStartupFlagWhileLocked().
  lock_.AssertAcquired();
  SessionFlags flags = session_flags_.load(std::memory_order_relaxed);
  flags.is_startup_tracing = true;
  flags.session_id++;
  session_flags_.store(flags, std::memory_order_relaxed);
}

bool TraceEventDataSource::IsStartupTracingActive() const {
  SessionFlags flags = session_flags_.load(std::memory_order_relaxed);
  return flags.is_startup_tracing;
}

void TraceEventDataSource::OnFlushFinished(
    const scoped_refptr<base::RefCountedString>&,
    bool has_more_events) {
  if (has_more_events) {
    return;
  }

  // Clear the pending task on the tracing service thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
  base::OnceClosure task;
  {
    AutoLockWithDeferredTaskPosting l(lock_);
    // Run any pending start or stop tracing
    // task.
    task = std::move(flush_complete_task_);
    flushing_trace_log_ = false;
  }
  if (task) {
    std::move(task).Run();
  }
}

void TraceEventDataSource::StartTracingImpl(
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
  {
    AutoLockWithDeferredTaskPosting l(lock_);
    if (flushing_trace_log_) {
      // Delay start tracing until flush is finished. Perfetto can call start
      // while flushing if startup tracing (started by ourself) is cancelled, or
      // when perfetto force aborts session without waiting for stop acks.
      // |flush_complete_task_| will not be null here if perfetto calls start,
      // stop and start again all while flushing trace log for a previous
      // session, without waiting for stop complete callback for both. In all
      // these cases it is safe to just drop the |flush_complete_callback_|,
      // which is supposed to run OnStopTracingDone() and send stop ack to
      // Perfetto, but Perfetto already ignored the ack and continued.
      // Unretained is fine here because the producer will be valid till stop
      // tracing is called and at stop this task will be cleared.
      flush_complete_task_ = base::BindOnce(
          &TraceEventDataSource::StartTracingInternal, base::Unretained(this),
          base::Unretained(producer), data_source_config);
      return;
    }
  }
  StartTracingInternal(producer, data_source_config);
}

void TraceEventDataSource::StartTracingInternal(
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  NOTREACHED_IN_MIGRATION() << "This is not expected to run in SDK build.";

  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
  auto trace_config =
      TraceConfig(data_source_config.chrome_config().trace_config());

  bool startup_tracing_active;
  uint32_t session_id;
  {
    AutoLockWithDeferredTaskPosting lock(lock_);

    bool should_enable_filtering =
        data_source_config.chrome_config().privacy_filtering_enabled();

    startup_tracing_active = IsStartupTracingActive();
    if (startup_tracing_active) {
      CHECK(!should_enable_filtering || privacy_filtering_enabled_)
          << "Startup tracing was active without privacy filtering when "
             "service started tracing with privacy filtering.";
      DCHECK_EQ(producer_, producer)
          << "Startup tracing was taken over by a different PerfettoProducer";
    }

    privacy_filtering_enabled_ = should_enable_filtering;
    record_mode_ = trace_config.GetTraceRecordMode();

    producer_ = producer;
    target_buffer_ = data_source_config.target_buffer();
    session_id = IncrementSessionIdOrClearStartupFlagWhileLocked();

    if (!trace_writer_) {
      trace_writer_ = CreateTraceWriterLocked();
    }
  }

  // SetupStartupTracing() will not setup a new startup session after we set
  // |producer_| above, so accessing |startup_tracing_active| outside the lock
  // is safe.
  if (startup_tracing_active) {
    // Binding startup buffers may cause tasks to be posted. Disable trace
    // events to avoid deadlocks due to reentrancy into tracing code.
    const base::AutoReset<bool> resetter(
        base::tracing::GetThreadIsInTraceEvent(), true, false);
    producer->BindStartupTargetBuffer(session_id,
                                      data_source_config.target_buffer());
  } else {
    RegisterWithTraceLog();
  }

  // We emit the track/process descriptor another time even if we were
  // previously startup tracing, because the process name may have changed.
  EmitRecurringUpdates();

  TraceLog::GetInstance()->SetEnabled(trace_config, TraceLog::RECORDING_MODE);

  CustomEventRecorder::GetInstance()->OnTracingStarted(data_source_config);
}

void TraceEventDataSource::StopTracingImpl(
    base::OnceClosure stop_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
  NOTREACHED_IN_MIGRATION() << "This is not expected to run in SDK build.";

  CustomEventRecorder::GetInstance()->OnTracingStopped(base::OnceClosure());

  stop_complete_callback_ = std::move(stop_complete_callback);

  bool was_enabled = TraceLog::GetInstance()->IsEnabled();
  if (was_enabled) {
    TraceLog::GetInstance()->SetDisabled();
  }

  auto on_tracing_stopped_callback =
      [](TraceEventDataSource* data_source,
         perfetto::TraceWriter* trace_writer_raw,
         const scoped_refptr<base::RefCountedString>&, bool has_more_events) {
        if (has_more_events) {
          return;
        }
        std::unique_ptr<perfetto::TraceWriter> trace_writer(trace_writer_raw);
        if (trace_writer) {
          trace_writer->Flush();
          data_source->ReturnTraceWriter(std::move(trace_writer));
        }
        data_source->OnStopTracingDone();
      };

  std::unique_ptr<perfetto::TraceWriter> trace_writer;
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (flush_complete_task_) {
      DCHECK(!producer_);
      // Skip start tracing task at this point if we still have not flushed
      // trace log. We would only replace a start tracing call here since the
      // current StopTracing call should have a matching start call. The service
      // never calls consecutive start or stop. It is ok to ignore the start
      // here since the session has already ended, before we finished flushing.
      flush_complete_task_ = base::BindOnce(
          std::move(on_tracing_stopped_callback), base::Unretained(this),
          nullptr, scoped_refptr<base::RefCountedString>(), false);
      return;
    }
    // Prevent recreation of ThreadLocalEventSinks after flush.
    DCHECK(producer_);
    producer_ = nullptr;
    target_buffer_ = 0;
    flushing_trace_log_ = was_enabled;
    trace_writer = std::move(trace_writer_);
  }

  // Keep the trace writer around until the stop is complete, so that it is
  // flushed last and its data has a high likelihood of making it into the
  // buffer when in ring-buffer mode.
  perfetto::TraceWriter* trace_writer_raw = trace_writer.release();

  if (was_enabled) {
    // TraceLog::SetDisabled will cause metadata events to be written; make
    // sure we flush the TraceWriter for this thread (TraceLog will only call
    // TraceEventDataSource::FlushCurrentThread for threads with a MessageLoop).
    // TODO(eseckler): Flush all worker threads.
    // TODO(oysteine): The perfetto service itself should be able to recover
    // unreturned chunks so technically this can go away at some point, but
    // seems needed for now.
    FlushCurrentThread();

    // Flush the remaining threads via TraceLog. We call CancelTracing because
    // we don't want/need TraceLog to do any of its own JSON serialization.
    TraceLog::GetInstance()->CancelTracing(
        base::BindRepeating(on_tracing_stopped_callback, base::Unretained(this),
                            base::Unretained(trace_writer_raw)));
  } else {
    on_tracing_stopped_callback(this, trace_writer_raw,
                                scoped_refptr<base::RefCountedString>(), false);
  }
}

void TraceEventDataSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
  DCHECK(TraceLog::GetInstance()->IsEnabled());
  TraceLog::GetInstance()->Flush(base::BindRepeating(
      [](base::RepeatingClosure flush_complete_callback,
         const scoped_refptr<base::RefCountedString>&, bool has_more_events) {
        if (has_more_events) {
          return;
        }

        flush_complete_callback.Run();
      },
      std::move(flush_complete_callback)));
}

void TraceEventDataSource::ClearIncrementalState() {
  NOTREACHED_IN_MIGRATION() << "This is not expected to run in SDK build.";

  TrackEventThreadLocalEventSink::ClearIncrementalState();
  EmitRecurringUpdates();
  base::trace_event::TraceLog::GetInstance()->OnIncrementalStateCleared();
}

std::unique_ptr<perfetto::TraceWriter>
TraceEventDataSource::CreateTraceWriterLocked() {
  lock_.AssertAcquired();

  if (IsStartupTracingActive()) {
    DCHECK(producer_);
    uint32_t session_id =
        session_flags_.load(std::memory_order_relaxed).session_id;
    return producer_->CreateStartupTraceWriter(session_id);
  }

  // |producer_| is reset when tracing is stopped, and trace writer creation can
  // happen in parallel on any thread.
  if (producer_) {
    return producer_->CreateTraceWriter(target_buffer_);
  }

  return nullptr;
}

TrackEventThreadLocalEventSink*
TraceEventDataSource::CreateThreadLocalEventSink() {
  AutoLockWithDeferredTaskPosting lock(lock_);
  uint32_t session_id =
      session_flags_.load(std::memory_order_relaxed).session_id;

  auto trace_writer = CreateTraceWriterLocked();
  if (!trace_writer) {
    return nullptr;
  }

  return new TrackEventThreadLocalEventSink(std::move(trace_writer), session_id,
                                            disable_interning_,
                                            privacy_filtering_enabled_);
}

// static
void TraceEventDataSource::OnAddLegacyTraceEvent(
    TraceEvent* trace_event,
    bool thread_will_flush,
    base::trace_event::TraceEventHandle* handle) {
  auto* thread_local_event_sink = GetOrPrepareEventSink();
  if (thread_local_event_sink) {
    const base::AutoReset<bool> resetter(
        base::tracing::GetThreadIsInTraceEvent(), true, false);
    thread_local_event_sink->AddLegacyTraceEvent(trace_event, handle);
  }
}

// static
base::trace_event::TrackEventHandle TraceEventDataSource::OnAddTypedTraceEvent(
    base::trace_event::TraceEvent* trace_event) {
  auto* thread_local_event_sink = GetOrPrepareEventSink();
  if (thread_local_event_sink) {
    // GetThreadIsInTraceEvent() is handled by the sink for typed events.
    return thread_local_event_sink->AddTypedTraceEvent(trace_event);
  }
  return base::trace_event::TrackEventHandle();
}

// static
void TraceEventDataSource::OnUpdateDuration(
    const unsigned char* category_group_enabled,
    const char* name,
    base::trace_event::TraceEventHandle handle,
    base::PlatformThreadId thread_id,
    bool explicit_timestamps,
    const base::TimeTicks& now,
    const base::ThreadTicks& thread_now) {
  if (*base::tracing::GetThreadIsInTraceEvent()) {
    return;
  }

  const base::AutoReset<bool> resetter(base::tracing::GetThreadIsInTraceEvent(),
                                       true);

  auto* thread_local_event_sink = static_cast<TrackEventThreadLocalEventSink*>(
      ThreadLocalEventSinkSlot()->Get());
  if (thread_local_event_sink) {
    thread_local_event_sink->UpdateDuration(
        category_group_enabled, name, handle, thread_id, explicit_timestamps,
        now, thread_now);
  }
}

// static
base::trace_event::TracePacketHandle TraceEventDataSource::OnAddTracePacket() {
  auto* thread_local_event_sink = GetOrPrepareEventSink();
  if (thread_local_event_sink) {
    // GetThreadIsInTraceEvent() is handled by the sink for trace packets.
    return thread_local_event_sink->AddTracePacket();
  }
  return base::trace_event::TracePacketHandle();
}

// static
void TraceEventDataSource::OnAddEmptyPacket() {
  auto* thread_local_event_sink = GetOrPrepareEventSink(false);
  if (thread_local_event_sink) {
    // GetThreadIsInTraceEvent() is handled by the sink for trace packets.
    thread_local_event_sink->AddEmptyPacket();
  }
}

// static
void TraceEventDataSource::FlushCurrentThread() {
  auto* thread_local_event_sink = static_cast<TrackEventThreadLocalEventSink*>(
      ThreadLocalEventSinkSlot()->Get());
  if (thread_local_event_sink) {
    // Prevent any events from being emitted while we're deleting
    // the sink (like from the TraceWriter being PostTask'ed for deletion).
    const base::AutoReset<bool> resetter(
        base::tracing::GetThreadIsInTraceEvent(), true, false);
    thread_local_event_sink->Flush();
    // TODO(oysteine): To support flushing while still recording, this needs to
    // be changed to not destruct the TLS object as that will emit any
    // uncompleted _COMPLETE events on the stack.
    delete thread_local_event_sink;
    ThreadLocalEventSinkSlot()->Set(nullptr);
  }
}

void TraceEventDataSource::ReturnTraceWriter(
    std::unique_ptr<perfetto::TraceWriter> trace_writer) {
  {
    // Prevent concurrent changes to |session_flags_|.
    AutoLockWithDeferredTaskPosting lock(lock_);

    // If we don't have a task runner yet, we cannot create the task runner
    // safely, because the thread pool may not have been brought up yet. This
    // should only happen during startup tracing.
    if (!PerfettoTracedProcess::GetTaskRunner()->HasTaskRunner()) {
      DCHECK(IsStartupTracingActive());
      // It's safe to destroy the TraceWriter on the current sequence, as the
      // destruction won't post tasks or make mojo calls, because the arbiter
      // wasn't bound yet.
      trace_writer.reset();
      return;
    }
  }

  // Return the TraceWriter on the sequence that the PerfettoProducers run on.
  // Needed as the TrackEventThreadLocalEventSink gets deleted on thread
  // shutdown and we can't safely call task runner GetCurrentDefault() at that
  // point (which can happen as the TraceWriter destructor might issue a Mojo
  // call synchronously, which can trigger a call to
  // task runner GetCurrentDefault()).
  auto* trace_writer_raw = trace_writer.release();
  ANNOTATE_LEAKING_OBJECT_PTR(trace_writer_raw);
  // Use PostTask() on PerfettoTaskRunner to ensure we comply with
  // base::ScopedDeferTaskPosting.
  PerfettoTracedProcess::GetTaskRunner()->PostTask(
      // Capture writer as raw pointer so that we leak it if task posting
      // fails (during shutdown).
      [trace_writer_raw]() { delete trace_writer_raw; });
}

void TraceEventDataSource::EmitRecurringUpdates() {
  CustomEventRecorder::EmitRecurringUpdates();
  EmitTrackDescriptor();
}

void TraceEventDataSource::EmitTrackDescriptor() {
  // Prevent reentrancy into tracing code (flushing the trace writer sends a
  // mojo message which can result in additional trace events).
  const base::AutoReset<bool> resetter(base::tracing::GetThreadIsInTraceEvent(),
                                       true, false);

  // It's safe to use this writer outside the lock because EmitTrackDescriptor()
  // is either called (a) when startup tracing is set up (from the main thread)
  // or (b) on the perfetto sequence. (a) is safe because the writer will not be
  // destroyed before startup tracing set up is complete. (b) is safe because
  // the writer is only destroyed on the perfetto sequence in this case.
  perfetto::TraceWriter* writer;
  bool privacy_filtering_enabled;
  base::trace_event::TraceRecordMode record_mode;
#if BUILDFLAG(IS_ANDROID)
  bool is_system_producer;
#endif  // BUILDFLAG(IS_ANDROID)
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    writer = trace_writer_.get();
    privacy_filtering_enabled = privacy_filtering_enabled_;
    record_mode = record_mode_;
#if BUILDFLAG(IS_ANDROID)
    is_system_producer =
        producer_ == PerfettoTracedProcess::Get()->system_producer();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  if (!writer) {
    return;
  }

  // In ring buffer mode, flush any prior packets now, so that they can be
  // overridden and cleared away by later data, and so that new packets are
  // fully contained within one chunk (reducing data loss risk).
  if (record_mode == base::trace_event::RECORD_CONTINUOUSLY)
    trace_writer_->Flush();

  base::ProcessId process_id = TraceLog::GetInstance()->process_id();
  if (process_id == base::kNullProcessId) {
    // Do not emit descriptor without process id.
    return;
  }

  std::string process_name = base::CurrentProcess::GetInstance().GetName({});

  TracePacketHandle trace_packet = writer->NewTracePacket();

  trace_packet->set_sequence_flags(
      perfetto::protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);
  trace_packet->set_timestamp(
      TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds());
  trace_packet->set_timestamp_clock_id(base::tracing::kTraceClockId);

  TrackDescriptor* track_descriptor = trace_packet->set_track_descriptor();
  auto process_track = perfetto::ProcessTrack::Current();

  // TODO(eseckler): Call process_track.Serialize() here instead once the
  // client lib also fills in the ProcessDescriptor's process_name, gets the
  // correct pid from Chrome, and supports privacy filtering.
  track_descriptor->set_uuid(process_track.uuid);
  PERFETTO_DCHECK(!process_track.parent_uuid);

  ProcessDescriptor* process = track_descriptor->set_process();
  process->set_pid(process_id);
  process->set_start_timestamp_ns(
      process_creation_time_ticks_.since_origin().InNanoseconds());
  if (!privacy_filtering_enabled) {
    process->set_process_name(process_name);
    for (const auto& label : TraceLog::GetInstance()->process_labels()) {
      process->add_process_labels(label.second);
    }
  }

  ChromeProcessDescriptor* chrome_process =
      track_descriptor->set_chrome_process();
  auto process_type = base::CurrentProcess::GetInstance().GetType({});
  if (process_type != ChromeProcessDescriptor::PROCESS_UNSPECIFIED) {
    chrome_process->set_process_type(process_type);
  }

  // Add the crash trace ID to all the traces uploaded. If there are crashes
  // during this tracing session, then the crash will contain the process's
  // trace ID as "chrome-trace-id" crash key. This should be emitted
  // periodically to ensure it is present in the traces when the process
  // crashes. Metadata can go missing if process crashes. So, record this in
  // process descriptor.
  static const std::optional<uint64_t> crash_trace_id = GetTraceCrashId();
  if (crash_trace_id) {
    chrome_process->set_crash_trace_id(*crash_trace_id);
  }

#if BUILDFLAG(IS_ANDROID)
  // Host app package name is only recorded if the corresponding TraceLog
  // setting is set to true and privacy filtering is disabled or this is a
  // system trace.
  if (TraceLog::GetInstance()->ShouldRecordHostAppPackageName() &&
      (!privacy_filtering_enabled || is_system_producer)) {
    // Host app package name is used to group information from different
    // processes that "belong" to the same WebView app.
    if (process_type == ChromeProcessDescriptor::PROCESS_RENDERER ||
        process_type == ChromeProcessDescriptor::PROCESS_BROWSER) {
      chrome_process->set_host_app_package_name(
          base::android::BuildInfo::GetInstance()->host_package_name());
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // TODO(eseckler): Set other fields on |chrome_process|.

  // Start a new empty packet to enable scraping of the old one from the SMB in
  // case the process crashes.
  trace_packet = TracePacketHandle();
  writer->NewTracePacket();

  // Flush the current chunk right after writing the packet when in discard
  // buffering mode. Otherwise there's a risk that the chunk will miss the
  // buffer and process metadata is lost. We don't do this in ring buffer mode
  // and instead flush before writing the latest packet then, to make it more
  // likely that the latest packet's chunk remains in the ring buffer.
  if (record_mode != base::trace_event::RECORD_CONTINUOUSLY)
    writer->Flush();
}

bool TraceEventDataSource::IsPrivacyFilteringEnabled() {
  AutoLockWithDeferredTaskPosting lock(lock_);
  return privacy_filtering_enabled_;
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::TraceEventMetadataSource::DataSourceProxy);
