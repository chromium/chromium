// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_process_sub_thread.h"

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/notification_service_impl.h"
#include "content/browser/utility_process_host.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread_delegate.h"
#include "content/public/common/process_type.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

namespace {
BrowserThreadDelegate* g_io_thread_delegate = nullptr;
}  // namespace

// static
void BrowserThread::SetIOThreadDelegate(BrowserThreadDelegate* delegate) {
  // |delegate| can only be set/unset while BrowserThread::IO isn't up.
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO));
  // and it cannot be set twice.
  DCHECK(!g_io_thread_delegate || !delegate);

  g_io_thread_delegate = delegate;
}

BrowserProcessSubThread::BrowserProcessSubThread(BrowserThread::ID identifier)
    : base::Thread(BrowserThreadImpl::GetThreadName(identifier)),
      identifier_(identifier) {
  // Not bound to creation thread.
  DETACH_FROM_THREAD(browser_thread_checker_);
}

BrowserProcessSubThread::~BrowserProcessSubThread() {
  Stop();
}

void BrowserProcessSubThread::RegisterAsBrowserThread() {
  DCHECK(IsRunning());

  DCHECK(!browser_thread_);
  browser_thread_.reset(new BrowserThreadImpl(identifier_, task_runner()));

  // Unretained(this) is safe as |this| outlives its underlying thread.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowserProcessSubThread::CompleteInitializationOnBrowserThread,
          Unretained(this)));
}

void BrowserProcessSubThread::AllowBlockingForTesting() {
  DCHECK(!IsRunning());
  is_blocking_allowed_for_testing_ = true;
}

// static
std::unique_ptr<BrowserProcessSubThread>
BrowserProcessSubThread::CreateIOThread() {
  TRACE_EVENT0("startup", "BrowserProcessSubThread::CreateIOThread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // Up the priority of the |io_thread_| as some of its IPCs relate to
  // display tasks.
  options.priority = base::ThreadPriority::DISPLAY;
#endif
  std::unique_ptr<BrowserProcessSubThread> io_thread(
      new BrowserProcessSubThread(BrowserThread::IO));
  if (!io_thread->StartWithOptions(options))
    LOG(FATAL) << "Failed to start BrowserThread:IO";
  return io_thread;
}

void BrowserProcessSubThread::Init() {
  DCHECK_CALLED_ON_VALID_THREAD(browser_thread_checker_);

#if defined(OS_WIN)
  com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
#endif

  if (!is_blocking_allowed_for_testing_) {
    base::DisallowUnresponsiveTasks();
  }
}

void BrowserProcessSubThread::Run(base::RunLoop* run_loop) {
  DCHECK_CALLED_ON_VALID_THREAD(browser_thread_checker_);

#if defined(OS_ANDROID)
  // Not to reset thread name to "Thread-???" by VM, attach VM with thread name.
  // Though it may create unnecessary VM thread objects, keeping thread name
  // gives more benefit in debugging in the platform.
  if (!thread_name().empty()) {
    base::android::AttachCurrentThreadWithName(thread_name());
  }
#endif

  switch (identifier_) {
    case BrowserThread::UI:
      // The main thread is usually promoted as the UI thread and doesn't go
      // through Run() but some tests do run a separate UI thread.
      UIThreadRun(run_loop);
      break;
    case BrowserThread::IO:
      IOThreadRun(run_loop);
      return;
    case BrowserThread::ID_COUNT:
      NOTREACHED();
      break;
  }
}

void BrowserProcessSubThread::CleanUp() {
  DCHECK_CALLED_ON_VALID_THREAD(browser_thread_checker_);

  // Run extra cleanup if this thread represents BrowserThread::IO.
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    IOThreadCleanUp();

  if (identifier_ == BrowserThread::IO && g_io_thread_delegate)
    g_io_thread_delegate->CleanUp();

  notification_service_.reset();

#if defined(OS_WIN)
  com_initializer_.reset();
#endif
}

void BrowserProcessSubThread::CompleteInitializationOnBrowserThread() {
  DCHECK_CALLED_ON_VALID_THREAD(browser_thread_checker_);

  notification_service_ = std::make_unique<NotificationServiceImpl>();

  if (identifier_ == BrowserThread::IO && g_io_thread_delegate) {
    // Allow blocking calls while initializing the IO thread.
    base::ScopedAllowBlocking allow_blocking_for_init;
    g_io_thread_delegate->Init();
  }
}

// Mark following two functions as NOINLINE so the compiler doesn't merge
// them together.

NOINLINE void BrowserProcessSubThread::UIThreadRun(base::RunLoop* run_loop) {
  const int line_number = __LINE__;
  Thread::Run(run_loop);
  base::debug::Alias(&line_number);
}

NOINLINE void BrowserProcessSubThread::IOThreadRun(base::RunLoop* run_loop) {
  const int line_number = __LINE__;
  Thread::Run(run_loop);
  base::debug::Alias(&line_number);
}

void BrowserProcessSubThread::IOThreadCleanUp() {
  DCHECK_CALLED_ON_VALID_THREAD(browser_thread_checker_);

  // Kill all things that might be holding onto
  // net::URLRequest/net::URLRequestContexts.

  // Destroy all URLRequests started by URLFetchers.
  net::URLFetcher::CancelAll();

  for (BrowserChildProcessHostIterator it(PROCESS_TYPE_UTILITY); !it.Done();
       ++it) {
    UtilityProcessHost* utility_process =
        static_cast<UtilityProcessHost*>(it.GetDelegate());
    if (utility_process->sandbox_type() ==
        service_manager::SANDBOX_TYPE_NETWORK) {
      // This ensures that cookies and cache are flushed to disk on shutdown.
      // https://crbug.com/841001
      const int kMaxSecondsToWaitForNetworkProcess = 10;
      ChildProcessHostImpl* child_process =
          static_cast<ChildProcessHostImpl*>(it.GetHost());
      auto& process = child_process->peer_process();
      if (!process.IsValid())
        continue;
      base::ScopedAllowBaseSyncPrimitives scoped_allow_base_sync_primitives;
      const base::TimeTicks start_time = base::TimeTicks::Now();
      process.WaitForExitWithTimeout(
          base::TimeDelta::FromSeconds(kMaxSecondsToWaitForNetworkProcess),
          nullptr);
      // Record time spent for the method call.
      base::TimeDelta network_wait_time = base::TimeTicks::Now() - start_time;
      UMA_HISTOGRAM_TIMES("NetworkService.ShutdownTime", network_wait_time);
      LOG(ERROR) << "Waited " << network_wait_time.InMilliseconds()
                 << " ms for network service";
    }
  }

  // If any child processes are still running, terminate them and
  // and delete the BrowserChildProcessHost instances to release whatever
  // IO thread only resources they are referencing.
  BrowserChildProcessHostImpl::TerminateAll();
}

}  // namespace content
