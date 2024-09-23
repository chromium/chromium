// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/cast_tracing_agent.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_config.h"
#include "chromecast/tracing/system_tracer.h"
#include "chromecast/tracing/system_tracing_common.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/system_trace_writer.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace content {
namespace {

std::string GetTracingCategories(
    const base::trace_event::TraceConfig& trace_config) {
  std::vector<std::string_view> categories;
  for (const char* category : chromecast::tracing::kCategories) {
    if (trace_config.category_filter().IsCategoryGroupEnabled(category)) {
      categories.push_back(category);
    }
  }
  return base::JoinString(categories, ",");
}

void DestroySystemTracerOnWorker(
    std::unique_ptr<chromecast::SystemTracer> tracer) {}

}  // namespace

class CastSystemTracingSession {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using TraceDataCallback =
      base::RepeatingCallback<void(chromecast::SystemTracer::Status status,
                                   std::string trace_data)>;

  CastSystemTracingSession(
      const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner)
      : worker_task_runner_(worker_task_runner) {
    DETACH_FROM_SEQUENCE(worker_sequence_checker_);
  }

  CastSystemTracingSession(const CastSystemTracingSession&) = delete;
  CastSystemTracingSession& operator=(const CastSystemTracingSession&) = delete;

  ~CastSystemTracingSession() {
    worker_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(&DestroySystemTracerOnWorker,
                                                 std::move(system_tracer_)));
  }

  // Begin tracing if configured in |config|. Calls |success_callback| on the
  // current sequence with |true| if tracing was started and |false| otherwise.
  void StartTracing(const std::string& config, SuccessCallback callback) {
    base::trace_event::TraceConfig trace_config(config);

    if (!trace_config.IsSystraceEnabled()) {
      std::move(callback).Run(false /* success */);
      return;
    }

    worker_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastSystemTracingSession::StartTracingOnWorker,
                       base::Unretained(this),
                       base::SequencedTaskRunner::GetCurrentDefault(),
                       GetTracingCategories(trace_config),
                       std::move(callback)));
  }

  // Stops the active tracing session, calls |callback| on the current sequence
  // at least once but possibly multiple times until all data was collected.
  void StopTracing(TraceDataCallback callback) {
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastSystemTracingSession::StopAndFlushOnWorker,
                       base::Unretained(this),
                       base::SequencedTaskRunner::GetCurrentDefault(),
                       callback));
  }

 private:
  void StartTracingOnWorker(scoped_refptr<base::TaskRunner> reply_task_runner,
                            const std::string& categories,
                            SuccessCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
    DCHECK(!is_tracing_);
    system_tracer_ = chromecast::SystemTracer::Create();
    system_tracer_->StartTracing(
        categories,
        base::BindOnce(&CastSystemTracingSession::FinishStartOnWorker,
                       base::Unretained(this), reply_task_runner,
                       std::move(callback)));
  }

  void FinishStartOnWorker(scoped_refptr<base::TaskRunner> reply_task_runner,
                           SuccessCallback callback,
                           chromecast::SystemTracer::Status status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
    is_tracing_ = status == chromecast::SystemTracer::Status::OK;
    if (callback) {
      reply_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), is_tracing_));
    }
  }

  void StopAndFlushOnWorker(scoped_refptr<base::TaskRunner> reply_task_runner,
                            TraceDataCallback callback) {
    if (!is_tracing_) {
      reply_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(callback, chromecast::SystemTracer::Status::OK,
                         std::string()));
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
    system_tracer_->StopTracing(base::BindRepeating(
        &CastSystemTracingSession::HandleTraceDataOnWorker,
        base::Unretained(this), reply_task_runner, std::move(callback)));
  }

  void HandleTraceDataOnWorker(
      scoped_refptr<base::TaskRunner> reply_task_runner,
      TraceDataCallback callback,
      chromecast::SystemTracer::Status status,
      std::string trace_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
    reply_task_runner->PostTask(
        FROM_HERE, base::BindOnce(callback, status, std::move(trace_data)));
  }

  SEQUENCE_CHECKER(worker_sequence_checker_);

  // Task runner for collecting traces in a worker thread.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  bool is_tracing_ = false;
  std::unique_ptr<chromecast::SystemTracer> system_tracer_;
};

namespace {

class CastDataSource : public tracing::PerfettoTracedProcess::DataSourceBase {
 public:
  static CastDataSource& GetInstance() {
    static base::NoDestructor<CastDataSource> instance;
    return *instance;
  }

  CastDataSource(const CastDataSource&) = delete;
  CastDataSource& operator=(const CastDataSource&) = delete;

  // Called from the tracing::PerfettoProducer on its sequence.
  void StartTracingImpl(
      tracing::PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
    DCHECK(!producer_);
    DCHECK(!session_);
    producer_ = producer;
    target_buffer_ = data_source_config.target_buffer();
    session_ = std::make_unique<CastSystemTracingSession>(worker_task_runner_);
    session_->StartTracing(data_source_config.chrome_config().trace_config(),
                           base::BindOnce(&CastDataSource::SystemTracerStarted,
                                          base::Unretained(this)));
  }

  // Called from the tracing::PerfettoProducer on its sequence.
  void StopTracingImpl(base::OnceClosure stop_complete_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
    DCHECK(session_);
    if (!session_started_) {
      session_started_callback_ =
          base::BindOnce(&CastDataSource::StopTracing, base::Unretained(this),
                         std::move(stop_complete_callback));
      return;
    }

    trace_writer_ = std::make_unique<SystemTraceWriter>(
        target_buffer_, SystemTraceWriter::TraceType::kFTrace);
    stop_complete_callback_ = std::move(stop_complete_callback);
    session_->StopTracing(base::BindRepeating(&CastDataSource::OnTraceData,
                                              base::Unretained(this)));
  }

  void Flush(base::RepeatingClosure flush_complete_callback) override {
    // Cast's SystemTracer doesn't currently support flushing while recording.
    flush_complete_callback.Run();
  }

 private:
  friend class base::NoDestructor<CastDataSource>;
  using DataSourceProxy =
      tracing::PerfettoTracedProcess::DataSourceProxy<CastDataSource>;
  using SystemTraceWriter =
      tracing::SystemTraceWriter<std::string, DataSourceProxy>;

  CastDataSource()
      : DataSourceBase(tracing::mojom::kSystemTraceDataSourceName),
        worker_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
    DETACH_FROM_SEQUENCE(perfetto_sequence_checker_);
    tracing::PerfettoTracedProcess::Get()->AddDataSource(this);
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name(tracing::mojom::kSystemTraceDataSourceName);
    DataSourceProxy::Register(dsd, this);
  }

  void SystemTracerStarted(bool success) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);
    session_started_ = true;
    if (session_started_callback_) {
      std::move(session_started_callback_).Run();
    }
  }

  void OnTraceData(chromecast::SystemTracer::Status status,
                   std::string trace_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(perfetto_sequence_checker_);

    if (!stop_complete_callback_) {
      return;
    }
    DCHECK(trace_writer_);
    DCHECK(session_);

    if (status != chromecast::SystemTracer::Status::FAIL) {
      trace_writer_->WriteData(trace_data);
    }

    if (status != chromecast::SystemTracer::Status::KEEP_GOING) {
      trace_writer_->Flush(base::BindOnce(&CastDataSource::OnTraceDataCommitted,
                                          base::Unretained(this)));
    }
  }

  void OnTraceDataCommitted() {
    DCHECK(stop_complete_callback_);
    trace_writer_.reset();
    session_.reset();
    session_started_ = false;
    producer_ = nullptr;
    std::move(stop_complete_callback_).Run();
  }

  SEQUENCE_CHECKER(perfetto_sequence_checker_);

  // Task runner for collecting traces in a worker thread.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  tracing::PerfettoProducer* producer_ = nullptr;
  std::unique_ptr<CastSystemTracingSession> session_;
  bool session_started_ = false;
  base::OnceClosure session_started_callback_;
  std::unique_ptr<SystemTraceWriter> trace_writer_;
  base::OnceClosure stop_complete_callback_;
  uint32_t target_buffer_ = 0;
};

}  // namespace

CastTracingAgent::CastTracingAgent() {
  tracing::PerfettoTracedProcess::Get()->AddDataSource(
      &CastDataSource::GetInstance());
}

CastTracingAgent::~CastTracingAgent() = default;

void CastTracingAgent::GetCategories(std::set<std::string>* category_set) {
  for (const char* category : chromecast::tracing::kCategories) {
    category_set->insert(category);
  }
}

}  // namespace content
