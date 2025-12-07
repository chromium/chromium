// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_thread_impl.h"

#include <array>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread_delegate.h"

namespace web {

namespace {

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
  static WebThreadGlobals& Get() {
    static base::NoDestructor<WebThreadGlobals> instance;
    return *instance;
  }

  static bool IsCreated() { return is_created; }

  // This lock protects `threads` and `states`. Do not read or modify those
  // arrays without holding this lock. Do not block while holding this lock.
  base::Lock lock;

  // This array is protected by `lock`. This array is filled as WebThreadImpls
  // are constructed and depopulated when they are destructed.
  std::array<scoped_refptr<base::SingleThreadTaskRunner>, WebThread::ID_COUNT>
      task_runners GUARDED_BY(lock);

  // This array is protected by `lock`. Holds the state of each WebThread::ID.
  std::array<WebThreadState, WebThread::ID_COUNT> states GUARDED_BY(lock) = {
      UNINITIALIZED,
      UNINITIALIZED,
  };

 private:
  friend class base::NoDestructor<WebThreadGlobals>;

  static bool is_created;

  WebThreadGlobals() { is_created = true; }
};

bool WebThreadGlobals::is_created = false;

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

  WebThreadGlobals& globals = WebThreadGlobals::Get();
  if (!target_thread_outlives_current) {
    globals.lock.Acquire();
  }

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

  if (!target_thread_outlives_current) {
    globals.lock.Release();
  }

  return accepting_tasks;
}

// An implementation of SingleThreadTaskRunner to be used in conjunction
// with WebThread.
class WebThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit WebThreadTaskRunner(WebThread::ID identifier) : id_(identifier) {}

  WebThreadTaskRunner(const WebThreadTaskRunner&) = delete;
  WebThreadTaskRunner& operator=(const WebThreadTaskRunner&) = delete;

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return PostTaskHelper(id_, from_here, std::move(task), delay,
                          true /* nestable */);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return PostTaskHelper(id_, from_here, std::move(task), delay,
                          false /* nestable */);
  }

  bool RunsTasksInCurrentSequence() const override {
    return WebThread::CurrentlyOn(id_);
  }

 protected:
  ~WebThreadTaskRunner() override {}

 private:
  WebThread::ID id_;
};

class WebThreadTaskExecutor {
 public:
  WebThreadTaskExecutor() {}
  ~WebThreadTaskExecutor() {}

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      WebThread::ID identifier,
      const base::TaskTraits& traits) const {
    // //TODO(crbug.com/40217644): Unlike content, iOS never honored
    // `traits.priority()`... but this is where it could.
    // Ref. content::BaseBrowserTaskExecutor::GetTaskRunner()
    switch (identifier) {
      case WebThread::UI:
        return ui_thread_task_runner_;
      case WebThread::IO:
        return io_thread_task_runner_;
      case WebThread::ID_COUNT:
        NOTREACHED();
    }
  }

  // A static getter that also verifies the instance was set and warns of steps
  // to take if not.
  static const WebThreadTaskExecutor* GetInstance() {
    DCHECK(g_instance)
        << "No web task executor created.\nHint: if this is in a unit test, "
           "you're likely missing a WebTaskEnvironment member in your "
           "fixture.";
    return g_instance;
  }

  // Creates the WebThreadTaskExecutor instance. It is intentionally leaked on
  // shutdown, except in unit tests which can reset it via
  // ResetInstanceForTesting().
  static void CreateInstance() {
    DCHECK(!g_instance);
    g_instance = new WebThreadTaskExecutor();
  }

  static void ResetInstanceForTesting() {
    DCHECK(g_instance);
    delete g_instance;
    g_instance = nullptr;
  }

 private:
  static WebThreadTaskExecutor* g_instance;

  scoped_refptr<WebThreadTaskRunner> ui_thread_task_runner_ =
      base::MakeRefCounted<WebThreadTaskRunner>(WebThread::UI);
  scoped_refptr<WebThreadTaskRunner> io_thread_task_runner_ =
      base::MakeRefCounted<WebThreadTaskRunner>(WebThread::IO);
};

// static
WebThreadTaskExecutor* WebThreadTaskExecutor::g_instance = nullptr;

}  // namespace

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadImpl::GetUIThreadTaskRunner(const WebTaskTraits& traits) {
  return WebThreadTaskExecutor::GetInstance()->GetTaskRunner(WebThread::UI,
                                                             traits);
}

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadImpl::GetIOThreadTaskRunner(const WebTaskTraits& traits) {
  return WebThreadTaskExecutor::GetInstance()->GetTaskRunner(WebThread::IO,
                                                             traits);
}

WebThreadImpl::WebThreadImpl(
    ID identifier,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : identifier_(identifier) {
  DCHECK(task_runner);

  WebThreadGlobals& globals = WebThreadGlobals::Get();

  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier_, 0);
  DCHECK_LT(identifier_, ID_COUNT);

  DCHECK_EQ(globals.states[identifier_], WebThreadState::UNINITIALIZED);
  globals.states[identifier_] = WebThreadState::RUNNING;

  DCHECK(!globals.task_runners[identifier_]);
  globals.task_runners[identifier_] = std::move(task_runner);
}

WebThreadImpl::~WebThreadImpl() {
  WebThreadGlobals& globals = WebThreadGlobals::Get();
  base::AutoLock lock(globals.lock);

  DCHECK_EQ(globals.states[identifier_], WebThreadState::RUNNING);
  globals.states[identifier_] = WebThreadState::SHUTDOWN;
}

// static
void WebThreadImpl::ResetGlobalsForTesting(WebThread::ID identifier) {
  WebThreadGlobals& globals = WebThreadGlobals::Get();

  base::AutoLock lock(globals.lock);
  DCHECK_EQ(globals.states[identifier], WebThreadState::SHUTDOWN);
  globals.states[identifier] = WebThreadState::UNINITIALIZED;
  globals.task_runners[identifier] = nullptr;
}
// Friendly names for the well-known threads.

// static
const char* WebThreadImpl::GetThreadName(WebThread::ID thread) {
  static constexpr std::array<const char*, WebThread::ID_COUNT>
      kWebThreadNames = {
          "Web_UIThread",  // UI
          "Web_IOThread",  // IO
      };

  if (WebThread::UI <= thread && thread < WebThread::ID_COUNT) {
    return kWebThreadNames[thread];
  }
  return "Unknown Thread";
}

// static
bool WebThreadImpl::IsThreadInitialized(ID identifier) {
  if (!WebThreadGlobals::IsCreated()) {
    return false;
  }

  WebThreadGlobals& globals = WebThreadGlobals::Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.states[identifier] == WebThreadState::RUNNING;
}

// static
bool WebThreadImpl::CurrentlyOn(ID identifier) {
  WebThreadGlobals& globals = WebThreadGlobals::Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.task_runners[identifier] &&
         globals.task_runners[identifier]->BelongsToCurrentThread();
}

// static
std::string WebThreadImpl::GetCurrentlyOnErrorMessage(ID expected) {
  std::string actual_name = base::PlatformThread::GetName();
  if (actual_name.empty()) {
    actual_name = "Unknown Thread";
  }

  std::string result = "Must be called on ";
  result += WebThreadImpl::GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

// static
bool WebThreadImpl::GetCurrentThreadIdentifier(ID* identifier) {
  if (!WebThreadGlobals::IsCreated()) {
    return false;
  }

  WebThreadGlobals& globals = WebThreadGlobals::Get();
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
void WebThreadImpl::CreateTaskExecutor() {
  WebThreadTaskExecutor::CreateInstance();
}

// static
void WebThreadImpl::ResetTaskExecutorForTesting() {
  WebThreadTaskExecutor::ResetInstanceForTesting();
}

}  // namespace web
