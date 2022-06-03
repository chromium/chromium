// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_THREAD_IMPL_H_
#define IOS_WEB_WEB_THREAD_IMPL_H_

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "ios/web/public/thread/web_thread.h"

namespace web {

class TestWebThread;
class WebMainLoop;
class WebSubThread;

// WebThreadImpl is a scoped object which maps a SingleThreadTaskRunner to a
// WebThread::ID. On ~WebThreadImpl() that ID enters a SHUTDOWN state
// (in which WebThread::IsThreadInitialized() returns false) but the mapping
// isn't undone to avoid shutdown races (the task runner is free to stop
// accepting tasks however).
//
// Very few users should use this directly. To mock WebThreads, tests should
// use WebTaskEnvironment instead.
class WebThreadImpl : public WebThread {
 public:
  ~WebThreadImpl();

  // Returns the thread name for |identifier|.
  static const char* GetThreadName(WebThread::ID identifier);

  // Creates and registers a TaskExecutor that facilitates posting tasks to a
  // WebThread via //base/task/post_task.h.
  static void CreateTaskExecutor();

  // Unregister and delete the TaskExecutor after a test.
  static void ResetTaskExecutorForTesting();

  // Resets globals for |identifier|. Used in tests to clear global state that
  // would otherwise leak to the next test. Globals are not otherwise fully
  // cleaned up in ~WebThreadImpl() as there are subtle differences between
  // UNINITIALIZED and SHUTDOWN state (e.g. globals.task_runners are kept around
  // on shutdown). Must be called after ~WebThreadImpl() for the given
  // |identifier|.
  //
  // Also unregisters and deletes the TaskExecutor.
  static void ResetGlobalsForTesting(WebThread::ID identifier);

 private:
  // Restrict instantiation to WebSubThread as it performs important
  // initialization that shouldn't be bypassed (except by WebMainLoop for
  // the main thread).
  friend class WebSubThread;
  friend class WebMainLoop;
  // TestWebThread is also allowed to construct this when instantiating fake
  // threads.
  friend class TestWebThread;

  // Binds |identifier| to |task_runner| for the web_thread.h API.
  WebThreadImpl(WebThread::ID identifier,
                scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_THREAD_IMPL_H_
