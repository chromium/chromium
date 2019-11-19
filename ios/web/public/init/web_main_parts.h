// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INIT_WEB_MAIN_PARTS_H_
#define IOS_WEB_PUBLIC_INIT_WEB_MAIN_PARTS_H_

namespace web {

// This class contains different "stages" to be executed by |WebMain()|.
// Each stage is represented by a single WebMainParts method, called from
// the corresponding method in |WebMainLoop| (e.g., EarlyInitialization())
// which does the following:
//  - calls a method (e.g., "PreEarlyInitialization()") which implements
//    platform / tookit specific code for that stage.
//  - calls various methods for things common to all platforms (for that stage).
//  - calls a method (e.g., "PostEarlyInitialization()") for platform-specific
//    code to be called after the common code.
//
// Stages:
//  - EarlyInitialization: things which should be done as soon as possible on
//    program start (such as setting up signal handlers) and things to be done
//    at some generic time before the start of the main message loop.
//  - MainMessageLoopStart: things beginning with the start of the main message
//    loop and ending with initialization of the main thread; things which
//    should be done immediately before the start of the main message loop
//    should go in |PreMainMessageLoopStart()|.
//  - RunMainMessageLoopParts:  things to be done before and after invoking the
//    main message loop run method (e.g. MessageLoopCurrentForUI::Get()->Run()).
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

  virtual void PreEarlyInitialization() {}

  virtual void PostEarlyInitialization() {}

  virtual void PreMainMessageLoopStart() {}

  virtual void PostMainMessageLoopStart() {}

  // Called just before any child threads owned by the web framework
  // are created.
  //
  // The main message loop has been started at this point (but has not
  // been run).
  virtual void PreCreateThreads() {}

  // This is called just before the main message loop is run.  The
  // various browser threads have all been created at this point
  virtual void PreMainMessageLoopRun() {}

  // This happens after the main message loop has stopped, but before
  // threads are stopped.
  virtual void PostMainMessageLoopRun() {}

  // Called as the very last part of shutdown, after threads have been
  // stopped and destroyed.
  virtual void PostDestroyThreads() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_INIT_WEB_MAIN_PARTS_H_
