// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/perfetto_trace_producer.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/perfetto_platform.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace remoting {

namespace {

constexpr uint32_t kFlushTimeoutMs = 2000;
constexpr base::TimeDelta kDefaultReadTimeout = base::Seconds(2);

}  // namespace

class PerfettoTraceProducer::ActiveSessionImpl {
 public:
  ActiveSessionImpl(PerfettoTraceProducer* producer,
                    const PerfettoTraceConfig& config,
                    TraceDataCallback callback);
  ActiveSessionImpl(const ActiveSessionImpl&) = delete;
  ActiveSessionImpl& operator=(const ActiveSessionImpl&) = delete;
  ~ActiveSessionImpl();

  base::WeakPtr<ActiveSessionImpl> GetWeakPtr();

  // Starts the underlying Perfetto `TracingSession`.
  bool Start();

  void StopAndFlush();
  void Cancel();
  void CancelIfRecording();

 private:
  enum class State {
    kRecording,
    kFlushing,
    kStopping,
    kReading,
  };

  void OnTracingError(perfetto::TracingError::ErrorCode code,
                      std::string message);
  void OnFlushComplete(bool flush_succeeded);
  void OnStopComplete();
  void StartReading();
  void OnTraceChunk(std::vector<uint8_t> chunk, bool has_more);
  void OnReadTimedOut();
  void FinishAndNotify();

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<PerfettoTraceProducer> producer_;
  PerfettoTraceConfig config_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  TraceDataCallback callback_;
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  base::OneShotTimer timer_;
  State state_ = State::kRecording;
  std::vector<uint8_t> trace_bytes_;
  base::WeakPtrFactory<ActiveSessionImpl> weak_factory_{this};
};

PerfettoTraceProducer::ActiveSessionImpl::ActiveSessionImpl(
    PerfettoTraceProducer* producer,
    const PerfettoTraceConfig& config,
    TraceDataCallback callback)
    : producer_(producer),
      config_(config),
      origin_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      callback_(std::move(callback)) {}

PerfettoTraceProducer::ActiveSessionImpl::~ActiveSessionImpl() = default;

base::WeakPtr<PerfettoTraceProducer::ActiveSessionImpl>
PerfettoTraceProducer::ActiveSessionImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool PerfettoTraceProducer::ActiveSessionImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto platform_task_runner =
      base::tracing::PerfettoPlatform::Get().task_runner();
  if (platform_task_runner &&
      platform_task_runner->RunsTasksInCurrentSequence()) {
    LOG(ERROR) << "Starting Perfetto trace on the platform sequence is not "
                  "supported.";
    return false;
  }

  tracing_session_ = perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
  tracing_session_->Setup(config_.ToPerfettoConfig());

  auto weak_self = weak_factory_.GetWeakPtr();
  auto task_runner = origin_task_runner_;
  tracing_session_->SetOnErrorCallback([task_runner, weak_self](
                                           perfetto::TracingError error) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&ActiveSessionImpl::OnTracingError, weak_self,
                                  error.code, std::move(error.message)));
  });

  tracing_session_->StartBlocking();

  if (config_.duration.is_positive()) {
    timer_.Start(FROM_HERE, config_.duration,
                 base::BindOnce(&ActiveSessionImpl::StopAndFlush,
                                base::Unretained(this)));
  }
  return true;
}

void PerfettoTraceProducer::ActiveSessionImpl::StopAndFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kRecording) {
    return;
  }
  state_ = State::kFlushing;
  timer_.Stop();

  base::TrackEvent::Flush();

  auto weak_self = weak_factory_.GetWeakPtr();
  auto task_runner = origin_task_runner_;
  tracing_session_->Flush(
      [task_runner, weak_self](bool flush_succeeded) {
        task_runner->PostTask(
            FROM_HERE, base::BindOnce(&ActiveSessionImpl::OnFlushComplete,
                                      weak_self, flush_succeeded));
      },
      kFlushTimeoutMs);
}

void PerfettoTraceProducer::ActiveSessionImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
  // Move `callback_` to a local so destroying bound state cannot re-entrantly
  // destroy `this` before `ClearActiveSession()` runs.
  TraceDataCallback discarded_callback = std::move(callback_);
  producer_->ClearActiveSession(this);
}

void PerfettoTraceProducer::ActiveSessionImpl::CancelIfRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kRecording) {
    Cancel();
  }
}

void PerfettoTraceProducer::ActiveSessionImpl::OnTracingError(
    perfetto::TracingError::ErrorCode code,
    std::string message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(WARNING) << "Perfetto tracing session error: " << message;
  if (code == perfetto::TracingError::kDisconnected) {
    // The consumer is gone, so nothing further can be read.
    FinishAndNotify();
    return;
  }
  if (state_ == State::kReading) {
    // A read is already in flight and is bounded by the read timeout.
    return;
  }
  // The session has stopped due to an error; make a best effort to read
  // whatever data remains buffered. Skip any pending flush/stop handshake
  // since Perfetto may drop a pending `Flush()` callback if the session was
  // torn down.
  StartReading();
}

void PerfettoTraceProducer::ActiveSessionImpl::OnFlushComplete(
    bool flush_succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kFlushing) {
    return;
  }
  if (!flush_succeeded) {
    LOG(WARNING) << "Perfetto trace flush timed out; reading partial buffer.";
  }
  state_ = State::kStopping;

  // Perfetto bounds `Stop()` with `TraceConfig::data_source_stop_timeout_ms`,
  // scraping any remaining shared-memory chunks before firing the callback.
  auto weak_self = weak_factory_.GetWeakPtr();
  auto task_runner = origin_task_runner_;
  tracing_session_->SetOnStopCallback([task_runner, weak_self]() {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&ActiveSessionImpl::OnStopComplete, weak_self));
  });
  tracing_session_->Stop();
}

void PerfettoTraceProducer::ActiveSessionImpl::OnStopComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kStopping) {
    return;
  }
  StartReading();
}

void PerfettoTraceProducer::ActiveSessionImpl::StartReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Perfetto supports only one read at a time per session.
  DCHECK(state_ != State::kReading);
  state_ = State::kReading;
  timer_.Stop();

  // Guard against a chunk never arriving. Re-armed as each chunk is received.
  if (!producer_->read_timeout_.is_max()) {
    timer_.Start(FROM_HERE, producer_->read_timeout_,
                 base::BindOnce(&ActiveSessionImpl::OnReadTimedOut,
                                base::Unretained(this)));
  }

  auto weak_self = weak_factory_.GetWeakPtr();
  auto task_runner = origin_task_runner_;
  tracing_session_->ReadTrace(
      [task_runner,
       weak_self](perfetto::TracingSession::ReadTraceCallbackArgs args) {
        std::vector<uint8_t> chunk;
        if (args.data && args.size > 0) {
          // SAFETY: Perfetto's `ReadTraceCallbackArgs` guarantees
          // `[args.data, args.data + args.size)` is a valid contiguous buffer
          // for the duration of this callback.
          auto bytes = UNSAFE_BUFFERS(base::span(
              reinterpret_cast<const uint8_t*>(args.data), args.size));
          chunk.assign(bytes.begin(), bytes.end());
        }
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&ActiveSessionImpl::OnTraceChunk, weak_self,
                           std::move(chunk), args.has_more));
      });
}

void PerfettoTraceProducer::ActiveSessionImpl::OnTraceChunk(
    std::vector<uint8_t> chunk,
    bool has_more) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kReading) {
    return;
  }
  if (!chunk.empty()) {
    if (trace_bytes_.empty()) {
      trace_bytes_ = std::move(chunk);
    } else {
      trace_bytes_.insert(trace_bytes_.end(), chunk.begin(), chunk.end());
    }
  }
  if (has_more) {
    if (timer_.IsRunning()) {
      timer_.Reset();
    }
    return;
  }
  FinishAndNotify();
}

void PerfettoTraceProducer::ActiveSessionImpl::OnReadTimedOut() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kReading) {
    return;
  }
  LOG(WARNING) << "Perfetto trace read timed out; completing with the data "
                  "received so far.";
  FinishAndNotify();
}

void PerfettoTraceProducer::ActiveSessionImpl::FinishAndNotify() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();

  // Release producer ownership *before* running `callback_` so callers (such
  // as flight-recorder snapshot harvesting) can immediately start a new
  // session from within `callback_`.
  std::vector<uint8_t> completed_bytes = std::move(trace_bytes_);
  TraceDataCallback completed_callback = std::move(callback_);
  producer_->ClearActiveSession(this);
  if (completed_callback) {
    std::move(completed_callback).Run(std::move(completed_bytes));
  }
}

class PerfettoTraceProducer::SessionHandle
    : public PerfettoTraceProducer::Session {
 public:
  explicit SessionHandle(base::WeakPtr<ActiveSessionImpl> impl);
  SessionHandle(const SessionHandle&) = delete;
  SessionHandle& operator=(const SessionHandle&) = delete;
  ~SessionHandle() override;

  // PerfettoTraceProducer::Session:
  void StopAndFlush() override;
  void Cancel() override;

 private:
  base::WeakPtr<ActiveSessionImpl> impl_;
};

PerfettoTraceProducer::SessionHandle::SessionHandle(
    base::WeakPtr<ActiveSessionImpl> impl)
    : impl_(std::move(impl)) {}

PerfettoTraceProducer::SessionHandle::~SessionHandle() {
  if (impl_) {
    impl_->CancelIfRecording();
  }
}

void PerfettoTraceProducer::SessionHandle::StopAndFlush() {
  if (impl_) {
    impl_->StopAndFlush();
  }
}

void PerfettoTraceProducer::SessionHandle::Cancel() {
  if (impl_) {
    impl_->Cancel();
  }
}

PerfettoTraceProducer::PerfettoTraceProducer()
    : read_timeout_(kDefaultReadTimeout) {}

PerfettoTraceProducer::~PerfettoTraceProducer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<PerfettoTraceProducer::Session>
PerfettoTraceProducer::StartTracing(const PerfettoTraceConfig& config,
                                    TraceDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!perfetto::Tracing::IsInitialized()) {
    LOG(ERROR) << "Cannot start Perfetto trace before InitPerfettoTracing().";
    return nullptr;
  }
  if (active_session_ || !config.IsValid()) {
    return nullptr;
  }

  active_session_ =
      std::make_unique<ActiveSessionImpl>(this, config, std::move(callback));
  if (!active_session_->Start()) {
    active_session_.reset();
    return nullptr;
  }
  return std::make_unique<SessionHandle>(active_session_->GetWeakPtr());
}

bool PerfettoTraceProducer::IsTracing() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return active_session_ != nullptr;
}

void PerfettoTraceProducer::SetReadTimeoutForTesting(base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  read_timeout_ = timeout;
}

void PerfettoTraceProducer::ClearActiveSession(ActiveSessionImpl* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (active_session_.get() == session) {
    active_session_.reset();
  }
}

}  // namespace remoting
