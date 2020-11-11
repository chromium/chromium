// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"

#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/dummy_producer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_platform.h"
#include "services/tracing/public/cpp/perfetto/perfetto_tracing_backend.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

#if defined(OS_POSIX)
// As per 'gn help check':
/*
  If you have conditional includes, make sure the build conditions and the
  preprocessor conditions match, and annotate the line with "nogncheck" (see
  "gn help nogncheck" for an example).
*/
// We add the nogncheck to ensure this doesn't trigger incorrect errors on
// non-android builds.
#include "services/tracing/public/cpp/perfetto/posix_system_producer.h"  // nogncheck
#include "third_party/perfetto/include/perfetto/ext/tracing/ipc/default_socket.h"  // nogncheck
#endif  // defined(OS_POSIX)

namespace tracing {
namespace {
std::unique_ptr<SystemProducer> NewSystemProducer(PerfettoTaskRunner* runner,
                                                  const char* socket_name) {
#if defined(OS_POSIX)
  DCHECK(socket_name);
  return std::make_unique<PosixSystemProducer>(socket_name, runner);
#endif  // defined(OS_POSIX)
  return std::make_unique<DummyProducer>(runner);
}

const char* MaybeSocket() {
#if defined(OS_POSIX)
  return perfetto::GetProducerSocket();
#else
  return nullptr;
#endif  // defined(OS_POSIX)
}
}  // namespace

PerfettoTracedProcess::DataSourceBase::DataSourceBase(const std::string& name)
    : name_(name) {
  DCHECK(!name.empty());
}

PerfettoTracedProcess::DataSourceBase::~DataSourceBase() = default;

void PerfettoTracedProcess::DataSourceBase::StartTracingWithID(
    uint64_t data_source_id,
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  data_source_id_ = data_source_id;
  // Producer may already be set if startup tracing in TraceEventDataSource.
  DCHECK(!producer_ || producer_ == producer) << name_;
  producer_ = producer;
  StartTracing(producer_, data_source_config);
}

// static
PerfettoTracedProcess* PerfettoTracedProcess::Get() {
  static base::NoDestructor<PerfettoTracedProcess> traced_process;
  return traced_process.get();
}

PerfettoTracedProcess::PerfettoTracedProcess()
    : producer_client_(std::make_unique<ProducerClient>(GetTaskRunner())),
      platform_(std::make_unique<PerfettoPlatform>()),
      tracing_backend_(std::make_unique<PerfettoTracingBackend>(*this)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PerfettoTracedProcess::~PerfettoTracedProcess() {}

void PerfettoTracedProcess::SetConsumerConnectionFactory(
    ConsumerConnectionFactory factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  consumer_connection_factory_ = factory;
  consumer_connection_task_runner_ = task_runner;
}

void PerfettoTracedProcess::ConnectProducer(
    mojo::PendingRemote<mojom::PerfettoService> perfetto_service) {
  if (base::FeatureList::IsEnabled(
          features::kEnablePerfettoClientApiProducer)) {
    DCHECK(pending_producer_callback_);
    std::move(pending_producer_callback_).Run(std::move(perfetto_service));
  } else {
    producer_client_->Connect(std::move(perfetto_service));
  }
}

void PerfettoTracedProcess::ClearDataSourcesForTesting() {
  base::AutoLock lock(data_sources_lock_);
  data_sources_.clear();
}

std::unique_ptr<ProducerClient>
PerfettoTracedProcess::SetProducerClientForTesting(
    std::unique_ptr<ProducerClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto old_producer_client_for_testing = std::move(producer_client_);
  producer_client_ = std::move(client);
  return old_producer_client_for_testing;
}

std::unique_ptr<SystemProducer>
PerfettoTracedProcess::SetSystemProducerForTesting(
    std::unique_ptr<SystemProducer> producer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto old_for_testing = std::move(system_producer_);
  system_producer_ = std::move(producer);
  return old_for_testing;
}

// static
void PerfettoTracedProcess::DeleteSoonForTesting(
    std::unique_ptr<PerfettoTracedProcess> perfetto_traced_process) {
  GetTaskRunner()->GetOrCreateTaskRunner()->DeleteSoon(
      FROM_HERE, std::move(perfetto_traced_process));
}

void PerfettoTracedProcess::CreateProducerConnection(
    base::OnceCallback<void(mojo::PendingRemote<mojom::PerfettoService>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Perfetto will attempt to create the producer connection as soon as the
  // client library is initialized, which is before we have a a connection to
  // the tracing service. Store the connection callback until ConnectProducer()
  // is called.
  DCHECK(!pending_producer_callback_);
  pending_producer_callback_ = std::move(callback);
}

void PerfettoTracedProcess::CreateConsumerConnection(
    base::OnceCallback<void(mojo::PendingRemote<mojom::ConsumerHost>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consumer_connection_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ConsumerConnectionFactory factory,
             base::OnceCallback<void(mojo::PendingRemote<mojom::ConsumerHost>)>
                 callback) {
            auto& tracing_service = factory();
            mojo::PendingRemote<mojom::ConsumerHost> consumer_host_remote;
            tracing_service.BindConsumerHost(
                consumer_host_remote.InitWithNewPipeAndPassReceiver());
            std::move(callback).Run(std::move(consumer_host_remote));
          },
          consumer_connection_factory_, std::move(callback)));
}

// We never destroy the taskrunner as we may need it for cleanup
// of TraceWriters in TLS, which could happen after the PerfettoTracedProcess
// is deleted.
// static
PerfettoTaskRunner* PerfettoTracedProcess::GetTaskRunner() {
  static base::NoDestructor<PerfettoTaskRunner> task_runner(nullptr);
  return task_runner.get();
}

// static
void PerfettoTracedProcess::ResetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  GetTaskRunner()->ResetTaskRunnerForTesting(task_runner);
  InitTracingPostThreadPoolStartAndFeatureList();
  // Detaching the sequence_checker_ must happen after we reset the task runner.
  // This is because the Get() could call the constructor (if this is the first
  // call to Get()) which would then PostTask which would create races if we
  // reset the task runner right afterwards.
  DETACH_FROM_SEQUENCE(PerfettoTracedProcess::Get()->sequence_checker_);
  // Call Get() explicitly. This ensures that we constructed the
  // PerfettoTracedProcess. On some tests (like cast linux) the DETACH macro is
  // compiled to nothing, which woud cause this PostTask to access a nullptr the
  // producer requires a PostTask from inside the constructor.
  PerfettoTracedProcess::Get();
  PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        PerfettoTracedProcess::Get()
            ->producer_client()
            ->ResetSequenceForTesting();
        if (PerfettoTracedProcess::Get()->system_producer()) {
          PerfettoTracedProcess::Get()
              ->system_producer()
              ->ResetSequenceForTesting();
        }
      }));
}

void PerfettoTracedProcess::AddDataSource(DataSourceBase* data_source) {
  bool inserted;
  {
    base::AutoLock lock(data_sources_lock_);
    inserted = data_sources_.insert(data_source).second;
  }

  // Before the thread pool is up, the producers are not yet connected to the
  // service, so don't need to be notified about new data source registrations.
  if (inserted && GetTaskRunner()->HasTaskRunner()) {
    GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](DataSourceBase* data_source) {
                         PerfettoTracedProcess* traced_process =
                             PerfettoTracedProcess::Get();
                         traced_process->producer_client()->NewDataSourceAdded(
                             data_source);
                         if (traced_process->system_producer()) {
                           traced_process->system_producer()
                               ->NewDataSourceAdded(data_source);
                         }
                       },
                       base::Unretained(data_source)));
  }
}

std::set<PerfettoTracedProcess::DataSourceBase*>
PerfettoTracedProcess::data_sources() {
  base::AutoLock lock(data_sources_lock_);
  return data_sources_;
}

bool PerfettoTracedProcess::SetupStartupTracing(
    PerfettoProducer* producer,
    const base::trace_event::TraceConfig& trace_config,
    bool privacy_filtering_enabled) {
  if (producer_client_->IsTracingActive() ||
      (system_producer_ && system_producer_->IsTracingActive())) {
    LOG(WARNING) << "Cannot setup startup tracing - tracing is already active";
    return false;
  }

  if (!producer->SetupStartupTracing(trace_config, privacy_filtering_enabled)) {
    LOG(ERROR) << "Failed to setup startup tracing for this process";
    return false;
  }

  return true;
}

void PerfettoTracedProcess::SetupClientLibrary() {
  perfetto::TracingInitArgs init_args;
  init_args.platform = platform_.get();
  init_args.custom_backend = tracing_backend_.get();
  init_args.backends |= perfetto::kCustomBackend;
  perfetto::Tracing::Initialize(init_args);
}

void PerfettoTracedProcess::OnThreadPoolAvailable() {
  // Create our task runner now, so that ProducerClient/SystemProducer are
  // notified about future data source registrations and schedule any necessary
  // startup tracing timeouts.
  GetTaskRunner()->GetOrCreateTaskRunner();

  producer_client_->OnThreadPoolAvailable();
  if (system_producer_)
    system_producer_->OnThreadPoolAvailable();
  if (!platform_->did_start_task_runner())
    platform_->StartTaskRunner(GetTaskRunner()->GetOrCreateTaskRunner());
}

void PerfettoTracedProcess::SetupSystemTracing(
    base::Optional<const char*> system_socket) {
  // Note: Not checking for a valid sequence here so that we don't inadvertently
  // bind this object on the wrong sequence during early initialization.
  DCHECK(!system_producer_);
  system_producer_ = NewSystemProducer(
      GetTaskRunner(), system_socket ? *system_socket : MaybeSocket());
  // If the thread pool is available, register all the known data sources with
  // the system producer too.
  if (!GetTaskRunner()->HasTaskRunner())
    return;
  system_producer_->OnThreadPoolAvailable();
  GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        PerfettoTracedProcess* traced_process = PerfettoTracedProcess::Get();
        base::AutoLock lock(traced_process->data_sources_lock_);
        for (auto* data_source : traced_process->data_sources_) {
          traced_process->system_producer()->NewDataSourceAdded(data_source);
        }
      }));
}

bool PerfettoTracedProcess::CanStartTracing(
    PerfettoProducer* producer,
    base::OnceCallback<void()> start_tracing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(producer);
  // If the |producer| asking is the local producer_client_ it has priority so
  // even if the other endpoint is tracing shut down the other endpoint and let
  // the |producer_client_| go. The system Producer will periodically attempt to
  // reconnect if we call DisconnectWithReply().
  if (producer == producer_client_.get()) {
    if (system_producer_ && system_producer_->IsTracingActive()) {
      system_producer_->DisconnectWithReply(std::move(start_tracing));
      return true;
    }
  } else if (producer == system_producer_.get()) {
    if (producer_client_->IsTracingActive()) {
      system_producer_->DisconnectWithReply(base::DoNothing().Once());
      return false;
    }
  } else {
    // In tests this is possible due to the periodic polling of CanStartTracing
    // by the PosixSystemProducer, when we swap it out for a
    // MockSystemProducer there can be three PerfettoProducers calling this
    // function. In production nothing ever calls the
    // |Set.*ProducerForTesting()| functions so this should never be reached.
    return false;
  }
  if (!start_tracing.is_null()) {
    std::move(start_tracing).Run();
  }
  return true;
}

void PerfettoTracedProcess::ActivateSystemTriggers(
    const std::vector<std::string>& triggers) {
  if (!GetTaskRunner()->GetOrCreateTaskRunner()->RunsTasksInCurrentSequence()) {
    GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PerfettoTracedProcess::ActivateSystemTriggers,
                       base::Unretained(this), triggers));
    return;
  }
  system_producer_->ActivateTriggers(triggers);
}

ProducerClient* PerfettoTracedProcess::producer_client() const {
  return producer_client_.get();
}

SystemProducer* PerfettoTracedProcess::system_producer() const {
  return system_producer_.get();
}

}  // namespace tracing
