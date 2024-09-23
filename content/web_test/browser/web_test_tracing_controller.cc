// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_tracing_controller.h"

#include "base/compiler_specific.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace content {

WebTestTracingController::WebTestTracingController(
    base::FilePath trace_file_path)
    : trace_file_path_(std::move(trace_file_path)) {}

WebTestTracingController::~WebTestTracingController() {
  CHECK(!tracing_file_.IsValid());
  // If tracing_session_ is still populated, it might be holding a reference to
  // function binding with a raw pointer to `this`.
  CHECK(!tracing_session_);
  CHECK(!stop_tracing_run_loop_.has_value());
  CHECK(!tracing_is_stopping_);
}

void WebTestTracingController::StartTracing() {
  CHECK(!tracing_session_);
  CHECK(!tracing_file_.IsValid());
  perfetto::TraceConfig trace_config =
      tracing::GetDefaultPerfettoConfig(base::trace_event::TraceConfig(
          "*", base::trace_event::RECORD_UNTIL_FULL));
  tracing_session_ =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
  // TODO(crbug.com/40736989): Perfetto does not (yet) support writing directly
  // to a file on Windows. For non-Windows, we pass an open file handle to the
  // tracing session initializer for incremental writes. For windows, the
  // tracing session will buffer all data in memory and we write out the
  // complete trace file only when tracing stops.
#if BUILDFLAG(IS_WIN)
  tracing_session_->Setup(trace_config);
#else
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    tracing_file_.Initialize(trace_file_path_, base::File::FLAG_CREATE_ALWAYS |
                                                   base::File::FLAG_WRITE);
  }
  tracing_session_->Setup(trace_config, tracing_file_.GetPlatformFile());
#endif
  scoped_refptr<base::SequencedTaskRunner> task_runner(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  // `this` can be bound because this object is freed only after a call to
  // this->StopTracing(), and that method cannot return before the posted
  // task has run. See WebTestControlHost::ResetBrowserAfterWebTest().
  tracing_session_->SetOnStopCallback([this, task_runner]() {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&WebTestTracingController::OnTracingStopped,
                                  base::Unretained(this)));
  });
  base::RunLoop run_loop;
  tracing_session_->SetOnStartCallback([&run_loop]() { run_loop.Quit(); });
  tracing_session_->Start();
  run_loop.Run();
}

void WebTestTracingController::StopTracing() {
  // It's possible that the tracing session was stopped prematurely due to an
  // error. If tracing_session_ is null, then TracingFinished() has already
  // run and there's nothing to do here.
  if (tracing_session_) {
    // StopTracing() should never be called twice.
    CHECK(!stop_tracing_run_loop_.has_value());
    stop_tracing_run_loop_.emplace();
    // This sequence can happen:
    //   - Tracing stops prematurely, OnTracingStopped() runs
    //     - OnTracingStopped() sets tracing_is_stopping_ to true,
    //       calls tracing_session_->ReadTrace(), and returns.
    //   - StopTracing() runs. Because the tracing session has already stopped,
    //     we don't need to call tracing_session_->Stop(). However, we can't
    //     call TracingFinished() from here; we have to wait for
    //     WriteTraceData() to do that.
    //
    // If that happens, then tracing_session_ will be non-null but
    // tracing_is_stopping_ will be true. The Run() call below will Quit()
    // from TracingFinished().
    if (!tracing_is_stopping_) {
      tracing_is_stopping_ = true;
      // This will cause OnTracingStopped to be (unconditionally) called.
      tracing_session_->Stop();
    }
    stop_tracing_run_loop_->Run();
    stop_tracing_run_loop_.reset();
  }
}

void WebTestTracingController::OnTracingStopped() {
#if BUILDFLAG(IS_WIN)
  tracing_is_stopping_ = true;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    tracing_file_.Initialize(trace_file_path_, base::File::FLAG_CREATE_ALWAYS |
                                                   base::File::FLAG_WRITE);
  }
  // `this` can be bound because this object is freed only after a call to
  // this->StopTracing(), that method cannot return before TracingFinished()
  // has run, and this binding can't run after TracingFinished().
  tracing_session_->ReadTrace(
      [this](perfetto::TracingSession::ReadTraceCallbackArgs args) {
        CHECK(tracing_file_.IsValid());
        if (args.size > 0) {
          UNSAFE_TODO(tracing_file_.WriteAtCurrentPos(args.data, args.size));
        }
        if (!args.has_more) {
          TracingFinished();
        }
      });
#else
  TracingFinished();
#endif
}

void WebTestTracingController::TracingFinished() {
  tracing_is_stopping_ = false;
  tracing_session_.reset();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    tracing_file_.Close();
  }
  tracing_file_ = base::File();
  if (stop_tracing_run_loop_.has_value()) {
    stop_tracing_run_loop_->Quit();
  }
}

}  // namespace content
