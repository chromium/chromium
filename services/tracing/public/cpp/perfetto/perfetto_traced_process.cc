// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "base/tracing/perfetto_platform.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/custom_event_recorder.h"
#include "services/tracing/public/cpp/perfetto/dummy_producer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_tracing_backend.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
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
#endif  // BUILDFLAG(IS_POSIX)

namespace tracing {
namespace {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// Set to use the dummy producer for Chrome OS browser_tests and
// content_browsertests to keep the system producer from causing flakes.
static bool g_system_producer_enabled = true;
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

std::unique_ptr<SystemProducer> NewSystemProducer(
    base::tracing::PerfettoTaskRunner* runner,
    const char* socket_name) {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  DCHECK(socket_name);
  if (g_system_producer_enabled)
    return std::make_unique<PosixSystemProducer>(socket_name, runner);
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return std::make_unique<DummyProducer>(runner);
}

const char* MaybeSocket() {
#if BUILDFLAG(IS_POSIX)
  return perfetto::GetProducerSocket();
#elif BUILDFLAG(IS_FUCHSIA)
  // The socket is connected via a socket pair passed over IPC, which is not
  // accessible via an address.
  return "";
#else
  return nullptr;
#endif  // BUILDFLAG(IS_POSIX)
}

void OnPerfettoLogMessage(perfetto::base::LogMessageCallbackArgs args) {
  // Perfetto levels start at 0, base's at -1.
  int severity = static_cast<int>(args.level) - 1;
  ::logging::LogMessage(args.filename, args.line, severity).stream()
      << args.message;
}

}  // namespace

PerfettoTracedProcess::DataSourceBase::DataSourceBase(const std::string& name)
    : name_(name) {
  DCHECK(!name.empty());
  DETACH_FROM_SEQUENCE(perfetto_sequence_checker_);
}

PerfettoTracedProcess::DataSourceBase::~DataSourceBase() = default;

void PerfettoTracedProcess::DataSourceBase::StartTracing(
    uint64_t data_source_id,
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

  data_source_id_ = data_source_id;
  // Producer may already be set if startup tracing in TraceEventDataSource.
  DCHECK(!producer_ || producer_ == producer) << name_;
  producer_ = producer;
  StartTracingImpl(producer_, data_source_config);
}

void PerfettoTracedProcess::DataSourceBase::StopTracing(
    base::OnceClosure stop_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

  StopTracingImpl(base::BindOnce(
      [](DataSourceBase* self, base::OnceClosure original_callback) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(self->perfetto_sequence_checker_);
        self->producer_ = nullptr;
        if (original_callback)
          std::move(original_callback).Run();
      },
      this,  // OK to capture |this| because the callback is called by |this|.
      std::move(stop_complete_callback)));
}

void PerfettoTracedProcess::DataSourceBase::StartTracingImpl(
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {}

void PerfettoTracedProcess::DataSourceBase::StopTracingImpl(
    base::OnceClosure stop_complete_callback) {
  if (stop_complete_callback)
    std::move(stop_complete_callback).Run();
}

void PerfettoTracedProcess::DataSourceBase::Flush(
    base::RepeatingClosure flush_complete_callback) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  base::TrackEvent::Flush();
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  if (flush_complete_callback)
    std::move(flush_complete_callback).Run();
}

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
base::SequencedTaskRunner*
PerfettoTracedProcess::DataSourceBase::GetTaskRunner() {
  return PerfettoTracedProcess::Get()
      ->GetTaskRunner()
      ->GetOrCreateTaskRunner()
      .get();
}
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

// static
PerfettoTracedProcess* PerfettoTracedProcess::Get() {
  static base::NoDestructor<PerfettoTracedProcess> traced_process;
  return traced_process.get();
}

PerfettoTracedProcess::PerfettoTracedProcess()
    : producer_client_(std::make_unique<ProducerClient>(GetTaskRunner())),
      platform_(std::make_unique<base::tracing::PerfettoPlatform>()),
      tracing_backend_(std::make_unique<PerfettoTracingBackend>()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PerfettoTracedProcess::~PerfettoTracedProcess() {}

void PerfettoTracedProcess::SetConsumerConnectionFactory(
    ConsumerConnectionFactory factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  tracing_backend_->SetConsumerConnectionFactory(factory, task_runner);
}

void PerfettoTracedProcess::ConnectProducer(
    mojo::PendingRemote<mojom::PerfettoService> perfetto_service) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  tracing_backend_->OnProducerConnected(std::move(perfetto_service));
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  producer_client_->Connect(std::move(perfetto_service));
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
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

// We never destroy the taskrunner as we may need it for cleanup
// of TraceWriters in TLS, which could happen after the PerfettoTracedProcess
// is deleted.
// static
base::tracing::PerfettoTaskRunner* PerfettoTracedProcess::GetTaskRunner() {
  static base::NoDestructor<base::tracing::PerfettoTaskRunner> task_runner(
      nullptr);
  return task_runner.get();
}

// static
std::unique_ptr<PerfettoTracedProcess::TestHandle>
PerfettoTracedProcess::SetupForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // Make sure Perfetto was properly torn down in any prior tests.
  DCHECK(!perfetto::Tracing::IsInitialized());
  GetTaskRunner()->ResetTaskRunnerForTesting(task_runner);
  Get()->ClearDataSourcesForTesting();  // IN-TEST
  // On the first call within the process's lifetime, this will call
  // PerfettoTracedProcess::Get(), ensuring PerfettoTracedProcess is created.
  InitTracingPostThreadPoolStartAndFeatureList(
      /* enable_consumer */ true);
  // Disassociate the PerfettoTracedProcess from any prior task runner.
  DETACH_FROM_SEQUENCE(PerfettoTracedProcess::Get()->sequence_checker_);
  PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        // Lock the sequence checker onto the new task runner.
        DCHECK_CALLED_ON_VALID_SEQUENCE(
            PerfettoTracedProcess::Get()->sequence_checker_);
        PerfettoTracedProcess::Get()
            ->producer_client()
            ->ResetSequenceForTesting();
        if (PerfettoTracedProcess::Get()->system_producer()) {
          PerfettoTracedProcess::Get()
              ->system_producer()
              ->ResetSequenceForTesting();
        }
      }));
  return std::make_unique<TestHandle>();
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

void PerfettoTracedProcess::RequestStartupTracing(
    const perfetto::TraceConfig& config,
    const perfetto::Tracing::SetupStartupTracingOpts& opts) {
  if (platform_->did_start_task_runner()) {
    perfetto::Tracing::SetupStartupTracingBlocking(config, opts);
  } else {
    saved_config_ = config;
    saved_opts_ = opts;
    startup_tracing_needed_ = true;
  }
}

void PerfettoTracedProcess::SetupClientLibrary(bool enable_consumer) {
  perfetto::TracingInitArgs init_args;
  init_args.platform = platform_.get();
  init_args.custom_backend = tracing_backend_.get();
  init_args.backends |= perfetto::kCustomBackend;
  init_args.supports_multiple_data_source_instances = false;
  init_args.shmem_batch_commits_duration_ms = 1000;
  init_args.use_monotonic_clock = true;
// TODO(eseckler): Not yet supported on Android to avoid binary size regression
// of the consumer IPC messages. We'll need a way to exclude them.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  // In non-SDK build we only use the client library system backend for the
  // consumer side, which is only allowed in the browser process.
  // In SDK build we use system backend for producers too, but note that
  // currently the connection to the service fails from sandboxed processes.
  // TODO(khokhlov): Delegate socket connections from sandboxed processes
  // to the browser.
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  if (ShouldSetupSystemTracing()) {
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  if (ShouldSetupSystemTracing() && enable_consumer) {
#endif  // @BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    init_args.backends |= perfetto::kSystemBackend;
    init_args.tracing_policy = this;
  }
#endif
  // Proxy perfetto log messages into Chrome logs, so they are retained on all
  // platforms. In particular, on Windows, Perfetto's stderr log messages are
  // not reliable.
  init_args.log_message_callback = &OnPerfettoLogMessage;
  perfetto::Tracing::Initialize(init_args);

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  base::TrackEvent::Register();
  tracing::TracingSamplerProfiler::RegisterDataSource();
  TrackNameRecorder::GetInstance();
  CustomEventRecorder::GetInstance();
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

void PerfettoTracedProcess::OnThreadPoolAvailable(bool enable_consumer) {
  SetupClientLibrary(enable_consumer);

  // Create our task runner now, so that ProducerClient/SystemProducer are
  // notified about future data source registrations and schedule any necessary
  // startup tracing timeouts.
  GetTaskRunner()->GetOrCreateTaskRunner();

  producer_client_->OnThreadPoolAvailable();
  if (system_producer_)
    system_producer_->OnThreadPoolAvailable();
  if (!platform_->did_start_task_runner())
    platform_->StartTaskRunner(GetTaskRunner()->GetOrCreateTaskRunner());

  if (startup_tracing_needed_) {
    perfetto::Tracing::SetupStartupTracingBlocking(saved_config_, saved_opts_);
    startup_tracing_needed_ = false;
  }
}

void PerfettoTracedProcess::SetAllowSystemTracingConsumerCallback(
    base::RepeatingCallback<bool()> callback) {
  base::AutoLock lock(allow_system_consumer_lock_);
  DCHECK(!allow_system_consumer_callback_ || !callback);
  allow_system_consumer_callback_ = std::move(callback);
  allow_system_consumer_callback_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
}

void PerfettoTracedProcess::SetAllowSystemTracingConsumerForTesting(
    bool enabled) {
  base::AutoLock lock(allow_system_consumer_lock_);
  system_consumer_enabled_for_testing_ = enabled;
}

void PerfettoTracedProcess::ShouldAllowConsumerSession(
    const perfetto::TracingPolicy::ShouldAllowConsumerSessionArgs& args) {
  // Consumer connections should only be attempted in the browser process.
  CHECK(base::CommandLine::ForCurrentProcess()
            ->GetSwitchValueASCII("type")
            .empty());

  // Integrated tracing backends are always allowed.
  if (args.backend_type != perfetto::BackendType::kSystemBackend) {
    args.result_callback(true);
    return;
  }

  // System backend is only allowed in tests or if the embedder provided a
  // callback that allows it.
  ShouldAllowSystemConsumerSession(args.result_callback);
}

void PerfettoTracedProcess::ShouldAllowSystemConsumerSession(
    std::function<void(bool)> result_callback) {
  base::AutoLock lock(allow_system_consumer_lock_);

  if (system_consumer_enabled_for_testing_) {
    result_callback(true);
    return;
  }
  if (!allow_system_consumer_callback_) {
    result_callback(false);
    return;
  }

  if (!allow_system_consumer_callback_runner_->RunsTasksInCurrentSequence()) {
    allow_system_consumer_callback_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PerfettoTracedProcess::ShouldAllowSystemConsumerSession,
                       base::Unretained(this), result_callback));
    return;
  }

  bool result = allow_system_consumer_callback_.Run();
  result_callback(result);
}

void PerfettoTracedProcess::SetSystemProducerEnabledForTesting(bool enabled) {
#if BUILDFLAG(IS_POSIX)
  // If set to disabled, use the dummy implementation to prevent the real system
  // producer from interfering with browser tests.
  g_system_producer_enabled = enabled;
#endif
}

void PerfettoTracedProcess::SetupSystemTracing(
    absl::optional<const char*> system_socket) {
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
      system_producer_->DisconnectWithReply(base::DoNothing());
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

PerfettoTracedProcess::TestHandle::~TestHandle() {
  // TODO(skyostil): We only uninitialize Perfetto for now, but there may also
  // be other tracing-related state which should not leak between tests.
  perfetto::Tracing::ResetForTesting();
}

}  // namespace tracing
