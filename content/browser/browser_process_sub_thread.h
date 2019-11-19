// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PROCESS_SUB_THREAD_H_
#define CONTENT_BROWSER_BROWSER_PROCESS_SUB_THREAD_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
namespace base {
namespace win {
class ScopedCOMInitializer;
}
}
#endif

namespace content {
class NotificationService;
}

namespace content {

// ----------------------------------------------------------------------------
// A BrowserProcessSubThread is a physical thread backing a BrowserThread.
//
// Applications must initialize the COM library before they can call
// COM library functions other than CoGetMalloc and memory allocation
// functions, so this class initializes COM for those users.
class CONTENT_EXPORT BrowserProcessSubThread : public base::Thread {
 public:
  // Constructs a BrowserProcessSubThread for |identifier|.
  explicit BrowserProcessSubThread(BrowserThread::ID identifier);
  ~BrowserProcessSubThread() override;

  // Registers this thread to represent |identifier_| in the browser_thread.h
  // API. This thread must already be running when this is called. This can only
  // be called once per BrowserProcessSubThread instance.
  void RegisterAsBrowserThread();

  // Ideally there wouldn't be a special blanket allowance to block the
  // BrowserThreads in tests but TestBrowserThreadImpl previously bypassed
  // BrowserProcessSubThread and hence wasn't subject to ThreadRestrictions...
  // Flipping that around in favor of explicit scoped allowances would be
  // preferable but a non-trivial amount of work. Can only be called before
  // starting this BrowserProcessSubThread.
  void AllowBlockingForTesting();

 protected:
  void Init() override;
  void Run(base::RunLoop* run_loop) override;
  void CleanUp() override;

 private:
  // Second Init() phase that must happen on this thread but can only happen
  // after it's promoted to a BrowserThread in |RegisterAsBrowserThread()|.
  void CompleteInitializationOnBrowserThread();

  // These methods merely forwards to Thread::Run() but are useful to identify
  // which BrowserThread this represents in stack traces.
  void UIThreadRun(base::RunLoop* run_loop);
  void IOThreadRun(base::RunLoop* run_loop);

  // This method encapsulates cleanup that needs to happen on the IO thread.
  void IOThreadCleanUp();

  const BrowserThread::ID identifier_;

  // BrowserThreads are not allowed to do file I/O nor wait on synchronization
  // primivives except when explicitly allowed in tests.
  bool is_blocking_allowed_for_testing_ = false;

  // The BrowserThread registration for this |identifier_|, initialized in
  // RegisterAsBrowserThread().
  std::unique_ptr<BrowserThreadImpl> browser_thread_;

#if defined (OS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  // Each specialized thread has its own notification service.
  std::unique_ptr<NotificationService> notification_service_;

  THREAD_CHECKER(browser_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessSubThread);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PROCESS_SUB_THREAD_H_
