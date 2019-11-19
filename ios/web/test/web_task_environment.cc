// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/web_task_environment.h"

#include <memory>

#include "base/run_loop.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/web_thread_impl.h"

namespace web {

WebTaskEnvironment::WebTaskEnvironment(int options)
    : base::test::TaskEnvironment(options == IO_MAINLOOP ? MainThreadType::IO
                                                         : MainThreadType::UI) {
  Init(options);
}

WebTaskEnvironment::~WebTaskEnvironment() {
  // To ensure a clean teardown, each thread's message loop must be flushed
  // just before the thread is destroyed. But stopping a fake thread does not
  // automatically flush the message loop, so do it manually.
  // See http://crbug.com/247525 for discussion.
  base::RunLoop().RunUntilIdle();
  io_thread_->Stop();
  base::RunLoop().RunUntilIdle();
  ui_thread_->Stop();
  base::RunLoop().RunUntilIdle();

  // This is required to ensure that all remaining MessageLoop and ThreadPool
  // tasks run in an atomic step. This is a bit different than production where
  // the main thread is not flushed after it's done running but this approach is
  // preferred in unit tests as running more tasks can merely uncover more
  // issues (e.g. if a bad tasks is posted but never blocked upon it could make
  // a test flaky whereas by flushing, the test will always fail).
  RunUntilIdle();

  WebThreadImpl::ResetTaskExecutorForTesting();
}

void WebTaskEnvironment::Init(int options) {
  WebThreadImpl::CreateTaskExecutor();

  ui_thread_ =
      std::make_unique<TestWebThread>(WebThread::UI, GetMainThreadTaskRunner());

  if (options & WebTaskEnvironment::REAL_IO_THREAD) {
    io_thread_ = std::make_unique<TestWebThread>(WebThread::IO);
    io_thread_->StartIOThread();
  } else {
    io_thread_ = std::make_unique<TestWebThread>(WebThread::IO,
                                                 GetMainThreadTaskRunner());
  }
}

}  // namespace web
