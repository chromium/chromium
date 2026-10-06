// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/test/run_inside_nsapplication_run_mac.h"

#import <AppKit/AppKit.h>

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"

namespace content {

namespace {

// Stops the -[NSApplication run] that is lowest on the stack.
void StopNSApplication() {
  [NSApp stop:nil];
  // -stop: only takes effect after the next event has been processed, so post
  // a dummy event to wake up the event loop.
  [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                       subtype:0
                                         data1:0
                                         data2:0]
           atStart:YES];
}

}  // namespace

void RunInsideNSApplicationRun(base::OnceClosure closure) {
  CHECK(NSThread.isMainThread);
  CHECK(NSApp);
  CHECK(!NSApp.running);

  // Blocks can't capture move-only C++ objects, so capture pointers instead.
  // Both outlive -[NSApplication run] below.
  base::OnceClosure* closure_ptr = &closure;
  bool did_run = false;
  bool* did_run_ptr = &did_run;

  // CFRunLoopPerformBlock() is used rather than dispatch_async() to the main
  // queue, because the main queue is serial: while `closure` runs inside a
  // main queue block, nested run loops spun by `closure` could not service the
  // main queue. It is also used rather than posting an NSEvent, so that
  // `closure` doesn't run inside -[NSApplication sendEvent:].
  CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopDefaultMode, ^{
    CHECK(NSApp.running);
    *did_run_ptr = true;
    std::move(*closure_ptr).Run();
    StopNSApplication();
  });
  [NSApp run];
  CHECK(did_run);
}

}  // namespace content
