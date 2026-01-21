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
    // https://crbug.com/41406192.
    @"NSAppSleepDisabled" : @YES,
  }];

  // Disable NSAutoFillHeuristicController on macOS 26.0 and 26.1. On those OS
  // releases, NSAutoFillHeuristicController triggers a large number of
  // synchronous IME IPCs, which block the main thread and cause stalling and
  // other usability issues. See https://crbug.com/446070423 and
  // https://crbug.com/446481994.
  //
  // "NSAutoFillHeuristicControllerEnabled" no longer exists as of macOS 26.2 so
  // don't bother setting it in that case.
  //
  // TODO(https://crbug.com/452372350): Verify if the main thread stalling
  // issues still exist. If so, determine a new way to disable
  // NSAutoFillHeuristicController, and if not, do further cleanup here.
  if (base::mac::MacOSVersion() >= 26'00'00 &&
      base::mac::MacOSVersion() < 26'02'00) {
    [NSUserDefaults.standardUserDefaults registerDefaults:@{
      @"NSAutoFillHeuristicControllerEnabled" : @NO,
    }];
  }

  SetSystemPolicyCrashKeys();
}

}  // namespace content
