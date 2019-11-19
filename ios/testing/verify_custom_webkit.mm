// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/verify_custom_webkit.h"

#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>

#include "base/command_line.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The switch used when running with custom WebKit frameworks.
const char kRunWithCustomWebKit[] = "run-with-custom-webkit";

}

bool IsCustomWebKitLoadedIfRequested() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kRunWithCustomWebKit)) {
    return true;
  }

  bool foundIncorrectLoadLocation = false;
  NSArray<NSString*>* frameworks = @[
    @"JavaScriptCore.framework/JavaScriptCore",
    @"WebCore.framework/WebCore",
    @"WebKit.framework/WebKit",
    @"WebKitLegacy.framework/WebKitLegacy",
  ];

  uint32_t numImages = _dyld_image_count();
  for (uint32_t i = 0; i < numImages; i++) {
    NSString* imagePath =
        [NSString stringWithUTF8String:_dyld_get_image_name(i)];
    for (NSString* framework in frameworks) {
      if ([imagePath containsString:framework]) {
        // Custom frameworks are bundled inside a "WebKitFrameworks"
        // subdirectory, so look for that string to be present in |full_path|.
        if (![imagePath containsString:@"WebKitFrameworks"]) {
          // This framework was loaded from an unexpected location.
          NSLog(@"Unexpectedly loaded %@ from %@ ", framework, imagePath);
          foundIncorrectLoadLocation = true;
        }
      }
    }
  }

  // Note that checks are not performed on frameworks that were never loaded at
  // all.  This function checks that *if* they were loaded, they were loaded
  // from the correct location.
  return !foundIncorrectLoadLocation;
}
