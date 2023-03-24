// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INIT_WEB_MAIN_PARTS_H_
#define IOS_WEB_PUBLIC_INIT_WEB_MAIN_PARTS_H_

namespace web {

// This class contains different "stages" to be executed by `WebMain()`.
// Each stage is represented by a single WebMainParts method, called from
// the corresponding method in `WebMainLoop` (e.g., EarlyInitialization())
// which does the following:
//  - calls a method (e.g., "PreEarlyInitialization()") which implements
//    platform / tookit specific code for that stage.
//  - calls various methods for things common to all platforms (for that stage).
//  - calls a method (e.g., "PostEarlyInitialization()") for platform-specific
//    code to be called after the common code.
//
// Stages:
//
//  - EarlyInitialization: things which should be done as soon as possible on
//    program start (such as setting up signal handlers)
//
//  - PreCreateMainMessageLoop: things to be done at some generic time before
//    the creation of the main message loop.
//
//  - PostCreateMainMessageLoop: things that should be done as early as possible
//    but need the main message loop to be around (i.e. APIs like
//    SingleThreadTaskRunner::CurrentDefaultHandle, WebThread::UI are up).
//
//  - PreCreateThreads: things that don't need to happen super early but still
//    need to happen during single-threaded initialization (e.g. immutable
//    Singletons that are initialized once and read-only from all threads
//    thereafter).
//    Note: other threads might exist before this point but no child threads
//    owned by //ios. As such, this is still "single-threaded" initialization
//    as far as //ios is concerned and the right place to initialize
//    thread-compatible objects:
//    https://chromium.googlesource.com/chromium/src/+/main/docs/threading_and_tasks.md#threading-lexicon
//
//  - PreMainMessageLoopRun: in doubt, put things here. At this stage all core
//    APIs have been initialized. Services that must be initialized before the
//    browser is considered functional can be initialized from here. Ideally
//    only the frontend is initialized here while the backend takes advantage of
//    a base::ThreadPool worker to come up asynchronously. Things that must
//    happen on the main thread eventually but don't need to block startup
//    should post a BEST_EFFORT task from this stage.
//
//  - PostMainMessageLoopRun: stop and cleanup things that can/should be cleaned
//    up while base::ThreadPool and WebThread::IO are still running.
//    Note: Also see WebMainLoop::ShutdownThreadsAndCleanUp() which is often a
//    good fit to stop services (PostMainMessageLoopRun() is called from it).
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
//  - Unless your new code is just one or two lines, put it into a separate
//    method with a well-defined purpose. (Likewise, if you're adding to an
//    existing chunk which makes it longer than one or two lines, please move
//    the code out into a separate method.)
//
class WebMainParts {
 public:
  WebMainParts() {}
  virtual ~WebMainParts() {}

  // See class comment above for a description of each phase.
  virtual void PreEarlyInitialization() {}
  virtual void PostEarlyInitialization() {}
  virtual void PreCreateMainMessageLoop() {}
  virtual void PostCreateMainMessageLoop() {}
  virtual void PreCreateThreads() {}
  virtual void PostCreateThreads() {}
  virtual void PreMainMessageLoopRun() {}
  virtual void PostMainMessageLoopRun() {}
  virtual void PostDestroyThreads() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_INIT_WEB_MAIN_PARTS_H_
