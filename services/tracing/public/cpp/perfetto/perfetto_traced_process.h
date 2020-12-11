// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "services/tracing/public/cpp/perfetto/perfetto_tracing_backend.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"

namespace base {
namespace trace_event {
class TraceConfig;
}  // namespace trace_event
}  // namespace base

namespace tracing {
namespace mojom {
class TracingService;
}  // namespace mojom

class PerfettoPlatform;
class PerfettoProducer;
class ProducerClient;
class SystemProducer;

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
    : public PerfettoTracingBackend::Delegate {
 public:
  // If not noted otherwise, a DataSourceBase's methods are only called on
  // PerfettoTracedProcess::GetTaskRunner()'s sequence.
  class COMPONENT_EXPORT(TRACING_CPP) DataSourceBase {
   public:
    explicit DataSourceBase(const std::string& name);
    virtual ~DataSourceBase();

    void StartTracingWithID(
        uint64_t data_source_id,
        PerfettoProducer* producer,
        const perfetto::DataSourceConfig& data_source_config);

    virtual void StartTracing(
        PerfettoProducer* producer,
        const perfetto::DataSourceConfig& data_source_config) = 0;
    // StopTracing must set |producer_| to nullptr before invoking the callback.
    // TODO(nuskos): Refactor this so that the implementation doesn't have to
    // remember to do this.
    virtual void StopTracing(
        base::OnceClosure stop_complete_callback = base::OnceClosure()) = 0;

    // Flush the data source.
    virtual void Flush(base::RepeatingClosure flush_complete_callback) = 0;

    virtual void ClearIncrementalState() {}

    // Enable startup tracing. The data source can use
    // SharedMemoryArbiter::CreateStartupTraceWriter() to begin tracing
    // immediately (the arbiter will buffer commits locally until connection to
    // the perfetto service is established). The data source should expect a
    // later call to StartTracing() to bind to the perfetto service, or a call
    // to AbortStartupTracing(). Called on any thread.
    virtual void SetupStartupTracing(
        PerfettoProducer* producer,
        const base::trace_event::TraceConfig& trace_config,
        bool privacy_filtering_enabled) {}

    // Cancel the tracing session started by SetupStartupTracing() after a
    // timeout. The data source should invalidate any startup trace buffer
    // reservations made with SharedMemoryArbiter.
    virtual void AbortStartupTracing() {}

    const std::string& name() const { return name_; }
    uint64_t data_source_id() const { return data_source_id_; }
    const PerfettoProducer* producer() const { return producer_; }

   protected:
    PerfettoProducer* producer_ = nullptr;

   private:
    uint64_t data_source_id_ = 0;
    std::string name_;
  };

  // Returns the process-wide instance of the PerfettoTracedProcess.
  static PerfettoTracedProcess* Get();

  // Provide a factory for lazily creating mojo consumer connections to the
  // tracing service. Allows using Perfetto's Client API for recording traces.
  using ConsumerConnectionFactory = mojom::TracingService& (*)();
  void SetConsumerConnectionFactory(ConsumerConnectionFactory,
                                    scoped_refptr<base::SequencedTaskRunner>);

  // Connect the current process to the mojo trace producer API. Depending on
  // the configuration, this will either set up the Perfetto Client API or the
  // legacy TraceLog to become the trace producer for this process.
  void ConnectProducer(mojo::PendingRemote<mojom::PerfettoService>);

  ProducerClient* producer_client() const;
  SystemProducer* system_producer() const;  // May be null.

  ~PerfettoTracedProcess() override;

  // Returns the task runner used by any Perfetto service. Can be called on any
  // thread.
  static PerfettoTaskRunner* GetTaskRunner();

  // Add a new data source to the PerfettoTracedProcess; the caller retains
  // ownership and is responsible for making sure the data source outlives the
  // PerfettoTracedProcess. Except for tests, this means the data source should
  // never be destroyed. Can be called on any thread.
  void AddDataSource(DataSourceBase*);
  // Returns a copy of the set of currently registered data sources. Can be
  // called on any thread.
  std::set<DataSourceBase*> data_sources();

  // Attempt to enable startup tracing for the current process and given
  // producer. Returns false on failure, e.g. because another concurrent tracing
  // session is already active. Can be called on any thread.
  bool SetupStartupTracing(PerfettoProducer*,
                           const base::trace_event::TraceConfig&,
                           bool privacy_filtering_enabled);

  // Initialize the Perfetto client library (i.e., perfetto::Tracing) for this
  // process. Should be called early during startup.
  void SetupClientLibrary();

  // Called on the process's main thread once the thread pool is ready.
  void OnThreadPoolAvailable();

  // Called to initialize system tracing, i.e., connecting to a system Perfetto
  // daemon as a producer. If |system_socket| isn't provided, Perfetto's default
  // socket name is used.
  void SetupSystemTracing(base::Optional<const char*> system_socket =
                              base::Optional<const char*>());

  // If the provided |producer| can begin tracing then |start_tracing| will be
  // invoked (unless cancelled by the Perfetto service) at some point later
  // using the GetTaskRunner()'s sequence and this function will return true.
  // Otherwise the return value will be false and start_tracing will not be
  // invoked at all. This function must be called on GetTaskRunners()'s
  // sequence.
  bool CanStartTracing(PerfettoProducer* producer,
                       base::OnceCallback<void()> start_tracing);

  // Can be called on any thread, but only after OnThreadPoolAvailable().
  void ActivateSystemTriggers(const std::vector<std::string>& triggers);

  // Be careful when using ResetTaskRunnerForTesting. There is a PostTask in the
  // constructor of PerfettoTracedProcess, so before this class is constructed
  // is the only safe time to call this.
  static void ResetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);

  // Sets the ProducerClient (or SystemProducer) and returns the old pointer. If
  // tests want to restore the state of the world they should store the pointer
  // and call this method again with it as the parameter when finished.
  std::unique_ptr<ProducerClient> SetProducerClientForTesting(
      std::unique_ptr<ProducerClient> client);
  std::unique_ptr<SystemProducer> SetSystemProducerForTesting(
      std::unique_ptr<SystemProducer> producer);

  void ClearDataSourcesForTesting();
  static void DeleteSoonForTesting(std::unique_ptr<PerfettoTracedProcess>);

  PerfettoPlatform* perfetto_platform_for_testing() const {
    return platform_.get();
  }

  // PerfettoTracingBackend::Delegate implementation.
  void CreateProducerConnection(
      base::OnceCallback<void(mojo::PendingRemote<mojom::PerfettoService>)>)
      override;
  void CreateConsumerConnection(
      base::OnceCallback<void(mojo::PendingRemote<mojom::ConsumerHost>)>)
      override;

 protected:
  // protected for testing.
  PerfettoTracedProcess();

 private:
  friend class base::NoDestructor<PerfettoTracedProcess>;

  base::Lock data_sources_lock_;
  // The canonical set of DataSourceBases alive in this process. These will be
  // registered with the tracing service.
  std::set<DataSourceBase*> data_sources_;

  // A PerfettoProducer that connects to the chrome Perfetto service through
  // mojo.
  std::unique_ptr<ProducerClient> producer_client_;
  // A PerfettoProducer that connects to the system Perfetto service. If there
  // is no system Perfetto service this pointer will be valid, but all function
  // calls will be noops.
  std::unique_ptr<SystemProducer> system_producer_;

  // Platform implementation for the Perfetto client library.
  std::unique_ptr<PerfettoPlatform> platform_;
  std::unique_ptr<PerfettoTracingBackend> tracing_backend_;

  scoped_refptr<base::SequencedTaskRunner> consumer_connection_task_runner_;
  ConsumerConnectionFactory consumer_connection_factory_;

  base::OnceCallback<void(mojo::PendingRemote<mojom::PerfettoService>)>
      pending_producer_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(PerfettoTracedProcess);
};
}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_
