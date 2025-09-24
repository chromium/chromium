// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mac_init.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "content/common/mac/system_policy.h"

namespace content {

void InitializeMac() {
  [NSUserDefaults.standardUserDefaults registerDefaults:@{
    // Exceptions routed to -[NSApplication reportException:] should crash
    // immediately, as opposed being swallowed or presenting UI that gives the
    // user a choice in the matter.
    @"NSApplicationCrashOnExceptions" : @YES,

    // Prevent Cocoa from turning command-line arguments into -[NSApplication
    // application:openFile:], because they are handled directly. @"NO" looks
    // like a mistake, but the value really is supposed to be a string.
    @"NSTreatUnknownArgumentsAsOpen" : @"NO",

    // Don't allow the browser process to enter AppNap. Doing so will result in
    // large number of queued IPCs from renderers, potentially so many that
    // Chrome is unusable for a long period after returning from sleep.
    // https://crbug.com/871235.
    @"NSAppSleepDisabled" : @YES,
  }];

  if (base::mac::MacOSVersion() >= 26'00'00) {
    [NSUserDefaults.standardUserDefaults registerDefaults:@{
      // Disable NSAutoFillHeuristicController on macOS 26. On macOS 26, the
      // browser process sends synchronized IPC messages to the renderer process
      // on pages with <input> tags. At this point, if the renderer process
      // sends a synchronized IPC message to the browser process, it will cause
      // a deadlock.
      // https://crbug.com/446070423
      // https://crbug.com/446481994
      @"NSAutoFillHeuristicControllerEnabled" : @NO,
    }];
  }

  SetSystemPolicyCrashKeys();
}

}  // namespace content
