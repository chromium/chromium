// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_process_io_thread.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/clang_profiling_buildflags.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/notification_service_impl.h"
#include "content/browser/utility_process_host.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/common/process_type.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

BrowserProcessIOThread::BrowserProcessIOThread()
    : base::Thread(BrowserThreadImpl::GetThreadName(BrowserThread::IO)) {
  // Not bound to creation thread.
  DETACH_FROM_THREAD(browser_thread_checker_);
}

BrowserProcessIOThread::~BrowserProcessIOThread() {
  Stop();
}

void BrowserProcessIOThread::RegisterAsBrowserThread() {
  DCHECK(IsRunning());

  DCHECK(!browser_thread_);
  browser_thread_.reset(
      new BrowserThreadImpl(BrowserThread::IO, task_runner()));

  // Unretained(this) is safe as |this| outlives its underlying thread.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowserProcessIOThread::CompleteInitializationOnBrowserThread,
          Unretained(this)));
}

void BrowserProcessIOThread::AllowBlockingForTesting() {
  DCHECK(!IsRunning());
  is_blocking_allowed_for_testing_ = true;
}

void BrowserProcessIOThread::Init() {
  DCHECK_CALLED_ON_VALID_THREAD(browser_thread_checker_);

#if BUILDFLAG(IS_WIN)
  com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
#endif

  if (!is_blocking_allowed_for_testing_) {
    base::DisallowUnresponsiveTasks();
  }
}

void BrowserProcessIOThread::Run(base::RunLoop* run_loop) {
  DCHECK_CALLED_ON_VALID_THREAD(browser_thread_checker_);

#if BUILDFLAG(IS_ANDROID)
  // Not to reset thread name to "Thread-???" by VM, attach VM with thread name.
  // Though it may create unnecessary VM thread objects, keeping thread name
  // gives more benefit in debugging in the platform.
  if (!thread_name().empty()) {
    base::android::AttachCurrentThreadWithName(thread_name());
  }
#endif

  IOThreadRun(run_loop);
}

void BrowserProcessIOThread::CleanUp() {
  DCHECK_CALLED_ON_VALID_THREAD(browser_thread_checker_);

  notification_service_.reset();

#if BUILDFLAG(IS_WIN)
  com_initializer_.reset();
#endif
}

void BrowserProcessIOThread::CompleteInitializationOnBrowserThread() {
  DCHECK_CALLED_ON_VALID_THREAD(browser_thread_checker_);

  notification_service_ = std::make_unique<NotificationServiceImpl>();
}

void BrowserProcessIOThread::IOThreadRun(base::RunLoop* run_loop) {
  // Register the IO thread for hang watching before it starts running and set
  // up a closure to automatically unregister it when Run() returns.
  base::ScopedClosureRunner unregister_thread_closure;
  if (base::HangWatcher::IsIOThreadHangWatchingEnabled()) {
    unregister_thread_closure = base::HangWatcher::RegisterThread(
        base::HangWatcher::ThreadType::kIOThread);
  }

  Thread::Run(run_loop);

  // Inhibit tail calls of Run and inhibit code folding.
  const int line_number = __LINE__;
  base::debug::Alias(&line_number);
}

void BrowserProcessIOThread::ProcessHostCleanUp() {
  for (BrowserChildProcessHostIterator it(PROCESS_TYPE_UTILITY); !it.Done();
       ++it) {
    if (it.GetDelegate()->GetServiceName() ==
        network::mojom::NetworkService::Name_) {
      // This ensures that cookies and cache are flushed to disk on shutdown.
      // https://crbug.com/841001
#if BUILDFLAG(CLANG_PROFILING)
      // On profiling build, browser_tests runs 10x slower.
      const int kMaxSecondsToWaitForNetworkProcess = 100;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
      // ChromeOS will kill the browser process if it doesn't shut down within
      // 3 seconds, so make sure we wait for less than that.
      const int kMaxSecondsToWaitForNetworkProcess = 1;
#else
      const int kMaxSecondsToWaitForNetworkProcess = 10;
#endif

      ChildProcessHostImpl* child_process =
          static_cast<ChildProcessHostImpl*>(it.GetHost());
      auto& process = child_process->GetPeerProcess();
      if (!process.IsValid())
        continue;
      base::ScopedAllowBaseSyncPrimitives scoped_allow_base_sync_primitives;
      const base::TimeTicks start_time = base::TimeTicks::Now();
      process.WaitForExitWithTimeout(
          base::Seconds(kMaxSecondsToWaitForNetworkProcess), nullptr);
      // Record time spent for the method call.
      base::TimeDelta network_wait_time = base::TimeTicks::Now() - start_time;
      UMA_HISTOGRAM_TIMES("NetworkService.ShutdownTime", network_wait_time);
      DVLOG(1) << "Waited " << network_wait_time.InMilliseconds()
               << " ms for network service";
    }
  }

  // If any child processes are still running, terminate them and
  // and delete the BrowserChildProcessHost instances to release whatever
  // IO thread only resources they are referencing.
  BrowserChildProcessHostImpl::TerminateAll();
}

}  // namespace content
