// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_THREAD_WEB_THREAD_H_
#define IOS_WEB_PUBLIC_THREAD_WEB_THREAD_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"

namespace base {
class Location;
}

namespace web {

// TODO(crbug.com/1026641): Include web_task_traits.h directly when the
// migration to Get(UI|IO)ThreadTaskrunner() is complete and the cyclic
// dependency of web_task_traits.h on WebThread::ID is broken.
class WebTaskTraits;

class WebThreadDelegate;

// Use DCHECK_CURRENTLY_ON(WebThread::ID) to assert that a function can only be
// called on the named WebThread.
#define DCHECK_CURRENTLY_ON(thread_identifier)             \
  DCHECK(::web::WebThread::CurrentlyOn(thread_identifier)) \
      << ::web::WebThread::GetDCheckCurrentlyOnErrorMessage(thread_identifier)

// The main entry point to post tasks to the UI thread. Tasks posted with the
// same `traits` will run in posting order (i.e. according to the
// SequencedTaskRunner contract). Tasks posted with different `traits` can be
// re-ordered. You may keep a reference to this task runner, it's always
// thread-safe to post to it though it may start returning false at some point
// during shutdown when it definitely is no longer accepting tasks.
//
// In unit tests, there must be a WebTaskEnvironment in scope for this API to be
// available.
//
// TODO(crbug.com/1026641): Make default traits |{}| the default param when it's
// possible to include web_task_traits.h in this file (see note above on the
// WebTaskTraits fwd-decl).
scoped_refptr<base::SingleThreadTaskRunner> GetUIThreadTaskRunner(
    const WebTaskTraits& traits);

// The WebThread::IO counterpart to GetUIThreadTaskRunner().
scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner(
    const WebTaskTraits& traits);

///////////////////////////////////////////////////////////////////////////////
// WebThread
//
// Utility functions for threads that are known by name.
class WebThread {
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

  WebThread(const WebThread&) = delete;
  WebThread& operator=(const WebThread&) = delete;

  // Delete/ReleaseSoon() helpers allow future deletion of an owned object on
  // its associated thread. If you already have a task runner bound to a
  // WebThread you should use its SequencedTaskRunner::DeleteSoon() member
  // method.
  // TODO(crbug.com/1026641): Get rid of the last few callers to these in favor
  // of an explicit call to web::GetUIThreadTaskRunner({})->DeleteSoon(...).

  template <class T>
  static bool DeleteSoon(ID identifier,
                         const base::Location& from_here,
                         const T* object) {
    return GetTaskRunnerForThread(identifier)->DeleteSoon(from_here, object);
  }

  // Callable on any thread.  Returns whether the given well-known thread is
  // initialized.
  [[nodiscard]] static bool IsThreadInitialized(ID identifier);

  // Callable on any thread.  Returns whether execution is currently on the
  // given thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON() macro above.
  [[nodiscard]] static bool CurrentlyOn(ID identifier);

  // If the current message loop is one of the known threads, returns true and
  // sets identifier to its ID.
  [[nodiscard]] static bool GetCurrentThreadIdentifier(ID* identifier);

  // Sets the delegate for WebThread::IO.
  //
  // This only supports the IO thread.
  //
  // Only one delegate may be registered at a time. Delegates may be
  // unregistered by providing a nullptr pointer.
  //
  // The delegate can only be registered through this call before
  // WebThreadImpl(WebThread::IO) is created and unregistered after
  // it was destroyed and its underlying thread shutdown.
  static void SetIOThreadDelegate(WebThreadDelegate* delegate);

  // Returns an appropriate error message for when DCHECK_CURRENTLY_ON() fails.
  static std::string GetDCheckCurrentlyOnErrorMessage(ID expected);

  // Use these templates in conjunction with RefCountedThreadSafe or
  // std::unique_ptr when you want to ensure that an object is deleted on a
  // specific thread. This is needed when an object can hop between threads
  // (i.e. IO -> UI -> IO), and thread switching delays can mean that the final
  // IO tasks executes before the UI task's stack unwinds. This would lead to
  // the object destructing on the UI thread, which often is not what you want
  // (i.e. to unregister from NotificationService, to notify other objects on
  // the creating thread etc).
  template <ID thread>
  struct DeleteOnThread {
    template <typename T>
    static void Destruct(const T* x) {
      if (CurrentlyOn(thread)) {
        delete x;
      } else {
        if (!DeleteSoon(thread, FROM_HERE, x)) {
          // Leaks at shutdown are acceptable under normal circumstances,
          // do not report.
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
  //           Foo, web::WebThread::DeleteOnIOThread> {
  //
  // ...
  //  private:
  //   friend struct web::WebThread::DeleteOnThread<web::WebThread::IO>;
  //   friend class base::DeleteHelper<Foo>;
  //
  //   ~Foo();
  //
  // Sample usage with std::unique_ptr:
  // std::unique_ptr<Foo, web::WebThread::DeleteOnIOThread> ptr;
  struct DeleteOnUIThread : public DeleteOnThread<UI> {};
  struct DeleteOnIOThread : public DeleteOnThread<IO> {};

 private:
  friend class WebThreadImpl;

  // For DeleteSoon() only.
  static scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForThread(
      ID identifier);

  WebThread() = default;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_THREAD_WEB_THREAD_H_
