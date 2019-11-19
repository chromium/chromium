// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/dummy_producer.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/tracing_features.h"

#if defined(OS_ANDROID)
// As per 'gn help check':
/*
  If you have conditional includes, make sure the build conditions and the
  preprocessor conditions match, and annotate the line with "nogncheck" (see
  "gn help nogncheck" for an example).
*/
// We add the nogncheck to ensure this doesn't trigger incorrect errors on
// non-android builds.
#include "services/tracing/public/cpp/perfetto/android_system_producer.h"  // nogncheck
#include "third_party/perfetto/include/perfetto/ext/tracing/ipc/default_socket.h"  // nogncheck
#endif  // defined(OS_ANDROID)

namespace tracing {
namespace {
std::unique_ptr<SystemProducer> NewSystemProducer(PerfettoTaskRunner* runner,
                                                  const char* socket_name) {
#if defined(OS_ANDROID)
  if (ShouldSetupSystemTracing()) {
    DCHECK(socket_name);
    return std::make_unique<AndroidSystemProducer>(socket_name, runner);
  }
#endif  // defined(OS_ANDROID)
  return std::make_unique<DummyProducer>(runner);
}

const char* MaybeSocket() {
#if defined(OS_ANDROID)
  return perfetto::GetProducerSocket();
#else
  return nullptr;
#endif  // defined(OS_ANDROID)
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
  DCHECK(!producer_) << name_;
  producer_ = producer;
  StartTracing(producer_, data_source_config);
}

// static
PerfettoTracedProcess* PerfettoTracedProcess::Get() {
  static base::NoDestructor<PerfettoTracedProcess> traced_process;
  return traced_process.get();
}

PerfettoTracedProcess::PerfettoTracedProcess()
    : PerfettoTracedProcess(MaybeSocket()) {}

PerfettoTracedProcess::PerfettoTracedProcess(const char* system_socket)
    : producer_client_(std::make_unique<ProducerClient>(GetTaskRunner())) {
  CHECK(IsTracingInitialized());
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // All communication with the system Perfetto service should occur on a single
  // sequence. To ensure we set up the socket correctly we construct the
  // |system_producer_endpoint_| on the task runner it will use.
  GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PerfettoTracedProcess* ptr, const char* socket) {
                       ptr->system_producer_endpoint_ =
                           NewSystemProducer(GetTaskRunner(), socket);
                     },
                     base::Unretained(this), system_socket));
}

PerfettoTracedProcess::~PerfettoTracedProcess() {}

void PerfettoTracedProcess::ClearDataSourcesForTesting() {
  base::RunLoop source_cleared;
  GetTaskRunner()->GetOrCreateTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](PerfettoTracedProcess* traced_process) {
            traced_process->data_sources_.clear();
          },
          base::Unretained(this)),
      source_cleared.QuitClosure());
  source_cleared.Run();
}

std::unique_ptr<ProducerClient>
PerfettoTracedProcess::SetProducerClientForTesting(
    std::unique_ptr<ProducerClient> client) {
  auto old_producer_client_for_testing = std::move(producer_client_);
  producer_client_ = std::move(client);
  return old_producer_client_for_testing;
}

std::unique_ptr<SystemProducer>
PerfettoTracedProcess::SetSystemProducerForTesting(
    std::unique_ptr<SystemProducer> producer) {
  auto old_for_testing = std::move(system_producer_endpoint_);
  system_producer_endpoint_ = std::move(producer);
  return old_for_testing;
}

// static
void PerfettoTracedProcess::DeleteSoonForTesting(
    std::unique_ptr<PerfettoTracedProcess> perfetto_traced_process) {
  GetTaskRunner()->GetOrCreateTaskRunner()->DeleteSoon(
      FROM_HERE, std::move(perfetto_traced_process));
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
        auto* producer =
            PerfettoTracedProcess::Get()->SystemProducerForTesting();
        CHECK(producer);
        producer->ResetSequenceForTesting();
      }));
}

// static
void PerfettoTracedProcess::ReconstructForTesting(const char* socket_name) {
  base::RunLoop finished_reconstruction_runloop;
  // The Get() call ensures that the construct has run and any required tasks
  // have been completed before this lambda below is executed.
  Get()->GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure on_finish, const char* socket_name) {
            PerfettoTracedProcess::Get()->~PerfettoTracedProcess();
            new (PerfettoTracedProcess::Get())
                PerfettoTracedProcess(socket_name);
            // The constructor and destructor needs to be run on the proper
            // sequence, but the constructor also calls PostTask so we need to
            // place a task afterwards to block until everything is initialized.
            GetTaskRunner()->GetOrCreateTaskRunner()->PostTaskAndReply(
                FROM_HERE, base::DoNothing(), std::move(on_finish));
          },
          finished_reconstruction_runloop.QuitClosure(), socket_name));
  finished_reconstruction_runloop.Run();
}

void PerfettoTracedProcess::AddDataSource(DataSourceBase* data_source) {
  GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&PerfettoTracedProcess::AddDataSourceOnSequence,
                                base::Unretained(this), data_source));
}

const std::set<PerfettoTracedProcess::DataSourceBase*>&
PerfettoTracedProcess::data_sources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_sources_;
}

bool PerfettoTracedProcess::CanStartTracing(
    PerfettoProducer* producer,
    base::OnceCallback<void()> start_tracing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the |producer| asking is the local producer_client_ it has priority so
  // even if the other endpoint is tracing shut down the other endpoint and let
  // the |producer_client_| go. The system Producer will periodically attempt to
  // reconnect if we call DisconnectWithReply().
  if (producer == producer_client_.get()) {
    if (system_producer_endpoint_->IsTracingActive()) {
      system_producer_endpoint_->DisconnectWithReply(std::move(start_tracing));
      return true;
    }
  } else if (producer == system_producer_endpoint_.get()) {
    if (producer_client_->IsTracingActive()) {
      system_producer_endpoint_->DisconnectWithReply(base::DoNothing().Once());
      return false;
    }
  } else {
    // In tests this is possible due to the periodic polling of CanStartTracing
    // by the AndroidSystemProducer, when we swap it out for a
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
  DCHECK(system_producer_endpoint_.get());
  if (!GetTaskRunner()->GetOrCreateTaskRunner()->RunsTasksInCurrentSequence()) {
    GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PerfettoTracedProcess::ActivateSystemTriggers,
                       base::Unretained(this), triggers));
    return;
  }
  system_producer_endpoint_->ActivateTriggers(triggers);
}

ProducerClient* PerfettoTracedProcess::producer_client() {
  return producer_client_.get();
}

SystemProducer* PerfettoTracedProcess::SystemProducerForTesting() {
  return system_producer_endpoint_.get();
}

void PerfettoTracedProcess::AddDataSourceOnSequence(
    DataSourceBase* data_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data_sources_.insert(data_source).second) {
    DCHECK(producer_client_);
    DCHECK(system_producer_endpoint_);
    producer_client_->NewDataSourceAdded(data_source);
    system_producer_endpoint_->NewDataSourceAdded(data_source);
  }
}
}  // namespace tracing
