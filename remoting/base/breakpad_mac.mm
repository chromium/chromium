// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad.h"

#include <Foundation/Foundation.h>

#include "base/logging.h"
#import "third_party/breakpad/breakpad/src/client/mac/Framework/Breakpad.h"

namespace remoting {

void InitializeCrashReporting() {
  @autoreleasepool {
    NSBundle* main_bundle = [NSBundle mainBundle];

    // Tell Breakpad where crash_inspector and crash_report_sender are.
    NSString* resource_path = [main_bundle resourcePath];
    NSString* inspector_location =
        [resource_path stringByAppendingPathComponent:@"crash_inspector"];
    NSString* reporter_bundle_location = [resource_path
        stringByAppendingPathComponent:@"crash_report_sender.app"];
    NSString* reporter_location =
        [[NSBundle bundleWithPath:reporter_bundle_location] executablePath];

    NSDictionary* info_dictionary = [main_bundle infoDictionary];
    NSMutableDictionary* breakpad_config =
        [[info_dictionary mutableCopy] autorelease];
    breakpad_config[@BREAKPAD_INSPECTOR_LOCATION] = inspector_location;
    breakpad_config[@BREAKPAD_REPORTER_EXE_LOCATION] = reporter_location;

    // Configure Breakpad settings here, if they are not already customized in
    // the Info.plist. These settings should be added to the plist, but the
    // problem is that the Breakpad URL contains a double-slash, which is broken
    // by the INFOPLIST_PREPROCESS step.
    // TODO(lambroslambrou): Add these to the Info.plist, similarly to what is
    // done for Chrome Framework - see 'Tweak Info.plist' in
    // chrome/chrome_dll_bundle.gypi.
    if (!breakpad_config[@BREAKPAD_SKIP_CONFIRM]) {
      // Skip the upload confirmation dialog, since this is a remote-access
      // service that shouldn't rely on a console user to dismiss any prompt.
      // Also, this may be running in the LoginWindow context, where prompting
      // might not be possible.
      breakpad_config[@BREAKPAD_SKIP_CONFIRM] = @"YES";
    }
    if (!breakpad_config[@BREAKPAD_REPORT_INTERVAL]) {
      // Set a minimum 6-hour interval between crash-reports, to match the
      // throttling used on Windows.
      breakpad_config[@BREAKPAD_REPORT_INTERVAL] = @"21600";
    }
    if (!breakpad_config[@BREAKPAD_URL]) {
      breakpad_config[@BREAKPAD_URL] = @"https://clients2.google.com/cr/report";
    }

    if (!BreakpadCreate(breakpad_config)) {
      LOG(ERROR) << "Breakpad initialization failed";
    }
  }
}

}  // namespace remoting
