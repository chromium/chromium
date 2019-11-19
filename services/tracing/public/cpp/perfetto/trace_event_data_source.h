// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_DATA_SOURCE_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_DATA_SOURCE_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_base.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"

namespace base {
namespace trace_event {
class ThreadInstructionCount;
class TraceEvent;
struct TraceEventHandle;
}  // namespace trace_event
}  // namespace base

namespace perfetto {
class StartupTraceWriter;
class StartupTraceWriterRegistry;
class TraceWriter;
}

namespace tracing {

class ThreadLocalEventSink;

class AutoThreadLocalBoolean {
 public:
  explicit AutoThreadLocalBoolean(
      base::ThreadLocalBoolean* thread_local_boolean)
      : thread_local_boolean_(thread_local_boolean) {
    DCHECK(!thread_local_boolean_->Get());
    thread_local_boolean_->Set(true);
  }
  ~AutoThreadLocalBoolean() { thread_local_boolean_->Set(false); }

 private:
  base::ThreadLocalBoolean* thread_local_boolean_;
  DISALLOW_COPY_AND_ASSIGN(AutoThreadLocalBoolean);
};

// This class is a data source that clients can use to provide
// global metadata in dictionary form, by registering callbacks.
class COMPONENT_EXPORT(TRACING_CPP) TraceEventMetadataSource
    : public PerfettoTracedProcess::DataSourceBase {
 public:
  static TraceEventMetadataSource* GetInstance();

  using JsonMetadataGeneratorFunction =
      base::RepeatingCallback<std::unique_ptr<base::DictionaryValue>()>;

  using MetadataGeneratorFunction = base::RepeatingCallback<void(
      perfetto::protos::pbzero::ChromeMetadataPacket*,
      bool /* privacy_filtering_enabled */)>;

  // Any callbacks passed here will be called when tracing. Note that if tracing
  // is enabled while calling this method, the callback may be invoked
  // directly.
  void AddGeneratorFunction(JsonMetadataGeneratorFunction generator);
  // Same as above, but for filling in proto format.
  void AddGeneratorFunction(MetadataGeneratorFunction generator);
  // For background tracing, the legacy crash uploader needs
  // metadata fields to be uploaded as POST args in addition to being
  // embedded in the trace. TODO(oysteine): Remove when only the
  // UMA uploader path is used.
  std::unique_ptr<base::DictionaryValue> GenerateLegacyMetadataDict();

  // PerfettoTracedProcess::DataSourceBase implementation, called by
  // ProducerClent.
  void StartTracing(
      PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override;
  void StopTracing(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;

  void ResetForTesting();

 private:
  friend class base::NoDestructor<TraceEventMetadataSource>;

  TraceEventMetadataSource();
  ~TraceEventMetadataSource() override;

  void GenerateMetadata(
      std::unique_ptr<std::vector<JsonMetadataGeneratorFunction>>
          json_generators,
      std::unique_ptr<std::vector<MetadataGeneratorFunction>> proto_generators);
  void GenerateMetadataFromGenerator(
      const MetadataGeneratorFunction& generator);
  void GenerateJsonMetadataFromGenerator(
      const JsonMetadataGeneratorFunction& generator,
      perfetto::protos::pbzero::ChromeEventBundle* event_bundle);
  std::unique_ptr<base::DictionaryValue> GenerateTraceConfigMetadataDict();

  // All members are protected by |lock_|.
  base::Lock lock_;
  std::vector<JsonMetadataGeneratorFunction> json_generator_functions_;
  std::vector<MetadataGeneratorFunction> generator_functions_;

  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;

  std::unique_ptr<perfetto::TraceWriter> trace_writer_;
  bool privacy_filtering_enabled_ = false;
  std::string chrome_config_;
  std::unique_ptr<base::trace_event::TraceConfig> parsed_chrome_config_;
  bool emit_metadata_at_start_ = false;

  DISALLOW_COPY_AND_ASSIGN(TraceEventMetadataSource);
};

// This class acts as a bridge between the TraceLog and
// the PerfettoProducer. It converts incoming
// trace events to ChromeTraceEvent protos and writes
// them into the Perfetto shared memory.
class COMPONENT_EXPORT(TRACING_CPP) TraceEventDataSource
    : public PerfettoTracedProcess::DataSourceBase {
 public:
  static TraceEventDataSource* GetInstance();

  // Destroys and recreates the global instance for testing.
  static void ResetForTesting();

  // Flushes and deletes the TraceWriter for the current thread, if any.
  static void FlushCurrentThread();

  static base::ThreadLocalBoolean* GetThreadIsInTraceEventTLS();

  // Installs TraceLog overrides for tracing during Chrome startup. Trace data
  // is locally buffered until connection to the perfetto service is
  // established. Expects a later call to StartTracing() to bind to the perfetto
  // service. Should only be called once.
  void SetupStartupTracing(bool privacy_filtering_enabled);

  void OnTaskSchedulerAvailable();

  // The PerfettoProducer is responsible for calling StopTracing
  // which will clear the stored pointer to it, before it
  // gets destroyed. PerfettoProducer::CreateTraceWriter can be
  // called by the TraceEventDataSource on any thread.
  void StartTracing(
      PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override;

  // Called from the PerfettoProducer.
  void StopTracing(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;
  void ClearIncrementalState() override;

  // Deletes TraceWriter safely on behalf of a ThreadLocalEventSink.
  void ReturnTraceWriter(
      std::unique_ptr<perfetto::StartupTraceWriter> trace_writer);

  void set_startup_tracing_timeout_for_testing(base::TimeDelta timeout_us) {
    startup_tracing_timeout_ = timeout_us;
  }

  bool privacy_filtering_enabled() const { return privacy_filtering_enabled_; }

  bool IsEnabled();

 private:
  friend class base::NoDestructor<TraceEventDataSource>;

  TraceEventDataSource();
  ~TraceEventDataSource() override;

  void StartupTracingTimeoutFired();
  void OnFlushFinished(const scoped_refptr<base::RefCountedString>&,
                       bool has_more_events);

  void StartTracingInternal(
      PerfettoProducer* producer_client,
      const perfetto::DataSourceConfig& data_source_config);

  void RegisterWithTraceLog();
  void UnregisterFromTraceLog();

  std::unique_ptr<perfetto::StartupTraceWriter> CreateTraceWriterLocked();
  ThreadLocalEventSink* CreateThreadLocalEventSink(bool thread_will_flush);

  // Callback from TraceLog, can be called from any thread.
  static void OnAddTraceEvent(base::trace_event::TraceEvent* trace_event,
                              bool thread_will_flush,
                              base::trace_event::TraceEventHandle* handle);
  static void OnUpdateDuration(
      base::trace_event::TraceEventHandle handle,
      const base::TimeTicks& now,
      const base::ThreadTicks& thread_now,
      base::trace_event::ThreadInstructionCount thread_instruction_now);

  // Extracts UMA histogram names that should be logged in traces and logs their
  // starting values.
  void ResetHistograms(const base::trace_event::TraceConfig& trace_config);
  // Logs selected UMA histogram.
  void LogHistograms();
  // Logs a given histogram in traces.
  void LogHistogram(base::HistogramBase* histogram);
  void EmitProcessDescriptor();

  bool disable_interning_ = false;
  base::OnceClosure stop_complete_callback_;

  // Incremented and accessed atomically but without memory order guarantees.
  // This ID is incremented whenever a new tracing session is started.
  static constexpr uint32_t kInvalidSessionID = 0;
  static constexpr uint32_t kFirstSessionID = 1;
  base::TimeDelta startup_tracing_timeout_ = base::TimeDelta::FromSeconds(60);
  std::atomic<uint32_t> session_id_{kInvalidSessionID};

  // To avoid lock-order inversion, this lock should not be held while making
  // calls to mojo interfaces or posting tasks, or calling any other code path
  // that may acquire another lock that may also be held while emitting a trace
  // event (crbug.com/986248).
  base::Lock lock_;  // Protects subsequent members.
  uint32_t target_buffer_ = 0;
  // We own the registry during startup, but transfer its ownership to the
  // PerfettoProducer once the perfetto service is available. Only set if
  // SetupStartupTracing() is called.
  std::unique_ptr<perfetto::StartupTraceWriterRegistry>
      startup_writer_registry_;
  std::unique_ptr<perfetto::StartupTraceWriter> trace_writer_;
  base::OneShotTimer startup_tracing_timer_;
  bool is_enabled_ = false;
  bool flushing_trace_log_ = false;
  base::OnceClosure flush_complete_task_;
  std::vector<std::string> histograms_;
  bool privacy_filtering_enabled_ = false;
  std::string process_name_;
  int process_id_ = base::kNullProcessId;
  SEQUENCE_CHECKER(perfetto_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(TraceEventDataSource);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_DATA_SOURCE_H_
