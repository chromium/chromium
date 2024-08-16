// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/web_task_environment.h"

#include <memory>

#include "base/notreached.h"
#include "base/run_loop.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/web_thread_impl.h"

namespace web {
namespace {

// Returns the base::TaskEnvironment::MainThreadType corresponding to
// `main_thread_type`.
base::test::TaskEnvironment::MainThreadType ConvertMainThreadType(
    WebTaskEnvironment::MainThreadType main_thread_type) {
  switch (main_thread_type) {
    case WebTaskEnvironment::MainThreadType::UI:
      return base::test::TaskEnvironment::MainThreadType::UI;

    case WebTaskEnvironment::MainThreadType::IO:
      return base::test::TaskEnvironment::MainThreadType::IO;
  }

  NOTREACHED();
}

}  // namespace

WebTaskEnvironment::WebTaskEnvironment(TimeSource time_source,
                                       MainThreadType main_thread_type,
                                       IOThreadType io_thread_type,
                                       base::trait_helpers::NotATraitTag tag)
    : TaskEnvironment(time_source, ConvertMainThreadType(main_thread_type)),
      io_thread_type_(io_thread_type) {
  WebThreadImpl::CreateTaskExecutor();

  ui_thread_ =
      std::make_unique<TestWebThread>(WebThread::UI, GetMainThreadTaskRunner());

  if (io_thread_type_ != IOThreadType::REAL_THREAD_DELAYED) {
    StartIOThreadInternal();
  }
}

WebTaskEnvironment::~WebTaskEnvironment() {
  // To ensure a clean teardown, each thread's message loop must be flushed
  // just before the thread is destroyed. But stopping a fake thread does not
  // automatically flush the message loop, so do it manually.
  // See http://crbug.com/247525 for discussion.
  base::RunLoop().RunUntilIdle();
  if (io_thread_) {
    io_thread_->Stop();
  }

  base::RunLoop().RunUntilIdle();
  ui_thread_->Stop();
  base::RunLoop().RunUntilIdle();

  // This is required to ensure that all remaining MessageLoop and ThreadPool
  // tasks run in an atomic step. This is a bit different than production
  // where the main thread is not flushed after it's done running but this
  // approach is preferred in unit tests as running more tasks can merely
  // uncover more issues (e.g. if a bad tasks is posted but never blocked upon
  // it could make a test flaky whereas by flushing, the test will always
  // fail).
  RunUntilIdle();

  WebThreadImpl::ResetTaskExecutorForTesting();
}

void WebTaskEnvironment::StartIOThread() {
  DCHECK_EQ(io_thread_type_, IOThreadType::REAL_THREAD_DELAYED);
  StartIOThreadInternal();
}

void WebTaskEnvironment::StartIOThreadInternal() {
  DCHECK(!io_thread_);

  if (io_thread_type_ == IOThreadType::FAKE_THREAD) {
    io_thread_ = std::make_unique<TestWebThread>(WebThread::IO,
                                                 GetMainThreadTaskRunner());
  } else {
    io_thread_ = std::make_unique<TestWebThread>(WebThread::IO);
    io_thread_->StartIOThread();
  }
}

}  // namespace web
