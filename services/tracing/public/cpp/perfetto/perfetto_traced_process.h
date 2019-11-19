// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"

namespace tracing {

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
class COMPONENT_EXPORT(TRACING_CPP) PerfettoTracedProcess final {
 public:
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

  ProducerClient* producer_client();
  SystemProducer* SystemProducerForTesting();

  ~PerfettoTracedProcess();

  // Sets the ProducerClient (or SystemProducer) and returns the old pointer. If
  // tests want to restore the state of the world they should store the pointer
  // and call this method again with it as the parameter when finished.
  std::unique_ptr<ProducerClient> SetProducerClientForTesting(
      std::unique_ptr<ProducerClient> client);
  std::unique_ptr<SystemProducer> SetSystemProducerForTesting(
      std::unique_ptr<SystemProducer> producer);
  void ClearDataSourcesForTesting();
  static void DeleteSoonForTesting(std::unique_ptr<PerfettoTracedProcess>);

  // Returns the taskrunner used by any Perfetto service.
  static PerfettoTaskRunner* GetTaskRunner();

  // Add a new data source to the PerfettoTracedProcess; the caller retains
  // ownership and is responsible for making sure the data source outlives the
  // PerfettoTracedProcess.
  void AddDataSource(DataSourceBase*);
  // Can only be called on GetTaskRunner()'s sequence.
  const std::set<DataSourceBase*>& data_sources();

  // If the provided |producer| can begin tracing then |start_tracing| will be
  // invoked (unless cancelled by the Perfetto service) at some point later
  // using the GetTaskRunner()'s sequence and this function will return true.
  // Otherwise the return value will be false and start_tracing will not be
  // invoked at all. This function must be called on GetTaskRunners()'s
  // sequence.
  bool CanStartTracing(PerfettoProducer* producer,
                       base::OnceCallback<void()> start_tracing);

  void ActivateSystemTriggers(const std::vector<std::string>& triggers);

  // Be careful when using ResetTaskRunnerForTesting. There is a PostTask in the
  // constructor of PerfettoTracedProcess, so before this class is constructed
  // is the only safe time to call this.
  static void ResetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);

  static void ReconstructForTesting(const char* system_socket);

 protected:
  // protected for testing.
  PerfettoTracedProcess();
  explicit PerfettoTracedProcess(const char* system_socket);

 private:
  friend class base::NoDestructor<PerfettoTracedProcess>;

  void AddDataSourceOnSequence(DataSourceBase* data_source);

  // The canonical set of DataSourceBases alive in this process. These will be
  // registered with the tracing service.
  std::set<DataSourceBase*> data_sources_;
  // A PerfettoProducer that connects to the chrome Perfetto service through
  // mojo.
  std::unique_ptr<ProducerClient> producer_client_;
  // A PerfettoProducer that connects to the system Perfetto service. If there
  // is no system Perfetto service this pointer will be valid, but all function
  // calls will be noops.
  std::unique_ptr<SystemProducer> system_producer_endpoint_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(PerfettoTracedProcess);
};
}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACED_PROCESS_H_
