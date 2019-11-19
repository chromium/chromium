// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_MESSAGE_LOOP_PROXY_H_
#define PPAPI_PROXY_PPB_MESSAGE_LOOP_PROXY_H_

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/ppb_message_loop_shared.h"
#include "ppapi/thunk/ppb_message_loop_api.h"

struct PPB_MessageLoop_1_0;

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT MessageLoopResource : public MessageLoopShared {
 public:
  explicit MessageLoopResource(PP_Instance instance);
  // Construct the one MessageLoopResource for the main thread. This must be
  // invoked on the main thread.
  explicit MessageLoopResource(ForMainThread);
  ~MessageLoopResource() override;

  // Resource overrides.
  thunk::PPB_MessageLoop_API* AsPPB_MessageLoop_API() override;

  // PPB_MessageLoop_API implementation.
  int32_t AttachToCurrentThread() override;
  int32_t Run() override;
  int32_t PostWork(PP_CompletionCallback callback, int64_t delay_ms) override;
  int32_t PostQuit(PP_Bool should_destroy) override;

  static MessageLoopResource* GetCurrent();
  void DetachFromThread();
  bool is_main_thread_loop() const {
    return is_main_thread_loop_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() {
    return task_runner_;
  }

  void set_currently_handling_blocking_message(bool handling_blocking_message) {
    currently_handling_blocking_message_ = handling_blocking_message;
  }

 private:
  struct TaskInfo {
    base::Location from_here;
    base::Closure closure;
    int64_t delay_ms;
  };

  // Returns true if the object is associated with the current thread.
  bool IsCurrent() const;

  // MessageLoopShared implementation.
  //
  // Handles posting to the task executor if there is one, or the pending queue
  // if there isn't.
  // NOTE: The given closure will be run *WITHOUT* acquiring the Proxy lock.
  //       This only makes sense for user code and completely thread-safe
  //       proxy operations (e.g., MessageLoop::QuitClosure).
  void PostClosure(const base::Location& from_here,
                   const base::Closure& closure,
                   int64_t delay_ms) override;
  base::SingleThreadTaskRunner* GetTaskRunner() override;
  bool CurrentlyHandlingBlockingMessage() override;

  // Quits |run_loop_|. Must be called from the thread that runs the RunLoop.
  void QuitRunLoopWhenIdle();

  // TLS destructor function.
  static void ReleaseMessageLoop(void* value);

  // Created when we attach to the current thread. NULL for the main thread,
  // since that's owned by somebody else. This is needed for Run and Quit.
  // Any time we post tasks, we should post them using task_runner_.
  std::unique_ptr<base::SingleThreadTaskExecutor> single_thread_task_executor_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // RunLoop currently on the stack.
  base::RunLoop* run_loop_ = nullptr;

  // Number of invocations of Run currently on the stack.
  int nested_invocations_;

  // Set to true when the task executor is destroyed to prevent forther
  // posting of work.
  bool destroyed_;

  // Set to true if all task executor invocations should exit and that the
  // loop should be destroyed once it reaches the outermost Run invocation.
  bool should_destroy_;

  bool is_main_thread_loop_;

  bool currently_handling_blocking_message_;

  // Since we allow tasks to be posted before the task executor is actually
  // created (when it's associated with a thread), we keep tasks posted here
  // until that happens. Once the loop_ is created, this is unused.
  std::vector<TaskInfo> pending_tasks_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopResource);
};

class PPB_MessageLoop_Proxy : public InterfaceProxy {
 public:
  explicit PPB_MessageLoop_Proxy(Dispatcher* dispatcher);
  ~PPB_MessageLoop_Proxy() override;

  static const PPB_MessageLoop_1_0* GetInterface();

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_MessageLoop_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_MESSAGE_LOOP_PROXY_H_
