// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_THREAD_WEB_THREAD_H_
#define IOS_WEB_PUBLIC_THREAD_WEB_THREAD_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"

namespace base {
class Location;
}

namespace web {

class WebThreadDelegate;

// Use DCHECK_CURRENTLY_ON(WebThread::ID) to assert that a function can only be
// called on the named WebThread.
#define DCHECK_CURRENTLY_ON(thread_identifier)              \
  (DCHECK(::web::WebThread::CurrentlyOn(thread_identifier)) \
   << ::web::WebThread::GetDCheckCurrentlyOnErrorMessage(thread_identifier))

///////////////////////////////////////////////////////////////////////////////
// WebThread
//
// Utility functions for threads that are known by name. For example, there is
// one IO thread for the entire process, and various pieces of code find it
// useful to retrieve a pointer to the IO thread's message loop.
//
// See web_task_traits.h for posting Tasks to a WebThread.
//
// This class automatically handles the lifetime of different threads. You
// should never need to cache pointers to MessageLoops, since they're not thread
// safe.
class WebThread {
 public:
  // An enumeration of the well-known threads.
  // NOTE: threads must be listed in the order of their life-time, with each
  // thread outliving every other thread below it.
  enum ID {
    // The main thread in the browser.
    UI,

    // This is the thread that processes non-blocking IO, i.e. IPC and network.
    // Blocking IO should happen in ThreadPool.
    IO,

    // NOTE: do not add new threads here. Instead you should just use
    // base::Create*TaskRunner to run tasks on the ThreadPool.

    // This identifier does not represent a thread.  Instead it counts the
    // number of well-known threads.  Insert new well-known threads before this
    // identifier.
    ID_COUNT
  };

  // NOTE: Task posting APIs have moved to post_task.h. See web_task_traits.h.

  // Delete/ReleaseSoon() helpers allow future deletion of an owned object on
  // its associated thread. If you already have a task runner bound to a
  // WebThread you should use its SequencedTaskRunner::DeleteSoon() member
  // method. If you don't, the helpers below avoid having to do
  // base::CreateSingleThreadTaskRunner({WebThread::ID})->DeleteSoon(...) which
  // is equivalent.

  template <class T>
  static bool DeleteSoon(ID identifier,
                         const base::Location& from_here,
                         const T* object) {
    return GetTaskRunnerForThread(identifier)->DeleteSoon(from_here, object);
  }

  // Callable on any thread.  Returns whether the given well-known thread is
  // initialized.
  static bool IsThreadInitialized(ID identifier) WARN_UNUSED_RESULT;

  // Callable on any thread.  Returns whether execution is currently on the
  // given thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON() macro above.
  static bool CurrentlyOn(ID identifier) WARN_UNUSED_RESULT;

  // If the current message loop is one of the known threads, returns true and
  // sets identifier to its ID.
  static bool GetCurrentThreadIdentifier(ID* identifier) WARN_UNUSED_RESULT;

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

  WebThread() {}
  DISALLOW_COPY_AND_ASSIGN(WebThread);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_THREAD_WEB_THREAD_H_
