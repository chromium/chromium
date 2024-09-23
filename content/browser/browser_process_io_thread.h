// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PROCESS_IO_THREAD_H_
#define CONTENT_BROWSER_BROWSER_PROCESS_IO_THREAD_H_

#include <memory>

#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN)
namespace base {
namespace win {
class ScopedCOMInitializer;
}
}  // namespace base
#endif

namespace content {
class BrowserThreadImpl;
}

namespace content {

// ----------------------------------------------------------------------------
// A BrowserProcessIOThread is a physical thread backing the IO thread.
//
// Applications must initialize the COM library before they can call
// COM library functions other than CoGetMalloc and memory allocation
// functions, so this class initializes COM for those users.
class CONTENT_EXPORT BrowserProcessIOThread : public base::Thread {
 public:
  // Constructs a BrowserProcessIOThread.
  BrowserProcessIOThread();

  BrowserProcessIOThread(const BrowserProcessIOThread&) = delete;
  BrowserProcessIOThread& operator=(const BrowserProcessIOThread&) = delete;

  ~BrowserProcessIOThread() override;

  // Registers this thread to represent the IO thread in the browser_thread.h
  // API. This thread must already be running when this is called. This can only
  // be called once per BrowserProcessIOThread instance.
  void RegisterAsBrowserThread();

  // Ideally there wouldn't be a special blanket allowance to block the
  // BrowserThreads in tests but TestBrowserThreadImpl previously bypassed
  // BrowserProcessIOThread and hence wasn't subject to ThreadRestrictions...
  // Flipping that around in favor of explicit scoped allowances would be
  // preferable but a non-trivial amount of work. Can only be called before
  // starting this BrowserProcessIOThread.
  void AllowBlockingForTesting();

  static void ProcessHostCleanUp();

 protected:
  void Init() override;
  void Run(base::RunLoop* run_loop) override;
  void CleanUp() override;

 private:
  void IOThreadRun(base::RunLoop* run_loop);

  // BrowserThreads are not allowed to do file I/O nor wait on synchronization
  // primivives except when explicitly allowed in tests.
  bool is_blocking_allowed_for_testing_ = false;

  // The BrowserThread registration for this IO thread, initialized in
  // RegisterAsBrowserThread().
  std::unique_ptr<BrowserThreadImpl> browser_thread_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  THREAD_CHECKER(browser_thread_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PROCESS_IO_THREAD_H_
