// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_MAIN_PARTS_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/types/strong_alias.h"
#include "content/common/content_export.h"

namespace base {
class RunLoop;
}

namespace content {

// This class contains different "stages" to be executed by |BrowserMain()|.
//
// Stages:
//
//  ** Cross-platform startup stages.
//  ** Invoked during BrowserMainRunnerImpl::Initialize(), after
//     ContentMainRunner's full initialization.
//
//  - PreEarlyInitialization: things to be be done as soon as possible on
//    program start (such as setting up signal handlers; checking auto-restarts
//    on update; etc.). Core APIs like base::FeatureList,
//    base::SingleThreadTaskRunner::CurrentDefaultHandle, and base::ThreadPool
//    are already functional at this point (ThreadPool will accept but not run
//    tasks until PostCreateThreads).
//
//  - PostEarlyInitialization: things to be be done as soon as possible but that
//    can/must wait until after the few things in BrowserMainLoop's own
//    EarlyInitialization have completed.
//
//  - ToolkitInitialized: similar to PostEarlyInitialization but for the UI
//    toolkit. Allows an embedder to do any extra toolkit initialization.
//
//  - PreCreateMainMessageLoop: things to be done at some generic time before
//    the creation of the main message loop.
//
//  - PostCreateMainMessageLoop: things to be done as early as possible but that
//    need the main message loop to be around (i.e. BrowserThread::UI is up).
//
//  - PreCreateThreads: things that don't need to happen super early but still
//    need to happen during single-threaded initialization (e.g. immutable
//    Singletons that are initialized once and read-only from all threads
//    thereafter).
//    Note: other threads might exist before this point but no child threads
//    owned by content. As such, this is still "single-threaded" initialization
//    as far as content and its embedders are concerned and the right place to
//    initialize thread-compatible objects:
//    https://chromium.googlesource.com/chromium/src/+/main/docs/threading_and_tasks.md#threading-lexicon
//
//  - PostCreateThreads: things that should be done as early as possible but
//    need browser process threads to be alive (i.e. BrowserThread::IO is up and
//    base::ThreadPool is running tasks).
//
//  - PreMainMessageLoopRun: IN DOUBT, PUT THINGS HERE. At this stage all core
//    APIs have been initialized. Services that must be initialized before the
//    browser is considered functional can be initialized from here. Ideally
//    only the frontend is initialized here while the backend takes advantage of
//    a base::ThreadPool worker to come up asynchronously. Things that must
//    happen on the main thread eventually but don't need to block startup
//    should post a BEST_EFFORT task from this stage.
//
//  ** End of cross-platform startup stages.
//  ** Stages above are run as part of startup stages in
//  ** BrowserMainLoop::CreateStartupTasks() and can even be run eagerly (e.g.
//  ** Android app warmup attempts to run these async.
//
//  - WillRunMainMessageLoop: The main thread's RunLoop will be run
//    *immediately* upon returning from this method. While PreMainMessageLoopRun
//    gives that impression, in practice it's part of initialization phases
//    which are triggered independently from MainMessageLoopRun (and can even
//    happen async). In browser tests, PreMainMessageLoopRun() will run before
//    entering test bodies whereas WillRunMainMessageLoop() won't (the control
//    is given to the test rather running the loop). Furthermore, this is only
//    called on platforms where BrowserMainLoop::RunMainMessageLoop is called.
//    Thus, very few things should be done at this stage. It's mostly intended
//    as a way for embedders to override or cancel the default RunLoop if
//    needed.
//
//  - OnFirstIdle: The main thread reached idle for the first time since
//    WillRunMainMessageLoop(). In other words, it's done running any tasks
//    posted as part of the above phases and anything else posted from these.
//
//  - PostMainMessageLoopRun: stop and cleanup things that can/should be cleaned
//    up while base::ThreadPool and BrowserThread::IO are still running.
//    Note: Also see BrowserMainLoop::ShutdownThreadsAndCleanUp() which is often
//    a good fit to stop services (PostMainMessageLoopRun() is called from it).
//
//  - PostDestroyThreads: stop and cleanup things that need to be cleaned up in
//    the single-threaded teardown phase (i.e. typically things that had to
//    created in PreCreateThreads()).
//
//
// How to add stuff (to existing parts):
//  - Figure out when your new code should be executed. What must happen
//    before/after your code is executed? Are there performance reasons for
//    running your code at a particular time? Document these things!
//  - Split out any platform-specific bits. Please avoid #ifdefs it at all
//    possible. You have two choices for platform-specific code: (1) Execute it
//    from one of the |Pre/Post...()| methods in an embedder's platform-specific
//    override (e.g., ChromeBrowserMainPartsWin::PreCreateMainMessageLoop()); do
//    this if the code is unique to an embedder and platform type. Or (2)
//    execute it from one of the "stages" (e.g.,
//    |BrowserMainLoop::EarlyInitialization()|) and provide platform-specific
//    implementations of your code (in a virtual method); do this if you need to
//    provide different implementations across most/all platforms.
//  - Unless your new code is just one or two lines, put it into a separate
//    method with a well-defined purpose. (Likewise, if you're adding to an
//    existing chunk which makes it longer than one or two lines, please move
//    the code out into a separate method.)
//
class CONTENT_EXPORT BrowserMainParts {
 public:
  BrowserMainParts() {}
  virtual ~BrowserMainParts() {}

  // See class comment above for a description of each phase.
  //
  // A return value other than RESULT_CODE_NORMAL_EXIT on any of these methods
  // indicates an error, aborts startup, and is used as the exit status.
  virtual int PreEarlyInitialization();
  virtual void PostEarlyInitialization() {}
  virtual void ToolkitInitialized() {}
  virtual void PreCreateMainMessageLoop() {}
  virtual void PostCreateMainMessageLoop() {}
  virtual int PreCreateThreads();
  virtual void PostCreateThreads() {}
  virtual int PreMainMessageLoopRun();

  // This method returns true by default, telling InterceptMainMessageLoopRun
  // that it should attempt to intercept the main message loop run. Overriding
  // it enables the embedder to conditionally cancel that attempt and the
  // message loop run itself(by returning false). This is key in some
  // integration tests that verify early exit by testing that the test body
  // (entered when the main message loop run is intercepted) is never entered.
  // On Android, BrowserMainLoop never enters MainMessageLoopRun() but this
  // method is still relevant to control whether InterceptMainMessageLoopRun()
  // is allowed to take control of the browser main loop (browser tests).
  virtual bool ShouldInterceptMainMessageLoopRun();

  // This gives BrowserMainParts one last opportunity to tweak the upcoming main
  // message loop run. The embedder may replace |run_loop| to alter the default
  // RunLoop about to be run (must not be nullified, override
  // ShouldInterceptMainMessageLoopRun to cancel the run). Note: This point is
  // never reached on Android as it never invokes MainMessageLoopRun(),
  // InterceptMainMessageLoopRun() is Android's last chance at altering the
  // default native loop run.
  virtual void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) {}

  virtual void OnFirstIdle() {}
  virtual void PostMainMessageLoopRun() {}
  virtual void PostDestroyThreads() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_MAIN_PARTS_H_
