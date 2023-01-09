// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_web_thread.h"

#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "ios/web/web_sub_thread.h"
#include "ios/web/web_thread_impl.h"

namespace web {

TestWebThread::TestWebThread(WebThread::ID identifier)
    : identifier_(identifier),
      real_thread_(std::make_unique<WebSubThread>(identifier_)) {
  real_thread_->AllowBlockingForTesting();
}

TestWebThread::TestWebThread(
    WebThread::ID identifier,
    scoped_refptr<base::SingleThreadTaskRunner> thread_runner)
    : identifier_(identifier),
      fake_thread_(new WebThreadImpl(identifier_, thread_runner)) {}

TestWebThread::~TestWebThread() {
  // The upcoming WebThreadImpl::ResetGlobalsForTesting() call requires that
  // `identifier_` completed its shutdown phase.
  real_thread_.reset();
  fake_thread_.reset();

  // Resets WebThreadImpl's globals so that `identifier_` is no longer
  // bound. This is fine since the underlying MessageLoop has already been
  // flushed and deleted above. In the case of an externally provided
  // MessageLoop however, this means that TaskRunners obtained through
  // `WebThreadImpl::GetTaskRunnerForThread(identifier_)` will no longer
  // recognize their WebThreadImpl for RunsTasksInCurrentSequence(). This
  // happens most often when such verifications are made from
  // MessageLoop::DestructionObservers. Callers that care to work around that
  // should instead use this shutdown sequence:
  //   1) TestWebThread::Stop()
  //   2) ~MessageLoop()
  //   3) ~TestWebThread()
  // (~WebTaskEnvironment() does this).
  WebThreadImpl::ResetGlobalsForTesting(identifier_);
}

void TestWebThread::Start() {
  CHECK(real_thread_->Start());
  RegisterAsWebThread();
}

void TestWebThread::StartIOThread() {
  StartIOThreadUnregistered();
  RegisterAsWebThread();
}

void TestWebThread::StartIOThreadUnregistered() {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  CHECK(real_thread_->StartWithOptions(std::move(options)));
}

void TestWebThread::RegisterAsWebThread() {
  real_thread_->RegisterAsWebThread();
}

void TestWebThread::Stop() {
  if (real_thread_)
    real_thread_->Stop();
}

}  // namespace web
