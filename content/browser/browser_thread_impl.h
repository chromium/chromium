// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
#define CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/optional.h"
#endif

namespace content {

class BrowserMainLoop;
class BrowserProcessSubThread;
class TestBrowserThread;

// BrowserThreadImpl is a scoped object which maps a SingleThreadTaskRunner to a
// BrowserThread::ID. On ~BrowserThreadImpl() that ID enters a SHUTDOWN state
// (in which BrowserThread::IsThreadInitialized() returns false) but the mapping
// isn't undone to avoid shutdown races (the task runner is free to stop
// accepting tasks by then however).
//
// Very few users should use this directly. To mock BrowserThreads, tests should
// use BrowserTaskEnvironment instead.
class CONTENT_EXPORT BrowserThreadImpl : public BrowserThread {
 public:
  ~BrowserThreadImpl();

  // Returns the thread name for |identifier|.
  static const char* GetThreadName(BrowserThread::ID identifier);

  // Resets globals for |identifier|. Used in tests to clear global state that
  // would otherwise leak to the next test. Globals are not otherwise fully
  // cleaned up in ~BrowserThreadImpl() as there are subtle differences between
  // UNINITIALIZED and SHUTDOWN state (e.g. globals.task_runners are kept around
  // on shutdown). Must be called after ~BrowserThreadImpl() for the given
  // |identifier|.
  static void ResetGlobalsForTesting(BrowserThread::ID identifier);

  // Exposed for BrowserTaskExecutor. Other code should use
  // base::CreateSingleThreadTaskRunner({BrowserThread::UI/IO}).
  using BrowserThread::GetTaskRunnerForThread;

 private:
  // Restrict instantiation to BrowserProcessSubThread as it performs important
  // initialization that shouldn't be bypassed (except by BrowserMainLoop for
  // the main thread).
  friend class BrowserProcessSubThread;
  friend class BrowserMainLoop;
  // TestBrowserThread is also allowed to construct this when instantiating fake
  // threads.
  friend class TestBrowserThread;

  // Binds |identifier| to |task_runner| for the browser_thread.h API. This
  // needs to happen on the main thread before //content and embedders are
  // kicked off and enabled to invoke the BrowserThread API from other threads.
  BrowserThreadImpl(BrowserThread::ID identifier,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;

#if defined(OS_POSIX)
  // Allows usage of the FileDescriptorWatcher API on the UI thread.
  base::Optional<base::FileDescriptorWatcher> file_descriptor_watcher_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
