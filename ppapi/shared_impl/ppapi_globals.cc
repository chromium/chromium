// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_globals.h"

#include "base/check.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"

namespace ppapi {

namespace {

// Thread-local globals for testing. See SetPpapiGlobalsOnThreadForTest for more
// information.
constinit thread_local PpapiGlobals* ppapi_globals_for_test = nullptr;

}  // namespace

PpapiGlobals* ppapi_globals = NULL;

PpapiGlobals::PpapiGlobals() {
  DCHECK(!ppapi_globals);
  ppapi_globals = this;
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

PpapiGlobals::PpapiGlobals(PerThreadForTest) {
  DCHECK(!ppapi_globals);
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

PpapiGlobals::~PpapiGlobals() {
  DCHECK(ppapi_globals == this || !ppapi_globals);
  while (!message_loop_quit_closures_.empty()) {
    QuitMsgLoop();
  }
  ppapi_globals = NULL;
}

// Static Getter for the global singleton.
PpapiGlobals* PpapiGlobals::Get() {
  if (ppapi_globals)
    return ppapi_globals;
  // In unit tests, the following might be valid (see
  // SetPpapiGlobalsOnThreadForTest). Normally, this will just return NULL.
  return GetThreadLocalPointer();
}

// static
void PpapiGlobals::SetPpapiGlobalsOnThreadForTest(PpapiGlobals* ptr) {
  // If we're using a per-thread PpapiGlobals, we should not have a global one.
  // If we allowed it, it would always over-ride the "test" versions.
  DCHECK(!ppapi_globals);
  ppapi_globals_for_test = ptr;
}

void PpapiGlobals::RunMsgLoop() {
  base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
  message_loop_quit_closures_.push(loop.QuitClosure());
  loop.Run();
}

void PpapiGlobals::QuitMsgLoop() {
  if (!message_loop_quit_closures_.empty()) {
    std::move(message_loop_quit_closures_.top()).Run();
    message_loop_quit_closures_.pop();
  }
}

base::SingleThreadTaskRunner* PpapiGlobals::GetMainThreadMessageLoop() {
  return main_task_runner_.get();
}

void PpapiGlobals::ResetMainThreadMessageLoopForTesting() {
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

bool PpapiGlobals::IsHostGlobals() const { return false; }

bool PpapiGlobals::IsPluginGlobals() const { return false; }

// static
PpapiGlobals* PpapiGlobals::GetThreadLocalPointer() {
  return ppapi_globals_for_test;
}

}  // namespace ppapi
