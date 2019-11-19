// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_thread_impl.h"

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_executor.h"
#include "base/time/time.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread_delegate.h"

namespace web {

namespace {

// An implementation of SingleThreadTaskRunner to be used in conjunction
// with WebThread.
class WebThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit WebThreadTaskRunner(WebThread::ID identifier) : id_(identifier) {}

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return base::PostDelayedTask(from_here, {id_}, std::move(task), delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return base::PostDelayedTask(from_here, {id_, NonNestable()},
                                 std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return WebThread::CurrentlyOn(id_);
  }

 protected:
  ~WebThreadTaskRunner() override {}

 private:
  WebThread::ID id_;
  DISALLOW_COPY_AND_ASSIGN(WebThreadTaskRunner);
};

// A separate helper is used just for the task runners, in order to avoid
// needing to initialize the globals to create a task runner.
struct WebThreadTaskRunners {
  WebThreadTaskRunners() {
    for (int i = 0; i < WebThread::ID_COUNT; ++i) {
      task_runners[i] = new WebThreadTaskRunner(static_cast<WebThread::ID>(i));
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runners[WebThread::ID_COUNT];
};

base::LazyInstance<WebThreadTaskRunners>::Leaky g_task_runners =
    LAZY_INSTANCE_INITIALIZER;

// State of a given WebThread::ID.
enum WebThreadState {
  // WebThread::ID does not exist.
  UNINITIALIZED = 0,
  // WebThread::ID is associated to a TaskRunner and is accepting tasks.
  RUNNING,
  // WebThread::ID no longer accepts tasks.
  SHUTDOWN,
};

struct WebThreadGlobals {
  WebThreadGlobals() {
  }

  // This lock protects |threads| and |states|. Do not read or modify those
  // arrays without holding this lock. Do not block while holding this lock.
  base::Lock lock;

  // This array is protected by |lock|. This array is filled as WebThreadImpls
  // are constructed and depopulated when they are destructed.
  scoped_refptr<base::SingleThreadTaskRunner>
      task_runners[WebThread::ID_COUNT] GUARDED_BY(lock);

  // This array is protected by |lock|. Holds the state of each WebThread::ID.
  WebThreadState states[WebThread::ID_COUNT] GUARDED_BY(lock) = {};
};

base::LazyInstance<WebThreadGlobals>::Leaky g_globals =
    LAZY_INSTANCE_INITIALIZER;

bool PostTaskHelper(WebThread::ID identifier,
                    const base::Location& from_here,
                    base::OnceClosure task,
                    base::TimeDelta delay,
                    bool nestable) NO_THREAD_SAFETY_ANALYSIS {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, WebThread::ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the target thread
  // outlives current thread.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  WebThread::ID current_thread = WebThread::ID_COUNT;
  bool target_thread_outlives_current =
      WebThread::GetCurrentThreadIdentifier(&current_thread) &&
      current_thread >= identifier;

  WebThreadGlobals& globals = g_globals.Get();
  if (!target_thread_outlives_current)
    globals.lock.Acquire();

  const bool accepting_tasks =
      globals.states[identifier] == WebThreadState::RUNNING;
  if (accepting_tasks) {
    base::SingleThreadTaskRunner* task_runner =
        globals.task_runners[identifier].get();
    DCHECK(task_runner);
    if (nestable) {
      task_runner->PostDelayedTask(from_here, std::move(task), delay);
    } else {
      task_runner->PostNonNestableDelayedTask(from_here, std::move(task),
                                              delay);
    }
  }

  if (!target_thread_outlives_current)
    globals.lock.Release();

  return accepting_tasks;
}

const scoped_refptr<base::SequencedTaskRunner>& GetNullTaskRunner() {
  static const base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      null_task_runner;
  return *null_task_runner;
}

// Task executor for UI and IO threads.
class WebThreadTaskExecutor : public base::TaskExecutor {
 public:
  WebThreadTaskExecutor() {}
  ~WebThreadTaskExecutor() override {}

  // base::TaskExecutor implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       const base::TaskTraits& traits,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return PostTaskHelper(
        GetWebThreadIdentifier(traits), from_here, std::move(task), delay,
        traits.GetExtension<WebTaskTraitsExtension>().nestable());
  }

  scoped_refptr<base::TaskRunner> CreateTaskRunner(
      const base::TaskTraits& traits) override {
    return GetTaskRunnerForThread(GetWebThreadIdentifier(traits));
  }

  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunner(
      const base::TaskTraits& traits) override {
    return GetTaskRunnerForThread(GetWebThreadIdentifier(traits));
  }

  scoped_refptr<base::SingleThreadTaskRunner> CreateSingleThreadTaskRunner(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override {
    // It's not possible to request DEDICATED access to a WebThread.
    DCHECK_EQ(thread_mode, base::SingleThreadTaskRunnerThreadMode::SHARED);
    return GetTaskRunnerForThread(GetWebThreadIdentifier(traits));
  }

  const scoped_refptr<base::SequencedTaskRunner>& GetContinuationTaskRunner()
      override {
    NOTREACHED() << "WebThreadTaskExecutor isn't registered via "
                    "base::SetTaskExecutorForCurrentThread";
    return GetNullTaskRunner();
  }

 private:
  WebThread::ID GetWebThreadIdentifier(const base::TaskTraits& traits) {
    DCHECK_EQ(traits.extension_id(), WebTaskTraitsExtension::kExtensionId);
    WebThread::ID id =
        traits.GetExtension<WebTaskTraitsExtension>().web_thread();
    DCHECK_LT(id, WebThread::ID_COUNT);
    DCHECK(!traits.use_current_thread())
        << "WebThreadTaskExecutor isn't registered via "
           "base::SetTaskExecutorForCurrentThread";

    // TODO(crbug.com/872372): Support shutdown behavior on UI/IO threads.
    if (traits.shutdown_behavior_set_explicitly()) {
      if (id == WebThread::UI) {
        DCHECK_EQ(base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
                  traits.shutdown_behavior())
            << "Only SKIP_ON_SHUTDOWN is supported on UI thread.";
      } else if (id == WebThread::IO) {
        DCHECK_EQ(base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                  traits.shutdown_behavior())
            << "Only BLOCK_SHUTDOWN is supported on IO thread.";
      }
    }

    return id;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForThread(
      WebThread::ID identifier) {
    return g_task_runners.Get().task_runners[identifier];
  }
};

// |g_web_thread_task_executor| is intentionally leaked on shutdown.
WebThreadTaskExecutor* g_web_thread_task_executor = nullptr;

}  // namespace

WebThreadImpl::WebThreadImpl(
    ID identifier,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : identifier_(identifier) {
  DCHECK(task_runner);

  if (identifier == WebThread::UI) {
    DCHECK(task_runner->BelongsToCurrentThread());
    // TODO(scheduler-dev): Pass the backing SequenceManager in here to ensure
    // GetContinuationTaskRunner DCHECKS when there's no task running.
    ui_thread_tls_executor_.emplace(nullptr, task_runner);
  }

  WebThreadGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier_, 0);
  DCHECK_LT(identifier_, ID_COUNT);

  DCHECK_EQ(globals.states[identifier_], WebThreadState::UNINITIALIZED);
  globals.states[identifier_] = WebThreadState::RUNNING;

  DCHECK(!globals.task_runners[identifier_]);
  globals.task_runners[identifier_] = std::move(task_runner);
}

WebThreadImpl::~WebThreadImpl() {
  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);

  DCHECK_EQ(globals.states[identifier_], WebThreadState::RUNNING);
  globals.states[identifier_] = WebThreadState::SHUTDOWN;
}

// static
void WebThreadImpl::ResetGlobalsForTesting(WebThread::ID identifier) {
  WebThreadGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK_EQ(globals.states[identifier], WebThreadState::SHUTDOWN);
  globals.states[identifier] = WebThreadState::UNINITIALIZED;
  globals.task_runners[identifier] = nullptr;
}

// Friendly names for the well-known threads.

// static
const char* WebThreadImpl::GetThreadName(WebThread::ID thread) {
  static const char* const kWebThreadNames[WebThread::ID_COUNT] = {
      "Web_UIThread",  // UI
      "Web_IOThread",  // IO
  };

  if (WebThread::UI <= thread && thread < WebThread::ID_COUNT)
    return kWebThreadNames[thread];
  return "Unknown Thread";
}

// static
bool WebThread::IsThreadInitialized(ID identifier) {
  if (!g_globals.IsCreated())
    return false;

  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.states[identifier] == WebThreadState::RUNNING;
}

// static
bool WebThread::CurrentlyOn(ID identifier) {
  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.task_runners[identifier] &&
         globals.task_runners[identifier]->BelongsToCurrentThread();
}

// static
std::string WebThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  std::string actual_name = base::PlatformThread::GetName();
  if (actual_name.empty())
    actual_name = "Unknown Thread";

  std::string result = "Must be called on ";
  result += WebThreadImpl::GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

// static
bool WebThread::GetCurrentThreadIdentifier(ID* identifier) {
  if (!g_globals.IsCreated())
    return false;

  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  for (int i = 0; i < ID_COUNT; ++i) {
    if (globals.task_runners[i] &&
        globals.task_runners[i]->BelongsToCurrentThread()) {
      *identifier = static_cast<ID>(i);
      return true;
    }
  }

  return false;
}

// static
scoped_refptr<base::SingleThreadTaskRunner> WebThread::GetTaskRunnerForThread(
    ID identifier) {
  return g_task_runners.Get().task_runners[identifier];
}

// static
void WebThreadImpl::CreateTaskExecutor() {
  DCHECK(!g_web_thread_task_executor);
  g_web_thread_task_executor = new WebThreadTaskExecutor();
  base::RegisterTaskExecutor(WebTaskTraitsExtension::kExtensionId,
                             g_web_thread_task_executor);
}

// static
void WebThreadImpl::ResetTaskExecutorForTesting() {
  DCHECK(g_web_thread_task_executor);
  base::UnregisterTaskExecutorForTesting(WebTaskTraitsExtension::kExtensionId);
  delete g_web_thread_task_executor;
  g_web_thread_task_executor = nullptr;
}

}  // namespace web
