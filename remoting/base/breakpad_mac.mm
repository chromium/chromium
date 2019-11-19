// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
    [breakpad_config setObject:inspector_location
                        forKey:@BREAKPAD_INSPECTOR_LOCATION];
    [breakpad_config setObject:reporter_location
                        forKey:@BREAKPAD_REPORTER_EXE_LOCATION];

    // Configure Breakpad settings here, if they are not already customized in
    // the Info.plist. These settings should be added to the plist, but the
    // problem is that the Breakpad URL contains a double-slash, which is broken
    // by the INFOPLIST_PREPROCESS step.
    // TODO(lambroslambrou): Add these to the Info.plist, similarly to what is
    // done for Chrome Framework - see 'Tweak Info.plist' in
    // chrome/chrome_dll_bundle.gypi.
    if (![breakpad_config objectForKey:@BREAKPAD_SKIP_CONFIRM]) {
      // Skip the upload confirmation dialog, since this is a remote-access
      // service that shouldn't rely on a console user to dismiss any prompt.
      // Also, this may be running in the LoginWindow context, where prompting
      // might not be possible.
      [breakpad_config setObject:@"YES" forKey:@BREAKPAD_SKIP_CONFIRM];
    }
    if (![breakpad_config objectForKey:@BREAKPAD_REPORT_INTERVAL]) {
      // Set a minimum 6-hour interval between crash-reports, to match the
      // throttling used on Windows.
      [breakpad_config setObject:@"21600" forKey:@BREAKPAD_REPORT_INTERVAL];
    }
    if (![breakpad_config objectForKey:@BREAKPAD_URL]) {
      [breakpad_config setObject:@"https://clients2.google.com/cr/report"
                          forKey:@BREAKPAD_URL];
    }

    if (!BreakpadCreate(breakpad_config)) {
      LOG(ERROR) << "Breakpad initialization failed";
    }
  }
}

}  // namespace remoting
