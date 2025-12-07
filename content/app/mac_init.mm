// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mac_init.h"

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "content/common/mac/system_policy.h"

namespace {

constexpr char kAllowNSAutoFillHeuristicController[] =
    "allow-ns-autofill-heuristic-controller";

}  // namespace

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

  if (base::mac::MacOSVersion() >= 26'00'00 &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAllowNSAutoFillHeuristicController)) {
    [NSUserDefaults.standardUserDefaults registerDefaults:@{
      // Disable NSAutoFillHeuristicController on macOS 26. On macOS 26,
      // NSAutoFillHeuristicController triggers a large number of synchronous
      // IME IPCs, which block the main thread and cause stalling and other
      // usability issues. See https://crbug.com/446070423 and
      // https://crbug.com/446481994.
      //
      // A command-line flag is provided to enable NSAutoFillHeuristicController
      // for testing purposes. A base::Feature isn't used because this function
      // is called too early in startup for that to work.
      //
      // TODO(https://crbug.com/452372350): Figure out a sustainable approach to
      // getting NSAutoFillHeuristicController to work.
      @"NSAutoFillHeuristicControllerEnabled" : @NO,
    }];
  }

  SetSystemPolicyCrashKeys();
}

}  // namespace content
