// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_sub_thread.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/threading/thread_restrictions.h"
#include "ios/web/public/thread/web_thread_delegate.h"
#include "ios/web/web_thread_impl.h"
#include "net/url_request/url_fetcher.h"

namespace web {

namespace {
WebThreadDelegate* g_io_thread_delegate = nullptr;
}  // namespace

// static
void WebThread::SetIOThreadDelegate(WebThreadDelegate* delegate) {
  // |delegate| can only be set/unset while WebThread::IO isn't up.
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

  // Unretained(this) is safe as |this| outlives its underlying thread.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebSubThread::CompleteInitializationOnWebThread,
                     Unretained(this)));
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
      NOTREACHED();
      break;
  }
}

void WebSubThread::CleanUp() {
  DCHECK_CALLED_ON_VALID_THREAD(web_thread_checker_);

  // Run extra cleanup if this thread represents WebThread::IO.
  if (WebThread::CurrentlyOn(WebThread::IO))
    IOThreadCleanUp();

  if (identifier_ == WebThread::IO && g_io_thread_delegate)
    g_io_thread_delegate->CleanUp();

  web_thread_.reset();
}

void WebSubThread::CompleteInitializationOnWebThread() {
  DCHECK_CALLED_ON_VALID_THREAD(web_thread_checker_);

  if (identifier_ == WebThread::IO && g_io_thread_delegate) {
    // Allow blocking calls while initializing the IO thread.
    base::ScopedAllowBlocking allow_blocking_for_init;
    g_io_thread_delegate->Init();
  }
}

void WebSubThread::UIThreadRun(base::RunLoop* run_loop) {
  const int line_number = __LINE__;
  Thread::Run(run_loop);
  base::debug::Alias(&line_number);
}

void WebSubThread::IOThreadRun(base::RunLoop* run_loop) {
  const int line_number = __LINE__;
  Thread::Run(run_loop);
  base::debug::Alias(&line_number);
}

void WebSubThread::IOThreadCleanUp() {
  DCHECK_CALLED_ON_VALID_THREAD(web_thread_checker_);

  // Kill all things that might be holding onto
  // net::URLRequest/net::URLRequestContexts.

  // Destroy all URLRequests started by URLFetchers.
  net::URLFetcher::CancelAll();
}

}  // namespace web
