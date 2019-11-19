// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_SUB_THREAD_H_
#define IOS_WEB_WEB_SUB_THREAD_H_

#include "base/macros.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "ios/web/public/thread/web_thread.h"

namespace web {
class WebThreadImpl;
}

namespace web {

// A WebSubThread is a physical thread backing a WebThread.
class WebSubThread : public base::Thread {
 public:
  // Constructs a WebSubThread for |identifier|.
  explicit WebSubThread(WebThread::ID identifier);
  ~WebSubThread() override;

  // Registers this thread to represent |identifier_| in the web_thread.h
  // API. This thread must already be running when this is called. This can only
  // be called once per WebSubThread instance.
  void RegisterAsWebThread();

  // Ideally there wouldn't be a special blanket allowance to block the
  // WebThreads in tests but TestWebThreadImpl previously bypassed
  // WebSubThread and hence wasn't subject to ThreadRestrictions...
  // Flipping that around in favor of explicit scoped allowances would be
  // preferable but a non-trivial amount of work. Can only be called before
  // starting this WebSubThread.
  void AllowBlockingForTesting();

 protected:
  void Init() override;
  void Run(base::RunLoop* run_loop) override;
  void CleanUp() override;

 private:
  // Second Init() phase that must happen on this thread but can only happen
  // after it's promoted to a WebThread in |RegisterAsWebThread()|.
  void CompleteInitializationOnWebThread();

  // These methods merely forwards to Thread::Run() but are useful to identify
  // which WebThread this represents in stack traces.
  void UIThreadRun(base::RunLoop* run_loop);
  void IOThreadRun(base::RunLoop* run_loop);

  // This method encapsulates cleanup that needs to happen on the IO thread.
  void IOThreadCleanUp();

  const WebThread::ID identifier_;

  // WebThreads are not allowed to do file I/O nor wait on synchronization
  // primitives except when explicitly allowed in tests.
  bool is_blocking_allowed_for_testing_ = false;

  // The WebThread registration for this |identifier_|, initialized in
  // RegisterAsWebThread().
  std::unique_ptr<WebThreadImpl> web_thread_;

  THREAD_CHECKER(web_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(WebSubThread);
};

}  // namespace web

#endif  // IOS_WEB_WEB_SUB_THREAD_H_
