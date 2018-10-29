// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/after_startup_task_utils.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

namespace {

base::test::ScopedTaskEnvironment::MainThreadType GetThreadTypeFromOptions(
    int options) {
  if (options & TestBrowserThreadBundle::PLAIN_MAINLOOP)
    return base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT;
  if (options & TestBrowserThreadBundle::IO_MAINLOOP)
    return base::test::ScopedTaskEnvironment::MainThreadType::IO;
  return base::test::ScopedTaskEnvironment::MainThreadType::UI;
}

}  // namespace

TestBrowserThreadBundle::TestBrowserThreadBundle()
    : TestBrowserThreadBundle(DEFAULT) {}

TestBrowserThreadBundle::TestBrowserThreadBundle(int options)
    : options_(options), threads_created_(false) {
  Init();
}

TestBrowserThreadBundle::~TestBrowserThreadBundle() {
  CHECK(threads_created_);

  // To ensure a clean teardown, each thread's message loop must be flushed
  // just before the thread is destroyed. But stopping a fake thread does not
  // automatically flush the message loop, so we have to do it manually.
  // See http://crbug.com/247525 for discussion.
  base::RunLoop().RunUntilIdle();
  io_thread_->Stop();
  base::RunLoop().RunUntilIdle();
  ui_thread_->Stop();
  base::RunLoop().RunUntilIdle();

  // Skip the following step when TaskScheduler isn't managed by this
  // TestBrowserThreadBundle, otherwise it can hang (e.g.
  // RunAllTasksUntilIdle() hangs when the TaskScheduler is managed
  // by an external ScopedTaskEnvironment with ExecutionMode::QUEUED). This is
  // fine as (1) it's rare and (2) it mimics production where BrowserThreads are
  // shutdown before TaskScheduler.
  if (scoped_task_environment_) {
    // This is required to ensure we run all remaining MessageLoop and
    // TaskScheduler tasks in an atomic step. This is a bit different than
    // production where the main thread is not flushed after it's done running
    // but this approach is preferred in unit tests as running more tasks can
    // merely uncover more issues (e.g. if a bad tasks is posted but never
    // blocked upon it could make a test flaky whereas by flushing we guarantee
    // it will blow up).
    RunAllTasksUntilIdle();
    CHECK(!scoped_task_environment_->MainThreadHasPendingTask());
  }

  BrowserTaskExecutor::ResetForTesting();

  // |scoped_task_environment_| needs to explicitly go away before fake threads
  // in order for DestructionObservers hooked to the main MessageLoop to be able
  // to invoke BrowserThread::CurrentlyOn() -- ref. ~TestBrowserThread().
  scoped_task_environment_.reset();

#if defined(OS_WIN)
  com_initializer_.reset();
#endif
}

void TestBrowserThreadBundle::Init() {
  // Check that the UI thread hasn't already been initialized. This will fail if
  // multiple TestBrowserThreadBundles are initialized in the same scope.
  CHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI));

  // Check for conflicting options can't have two IO threads.
  CHECK(!(options_ & IO_MAINLOOP) || !(options_ & REAL_IO_THREAD));
  // There must be a thread to start to use DONT_CREATE_BROWSER_THREADS
  CHECK((options_ & ~IO_MAINLOOP) != DONT_CREATE_BROWSER_THREADS);
  // Check for conflicting main loop options.
  CHECK(!(options_ & IO_MAINLOOP) || !(options_ & PLAIN_MAINLOOP));

#if defined(OS_WIN)
  // Similar to Chrome's UI thread, we need to initialize COM separately for
  // this thread as we don't call Start() for the UI TestBrowserThread; it's
  // already started!
  com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
  CHECK(com_initializer_->Succeeded());
#endif

  BrowserTaskExecutor::Create();

  // Create the ScopedTaskEnvironment if it doesn't already exist. A
  // ScopedTaskEnvironment may already exist if this TestBrowserThreadBundle is
  // instantiated in a test whose parent fixture provides a
  // ScopedTaskEnvironment.
  if (!base::ThreadTaskRunnerHandle::IsSet()) {
    scoped_task_environment_ =
        std::make_unique<base::test::ScopedTaskEnvironment>(
            GetThreadTypeFromOptions(options_));
  }
  if (options_ & IO_MAINLOOP)
    CHECK(base::MessageLoopCurrentForIO::IsSet());
  else if (!(options_ & PLAIN_MAINLOOP))
    CHECK(base::MessageLoopCurrentForUI::IsSet());

  // Set the current thread as the UI thread.
  ui_thread_ = std::make_unique<TestBrowserThread>(
      BrowserThread::UI, base::ThreadTaskRunnerHandle::Get());

  if (!(options_ & DONT_CREATE_BROWSER_THREADS))
    CreateBrowserThreads();
}

void TestBrowserThreadBundle::CreateBrowserThreads() {
  CHECK(!threads_created_);

  if (options_ & REAL_IO_THREAD) {
    io_thread_ = std::make_unique<TestBrowserThread>(BrowserThread::IO);
    io_thread_->StartIOThread();
  } else {
    io_thread_ = std::make_unique<TestBrowserThread>(
        BrowserThread::IO, base::ThreadTaskRunnerHandle::Get());
  }

  threads_created_ = true;

  // Consider startup complete such that after-startup-tasks always run in
  // the scope of the test they were posted from (http://crbug.com/732018).
  SetBrowserStartupIsCompleteForTesting();
}

void TestBrowserThreadBundle::RunUntilIdle() {
  scoped_task_environment_->RunUntilIdle();
}

void TestBrowserThreadBundle::RunIOThreadUntilIdle() {
  // Use a RunLoop to run until idle if already on BrowserThread::IO (which is
  // the main thread unless using Options::REAL_IO_THREAD).
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::WaitableEvent io_thread_idle(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          [](base::WaitableEvent* io_thread_idle) {
            base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed)
                .RunUntilIdle();
            io_thread_idle->Signal();
          },
          Unretained(&io_thread_idle)));
  io_thread_idle.Wait();
}

}  // namespace content
