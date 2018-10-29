// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/browser_thread_impl.h"

namespace content {

TestBrowserThread::TestBrowserThread(BrowserThread::ID identifier)
    : identifier_(identifier),
      real_thread_(std::make_unique<BrowserProcessSubThread>(identifier_)) {
  real_thread_->AllowBlockingForTesting();
}

TestBrowserThread::TestBrowserThread(
    BrowserThread::ID identifier,
    scoped_refptr<base::SingleThreadTaskRunner> thread_runner)
    : identifier_(identifier),
      fake_thread_(
          new BrowserThreadImpl(identifier_, std::move(thread_runner))) {}

TestBrowserThread::~TestBrowserThread() {
  // The upcoming BrowserThreadImpl::ResetGlobalsForTesting() call requires that
  // |identifier_| have completed its SHUTDOWN phase.
  real_thread_.reset();
  fake_thread_.reset();

  // Resets BrowserThreadImpl's globals so that |identifier_| is no longer
  // bound. This is fine since the underlying MessageLoop has already been
  // flushed and deleted above. In the case of an externally provided
  // MessageLoop however, this means that TaskRunners obtained through
  // |BrowserThreadImpl::GetTaskRunnerForThread(identifier_)| will no longer
  // recognize their BrowserThreadImpl for RunsTasksInCurrentSequence(). This
  // happens most often when such verifications are made from
  // MessageLoopCurrent::DestructionObservers. Callers that care to work around
  // that should instead use this shutdown sequence:
  //   1) TestBrowserThread::Stop()
  //   2) ~MessageLoop()
  //   3) ~TestBrowserThread()
  // (~TestBrowserThreadBundle() does this).
  BrowserThreadImpl::ResetGlobalsForTesting(identifier_);
}

void TestBrowserThread::Start() {
  CHECK(real_thread_->Start());
  RegisterAsBrowserThread();
}

void TestBrowserThread::StartAndWaitForTesting() {
  CHECK(real_thread_->StartAndWaitForTesting());
  RegisterAsBrowserThread();
}

void TestBrowserThread::StartIOThread() {
  StartIOThreadUnregistered();
  RegisterAsBrowserThread();
}

void TestBrowserThread::StartIOThreadUnregistered() {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  CHECK(real_thread_->StartWithOptions(options));
}

void TestBrowserThread::RegisterAsBrowserThread() {
  real_thread_->RegisterAsBrowserThread();
}

void TestBrowserThread::Stop() {
  if (real_thread_)
    real_thread_->Stop();
}

}  // namespace content
