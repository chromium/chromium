// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/android_system_producer.h"

#include <utility>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_log.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/startup_trace_writer_registry.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/ipc/producer_ipc_client.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_writer.h"
#include "third_party/perfetto/protos/perfetto/common/track_event_descriptor.pbzero.h"

namespace tracing {
namespace {
constexpr uint32_t kInitialConnectionBackoffMs = 100;
constexpr uint32_t kMaxConnectionBackoffMs = 30 * 1000;

perfetto::DataSourceConfig EnsureGuardRailsAreFollowed(
    const perfetto::DataSourceConfig& data_source_config) {
  if (!data_source_config.enable_extra_guardrails() ||
      data_source_config.chrome_config().privacy_filtering_enabled()) {
    return data_source_config;
  }
  // If extra_guardrails is enabled then we have to ensure we have privacy
  // filtering enabled.
  perfetto::DataSourceConfig config = data_source_config;
  config.mutable_chrome_config()->set_privacy_filtering_enabled(true);
  return config;
}

uint32_t IncreaseBackoff(uint32_t current, uint32_t max) {
  return std::min(current * 2, max);
}
}  // namespace

AndroidSystemProducer::AndroidSystemProducer(const char* socket,
                                             PerfettoTaskRunner* task_runner)
    : SystemProducer(task_runner),
      socket_name_(socket),
      connection_backoff_ms_(kInitialConnectionBackoffMs) {
  Connect();
}

AndroidSystemProducer::~AndroidSystemProducer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AndroidSystemProducer::SetDisallowPreAndroidPieForTesting(bool disallow) {
  disallow_pre_android_pie = disallow;
  if (!disallow && state_ == State::kUninitialized) {
    // If previously we would not have connected, we now attempt to connect
    // since we are now skipping a check.
    Connect();
  }
}

void AndroidSystemProducer::SetNewSocketForTesting(const char* socket) {
  socket_name_ = socket;
  if (state_ == State::kConnected) {
    // If we are fully connected we need to reset the service before we
    // reconnect.
    DisconnectWithReply(base::BindOnce(&AndroidSystemProducer::OnDisconnect,
                                       base::Unretained(this)));
  } else {
    // In any other case we just need to do a normal disconnect and
    // DisconnectWithReply will ensure we set up the retries on the new
    // |socket|.
    DisconnectWithReply(base::OnceClosure());
  }
}

void AndroidSystemProducer::ResetSequenceForTesting() {
  // DETACH the sequence and then immediately attach it. This is needed in tests
  // because we might be executing in a TaskEnvironment, but the global
  // PerfettoTracedProcess (which contains a pointer to AndroidSystemProducer)
  // will leak between tests, but the sequence will no longer be valid.
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool AndroidSystemProducer::IsTracingActive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_sources_tracing_ > 0;
}

void AndroidSystemProducer::NewDataSourceAdded(
    const PerfettoTracedProcess::DataSourceBase* const data_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kConnected) {
    return;
  }
  perfetto::DataSourceDescriptor new_registration;
  new_registration.set_name(data_source->name());
  new_registration.set_will_notify_on_start(true);
  new_registration.set_will_notify_on_stop(true);
  new_registration.set_handles_incremental_state_clear(true);

  // Add categories to the DataSourceDescriptor.
  protozero::ScatteredHeapBuffer buffer;
  protozero::ScatteredStreamWriter stream(&buffer);
  perfetto::protos::pbzero::TrackEventDescriptor proto;
  proto.Reset(&stream);
  buffer.set_writer(&stream);

  std::set<std::string> category_set;
  tracing::TracedProcessImpl::GetInstance()->GetCategories(&category_set);
  for (const std::string& s : category_set) {
    proto.add_available_categories(s.c_str());
  }

  auto raw_proto = buffer.StitchSlices();
  std::string track_event_descriptor_raw(raw_proto.begin(), raw_proto.end());
  new_registration.set_track_event_descriptor_raw(track_event_descriptor_raw);

  service_->RegisterDataSource(new_registration);
}

void AndroidSystemProducer::DisconnectWithReply(
    base::OnceClosure on_disconnect_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kConnected) {
    // We are connected and need to unregister the DataSources to
    // inform the service these data sources are going away. If we
    // are currently tracing the service will ask for them to shut
    // down asynchronously.
    //
    // Note that the system service may have concurrently posted a
    // task to request one of these data sources to start. However we
    // will ignore such requests by verifying that we're allowed to
    // trace in StartDataSource().
    for (const auto* const data_source :
         PerfettoTracedProcess::Get()->data_sources()) {
      DCHECK(service_.get());
      service_->UnregisterDataSource(data_source->name());
    }
  }
  // If we are tracing we need to wait until we're fully disconnected
  // to run the callback, otherwise we run it immediately (we will
  // still unregister the data sources but that can happen async in
  // the background).
  if (!on_disconnect_complete.is_null()) {
    if (IsTracingActive() || !on_disconnect_callbacks_.empty()) {
      on_disconnect_callbacks_.push_back(std::move(on_disconnect_complete));
    } else {
      std::move(on_disconnect_complete).Run();
    }
  }
  DelayedReconnect();
}

void AndroidSystemProducer::OnConnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!PerfettoTracedProcess::Get()->CanStartTracing(this,
                                                     base::OnceClosure())) {
    DisconnectWithReply();
    return;
  }
  state_ = State::kConnected;
  connection_backoff_ms_ = kInitialConnectionBackoffMs;
  for (const auto* const data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    NewDataSourceAdded(data_source);
  }
}

void AndroidSystemProducer::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_.get());
  // Currently our data sources don't support the concept of the service
  // disappearing and thus can't shut down cleanly (they would attempt to flush
  // data across the broken socket). Add a CHECK to catch this if its a problem.
  //
  // TODO(nuskos): Fix this, make it so we cleanly shut down on IPC errors.
  CHECK(!IsTracingActive());
  // This PostTask is needed because we want to clean up the state AFTER the
  // |ProducerEndpoint| has finished cleaning up.
  task_runner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<AndroidSystemProducer> weak_ptr) {
                       if (!weak_ptr) {
                         return;
                       }
                       weak_ptr->service_.reset();
                       weak_ptr->shared_memory_arbiter_.reset();
                       weak_ptr->shared_memory_ = nullptr;
                       weak_ptr->DelayedReconnect();
                     },
                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidSystemProducer::OnTracingSetup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(oysteine): plumb this through the service.
  const size_t kShmemBufferPageSize = 4096;
  DCHECK(!shared_memory_);
  DCHECK(!shared_memory_arbiter_);
  shared_memory_ = service_->shared_memory();
  DCHECK(shared_memory_);
  shared_memory_arbiter_ = perfetto::SharedMemoryArbiter::CreateInstance(
      shared_memory_, kShmemBufferPageSize, this,
      PerfettoTracedProcess::GetTaskRunner());
}

void AndroidSystemProducer::SetupDataSource(perfetto::DataSourceInstanceID,
                                            const perfetto::DataSourceConfig&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Always called before StartDataSource but not used for any setup currently.
}

void AndroidSystemProducer::StartDataSource(
    perfetto::DataSourceInstanceID id,
    const perfetto::DataSourceConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* const data_source : PerfettoTracedProcess::Get()->data_sources()) {
    if (data_source->name() == config.name()) {
      auto can_trace = PerfettoTracedProcess::Get()->CanStartTracing(
          this,
          base::BindOnce(
              [](base::WeakPtr<AndroidSystemProducer> weak_ptr,
                 PerfettoTracedProcess::DataSourceBase* data_source,
                 perfetto::DataSourceInstanceID id,
                 const perfetto::DataSourceConfig& data_source_config) {
                if (!weak_ptr) {
                  return;
                }
                DCHECK_CALLED_ON_VALID_SEQUENCE(weak_ptr->sequence_checker_);
                ++weak_ptr->data_sources_tracing_;
                data_source->StartTracingWithID(
                    id, weak_ptr.get(),
                    EnsureGuardRailsAreFollowed(data_source_config));
                weak_ptr->service_->NotifyDataSourceStarted(id);
              },
              weak_ptr_factory_.GetWeakPtr(), data_source, id, config));
      if (!can_trace) {
        DisconnectWithReply(base::OnceClosure());
      }
      return;
    }
  }
}

void AndroidSystemProducer::StopDataSource(perfetto::DataSourceInstanceID id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto* const data_source : PerfettoTracedProcess::Get()->data_sources()) {
    if (data_source->data_source_id() == id &&
        data_source->producer() == this) {
      data_source->StopTracing(base::BindOnce(
          [](base::WeakPtr<AndroidSystemProducer> weak_ptr,
             perfetto::DataSourceInstanceID id) {
            if (!weak_ptr) {
              return;
            }
            DCHECK_CALLED_ON_VALID_SEQUENCE(weak_ptr->sequence_checker_);
            weak_ptr->service_->NotifyDataSourceStopped(id);
            --weak_ptr->data_sources_tracing_;
            if (!weak_ptr->IsTracingActive()) {
              // If this is the last data source to be shut down then
              // perhaps we need to invoke any callbacks that were stored
              // (there might be none).
              weak_ptr->InvokeStoredOnDisconnectCallbacks();
            }
          },
          weak_ptr_factory_.GetWeakPtr(), id));
      return;
    }
  }
}

void AndroidSystemProducer::Flush(
    perfetto::FlushRequestID id,
    const perfetto::DataSourceInstanceID* data_source_ids,
    size_t num_data_sources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_replies_for_latest_flush_ = {id, num_data_sources};
  for (auto* const data_source : PerfettoTracedProcess::Get()->data_sources()) {
    if (std::find(data_source_ids, data_source_ids + num_data_sources,
                  data_source->data_source_id()) !=
        data_source_ids + num_data_sources) {
      data_source->Flush(base::BindRepeating(
          [](base::WeakPtr<AndroidSystemProducer> weak_ptr,
             perfetto::FlushRequestID flush_id) {
            if (weak_ptr) {
              weak_ptr->NotifyFlushComplete(flush_id);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), id));
    }
  }
}

void AndroidSystemProducer::ClearIncrementalState(
    const perfetto::DataSourceInstanceID* data_source_ids,
    size_t num_data_sources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_source_ids);
  DCHECK_GT(num_data_sources, 0u);
  std::unordered_set<perfetto::DataSourceInstanceID> to_clear{
      data_source_ids, data_source_ids + num_data_sources};
  for (auto* data_source : PerfettoTracedProcess::Get()->data_sources()) {
    if (to_clear.find(data_source->data_source_id()) != to_clear.end()) {
      data_source->ClearIncrementalState();
    }
  }
}

void AndroidSystemProducer::CommitData(
    const perfetto::CommitDataRequest& commit,
    CommitDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);
  service_->CommitData(commit, std::move(callback));
}

perfetto::SharedMemoryArbiter* AndroidSystemProducer::GetSharedMemoryArbiter() {
  return shared_memory_arbiter_.get();
}

perfetto::SharedMemory* AndroidSystemProducer::shared_memory() const {
  return shared_memory_;
}

void AndroidSystemProducer::NotifyFlushComplete(perfetto::FlushRequestID id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_replies_for_latest_flush_.first != id) {
    // Ignore; completed flush was for an earlier request.
    return;
  }

  DCHECK_NE(pending_replies_for_latest_flush_.second, 0u);
  if (--pending_replies_for_latest_flush_.second == 0) {
    shared_memory_arbiter_->NotifyFlushComplete(id);
  }
}

void AndroidSystemProducer::RegisterTraceWriter(uint32_t writer_id,
                                                uint32_t target_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);
  service_->RegisterTraceWriter(writer_id, target_buffer);
}

void AndroidSystemProducer::UnregisterTraceWriter(uint32_t writer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);
  service_->UnregisterTraceWriter(writer_id);
}

void AndroidSystemProducer::RegisterDataSource(
    const perfetto::DataSourceDescriptor&) {
  // Never called by SharedMemoryArbiter/TraceWriter.
  NOTREACHED();
}

void AndroidSystemProducer::UnregisterDataSource(const std::string& name) {
  // Never called by SharedMemoryArbiter/TraceWriter.
  NOTREACHED();
}

void AndroidSystemProducer::NotifyDataSourceStopped(
    perfetto::DataSourceInstanceID id) {
  // Never called by SharedMemoryArbiter/TraceWriter.
  NOTREACHED();
}

void AndroidSystemProducer::NotifyDataSourceStarted(
    perfetto::DataSourceInstanceID id) {
  // Never called by SharedMemoryArbiter/TraceWriter.
  NOTREACHED();
}

size_t AndroidSystemProducer::shared_buffer_page_size_kb() const {
  // Never called by SharedMemoryArbiter/TraceWriter.
  NOTREACHED();
  return 0;
}

perfetto::SharedMemoryArbiter*
AndroidSystemProducer::GetInProcessShmemArbiter() {
  // Never called by SharedMemoryArbiter/TraceWriter.
  NOTREACHED();
  return GetSharedMemoryArbiter();
}

void AndroidSystemProducer::ActivateTriggers(
    const std::vector<std::string>& triggers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kConnected) {
    service_->ActivateTriggers(triggers);
  }
}

void AndroidSystemProducer::ConnectSocket() {
  state_ = State::kConnecting;
  service_ = perfetto::ProducerIPCClient::Connect(
      socket_name_, this,
      base::StrCat(
          {mojom::kPerfettoProducerNamePrefix,
           base::NumberToString(
               base::trace_event::TraceLog::GetInstance()->process_id())}),
      task_runner(),
      perfetto::TracingService::ProducerSMBScrapingMode::kEnabled);
}

bool AndroidSystemProducer::SkipIfPreAndroidPie() const {
  return disallow_pre_android_pie &&
         base::android::BuildInfo::GetInstance()->sdk_int() <
             base::android::SDK_VERSION_P;
}

void AndroidSystemProducer::InvokeStoredOnDisconnectCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& callback : on_disconnect_callbacks_) {
    DCHECK(!callback.is_null());
    std::move(callback).Run();
  }
  on_disconnect_callbacks_.clear();
}

void AndroidSystemProducer::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (SkipIfPreAndroidPie()) {
    return;
  }
  switch (state_) {
    case State::kUninitialized:
      ConnectSocket();
      break;
    case State::kConnecting:
    case State::kConnected:
      // We are already connected (in which case do nothing). Or we're currently
      // connecting the socket and waiting for the OnConnect call from the
      // service.
      return;
    case State::kDisconnected:
      if (service_) {
        // We unregistered all our data sources due to a concurrent tracing
        // session but still have an open connection so just reregister
        // everything.
        OnConnect();
      } else {
        // We were disconnected by a connection error, so we need to recreate
        // our service pipe.
        ConnectSocket();
      }
      break;
  }
}

void AndroidSystemProducer::DelayedReconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (SkipIfPreAndroidPie()) {
    return;
  }
  if (state_ == State::kDisconnected) {
    return;
  }
  state_ = State::kDisconnected;

  task_runner()->GetOrCreateTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AndroidSystemProducer> weak_ptr) {
            if (!weak_ptr) {
              return;
            }
            if (PerfettoTracedProcess::Get()->CanStartTracing(
                    weak_ptr.get(), base::OnceClosure())) {
              weak_ptr->Connect();
            } else {
              weak_ptr->DelayedReconnect();
            }
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(connection_backoff_ms_));

  connection_backoff_ms_ =
      IncreaseBackoff(connection_backoff_ms_, kMaxConnectionBackoffMs);
}

}  // namespace tracing
