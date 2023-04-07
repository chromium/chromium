// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_THREAD_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_THREAD_H_

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_task_traits.h"

#if defined(UNIT_TEST)
#include "base/logging.h"
#endif

namespace content {

// Use DCHECK_CURRENTLY_ON(BrowserThread::ID) to assert that a function can only
// be called on the named BrowserThread.
#define DCHECK_CURRENTLY_ON(thread_identifier)                       \
  DCHECK(::content::BrowserThread::CurrentlyOn(thread_identifier))   \
      << ::content::BrowserThread::GetDCheckCurrentlyOnErrorMessage( \
             thread_identifier)

// The main entry point to post tasks to the UI thread. Tasks posted with the
// same |traits| will run in posting order (i.e. according to the
// SequencedTaskRunner contract). Tasks posted with different |traits| can be
// re-ordered. You may keep a reference to this task runner, it's always
// thread-safe to post to it though it may start returning false at some point
// during shutdown when it definitely is no longer accepting tasks.
//
// In unit tests, there must be a content::BrowserTaskEnvironment in scope for
// this API to be available.
CONTENT_EXPORT scoped_refptr<base::SingleThreadTaskRunner>
GetUIThreadTaskRunner(const BrowserTaskTraits& traits = {});

// The BrowserThread::IO counterpart to GetUIThreadTaskRunner().
CONTENT_EXPORT scoped_refptr<base::SingleThreadTaskRunner>
GetIOThreadTaskRunner(const BrowserTaskTraits& traits = {});

///////////////////////////////////////////////////////////////////////////////
// BrowserThread
//
// Utility functions for threads that are known by a browser-wide name.
class CONTENT_EXPORT BrowserThread {
 public:
  // An enumeration of the well-known threads.
  enum ID {
    // The main thread in the browser. It stops running tasks during shutdown
    // and is never joined.
    UI,

    // This is the thread that processes non-blocking I/O, i.e. IPC and network.
    // Blocking I/O should happen in base::ThreadPool. It is joined on shutdown
    // (and thus any task posted to it may block shutdown).
    IO,

    // NOTE: do not add new threads here. Instead you should just use
    // base::ThreadPool::Create*TaskRunner to run tasks on the base::ThreadPool.

    // This identifier does not represent a thread.  Instead it counts the
    // number of well-known threads.  Insert new well-known threads before this
    // identifier.
    ID_COUNT
  };

  BrowserThread(const BrowserThread&) = delete;
  BrowserThread& operator=(const BrowserThread&) = delete;

  // Delete/ReleaseSoon() helpers allow future deletion of an owned object on
  // its associated thread. If you already have a task runner bound to a
  // BrowserThread you should use its SequencedTaskRunner::DeleteSoon() member
  // method.
  // TODO(1026641): Get rid of the last few callers to these in favor of an
  // explicit call to content::GetUIThreadTaskRunner({})->DeleteSoon(...).

  template <class T>
  static bool DeleteSoon(ID identifier,
                         const base::Location& from_here,
                         const T* object) {
    return GetTaskRunnerForThread(identifier)->DeleteSoon(from_here, object);
  }

  template <class T>
  static bool DeleteSoon(ID identifier,
                         const base::Location& from_here,
                         std::unique_ptr<T> object) {
    return DeleteSoon(identifier, from_here, object.release());
  }

  template <class T>
  static void ReleaseSoon(ID identifier,
                          const base::Location& from_here,
                          scoped_refptr<T>&& object) {
    GetTaskRunnerForThread(identifier)
        ->ReleaseSoon(from_here, std::move(object));
  }

  // Posts a |task| to run at BEST_EFFORT priority using an arbitrary
  // |task_runner| for which we do not control the priority.
  //
  // This is useful when a task needs to run on |task_runner| (for thread-safety
  // reasons) but should be delayed until after critical phases (e.g. startup).
  // TODO(crbug.com/793069): Add support for sequence-funneling and remove this
  // method.
  static void PostBestEffortTask(const base::Location& from_here,
                                 scoped_refptr<base::TaskRunner> task_runner,
                                 base::OnceClosure task);

  // Callable on any thread.  Returns whether the given well-known thread is
  // initialized.
  [[nodiscard]] static bool IsThreadInitialized(ID identifier);

  // Callable on any thread.  Returns whether you're currently on a particular
  // thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON() macro above.
  [[nodiscard]] static bool CurrentlyOn(ID identifier);

  // If the current message loop is one of the known threads, returns true and
  // sets identifier to its ID.  Otherwise returns false.
  [[nodiscard]] static bool GetCurrentThreadIdentifier(ID* identifier);

  // Use these templates in conjunction with RefCountedThreadSafe or scoped_ptr
  // when you want to ensure that an object is deleted on a specific thread.
  // This is needed when an object can hop between threads (i.e. UI -> IO ->
  // UI), and thread switching delays can mean that the final UI tasks executes
  // before the IO task's stack unwinds. This would lead to the object
  // destructing on the IO thread, which often is not what you want (i.e. to
  // unregister from NotificationService, to notify other objects on the
  // creating thread etc). Note: see base::OnTaskRunnerDeleter and
  // base::RefCountedDeleteOnSequence to bind to SequencedTaskRunner instead of
  // specific BrowserThreads.
  template <ID thread>
  struct DeleteOnThread {
    template <typename T>
    static void Destruct(const T* x) {
      if (CurrentlyOn(thread)) {
        delete x;
      } else {
        if (!DeleteSoon(thread, FROM_HERE, x)) {
#if defined(UNIT_TEST)
          // Only logged under unit testing because leaks at shutdown
          // are acceptable under normal circumstances.
          LOG(ERROR) << "DeleteSoon failed on thread " << thread;
#endif  // UNIT_TEST
        }
      }
    }
    template <typename T>
    inline void operator()(T* ptr) const {
      enum { type_must_be_complete = sizeof(T) };
      Destruct(ptr);
    }
  };

  // Sample usage with RefCountedThreadSafe:
  // class Foo
  //     : public base::RefCountedThreadSafe<
  //           Foo, BrowserThread::DeleteOnIOThread> {
  //
  // ...
  //  private:
  //   friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  //   friend class base::DeleteHelper<Foo>;
  //
  //   ~Foo();
  //
  // Sample usage with scoped_ptr:
  // std::unique_ptr<Foo, BrowserThread::DeleteOnIOThread> ptr;
  //
  // Note: see base::OnTaskRunnerDeleter and base::RefCountedDeleteOnSequence to
  // bind to SequencedTaskRunner instead of specific BrowserThreads.
  struct DeleteOnUIThread : public DeleteOnThread<UI> {};
  struct DeleteOnIOThread : public DeleteOnThread<IO> {};

  // Returns an appropriate error message for when DCHECK_CURRENTLY_ON() fails.
  static std::string GetDCheckCurrentlyOnErrorMessage(ID expected);

  // Runs all pending tasks for the given thread. Tasks posted after this method
  // is called (in particular any task posted from within any of the pending
  // tasks) will be queued but not run. Conceptually this call will disable all
  // queues, run any pending tasks, and re-enable all the queues.
  //
  // If any of the pending tasks posted a task, these could be run by calling
  // this method again or running a regular RunLoop. But if that were the case
  // you should probably rewrite you tests to wait for a specific event instead.
  //
  // NOTE: Can only be called from the UI thread.
  static void RunAllPendingTasksOnThreadForTesting(ID identifier);

  // Helper that returns GetUIThreadTaskRunner({}) or GetIOThreadTaskRunner({})
  // based on |identifier|. Requires that the BrowserThread with the provided
  // |identifier| was started.
  static scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForThread(
      ID identifier);

 private:
  friend class BrowserThreadImpl;
  BrowserThread() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_THREAD_H_
