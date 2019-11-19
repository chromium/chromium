// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread_impl.h"

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task/task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"

namespace content {

namespace {

// State of a given BrowserThread::ID in chronological order throughout the
// browser process' lifetime.
enum BrowserThreadState {
  // BrowserThread::ID isn't associated with anything yet.
  UNINITIALIZED = 0,
  // BrowserThread::ID is associated to a TaskRunner and is accepting tasks.
  RUNNING,
  // BrowserThread::ID no longer accepts tasks (it's still associated to a
  // TaskRunner but that TaskRunner doesn't have to accept tasks).
  SHUTDOWN
};

struct BrowserThreadGlobals {
  BrowserThreadGlobals() {
    // A few unit tests which do not use a BrowserTaskEnvironment still invoke
    // code that reaches into CurrentlyOn()/IsThreadInitialized(). This can
    // result in instantiating BrowserThreadGlobals off the main thread.
    // |main_thread_checker_| being bound incorrectly would then result in a
    // flake in the next test that instantiates a BrowserTaskEnvironment in the
    // same process. Detaching here postpones binding |main_thread_checker_| to
    // the first invocation of BrowserThreadImpl::BrowserThreadImpl() and works
    // around this issue.
    DETACH_FROM_THREAD(main_thread_checker_);
  }

  // BrowserThreadGlobals must be initialized on main thread before it's used by
  // any other threads.
  THREAD_CHECKER(main_thread_checker_);

  // |task_runners[id]| is safe to access on |main_thread_checker_| as
  // well as on any thread once it's read-only after initialization
  // (i.e. while |states[id] >= RUNNING|).
  scoped_refptr<base::SingleThreadTaskRunner>
      task_runners[BrowserThread::ID_COUNT];

  // Tracks the runtime state of BrowserThreadImpls. Atomic because a few
  // methods below read this value outside |main_thread_checker_| to
  // confirm it's >= RUNNING and doing so requires an atomic read as it could be
  // in the middle of transitioning to SHUTDOWN (which the check is fine with
  // but reading a non-atomic value as it's written to by another thread can
  // result in undefined behaviour on some platforms).
  // Only NoBarrier atomic operations should be used on |states| as it shouldn't
  // be used to establish happens-after relationships but rather checking the
  // runtime state of various threads (once again: it's only atomic to support
  // reading while transitioning from RUNNING=>SHUTDOWN).
  base::subtle::Atomic32 states[BrowserThread::ID_COUNT] = {};
};

BrowserThreadGlobals& GetBrowserThreadGlobals() {
  static base::NoDestructor<BrowserThreadGlobals> globals;
  return *globals;
}

}  // namespace

BrowserThreadImpl::BrowserThreadImpl(
    ID identifier,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : identifier_(identifier) {
  DCHECK_GE(identifier_, 0);
  DCHECK_LT(identifier_, ID_COUNT);
  DCHECK(task_runner);

  BrowserThreadGlobals& globals = GetBrowserThreadGlobals();

  DCHECK_CALLED_ON_VALID_THREAD(globals.main_thread_checker_);

  DCHECK_EQ(base::subtle::NoBarrier_Load(&globals.states[identifier_]),
            BrowserThreadState::UNINITIALIZED);
  base::subtle::NoBarrier_Store(&globals.states[identifier_],
                                BrowserThreadState::RUNNING);

  DCHECK(!globals.task_runners[identifier_]);
  globals.task_runners[identifier_] = std::move(task_runner);

  if (identifier_ == BrowserThread::ID::UI) {
#if defined(OS_POSIX)
    // Allow usage of the FileDescriptorWatcher API on the UI thread, using the
    // IO thread to watch the file descriptors.
    //
    // In unit tests, usage of the  FileDescriptorWatcher API is already allowed
    // if the UI thread is running a MessageLoopForIO.
    if (!base::MessageLoopCurrentForIO::IsSet()) {
      file_descriptor_watcher_.emplace(
          base::CreateSingleThreadTaskRunner({BrowserThread::IO}));
    }
    base::FileDescriptorWatcher::AssertAllowed();
#endif
  }
}

BrowserThreadImpl::~BrowserThreadImpl() {
  BrowserThreadGlobals& globals = GetBrowserThreadGlobals();
  DCHECK_CALLED_ON_VALID_THREAD(globals.main_thread_checker_);

  DCHECK_EQ(base::subtle::NoBarrier_Load(&globals.states[identifier_]),
            BrowserThreadState::RUNNING);
  base::subtle::NoBarrier_Store(&globals.states[identifier_],
                                BrowserThreadState::SHUTDOWN);

  // The mapping is kept alive after shutdown to avoid requiring a lock only for
  // shutdown (the SingleThreadTaskRunner itself may stop accepting tasks at any
  // point -- usually soon before/after destroying the BrowserThreadImpl).
  DCHECK(globals.task_runners[identifier_]);
}

// static
void BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::ID identifier) {
  BrowserThreadGlobals& globals = GetBrowserThreadGlobals();
  DCHECK_CALLED_ON_VALID_THREAD(globals.main_thread_checker_);

  DCHECK_EQ(base::subtle::NoBarrier_Load(&globals.states[identifier]),
            BrowserThreadState::SHUTDOWN);
  base::subtle::NoBarrier_Store(&globals.states[identifier],
                                BrowserThreadState::UNINITIALIZED);

  globals.task_runners[identifier] = nullptr;
}

// static
const char* BrowserThreadImpl::GetThreadName(BrowserThread::ID thread) {
  static const char* const kBrowserThreadNames[BrowserThread::ID_COUNT] = {
      "",                 // UI (name assembled in browser_main_loop.cc).
      "Chrome_IOThread",  // IO
  };

  if (BrowserThread::UI < thread && thread < BrowserThread::ID_COUNT)
    return kBrowserThreadNames[thread];
  if (thread == BrowserThread::UI)
    return "Chrome_UIThread";
  return "Unknown Thread";
}

// static
bool BrowserThread::IsThreadInitialized(ID identifier) {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);

  BrowserThreadGlobals& globals = GetBrowserThreadGlobals();
  return base::subtle::NoBarrier_Load(&globals.states[identifier]) ==
         BrowserThreadState::RUNNING;
}

// static
bool BrowserThread::CurrentlyOn(ID identifier) {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);

  BrowserThreadGlobals& globals = GetBrowserThreadGlobals();

  // Thread-safe since |globals.task_runners| is read-only after being
  // initialized from main thread (which happens before //content and embedders
  // are kicked off and enabled to call the BrowserThread API from other
  // threads).
  return globals.task_runners[identifier] &&
         globals.task_runners[identifier]->RunsTasksInCurrentSequence();
}

// static
std::string BrowserThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  std::string actual_name = base::PlatformThread::GetName();
  if (actual_name.empty())
    actual_name = "Unknown Thread";

  std::string result = "Must be called on ";
  result += BrowserThreadImpl::GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

// static
bool BrowserThread::GetCurrentThreadIdentifier(ID* identifier) {
  BrowserThreadGlobals& globals = GetBrowserThreadGlobals();

  // Thread-safe since |globals.task_runners| is read-only after being
  // initialized from main thread (which happens before //content and embedders
  // are kicked off and enabled to call the BrowserThread API from other
  // threads).
  for (int i = 0; i < ID_COUNT; ++i) {
    if (globals.task_runners[i] &&
        globals.task_runners[i]->RunsTasksInCurrentSequence()) {
      *identifier = static_cast<ID>(i);
      return true;
    }
  }

  return false;
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
BrowserThread::GetTaskRunnerForThread(ID identifier) {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return base::CreateSingleThreadTaskRunner({identifier});
}

// static
void BrowserThread::RunAllPendingTasksOnThreadForTesting(ID identifier) {
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(identifier);
}

// static
void BrowserThread::PostBestEffortTask(
    const base::Location& from_here,
    scoped_refptr<base::TaskRunner> task_runner,
    base::OnceClosure task) {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO, base::TaskPriority::BEST_EFFORT},
      base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                     std::move(task_runner), from_here, std::move(task)));
}

}  // namespace content
