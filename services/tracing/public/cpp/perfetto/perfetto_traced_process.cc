// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_config.h"
#include "base/tracing/perfetto_platform.h"
#include "base/tracing/perfetto_task_runner.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/custom_event_recorder.h"
#include "services/tracing/public/cpp/perfetto/histogram_samples_data_source.h"
#include "services/tracing/public/cpp/perfetto/perfetto_tracing_backend.h"
#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/system_metrics_sampler.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
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
#include "third_party/perfetto/include/perfetto/tracing/default_socket.h"  // nogncheck
#endif  // BUILDFLAG(IS_POSIX)

namespace tracing {
namespace {

PerfettoTracedProcess* g_instance = nullptr;
bool g_system_consumer_enabled_for_testing = false;

static scoped_refptr<base::SequencedTaskRunner>& GetDataSourceTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner;
  return *task_runner;
}

void OnPerfettoLogMessage(perfetto::base::LogMessageCallbackArgs args) {
  // Perfetto levels start at 0, base's at -1.
  int severity = static_cast<int>(args.level) - 1;
  ::logging::LogMessage(args.filename, args.line, severity).stream()
      << args.message;
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
// The async socket connection function passed to the client library for
// connecting the producer socket in the browser process via mojo IPC.
// |cb| is a callback from within the client library this function calls when
// the socket is opened.
void ConnectProducerSocketViaMojo(perfetto::CreateSocketCallback cb,
                                  base::TimeDelta retry_delay) {
  // Binary backoff with a max of 30 sec.
  constexpr base::TimeDelta kMaxRetry = base::Milliseconds(30 * 1000);
  auto next_retry_delay = std::min(retry_delay * 2, kMaxRetry);
  // Delayed reconnect function is bound with the increased retry delay.
  auto delayed_reconnect_cb = base::BindRepeating(
      [](perfetto::CreateSocketCallback cb, base::TimeDelta next_retry_delay) {
        ConnectProducerSocketViaMojo(cb, next_retry_delay);
      },
      cb, next_retry_delay);

  auto& remote = TracedProcessImpl::GetInstance()->system_tracing_service();
  if (!remote.is_bound()) {
    // Retry if the mojo remote is not bound yet.
    PerfettoTracedProcess::GetTaskRunner()->PostDelayedTask(
        FROM_HERE, delayed_reconnect_cb, next_retry_delay);
    return;
  }

  auto callback = base::BindOnce(
      [](perfetto::CreateSocketCallback cb,
         base::RepeatingClosure delayed_reconnect_cb,
         base::TimeDelta retry_delay, base::File file) {
        if (!file.IsValid()) {
          PerfettoTracedProcess::GetTaskRunner()->PostDelayedTask(
              FROM_HERE, delayed_reconnect_cb, retry_delay);
          return;
        }

        // Success, call |cb| into the Perfetto client library with a valid
        // socket handle.
        cb(file.TakePlatformFile());
      },
      cb, delayed_reconnect_cb, retry_delay);

  // Open the socket remotely using Mojo.
  remote->OpenProducerSocket(std::move(callback));
}

// Wrapper for |ConnectProducerSocketViaMojo| to be used as a function pointer.
void ConnectProducerSocketAsync(perfetto::CreateSocketCallback cb) {
  PerfettoTracedProcess::Get().DeferOrConnectProducerSocket(std::move(cb));
}
#endif

}  // namespace

PerfettoTracedProcess::DataSourceBase::DataSourceBase(const std::string& name)
    : name_(name) {
  DCHECK(!name.empty());
  DETACH_FROM_SEQUENCE(perfetto_sequence_checker_);
}

PerfettoTracedProcess::DataSourceBase::~DataSourceBase() = default;

void PerfettoTracedProcess::DataSourceBase::StartTracing(
    uint64_t data_source_id,
    const perfetto::DataSourceConfig& data_source_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

  data_source_id_ = data_source_id;
  StartTracingImpl(data_source_config);
}

void PerfettoTracedProcess::DataSourceBase::StopTracing(
    base::OnceClosure stop_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

  StopTracingImpl(base::BindOnce(
      [](DataSourceBase* self, base::OnceClosure original_callback) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(self->perfetto_sequence_checker_);
        if (original_callback)
          std::move(original_callback).Run();
      },
      this,  // OK to capture |this| because the callback is called by |this|.
      std::move(stop_complete_callback)));
}

void PerfettoTracedProcess::DataSourceBase::StartTracingImpl(
    const perfetto::DataSourceConfig& data_source_config) {}

void PerfettoTracedProcess::DataSourceBase::StopTracingImpl(
    base::OnceClosure stop_complete_callback) {
  if (stop_complete_callback)
    std::move(stop_complete_callback).Run();
}

void PerfettoTracedProcess::DataSourceBase::Flush(
    base::RepeatingClosure flush_complete_callback) {
  base::TrackEvent::Flush();
  if (flush_complete_callback)
    std::move(flush_complete_callback).Run();
}

base::SequencedTaskRunner*
PerfettoTracedProcess::DataSourceBase::GetTaskRunner() {
  return GetDataSourceTaskRunner().get();
}

void PerfettoTracedProcess::DataSourceBase::ResetTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  GetDataSourceTaskRunner() = task_runner;
}

void PerfettoTracedProcess::RestartThreadInSandbox() {
  CHECK(trace_process_thread_->StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0)));
  DETACH_FROM_SEQUENCE(sequence_checker_);
  task_runner_ = trace_process_thread_->task_runner();
  platform_->ResetTaskRunner(trace_process_thread_->task_runner());
  DataSourceBase::ResetTaskRunner(trace_process_thread_->task_runner());
  tracing_backend_->DetachFromMuxerSequence();
  CustomEventRecorder::GetInstance()->DetachFromSequence();
  will_trace_thread_restart_ = false;
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  if (system_tracing_producer_socket_cb_) {
    task_runner_->PostTask(FROM_HERE,
                           std::move(system_tracing_producer_socket_cb_));
  }
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
}

// static
base::Thread* PerfettoTracedProcess::GetTraceThread() {
  return PerfettoTracedProcess::Get().trace_process_thread_.get();
}

// static
PerfettoTracedProcess& PerfettoTracedProcess::MaybeCreateInstance(
    bool will_trace_thread_restart) {
  static base::NoDestructor<PerfettoTracedProcess> traced_process(
      will_trace_thread_restart);
  return *traced_process;
}

// static
PerfettoTracedProcess& PerfettoTracedProcess::MaybeCreateInstanceForTesting() {
  static base::NoDestructor<PerfettoTracedProcess> traced_process(nullptr);
  return *traced_process;
}

// static
PerfettoTracedProcess& PerfettoTracedProcess::Get() {
  CHECK_NE(g_instance, nullptr);
  return *g_instance;
}

PerfettoTracedProcess::PerfettoTracedProcess(bool will_trace_thread_restart)
    : trace_process_thread_(std::make_unique<base::Thread>("PerfettoTrace")),
      task_runner_(trace_process_thread_->StartWithOptions(
                       base::Thread::Options(base::MessagePumpType::IO, 0))
                       ? trace_process_thread_->task_runner()
                       : nullptr),
      will_trace_thread_restart_(will_trace_thread_restart),
      tracing_backend_(std::make_unique<PerfettoTracingBackend>()) {
  base::tracing::PerfettoPlatform::Options options{
      .defer_delayed_tasks = will_trace_thread_restart_};
  platform_ =
      std::make_unique<base::tracing::PerfettoPlatform>(task_runner_, options);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  CHECK_EQ(g_instance, nullptr);
  CHECK(task_runner_);
  g_instance = this;
  GetDataSourceTaskRunner() = task_runner_;
}

PerfettoTracedProcess::PerfettoTracedProcess(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner),
      platform_(
          std::make_unique<base::tracing::PerfettoPlatform>(task_runner_)),
      tracing_backend_(std::make_unique<PerfettoTracingBackend>()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
  GetDataSourceTaskRunner() = task_runner_;
}

PerfettoTracedProcess::~PerfettoTracedProcess() = default;

void PerfettoTracedProcess::SetConsumerConnectionFactory(
    ConsumerConnectionFactory factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  tracing_backend_->SetConsumerConnectionFactory(factory, task_runner);
}

void PerfettoTracedProcess::ConnectProducer(
    mojo::PendingRemote<mojom::PerfettoService> perfetto_service) {
  tracing_backend_->OnProducerConnected(std::move(perfetto_service));
}

// We never destroy the taskrunner as we may need it for cleanup
// of TraceWriters in TLS, which could happen after the PerfettoTracedProcess
// is deleted.
// static
base::SequencedTaskRunner* PerfettoTracedProcess::GetTaskRunner() {
  return PerfettoTracedProcess::Get().task_runner_.get();
}

void PerfettoTracedProcess::SetupForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // Make sure Perfetto was properly torn down in any prior tests.
  DCHECK(!perfetto::Tracing::IsInitialized());

  task_runner_ = std::move(task_runner);
  platform_->ResetTaskRunner(task_runner_);
  DataSourceBase::ResetTaskRunner(task_runner_);

  tracing_backend_ = std::make_unique<PerfettoTracingBackend>();
  SetupClientLibrary(/*enable_consumer=*/true, ShouldSetupSystemTracing());
  // Disassociate the PerfettoTracedProcess from any prior task runner.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void PerfettoTracedProcess::ResetForTesting() {
  base::WaitableEvent on_reset_done;
  // The tracing backend is used internally in Perfetto on the |task_runner_|
  // sequence. Reset and destroy the backend on the task runner to avoid racing
  // in resetting Perfetto.
  auto reset_task = base::BindOnce(
      [](decltype(tracing_backend_) tracing_backend,
         base::WaitableEvent* on_reset_done) {
        tracing_backend.reset();
        // TODO(skyostil): We only uninitialize Perfetto
        // for now, but there may also be other
        // tracing-related state which should not leak
        // between tests.
        perfetto::Tracing::ResetForTesting();
        on_reset_done->Signal();
      },
      std::move(tracing_backend_), &on_reset_done);
  if (task_runner_->RunsTasksInCurrentSequence()) {
    std::move(reset_task).Run();
  } else {
    task_runner_->PostTask(FROM_HERE, std::move(reset_task));

    on_reset_done.Wait();
  }
  task_runner_ = nullptr;
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
void PerfettoTracedProcess::DeferOrConnectProducerSocket(
    perfetto::CreateSocketCallback cb) {
  CHECK(!system_tracing_producer_socket_cb_);
  // Hold off the attempts to get socket fd until trace thread restarts.
  if (will_trace_thread_restart_) {
    system_tracing_producer_socket_cb_ = base::BindOnce(
        ConnectProducerSocketViaMojo, cb, base::Milliseconds(100));
  } else {
    ConnectProducerSocketViaMojo(cb, base::Milliseconds(100));
  }
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

void PerfettoTracedProcess::SetupClientLibrary(
    bool enable_consumer,
    bool enable_system_backend,
    std::optional<uint64_t> process_track_uuid) {
  perfetto::TracingInitArgs init_args;
  init_args.platform = platform_.get();
  init_args.custom_backend = tracing_backend_.get();
  init_args.backends |= perfetto::kCustomBackend;
  init_args.shmem_batch_commits_duration_ms = 1000;
  init_args.shmem_size_hint_kb = 4 * 1024;
  init_args.shmem_direct_patching_enabled = true;
  init_args.use_monotonic_clock = true;
  init_args.disallow_merging_with_system_tracks = true;
  init_args.process_uuid = process_track_uuid;
#if BUILDFLAG(IS_POSIX)
  if (enable_system_backend) {
    init_args.backends |= perfetto::kSystemBackend;
    init_args.tracing_policy = this;
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
    auto type =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("type");
    if (!type.empty()) {  // Sandboxed. Need to delegate to the browser process
                          // using Mojo.
      init_args.create_socket_async = ConnectProducerSocketAsync;
    }
#endif
  }
#endif
  // Proxy perfetto log messages into Chrome logs, so they are retained on all
  // platforms. In particular, on Windows, Perfetto's stderr log messages are
  // not reliable.
  init_args.log_message_callback = &OnPerfettoLogMessage;
  perfetto::Tracing::Initialize(init_args);

  base::TrackEvent::Register();
  tracing::TracingSamplerProfiler::RegisterDataSource();
  tracing::HistogramSamplesDataSource::Register();
  // SystemMetricsSampler will be started when enabling
  // kSystemMetricsSourceName.
  tracing::SystemMetricsSampler::Register(/*system_wide=*/enable_consumer);
  TrackNameRecorder::GetInstance();
  CustomEventRecorder::GetInstance();
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
  g_system_consumer_enabled_for_testing = enabled;
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

  if (g_system_consumer_enabled_for_testing) {
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

}  // namespace tracing
