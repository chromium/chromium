// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_thread_impl.h"

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_executor.h"
#include "base/threading/thread_restrictions.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread_delegate.h"
#include "net/url_request/url_fetcher.h"

namespace web {

namespace {

// Friendly names for the well-known threads.
const char* const g_web_thread_names[WebThread::ID_COUNT] = {
    "Web_UIThread",                // UI
    "Web_IOThread",                // IO
};

static const char* GetThreadName(WebThread::ID thread) {
  if (WebThread::UI <= thread && thread < WebThread::ID_COUNT)
    return g_web_thread_names[thread];
  return "Unknown Thread";
}

// An implementation of SingleThreadTaskRunner to be used in conjunction
// with WebThread.
class WebThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit WebThreadTaskRunner(WebThread::ID identifier) : id_(identifier) {}

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return base::PostDelayedTaskWithTraits(from_here, {id_}, std::move(task),
                                           delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return base::PostDelayedTaskWithTraits(from_here, {id_, NonNestable()},
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

struct WebThreadGlobals {
  WebThreadGlobals() {
    memset(threads, 0, WebThread::ID_COUNT * sizeof(threads[0]));
    memset(thread_delegates, 0,
           WebThread::ID_COUNT * sizeof(thread_delegates[0]));
  }

  // This lock protects |threads|. Do not read or modify that array
  // without holding this lock. Do not block while holding this lock.
  base::Lock lock;

  // This array is protected by |lock|. The threads are not owned by this
  // array. Typically, the threads are owned on the UI thread by
  // WebMainLoop. WebThreadImpl objects remove themselves from this
  // array upon destruction.
  WebThreadImpl* threads[WebThread::ID_COUNT];

  // Only atomic operations are used on this array. The delegates are not owned
  // by this array, rather by whoever calls WebThread::SetDelegate.
  WebThreadDelegate* thread_delegates[WebThread::ID_COUNT];
};

base::LazyInstance<WebThreadGlobals>::Leaky g_globals =
    LAZY_INSTANCE_INITIALIZER;

bool PostTaskHelper(WebThread::ID identifier,
                    const base::Location& from_here,
                    base::OnceClosure task,
                    base::TimeDelta delay,
                    bool nestable) {
  DCHECK(identifier >= 0 && identifier < WebThread::ID_COUNT);
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

  base::MessageLoop* message_loop =
      globals.threads[identifier] ? globals.threads[identifier]->message_loop()
                                  : nullptr;
  if (message_loop) {
    if (nestable) {
      message_loop->task_runner()->PostDelayedTask(from_here, std::move(task),
                                                   delay);
    } else {
      message_loop->task_runner()->PostNonNestableDelayedTask(
          from_here, std::move(task), delay);
    }
  }

  if (!target_thread_outlives_current)
    globals.lock.Release();

  return !!message_loop;
}

class WebThreadTaskExecutor : public base::TaskExecutor {
 public:
  WebThreadTaskExecutor() {}
  ~WebThreadTaskExecutor() override {}

  // base::TaskExecutor implementation.
  bool PostDelayedTaskWithTraits(const base::Location& from_here,
                                 const base::TaskTraits& traits,
                                 base::OnceClosure task,
                                 base::TimeDelta delay) override {
    return PostTaskHelper(
        GetWebThreadIdentifier(traits), from_here, std::move(task), delay,
        traits.GetExtension<WebTaskTraitsExtension>().nestable());
  }

  scoped_refptr<base::TaskRunner> CreateTaskRunnerWithTraits(
      const base::TaskTraits& traits) override {
    return GetTaskRunnerForThread(GetWebThreadIdentifier(traits));
  }

  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunnerWithTraits(
      const base::TaskTraits& traits) override {
    return GetTaskRunnerForThread(GetWebThreadIdentifier(traits));
  }

  scoped_refptr<base::SingleThreadTaskRunner>
  CreateSingleThreadTaskRunnerWithTraits(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override {
    // It's not possible to request DEDICATED access to a WebThread.
    DCHECK_EQ(thread_mode, base::SingleThreadTaskRunnerThreadMode::SHARED);
    return GetTaskRunnerForThread(GetWebThreadIdentifier(traits));
  }

 private:
  WebThread::ID GetWebThreadIdentifier(const base::TaskTraits& traits) {
    DCHECK_EQ(traits.extension_id(), WebTaskTraitsExtension::kExtensionId);
    WebThread::ID id =
        traits.GetExtension<WebTaskTraitsExtension>().web_thread();
    DCHECK_LT(id, WebThread::ID_COUNT);
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

WebThreadImpl::WebThreadImpl(ID identifier)
    : Thread(GetThreadName(identifier)), identifier_(identifier) {
  Initialize();
}

WebThreadImpl::WebThreadImpl(ID identifier, base::MessageLoop* message_loop)
    : Thread(GetThreadName(identifier)), identifier_(identifier) {
  SetMessageLoop(message_loop);
  Initialize();
}

void WebThreadImpl::Init() {
  WebThreadGlobals& globals = g_globals.Get();

  using base::subtle::AtomicWord;
  AtomicWord* storage =
      reinterpret_cast<AtomicWord*>(&globals.thread_delegates[identifier_]);
  AtomicWord stored_pointer = base::subtle::NoBarrier_Load(storage);
  WebThreadDelegate* delegate =
      reinterpret_cast<WebThreadDelegate*>(stored_pointer);
  if (delegate) {
    delegate->Init();
  }

  if (WebThread::CurrentlyOn(WebThread::IO)) {
    // Though this thread is called the "IO" thread, it actually just routes
    // messages around; it shouldn't be allowed to perform any blocking disk
    // I/O.
    base::DisallowUnresponsiveTasks();
  }
}

NOINLINE void WebThreadImpl::UIThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void WebThreadImpl::IOThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

void WebThreadImpl::Run(base::RunLoop* run_loop) {
  WebThread::ID thread_id = ID_COUNT;
  if (!GetCurrentThreadIdentifier(&thread_id))
    return Thread::Run(run_loop);

  switch (thread_id) {
    case WebThread::UI:
      return UIThreadRun(run_loop);
    case WebThread::IO:
      return IOThreadRun(run_loop);
    case WebThread::ID_COUNT:
      CHECK(false);  // This shouldn't actually be reached!
      break;
  }
  Thread::Run(run_loop);
}

void WebThreadImpl::CleanUp() {
  if (WebThread::CurrentlyOn(WebThread::IO))
    IOThreadPreCleanUp();

  WebThreadGlobals& globals = g_globals.Get();

  using base::subtle::AtomicWord;
  AtomicWord* storage =
      reinterpret_cast<AtomicWord*>(&globals.thread_delegates[identifier_]);
  AtomicWord stored_pointer = base::subtle::NoBarrier_Load(storage);
  WebThreadDelegate* delegate =
      reinterpret_cast<WebThreadDelegate*>(stored_pointer);

  if (delegate)
    delegate->CleanUp();
}

void WebThreadImpl::Initialize() {
  WebThreadGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK(identifier_ >= 0 && identifier_ < ID_COUNT);
  DCHECK(globals.threads[identifier_] == nullptr);
  globals.threads[identifier_] = this;
}

void WebThreadImpl::IOThreadPreCleanUp() {
  // Kill all things that might be holding onto
  // net::URLRequest/net::URLRequestContexts.

  // Destroy all URLRequests started by URLFetchers.
  net::URLFetcher::CancelAll();
}

WebThreadImpl::~WebThreadImpl() {
  // All Thread subclasses must call Stop() in the destructor. This is
  // doubly important here as various bits of code check they are on
  // the right WebThread.
  Stop();

  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  globals.threads[identifier_] = nullptr;
#ifndef NDEBUG
  // Double check that the threads are ordered correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(!globals.threads[i])
        << "Threads must be listed in the reverse order that they die";
  }
#endif
}

// static
bool WebThread::IsThreadInitialized(ID identifier) {
  if (!g_globals.IsCreated())
    return false;

  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return globals.threads[identifier] != nullptr;
}

// static
bool WebThread::CurrentlyOn(ID identifier) {
  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return globals.threads[identifier] &&
         globals.threads[identifier]->message_loop() ==
             base::MessageLoop::current();
}

// static
std::string WebThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  std::string actual_name = base::PlatformThread::GetName();
  if (actual_name.empty())
    actual_name = "Unknown Thread";

  std::string result = "Must be called on ";
  result += GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

// static
bool WebThread::GetCurrentThreadIdentifier(ID* identifier) {
  if (!g_globals.IsCreated())
    return false;

  base::MessageLoop* cur_message_loop = base::MessageLoop::current();
  WebThreadGlobals& globals = g_globals.Get();
  for (int i = 0; i < ID_COUNT; ++i) {
    if (globals.threads[i] &&
        globals.threads[i]->message_loop() == cur_message_loop) {
      *identifier = globals.threads[i]->identifier_;
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
void WebThread::SetDelegate(ID identifier, WebThreadDelegate* delegate) {
  using base::subtle::AtomicWord;
  WebThreadGlobals& globals = g_globals.Get();
  AtomicWord* storage =
      reinterpret_cast<AtomicWord*>(&globals.thread_delegates[identifier]);
  AtomicWord old_pointer = base::subtle::NoBarrier_AtomicExchange(
      storage, reinterpret_cast<AtomicWord>(delegate));

  // This catches registration when previously registered.
  DCHECK(!delegate || !old_pointer);
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
