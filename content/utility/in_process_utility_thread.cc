// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/in_process_utility_thread.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "content/child/child_process.h"
#include "content/utility/utility_thread_impl.h"

namespace content {

// We want to ensure there's only one utility thread running at a time, as there
// are many globals used in the utility process.
static base::LazyInstance<base::Lock>::DestructorAtExit
    g_one_utility_thread_lock;

InProcessUtilityThread::InProcessUtilityThread(
    const InProcessChildThreadParams& params)
    : Thread("Chrome_InProcUtilityThread"), params_(params) {}

InProcessUtilityThread::~InProcessUtilityThread() {
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
  Stop();
}

void InProcessUtilityThread::Init() {
  // We need to return right away or else the main thread that started us will
  // hang.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&InProcessUtilityThread::InitInternal,
                                base::Unretained(this)));
}

void InProcessUtilityThread::CleanUp()
    UNLOCK_FUNCTION(g_one_utility_thread_lock.Get()) {
  child_process_.reset();

  // See comment in RendererMainThread.
  SetThreadWasQuitProperly(true);
  g_one_utility_thread_lock.Get().Release();
}

void InProcessUtilityThread::InitInternal()
    EXCLUSIVE_LOCK_FUNCTION(g_one_utility_thread_lock.Get()) {
  g_one_utility_thread_lock.Get().Acquire();
  child_process_ = std::make_unique<ChildProcess>();
  child_process_->set_main_thread(new UtilityThreadImpl(params_));
}

base::Thread* CreateInProcessUtilityThread(
    const InProcessChildThreadParams& params) {
  return new InProcessUtilityThread(params);
}

}  // namespace content
