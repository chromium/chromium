// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_sub_thread.h"

#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/threading/thread_restrictions.h"
#include "ios/web/public/thread/web_thread_delegate.h"
#include "ios/web/web_thread_impl.h"

namespace web {

namespace {
WebThreadDelegate* g_io_thread_delegate = nullptr;
}  // namespace

// static
void WebThread::SetIOThreadDelegate(WebThreadDelegate* delegate) {
  // `delegate` can only be set/unset while WebThread::IO isn't up.
  DCHECK(!WebThread::IsThreadInitialized(WebThread::IO));
  // and it cannot be set twice.
  DCHECK(!g_io_thread_delegate || !delegate);

  g_io_thread_delegate = delegate;
}

WebSubThread::WebSubThread(WebThread::ID identifier)
    : base::Thread(WebThreadImpl::GetThreadName(identifier)),
      identifier_(identifier) {
  // Not bound to creation thread.
  DETACH_FROM_THREAD(web_thread_checker_);
}

WebSubThread::~WebSubThread() {
  Stop();
}

void WebSubThread::RegisterAsWebThread() {
  DCHECK(IsRunning());

  DCHECK(!web_thread_);
  web_thread_.reset(new WebThreadImpl(identifier_, task_runner()));
}

void WebSubThread::AllowBlockingForTesting() {
  DCHECK(!IsRunning());
  is_blocking_allowed_for_testing_ = true;
}

void WebSubThread::Init() {
  DCHECK_CALLED_ON_VALID_THREAD(web_thread_checker_);

  if (!is_blocking_allowed_for_testing_) {
    base::DisallowUnresponsiveTasks();
  }
}

void WebSubThread::Run(base::RunLoop* run_loop) {
  DCHECK_CALLED_ON_VALID_THREAD(web_thread_checker_);

  switch (identifier_) {
    case WebThread::UI:
      // The main thread is usually promoted as the UI thread and doesn't go
      // through Run() but some tests do run a separate UI thread.
      UIThreadRun(run_loop);
      break;
    case WebThread::IO:
      IOThreadRun(run_loop);
      return;
    case WebThread::ID_COUNT:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void WebSubThread::CleanUp() {
  DCHECK_CALLED_ON_VALID_THREAD(web_thread_checker_);

  if (identifier_ == WebThread::IO && g_io_thread_delegate)
    g_io_thread_delegate->CleanUp();

  web_thread_.reset();
}

void WebSubThread::UIThreadRun(base::RunLoop* run_loop) {
  Thread::Run(run_loop);
  NO_CODE_FOLDING();
}

void WebSubThread::IOThreadRun(base::RunLoop* run_loop) {
  Thread::Run(run_loop);
  NO_CODE_FOLDING();
}

}  // namespace web
