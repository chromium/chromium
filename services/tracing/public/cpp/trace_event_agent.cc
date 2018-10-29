// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_event_agent.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"

#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_WIN)
#define PERFETTO_AVAILABLE
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#endif

namespace {

const char kTraceEventLabel[] = "traceEvents";

tracing::TraceEventAgent* g_trace_event_agent;

}  // namespace

namespace tracing {

#if defined(PERFETTO_AVAILABLE)

namespace {

ProducerClient* GetProducerClient() {
  static base::NoDestructor<ProducerClient> producer_client;
  return producer_client.get();
}

}  // namespace

class PerfettoTraceEventAgent : public TraceEventAgent {
 public:
  PerfettoTraceEventAgent(service_manager::Connector* connector,
                          bool request_clock_sync_marker_on_android)
      : TraceEventAgent(connector, request_clock_sync_marker_on_android) {
    mojom::PerfettoServicePtr perfetto_service;
    connector->BindInterface(mojom::kServiceName, &perfetto_service);

    GetProducerClient()->CreateMojoMessagepipes(base::BindOnce(
        [](mojom::PerfettoServicePtr perfetto_service,
           mojom::ProducerClientPtr producer_client_pipe,
           mojom::ProducerHostRequest producer_host_pipe) {
          perfetto_service->ConnectToProducerHost(
              std::move(producer_client_pipe), std::move(producer_host_pipe));
        },
        std::move(perfetto_service)));

    GetProducerClient()->AddDataSource(TraceEventDataSource::GetInstance());
  }

  void AddMetadataGeneratorFunction(
      MetadataGeneratorFunction generator) override {
    // Instantiate and register the metadata data source on the first
    // call.
    static TraceEventMetadataSource* metadata_source = []() {
      static base::NoDestructor<TraceEventMetadataSource> instance;
      GetProducerClient()->AddDataSource(instance.get());
      return instance.get();
    }();

    metadata_source->AddGeneratorFunction(generator);
  }
};
#endif

// static
std::unique_ptr<TraceEventAgent> TraceEventAgent::Create(
    service_manager::Connector* connector,
    bool request_clock_sync_marker_on_android) {
  std::unique_ptr<TraceEventAgent> new_agent;

  if (TracingUsesPerfettoBackend()) {
#if defined(PERFETTO_AVAILABLE)
    new_agent = std::make_unique<PerfettoTraceEventAgent>(
        connector, request_clock_sync_marker_on_android);
#else
    LOG(ERROR) << "Perfetto is not yet available for this platform; falling "
                  "back to using legacy TraceLog";
#endif
  }

  // Use legacy tracing if we're on an unsupported platform or the feature flag
  // is disabled.
  if (!new_agent) {
    new_agent = std::make_unique<LegacyTraceEventAgent>(
        connector, request_clock_sync_marker_on_android);
  }

  return new_agent;
}

TraceEventAgent::TraceEventAgent(service_manager::Connector* connector,
                                 bool request_clock_sync_marker_on_android)
    : BaseAgent(connector,
                kTraceEventLabel,
                mojom::TraceDataType::ARRAY,
#if defined(OS_ANDROID)
                request_clock_sync_marker_on_android,
#else
                false,
#endif
                base::trace_event::TraceLog::GetInstance()->process_id()) {
  DCHECK(!g_trace_event_agent);
  g_trace_event_agent = this;
}

TraceEventAgent::~TraceEventAgent() {
  g_trace_event_agent = nullptr;
}

void TraceEventAgent::RequestClockSyncMarker(
    const std::string& sync_id,
    Agent::RequestClockSyncMarkerCallback callback) {
#if defined(OS_ANDROID)
  base::trace_event::TraceLog::GetInstance()->AddClockSyncMetadataEvent();
  std::move(callback).Run(base::TimeTicks(), base::TimeTicks());
#else
  NOTREACHED();
#endif
}

void TraceEventAgent::GetCategories(GetCategoriesCallback callback) {
  std::vector<std::string> category_vector;
  base::trace_event::TraceLog::GetInstance()->GetKnownCategoryGroups(
      &category_vector);
  std::move(callback).Run(base::JoinString(category_vector, ","));
}

LegacyTraceEventAgent::LegacyTraceEventAgent(
    service_manager::Connector* connector,
    bool request_clock_sync_marker_on_android)
    : TraceEventAgent(connector, request_clock_sync_marker_on_android),
      enabled_tracing_modes_(0) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

LegacyTraceEventAgent::~LegacyTraceEventAgent() {
  DCHECK(!trace_log_needs_me_);
}

void LegacyTraceEventAgent::AddMetadataGeneratorFunction(
    MetadataGeneratorFunction generator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metadata_generator_functions_.push_back(generator);
}

void LegacyTraceEventAgent::StartTracing(const std::string& config,
                                         base::TimeTicks coordinator_time,
                                         StartTracingCallback callback) {
  DCHECK(!recorder_);
#if defined(__native_client__)
  // NaCl and system times are offset by a bit, so subtract some time from
  // the captured timestamps. The value might be off by a bit due to messaging
  // latency.
  base::TimeDelta time_offset = TRACE_TIME_TICKS_NOW() - coordinator_time;
  TraceLog::GetInstance()->SetTimeOffset(time_offset);
#endif
  enabled_tracing_modes_ = base::trace_event::TraceLog::RECORDING_MODE;
  const base::trace_event::TraceConfig trace_config(config);
  if (!trace_config.event_filters().empty())
    enabled_tracing_modes_ |= base::trace_event::TraceLog::FILTERING_MODE;
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      trace_config, enabled_tracing_modes_);
  std::move(callback).Run(true);
}

void LegacyTraceEventAgent::StopAndFlush(mojom::RecorderPtr recorder) {
  DCHECK(!recorder_);
  recorder_ = std::move(recorder);
  base::trace_event::TraceLog::GetInstance()->SetDisabled(
      enabled_tracing_modes_);
  enabled_tracing_modes_ = 0;
  for (const auto& generator : metadata_generator_functions_) {
    auto metadata = generator.Run();
    if (metadata)
      recorder_->AddMetadata(std::move(*metadata));
  }
  trace_log_needs_me_ = true;
  base::trace_event::TraceLog::GetInstance()->Flush(base::Bind(
      &LegacyTraceEventAgent::OnTraceLogFlush, base::Unretained(this)));
}

void LegacyTraceEventAgent::RequestBufferStatus(
    RequestBufferStatusCallback callback) {
  base::trace_event::TraceLogStatus status =
      base::trace_event::TraceLog::GetInstance()->GetStatus();
  std::move(callback).Run(status.event_capacity, status.event_count);
}

void LegacyTraceEventAgent::OnTraceLogFlush(
    const scoped_refptr<base::RefCountedString>& events_str,
    bool has_more_events) {
  if (!events_str->data().empty())
    recorder_->AddChunk(events_str->data());
  if (!has_more_events) {
    trace_log_needs_me_ = false;
    recorder_.reset();
  }
}

}  // namespace tracing
