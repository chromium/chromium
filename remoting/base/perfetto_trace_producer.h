// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PERFETTO_TRACE_PRODUCER_H_
#define REMOTING_BASE_PERFETTO_TRACE_PRODUCER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "remoting/base/perfetto_trace_config.h"

namespace remoting {

// Manages an in-process Perfetto `TracingSession` that records `TrackEvent`s
// strictly in memory (zero temporary disk I/O) and returns the serialized
// `.pftrace` bytes asynchronously.
//
// Mojo-free so it can be embedded directly in standalone CLI benchmarks as well
// as wrapped by `mojom::TraceProducer` in multi-process Me2Me hosts.
//
// Sequence-affine: all methods on `PerfettoTraceProducer` and its returned
// `Session` handles (including `~PerfettoTraceProducer()` and `~Session()`)
// must be called on the sequence where `PerfettoTraceProducer` was constructed.
class PerfettoTraceProducer {
 public:
  // Invoked once on the producer's sequence when a started trace session
  // finishes flushing and reading `.pftrace` packets. Not invoked if
  // `StartTracing()` returns `nullptr`.
  using TraceDataCallback = base::OnceCallback<void(std::vector<uint8_t>)>;

  // RAII handle controlling an active trace session. Must be used and destroyed
  // on the owning `PerfettoTraceProducer`'s sequence.
  //
  // Note: `Cancel()` and automatic cancellation in `~Session()` when dropped
  // while recording are `NOTIMPLEMENTED()` stubs in this CL and are implemented
  // in the follow-up CL; callers must invoke `StopAndFlush()` before dropping
  // the handle.
  class Session {
   public:
    virtual ~Session() = default;

    // Stops recording, flushes all `TrackEvent` writers, reads the in-memory
    // `.pftrace` buffer, and invokes `TraceDataCallback`. Idempotent if called
    // more than once.
    virtual void StopAndFlush() = 0;

    // Immediately aborts the session (even if a flush is in flight) and
    // discards any buffered trace data without invoking `TraceDataCallback`.
    virtual void Cancel() = 0;
  };

  PerfettoTraceProducer();
  PerfettoTraceProducer(const PerfettoTraceProducer&) = delete;
  PerfettoTraceProducer& operator=(const PerfettoTraceProducer&) = delete;
  ~PerfettoTraceProducer();

  // Starts an in-memory trace session. Returns `nullptr` if Perfetto has not
  // been initialized, if a session is already active on this producer, or if
  // `config` is invalid.
  //
  // Precondition: `PerfettoPlatform::Get().task_runner()` must be actively
  // executing tasks (for example, `base::ThreadPoolInstance` must be started
  // and not in `ThreadPoolExecutionMode::QUEUED`, and the caller must not block
  // the sole `ThreadPool` worker), because off-sequence startup blocks on a
  // Perfetto `WaitableEvent` signaled from that sequence.
  std::unique_ptr<Session> StartTracing(const PerfettoTraceConfig& config,
                                        TraceDataCallback callback);

  // Returns true while a session is recording or flushing on this producer.
  bool IsTracing() const;

 private:
  class ActiveSessionImpl;
  class SessionHandle;

  void ClearActiveSession(ActiveSessionImpl* session);

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<ActiveSessionImpl> active_session_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PERFETTO_TRACE_PRODUCER_H_
