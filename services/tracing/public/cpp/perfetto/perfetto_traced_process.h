// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/perfetto_task_runner.h"
#include "services/tracing/public/cpp/perfetto/perfetto_tracing_backend.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing_policy.h"

namespace base {
class Thread;
namespace trace_event {
class TraceConfig;
}  // namespace trace_event
namespace tracing {
class PerfettoPlatform;
}  // namespace tracing
}  // namespace base

namespace tracing {
namespace mojom {
class TracingService;
}  // namespace mojom

// This represents global process level state that the Perfetto tracing system
// expects to exist. This includes a single base implementation of DataSources
// all implementors should use and the perfetto task runner that should be used
// when talking to the tracing system to prevent deadlocks.
//
// Implementations of new DataSources should:
// * Implement PerfettoTracedProcess::DataSourceBase.
// * Add a new data source name in perfetto_service.mojom.
// * Register the data source with Perfetto in ProducerHost::OnConnect.
// * Construct the new implementation when requested to
//   in PerfettoProducer::StartDataSource.
class COMPONENT_EXPORT(TRACING_CPP) PerfettoTracedProcess final
    : public perfetto::TracingPolicy {
 public:
  // If not noted otherwise, a DataSourceBase's methods are only called on
  // PerfettoTracedProcess::GetTaskRunner()'s sequence.
  class COMPONENT_EXPORT(TRACING_CPP) DataSourceBase {
   public:
    explicit DataSourceBase(const std::string& name);
    virtual ~DataSourceBase();

    void StartTracing(uint64_t data_source_id,
                      const perfetto::DataSourceConfig& data_source_config);
    void StopTracing(
        base::OnceClosure stop_complete_callback = base::OnceClosure());

    virtual void StartTracingImpl(
        const perfetto::DataSourceConfig& data_source_config);
    virtual void StopTracingImpl(
        base::OnceClosure stop_complete_callback = base::OnceClosure());

    // Flush the data source.
    virtual void Flush(base::RepeatingClosure flush_complete_callback);

    virtual void ClearIncrementalState() {}

    // Enable startup tracing. The data source can use
    // SharedMemoryArbiter::CreateStartupTraceWriter() to begin tracing
    // immediately (the arbiter will buffer commits locally until connection to
    // the perfetto service is established). The data source should expect a
    // later call to StartTracing() to bind to the perfetto service, or a call
    // to AbortStartupTracing(). Called on any thread.
    virtual void SetupStartupTracing(
        const base::trace_event::TraceConfig& trace_config,
        bool privacy_filtering_enabled) {}

    // Cancel the tracing session started by SetupStartupTracing() after a
    // timeout. The data source should invalidate any startup trace buffer
    // reservations made with SharedMemoryArbiter.
    virtual void AbortStartupTracing() {}

    const std::string& name() const { return name_; }

    // These accessors are only allowed on the perfetto sequence.
    uint64_t data_source_id() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
      return data_source_id_;
    }

    // By default, data source callbacks (e.g., Start/StopTracingImpl) are
    // called on PerfettoTracedProcess::GetTaskRunner()'s sequence. This method
    // allows overriding that task runner.
    // Note: The task_runner's thread may stop and restart for Linux/ChromeOS
    // sandboxing so the task_runner can change and delayed tasks posted to it
    // may be silently dropped.
    virtual base::SequencedTaskRunner* GetTaskRunner();

    static void ResetTaskRunner(
        scoped_refptr<base::SequencedTaskRunner> task_runner);

   protected:
    SEQUENCE_CHECKER(perfetto_sequence_checker_);

   private:
    uint64_t data_source_id_ = 0;
    std::string name_;
  };

  // A proxy that adapts Chrome's DataSourceBase class into a Perfetto
  // DataSource, allowing the former to be registered as a data source in the
  // tracing service and participate in tracing sessions.
  //
  // Any subclass using this proxy should also use
  // PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS to instantiate the necessary
  // static data members.
  //
  // The subclass should also call DataSourceProxy::Register() with a data
  // source descriptor and a pointer to the DataSourceBase instance in order to
  // hook up the data source with Perfetto. The DataSourceBase instance passed
  // to Register() must have process-lifetime since Perfetto data sources are
  // never unregistered.
  template <typename T>
  class DataSourceProxy : public perfetto::DataSource<DataSourceProxy<T>> {
   public:
    // Create a proxy for a singleton data source instance.
    explicit DataSourceProxy(PerfettoTracedProcess::DataSourceBase*);
    // Create a proxy for a data source instance which may change, typically
    // between test iterations. Note that it is not safe to change the data
    // source instance while any tracing sessions are started or stopped.
    explicit DataSourceProxy(raw_ptr<PerfettoTracedProcess::DataSourceBase>*);
    ~DataSourceProxy() override;

    // perfetto::DataSource implementation:
    void OnSetup(const perfetto::DataSourceBase::SetupArgs&) override;
    void OnStart(const perfetto::DataSourceBase::StartArgs&) override;
    void OnStop(const perfetto::DataSourceBase::StopArgs&) override;
    void WillClearIncrementalState(
        const perfetto::DataSourceBase::ClearIncrementalStateArgs&) override;
    bool CanAdoptStartupSession(const perfetto::DataSourceConfig&,
                                const perfetto::DataSourceConfig&) override;

    static constexpr bool kSupportsMultipleInstances = false;

   private:
    const raw_ptr<PerfettoTracedProcess::DataSourceBase> data_source_ = nullptr;
    raw_ptr<const raw_ptr<PerfettoTracedProcess::DataSourceBase>>
        data_source_ptr_ = &data_source_;
    perfetto::DataSourceConfig data_source_config_;
  };

  // Restarts the trace thread and replaces the task_runner for tracing.
  void RestartThreadInSandbox();

  // Returns the process-wide ptr to the trace thread, returns nullptr if the
  // task_runner for tracing is from the thread-pool.
  static base::Thread* GetTraceThread();

  // Creates the process-wide instance of the PerfettoTracedProcess.
  static PerfettoTracedProcess& MaybeCreateInstance();
  static PerfettoTracedProcess& MaybeCreateInstanceWithThread(
      bool will_trace_thread_restart);
  static PerfettoTracedProcess& MaybeCreateInstanceForTesting();

  // Returns the process-wide instance of the PerfettoTracedProcess.
  static PerfettoTracedProcess& Get();

  PerfettoTracedProcess(const PerfettoTracedProcess&) = delete;
  PerfettoTracedProcess& operator=(const PerfettoTracedProcess&) = delete;

  // Provide a factory for lazily creating mojo consumer connections to the
  // tracing service. Allows using Perfetto's Client API for recording traces.
  using ConsumerConnectionFactory = mojom::TracingService& (*)();
  void SetConsumerConnectionFactory(ConsumerConnectionFactory,
                                    scoped_refptr<base::SequencedTaskRunner>);

  // Connect the current process to the mojo trace producer API. Depending on
  // the configuration, this will either set up the Perfetto Client API or the
  // legacy TraceLog to become the trace producer for this process.
  void ConnectProducer(mojo::PendingRemote<mojom::PerfettoService>);

  ~PerfettoTracedProcess() override;

  // Returns the task runner used by any Perfetto service. Can be called on any
  // thread.
  static base::SequencedTaskRunner* GetTaskRunner();

  // Attempt to enable startup tracing for the current process and given
  // producer. Returns false on failure, e.g. because another concurrent tracing
  // session is already active. Can be called on any thread.
  bool SetupStartupTracing(const base::trace_event::TraceConfig&,
                           bool privacy_filtering_enabled);

  // Called on the process's main thread once the thread pool is ready.
  void OnThreadPoolAvailable(bool enable_consumer);

  // Set a callback that returns whether a system tracing session is allowed.
  // The callback will be executed on the sequence that set it. Only a single
  // callback is supported. If no callback is set, all system consumer
  // connections are denied.
  void SetAllowSystemTracingConsumerCallback(base::RepeatingCallback<bool()>);

  // Overrides SetAllowSystemTracingConsumerCallback() for testing.
  static void SetAllowSystemTracingConsumerForTesting(bool allow);

  // Sets the task runner used by the tracing infrastructure in this process.
  // The returned handle will automatically tear down tracing when destroyed, so
  // it should be kept valid until the test terminates.
  //
  // Be careful when using SetupForTesting. There is a PostTask in the
  // constructor of PerfettoTracedProcess, so before this class is constructed
  // is the only safe time to call this.
  void SetupForTesting(scoped_refptr<base::SequencedTaskRunner> task_runner);
  void ResetForTesting();

  base::tracing::PerfettoPlatform* perfetto_platform_for_testing() const {
    return platform_.get();
  }

  // Indicate that startup tracing will need to start when thread pool becomes
  // available. This is used in Perfetto client library build, because currently
  // it requires a threadpool to run tracing tasks.
  // TODO(khokhlov): Remove this method once startup tracing no longer depends
  // on threadpool in client library build.
  void RequestStartupTracing(
      const perfetto::TraceConfig& config,
      const perfetto::Tracing::SetupStartupTracingOpts& opts);

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  void DeferOrConnectProducerSocket(perfetto::CreateSocketCallback cb);
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

 private:
  friend class base::NoDestructor<PerfettoTracedProcess>;

  // Default constructor would create a dedicated thread for tracing
  explicit PerfettoTracedProcess(bool will_trace_thread_restart);
  explicit PerfettoTracedProcess(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Initialize the Perfetto client library (i.e., perfetto::Tracing) for this
  // process.
  // |enable_consumer| should be true if the system consumer can be enabled.
  // Currently this is only the case if this is running in the browser process.
  void SetupClientLibrary(bool enable_consumer);

  // perfetto::TracingPolicy implementation:
  void ShouldAllowConsumerSession(
      const perfetto::TracingPolicy::ShouldAllowConsumerSessionArgs&) override;

  void ShouldAllowSystemConsumerSession(
      std::function<void(bool)> result_callback);

  base::Lock allow_system_consumer_lock_;
  base::RepeatingCallback<bool()> allow_system_consumer_callback_
      GUARDED_BY(allow_system_consumer_lock_);
  scoped_refptr<base::SequencedTaskRunner>
      allow_system_consumer_callback_runner_
          GUARDED_BY(allow_system_consumer_lock_);
  bool system_consumer_enabled_for_testing_
      GUARDED_BY(allow_system_consumer_lock_) = false;

  std::unique_ptr<base::Thread> trace_process_thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool will_trace_thread_restart_ = false;
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  base::OnceClosure system_tracing_producer_socket_cb_;
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

  // Platform implementation for the Perfetto client library.
  std::unique_ptr<base::tracing::PerfettoPlatform> platform_;
  std::unique_ptr<PerfettoTracingBackend> tracing_backend_;

  bool startup_tracing_needed_ = false;
  bool thread_pool_started_ = false;
  perfetto::TraceConfig saved_config_;
  perfetto::Tracing::SetupStartupTracingOpts saved_opts_;

  SEQUENCE_CHECKER(sequence_checker_);
};

template <typename T>
PerfettoTracedProcess::DataSourceProxy<T>::DataSourceProxy(
    PerfettoTracedProcess::DataSourceBase* data_source)
    : data_source_(data_source) {}

template <typename T>
PerfettoTracedProcess::DataSourceProxy<T>::DataSourceProxy(
    raw_ptr<PerfettoTracedProcess::DataSourceBase>* data_source_ptr)
    : data_source_ptr_(data_source_ptr) {}

template <typename T>
PerfettoTracedProcess::DataSourceProxy<T>::~DataSourceProxy() = default;

template <typename T>
void PerfettoTracedProcess::DataSourceProxy<T>::OnSetup(
    const perfetto::DataSourceBase::SetupArgs& args) {
  data_source_config_ = *args.config;
}

template <typename T>
void PerfettoTracedProcess::DataSourceProxy<T>::OnStart(
    const perfetto::DataSourceBase::StartArgs&) {
  (*data_source_ptr_)
      ->GetTaskRunner()
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &PerfettoTracedProcess::DataSourceBase::StartTracingImpl,
                     base::Unretained(*data_source_ptr_), data_source_config_));
}

template <typename T>
void PerfettoTracedProcess::DataSourceProxy<T>::OnStop(
    const perfetto::DataSourceBase::StopArgs& args) {
  std::function<void()> finish_async_stop = args.HandleStopAsynchronously();
  base::OnceClosure stop_callback = base::BindOnce(
      [](std::function<void()> callback) { callback(); }, finish_async_stop);
  (*data_source_ptr_)
      ->GetTaskRunner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &PerfettoTracedProcess::DataSourceBase::StopTracingImpl,
              base::Unretained(*data_source_ptr_), std::move(stop_callback)));
}

template <typename T>
void PerfettoTracedProcess::DataSourceProxy<T>::WillClearIncrementalState(
    const perfetto::DataSourceBase::ClearIncrementalStateArgs& args) {
  (*data_source_ptr_)->ClearIncrementalState();
}

template <typename T>
bool PerfettoTracedProcess::DataSourceProxy<T>::CanAdoptStartupSession(
    const perfetto::DataSourceConfig& startup_config,
    const perfetto::DataSourceConfig& service_config) {
  perfetto::DataSourceConfig startup_config_stripped = startup_config;
  perfetto::DataSourceConfig service_config_stripped = service_config;
  if (startup_config.has_chrome_config() &&
      service_config.has_chrome_config()) {
    base::trace_event::TraceConfig startup_trace_config(
        startup_config.chrome_config().trace_config());
    base::trace_event::TraceConfig service_trace_config(
        service_config.chrome_config().trace_config());
    if (!startup_trace_config.IsEquivalentTo(service_trace_config)) {
      return false;
    }
    *startup_config_stripped.mutable_chrome_config() = {};
    *service_config_stripped.mutable_chrome_config() = {};
  }

  return perfetto::DataSourceBase::CanAdoptStartupSession(
      startup_config_stripped, service_config_stripped);
}

}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_
