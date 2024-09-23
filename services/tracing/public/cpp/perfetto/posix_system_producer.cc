// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/public/cpp/perfetto/posix_system_producer.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/ipc/producer_ipc_client.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_writer.h"
#include "third_party/perfetto/protos/perfetto/common/track_event_descriptor.pbzero.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/system_tracing_service.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/files/scoped_file.h"
#endif

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

PosixSystemProducer::PosixSystemProducer(
    const char* socket,
    base::tracing::PerfettoTaskRunner* task_runner)
    : SystemProducer(task_runner),
      socket_name_(socket),
      connection_backoff_ms_(kInitialConnectionBackoffMs) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PosixSystemProducer::~PosixSystemProducer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PosixSystemProducer::SetDisallowPreAndroidPieForTesting(bool disallow) {
  bool was_disallowed = SkipIfOnAndroidAndPreAndroidPie();
  disallow_pre_android_pie_ = disallow;
  if (!disallow && was_disallowed && state_ == State::kDisconnected) {
    // If previously we would not have connected, we now attempt to connect
    // since we are now skipping a check.
    Connect();
  }
}

void PosixSystemProducer::SetNewSocketForTesting(const char* socket) {
  socket_name_ = socket;

  if (state_ == State::kDisconnected) {
    // Not connected yet, wait for ConnectToSystemService().
    return;
  }

  if (state_ == State::kConnected) {
    // If we are fully connected we need to reset the service before we
    // reconnect.
    DisconnectWithReply(base::BindOnce(&PosixSystemProducer::OnDisconnect,
                                       base::Unretained(this)));
    return;
  }

  // In any other case, we just need to do a normal disconnect and
  // DisconnectWithReply will ensure we set up the retries on the new |socket|.
  DisconnectWithReply(base::OnceClosure());
}

perfetto::SharedMemoryArbiter* PosixSystemProducer::MaybeSharedMemoryArbiter() {
  base::AutoLock lock(lock_);
  DCHECK(GetService());
  return GetService()->MaybeSharedMemoryArbiter();
}

void PosixSystemProducer::NewDataSourceAdded(
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
  std::set<std::string> category_set;
  tracing::TracedProcessImpl::GetInstance()->GetCategories(&category_set);
  protozero::HeapBuffered<perfetto::protos::pbzero::TrackEventDescriptor> proto;
  for (const std::string& s : category_set) {
    auto* cat = proto->add_available_categories();
    cat->set_name(s);
    if (s.find(TRACE_DISABLED_BY_DEFAULT("")) == 0) {
      cat->add_tags("slow");
    }
  }
  new_registration.set_track_event_descriptor_raw(proto.SerializeAsString());

  GetService()->RegisterDataSource(new_registration);
}

bool PosixSystemProducer::IsTracingActive() {
  base::AutoLock lock(lock_);
  return data_sources_tracing_ > 0;
}

void PosixSystemProducer::ConnectToSystemService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsTracingInitialized());
  DCHECK(state_ == State::kDisconnected);

  // Some Telemetry tests use sideloaded Perfetto library on pre-Pie devices.
  // We allow those tests to use system tracing by setting the
  // EnablePerfettoSystemTracing feature.
  disallow_pre_android_pie_ =
      !base::FeatureList::IsEnabled(features::kEnablePerfettoSystemTracing);

  Connect();
}

void PosixSystemProducer::ActivateTriggers(
    const std::vector<std::string>& triggers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kConnected) {
    GetService()->ActivateTriggers(triggers);
  }
}

void PosixSystemProducer::DisconnectWithReply(
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
    for (const PerfettoTracedProcess::DataSourceBase* const data_source :
         PerfettoTracedProcess::Get()->data_sources()) {
      DCHECK(GetService());
      GetService()->UnregisterDataSource(data_source->name());
    }
    state_ = State::kUnregistered;
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

void PosixSystemProducer::OnConnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!PerfettoTracedProcess::Get()->CanStartTracing(this,
                                                     base::OnceClosure())) {
    // We are succesfully connected, but we can't register the data sources
    // right now, so move into "kUnregistered".
    state_ = State::kUnregistered;
    DisconnectWithReply();
    return;
  }
  state_ = State::kConnected;
  connection_backoff_ms_ = kInitialConnectionBackoffMs;
  for (const PerfettoTracedProcess::DataSourceBase* const data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    NewDataSourceAdded(data_source);
  }
}

void PosixSystemProducer::FinishDisconnectingAndThenDelayedReconnect(
    State previous_state) {
  // This PostTask is needed because we want to clean up the state
  // AFTER the |ProducerEndpoint| has finished cleaning up.
  task_runner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PosixSystemProducer> weak_ptr,
             State previous_state) {
            if (!weak_ptr) {
              return;
            }
            if (previous_state == State::kConnecting) {
              base::AutoLock lock(weak_ptr->lock_);
              // We never connected, which means this disconnect
              // is an error from connecting, which means we don't
              // need to keep this endpoint (and associated memory
              // around forever) this prevents the memory leak
              // from getting excessive.
              weak_ptr->services_.erase(weak_ptr->services_.end() - 1);
            }
            weak_ptr->state_ = State::kDisconnected;
            weak_ptr->DelayedReconnect();
          },
          weak_ptr_factory_.GetWeakPtr(), previous_state));
}

void PosixSystemProducer::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetService());
  // If we never connected then we don't need to keep this service around
  // because no one will have pointers to its non-existent SMB.
  // We shouldn't have reentrancy.
  if (state_ == State::kDisconnecting) {
    return;
  }
  // Inform the service that no further IPCs should be accepted.
  State old_state = state_;
  state_ = State::kDisconnecting;
  GetService()->Disconnect();

  // If we aren't tracing then we are done just finish cleaning up the service
  if (!IsTracingActive()) {
    FinishDisconnectingAndThenDelayedReconnect(old_state);
    return;
  }

  // If we are tracing then we need to get the system back into a "normal" state
  // of no tracing.
  for (PerfettoTracedProcess::DataSourceBase* const data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    StopDataSource(data_source->data_source_id());
  }
}

void PosixSystemProducer::OnTracingSetup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Called by the IPC layer when tracing is first started and after shared
  // memory is set up.
  DCHECK(MaybeSharedMemoryArbiter());
  if (MaybeSharedMemoryArbiter()->EnableDirectSMBPatching()) {
    MaybeSharedMemoryArbiter()->SetBatchCommitsDuration(
        kShmArbiterBatchCommitDurationMs);
  }
}

void PosixSystemProducer::SetupDataSource(perfetto::DataSourceInstanceID,
                                          const perfetto::DataSourceConfig&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Always called before StartDataSource but not used for any setup currently.
}

void PosixSystemProducer::StartDataSource(
    perfetto::DataSourceInstanceID id,
    const perfetto::DataSourceConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kConnected) {
    // Because StartDataSource is async, its possible a previous StartDataSource
    // is still in the PostTask queue, so we just ignore it here (We'll get
    // a new one if/when we re-register the DataSource once we've moved into
    // kConnected).
    return;
  }

  for (PerfettoTracedProcess::DataSourceBase* const data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    if (data_source->name() == config.name()) {
      auto can_trace = PerfettoTracedProcess::Get()->CanStartTracing(
          this,
          base::BindOnce(
              [](base::WeakPtr<PosixSystemProducer> weak_ptr,
                 PerfettoTracedProcess::DataSourceBase* data_source,
                 perfetto::DataSourceInstanceID id,
                 const perfetto::DataSourceConfig& data_source_config) {
                if (!weak_ptr) {
                  return;
                }
                DCHECK_CALLED_ON_VALID_SEQUENCE(weak_ptr->sequence_checker_);
                {
                  base::AutoLock lock(weak_ptr->lock_);
                  ++weak_ptr->data_sources_tracing_;
                }
                data_source->StartTracing(
                    id, weak_ptr.get(),
                    EnsureGuardRailsAreFollowed(data_source_config));
                weak_ptr->GetService()->NotifyDataSourceStarted(id);
              },
              weak_ptr_factory_.GetWeakPtr(), data_source, id, config));
      if (!can_trace) {
        DisconnectWithReply(base::OnceClosure());
      }
      return;
    }
  }
}

void PosixSystemProducer::StopDataSource(perfetto::DataSourceInstanceID id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (PerfettoTracedProcess::DataSourceBase* const data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    if (data_source->data_source_id() == id &&
        data_source->producer() == this) {
      data_source->StopTracing(base::BindOnce(
          [](base::WeakPtr<PosixSystemProducer> weak_ptr,
             perfetto::DataSourceInstanceID id) {
            if (!weak_ptr) {
              return;
            }
            DCHECK_CALLED_ON_VALID_SEQUENCE(weak_ptr->sequence_checker_);
            // Flush any commits that might have been batched by
            // SharedMemoryArbiter.
            weak_ptr->GetService()
                ->MaybeSharedMemoryArbiter()
                ->FlushPendingCommitDataRequests();
            weak_ptr->GetService()->NotifyDataSourceStopped(id);
            {
              base::AutoLock lock(weak_ptr->lock_);
              --weak_ptr->data_sources_tracing_;
            }
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

void PosixSystemProducer::Flush(
    perfetto::FlushRequestID id,
    const perfetto::DataSourceInstanceID* data_source_ids,
    size_t num_data_sources,
    perfetto::FlushFlags /*ignored*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_replies_for_latest_flush_ = {id, num_data_sources};
  for (PerfettoTracedProcess::DataSourceBase* const data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    if (std::find(data_source_ids, data_source_ids + num_data_sources,
                  data_source->data_source_id()) !=
        data_source_ids + num_data_sources) {
      data_source->Flush(base::BindRepeating(
          [](base::WeakPtr<PosixSystemProducer> weak_ptr,
             perfetto::FlushRequestID flush_id) {
            if (weak_ptr) {
              weak_ptr->NotifyDataSourceFlushComplete(flush_id);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), id));
    }
  }
}

void PosixSystemProducer::ClearIncrementalState(
    const perfetto::DataSourceInstanceID* data_source_ids,
    size_t num_data_sources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_source_ids);
  DCHECK_GT(num_data_sources, 0u);
  std::unordered_set<perfetto::DataSourceInstanceID> to_clear{
      data_source_ids, data_source_ids + num_data_sources};
  for (PerfettoTracedProcess::DataSourceBase* data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    if (to_clear.find(data_source->data_source_id()) != to_clear.end()) {
      data_source->ClearIncrementalState();
    }
  }
}

bool PosixSystemProducer::SetupSharedMemoryForStartupTracing() {
  // TODO(eseckler): Support startup tracing using an unbound SMA.
  NOTIMPLEMENTED();
  return false;
}

void PosixSystemProducer::ConnectSocket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kConnecting;
  const char* host_package_name = nullptr;
#if BUILDFLAG(IS_ANDROID)
  host_package_name =
      base::android::BuildInfo::GetInstance()->host_package_name();
#endif  // BUILDFLAG(IS_ANDROID)

  // On android we want to include if this is webview inside of an app or
  // Android Chrome. To aid this we add the host_package_name to differentiate
  // the various apps and sources.
  std::string producer_name;
  if (host_package_name) {
    producer_name = base::StrCat(
        {mojom::kPerfettoProducerNamePrefix, host_package_name, "-",
         base::NumberToString(
             base::trace_event::TraceLog::GetInstance()->process_id())});
  } else {
    producer_name = base::StrCat(
        {mojom::kPerfettoProducerNamePrefix,
         base::NumberToString(
             base::trace_event::TraceLog::GetInstance()->process_id())});
  }

  // If the security sandbox allows making socket connections, open the producer
  // socket directly. Otherwise, use Mojo to open the socket in the browser
  // process.
  if (!SandboxForbidsSocketConnection()) {
#if BUILDFLAG(IS_FUCHSIA)
    fuchsia_connector_ = std::make_unique<FuchsiaPerfettoProducerConnector>(
        task_runner()->GetOrCreateTaskRunner());
    auto maybe_conn_args = fuchsia_connector_->Connect();
    if (!maybe_conn_args) {
      state_ = State::kDisconnected;
      fuchsia_connector_.reset();
      return;
    }
    perfetto::ipc::Client::ConnArgs conn_args = std::move(*maybe_conn_args);
#else
    perfetto::ipc::Client::ConnArgs conn_args(socket_name_.c_str(), false);
#endif

    auto service = perfetto::ProducerIPCClient::Connect(
        std::move(conn_args), this, std::move(producer_name), task_runner(),
        perfetto::TracingService::ProducerSMBScrapingMode::kEnabled,
        GetPreferredSmbSizeBytes(), kSMBPageSizeBytes);

    base::AutoLock lock(lock_);
    services_.push_back(std::move(service));
    return;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  // If the child process hasn't received the Mojo remote, try again later.
  auto& remote = TracedProcessImpl::GetInstance()->system_tracing_service();
  if (!remote.is_bound()) {
    // We don't really open the socket using ProducerIPCClient in child
    // processes and need to reset |state_| to make DelayedReconnect() retry the
    // connection using mojo.
    DCHECK(state_ == State::kConnecting);
    state_ = State::kDisconnected;
    DelayedReconnect();
    return;
  }

  auto callback = base::BindOnce(
      [](std::string producer_name, base::WeakPtr<PosixSystemProducer> self,
         base::File file) {
        if (!self)
          return;

        if (!file.IsValid()) {
          // Reset |state_| to make DelayedReconnect() retry the connection.
          DCHECK(self->state_ == State::kConnecting);
          self->state_ = State::kDisconnected;
          self->DelayedReconnect();
          return;
        }

        // Connect using an already connected socket.
        auto service = perfetto::ProducerIPCClient::Connect(
            perfetto::ipc::Client::ConnArgs(
                perfetto::base::ScopedFile(file.TakePlatformFile())),
            self.get(), std::move(producer_name), self->task_runner(),
            perfetto::TracingService::ProducerSMBScrapingMode::kEnabled,
            self->GetPreferredSmbSizeBytes(), kSMBPageSizeBytes);

        base::AutoLock lock(self->lock_);
        self->services_.push_back(std::move(service));
      },
      std::move(producer_name), weak_ptr_factory_.GetWeakPtr());

  // Open the socket remotely using Mojo.
  remote->OpenProducerSocket(std::move(callback));
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
}

bool PosixSystemProducer::SkipIfOnAndroidAndPreAndroidPie() const {
#if BUILDFLAG(IS_ANDROID)
  return disallow_pre_android_pie_ &&
         base::android::BuildInfo::GetInstance()->sdk_int() <
             base::android::SDK_VERSION_P;
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

void PosixSystemProducer::InvokeStoredOnDisconnectCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& callback : on_disconnect_callbacks_) {
    DCHECK(!callback.is_null());
    std::move(callback).Run();
  }
  on_disconnect_callbacks_.clear();
  if (state_ == State::kDisconnecting) {
    // Since this is invoked after stopping all data sources this service was
    // active and we were connected.
    FinishDisconnectingAndThenDelayedReconnect(
        /* previous_state = */ State::kConnected);
  }
}

void PosixSystemProducer::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (SkipIfOnAndroidAndPreAndroidPie()) {
    return;
  }
  switch (state_) {
    case State::kDisconnected:
      ConnectSocket();
      break;
    case State::kConnecting:
    case State::kConnected:
      // We are already connected (in which case do nothing). Or we're
      // currently connecting the socket and waiting for the OnConnect call
      // from the service.
      return;
    case State::kUnregistered:
      DCHECK(GetService());
      // We unregistered all our data sources due to a concurrent tracing
      // session but still have an open connection so just reregister
      // everything.
      OnConnect();
      break;
    case State::kDisconnecting:
      // We aren't currently fully disconnected wait for the state to settle.
      break;
  }
}

bool PosixSystemProducer::SandboxForbidsSocketConnection() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  // All process types can connect directly to the system tracing service.
  return false;
#else
  // Connect to the system tracing service using Mojo from non-browser
  // processes. Note that the network utility process can make socket
  // connections, but we make it connect using Mojo for simplicity.
  auto type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("type");
  return !type.empty();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
}

void PosixSystemProducer::DelayedReconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (SkipIfOnAndroidAndPreAndroidPie()) {
    return;
  }
  if (retrying_) {
    return;
  }
  retrying_ = true;

  task_runner()->GetOrCreateTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PosixSystemProducer> weak_ptr) {
            if (!weak_ptr) {
              return;
            }
            weak_ptr->retrying_ = false;
            if (PerfettoTracedProcess::Get()->CanStartTracing(
                    weak_ptr.get(), base::OnceClosure())) {
              weak_ptr->Connect();
            } else {
              weak_ptr->DelayedReconnect();
            }
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(connection_backoff_ms_));

  connection_backoff_ms_ =
      IncreaseBackoff(connection_backoff_ms_, kMaxConnectionBackoffMs);
}

void PosixSystemProducer::NotifyDataSourceFlushComplete(
    perfetto::FlushRequestID id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_replies_for_latest_flush_.first != id) {
    // Ignore; completed flush was for an earlier request.
    return;
  }

  DCHECK_NE(pending_replies_for_latest_flush_.second, 0u);
  if (--pending_replies_for_latest_flush_.second == 0) {
    DCHECK(MaybeSharedMemoryArbiter());
    MaybeSharedMemoryArbiter()->NotifyFlushComplete(id);
  }
}

perfetto::TracingService::ProducerEndpoint* PosixSystemProducer::GetService() {
#if DCHECK_IS_ON()
  // Requires lock to be held when called on a non-producer sequence/thread.
  if (!sequence_checker_.CalledOnValidSequence()) {
    lock_.AssertAcquired();
  }
#endif  // DCHECK_IS_ON()

  switch (state_) {
    case State::kConnecting:
    case State::kConnected:
    case State::kUnregistered:
    case State::kDisconnecting:
      return services_.back().get();
    default:
      return nullptr;
  }
}

}  // namespace tracing
