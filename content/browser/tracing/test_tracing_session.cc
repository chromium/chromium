// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/test_tracing_session.h"

#include "base/task/thread_pool.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.gen.h"

namespace content {

TestTracingSession::TestTracingSession() = default;
TestTracingSession::~TestTracingSession() = default;

void TestTracingSession::Setup(const perfetto::TraceConfig& config, int fd) {
  if (!config.data_sources().empty()) {
    start_should_fail_ = config.data_sources()[0].config().name() == "Invalid";
    should_spuriously_stop = config.data_sources()[0].config().name() == "Stop";
  }
}

void TestTracingSession::Start() {
  if (start_should_fail_) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            [](std::function<void(perfetto::TracingError)>  // nocheck
                   on_error_callback) {
              on_error_callback(
                  {perfetto::TracingError::kTracingFailed, "error"});
            },
            on_error_callback_));
    return;
  }
  if (should_spuriously_stop) {
    Stop();
    return;
  }
  // perfetto::TracingSession runs callbacks from its own background thread.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](std::function<void()> on_start_callback) {  // nocheck
            on_start_callback();
          },
          on_start_callback_));
}

void TestTracingSession::StartBlocking() {
  NOTIMPLEMENTED();
}

void TestTracingSession::SetOnStartCallback(
    std::function<void()> on_start) {  // nocheck
  on_start_callback_ = on_start;
}

void TestTracingSession::SetOnErrorCallback(
    std::function<void(perfetto::TracingError)> on_error)  // nocheck
{
  on_error_callback_ = on_error;
}

void TestTracingSession::Flush(std::function<void(bool)>,  // nocheck
                               uint32_t timeout_ms) {
  NOTIMPLEMENTED();
}

void TestTracingSession::Stop() {
  // perfetto::TracingSession runs callbacks from its own background thread.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](std::function<void()> on_stop_callback) {  // nocheck
            on_stop_callback();
          },
          on_stop_callback_));
}

void TestTracingSession::CloneTrace(CloneTraceArgs args,
                                    CloneTraceCallback on_session_cloned) {
  // perfetto::TracingSession runs callbacks from its own background thread.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](CloneTraceCallback on_session_cloned) {  // nocheck
            on_session_cloned(
                {true, "", kClonedSessionId.low(), kClonedSessionId.high()});
          },
          on_session_cloned));
}

void TestTracingSession::StopBlocking() {
  NOTIMPLEMENTED();
}

void TestTracingSession::SetOnStopCallback(
    std::function<void()> on_stop) {  // nocheck
  on_stop_callback_ = on_stop;
}

void TestTracingSession::ChangeTraceConfig(const perfetto::TraceConfig&) {
  NOTIMPLEMENTED();
}
void TestTracingSession::ReadTrace(ReadTraceCallback read_callback) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](ReadTraceCallback read_callback) {  // nocheck
            std::string trace_content = "this is a trace";
            read_callback({trace_content.data(), trace_content.size(),
                           /*has_more=*/false});
          },
          read_callback));
}
void TestTracingSession::GetTraceStats(GetTraceStatsCallback stats_callback) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](GetTraceStatsCallback stats_callback) {  // nocheck
            GetTraceStatsCallbackArgs args;
            args.success = true;
            stats_callback(args);
          },
          stats_callback));
}

void TestTracingSession::QueryServiceState(QueryServiceStateCallback) {
  NOTIMPLEMENTED();
}

}  // namespace content
