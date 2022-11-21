// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_globals.h"

#include "base/check.h"
#include "base/lazy_instance.h"  // For testing purposes only.
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_local.h"  // For testing purposes only.

namespace ppapi {

namespace {
// Thread-local globals for testing. See SetPpapiGlobalsOnThreadForTest for more
// information.
base::LazyInstance<base::ThreadLocalPointer<PpapiGlobals>>::Leaky
    tls_ppapi_globals_for_test = LAZY_INSTANCE_INITIALIZER;
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
  tls_ppapi_globals_for_test.Pointer()->Set(ptr);
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
  return tls_ppapi_globals_for_test.Pointer()->Get();
}

}  // namespace ppapi
